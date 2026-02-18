// dataframe - Max external wrapping the hmdf DataFrame library
// Provides pandas-like columnar data manipulation in Max.
// Uses named instances (like coll/buffer~) for shared data.

extern "C" {
#include "ext.h"
#include "ext_obex.h"
#include "ext_systhread.h"
#include "ext_dictionary.h"
#include "ext_dictobj.h"
}

#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameStatsVisitors.h>

#include <OpenXLSX/OpenXLSX.hpp>

extern "C" {
#include "xlsxwriter.h"
}

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace hmdf;
using ULDataFrame = StdDataFrame<unsigned long>;

// Column type tracking
enum class col_type { DOUBLE, LONG, STRING };

// Store shared by all objects with the same name
struct DataFrameStore {
    ULDataFrame df;
    std::unordered_map<std::string, col_type> col_types;
    size_t refcount = 0;
};

// The Max object struct
typedef struct _dataframe {
    t_object ob;
    void *outlet_data;
    void *outlet_info;
    t_symbol *name;
    std::shared_ptr<DataFrameStore> store;
    t_systhread thread;
} t_dataframe;

// Async I/O args for threaded read/write
struct AsyncIOArgs {
    t_dataframe *x;
    std::string filename;      // original name for messages
    std::string path;          // resolved absolute path
    std::string sheet;         // for xlsx read
    bool is_write;
    // Filled by worker (read only):
    ULDataFrame df;
    std::unordered_map<std::string, col_type> col_types;
    bool ok;
    std::string error_msg;
};

// Globals
static t_class *dataframe_class = nullptr;
static std::unordered_map<t_symbol *, std::shared_ptr<DataFrameStore>> g_registry;
static std::mutex g_registry_mutex;

// Forward declarations
extern "C" void ext_main(void *r);
static void *dataframe_new(t_symbol *s, long argc, t_atom *argv);
static void dataframe_free(t_dataframe *x);
static void dataframe_assist(t_dataframe *x, void *b, long m, long a, char *s);

// I/O
static void dataframe_read(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);
static void dataframe_write(t_dataframe *x, t_symbol *s);
static void dataframe_clear(t_dataframe *x);

// Inspection
static void dataframe_bang(t_dataframe *x);
static void dataframe_columns(t_dataframe *x);
static void dataframe_shape(t_dataframe *x);
static void dataframe_head(t_dataframe *x, long n);
static void dataframe_tail(t_dataframe *x, long n);
static void dataframe_getcol(t_dataframe *x, t_symbol *s);
static void dataframe_describe(t_dataframe *x, t_symbol *s);

// Statistics
static void dataframe_mean(t_dataframe *x, t_symbol *s);
static void dataframe_median(t_dataframe *x, t_symbol *s);
static void dataframe_std(t_dataframe *x, t_symbol *s);
static void dataframe_var(t_dataframe *x, t_symbol *s);
static void dataframe_sum(t_dataframe *x, t_symbol *s);
static void dataframe_min(t_dataframe *x, t_symbol *s);
static void dataframe_max(t_dataframe *x, t_symbol *s);
static void dataframe_count(t_dataframe *x, t_symbol *s);
static void dataframe_corr(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);
static void dataframe_quantile(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Filtering
static void dataframe_sel(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Sorting
static void dataframe_sort(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Groupby
static void dataframe_groupby(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Join
static void dataframe_join(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Data manipulation
static void dataframe_fill_missing(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);
static void dataframe_curvefit(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);
static void dataframe_apply(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Reshape
static void dataframe_melt(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);
static void dataframe_pivot(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);
static void dataframe_transpose(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// Dict interop
static void dataframe_todict(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

static void post_error(t_dataframe *x, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    object_error((t_object *)x, "%s", buf);
    outlet_anything(x->outlet_info, gensym("error"), 0, nullptr);
}

static void post_info(t_dataframe *x, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    object_post((t_object *)x, "%s", buf);
}

static bool has_col(t_dataframe *x, const char *col_name) {
    auto &types = x->store->col_types;
    return types.find(col_name) != types.end();
}

static col_type get_col_type(t_dataframe *x, const char *col_name) {
    return x->store->col_types.at(col_name);
}

// Detect column types after reading a CSV/JSON.
// get_columns_info returns vector of tuple<FixedSizeString<63>, size_t, type_index>
static void detect_column_types(t_dataframe *x) {
    auto &store = *x->store;
    store.col_types.clear();

    auto col_info = store.df.get_columns_info<double, long, std::string>();

    for (auto &info : col_info) {
        std::string name(std::get<0>(info).c_str());
        if (name == DF_INDEX_COL_NAME) continue;

        auto ti = std::get<2>(info);
        if (ti == std::type_index(typeid(double))) {
            store.col_types[name] = col_type::DOUBLE;
        } else if (ti == std::type_index(typeid(long))) {
            store.col_types[name] = col_type::LONG;
        } else if (ti == std::type_index(typeid(std::string))) {
            store.col_types[name] = col_type::STRING;
        }
    }
}

// Get column names (excluding index)
static std::vector<std::string> get_column_names(t_dataframe *x) {
    std::vector<std::string> names;
    for (auto &pair : x->store->col_types) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Output a row as a list (index + all column values)
static void output_row(t_dataframe *x, unsigned long row_idx) {
    auto &store = *x->store;
    auto col_names = get_column_names(x);
    long num_atoms = static_cast<long>(col_names.size());

    std::vector<t_atom> atoms(num_atoms);

    for (long i = 0; i < num_atoms; i++) {
        const auto &name = col_names[i];
        auto ct = store.col_types[name];
        switch (ct) {
            case col_type::DOUBLE: {
                auto &col = store.df.get_column<double>(name.c_str());
                if (row_idx < col.size())
                    atom_setfloat(&atoms[i], col[row_idx]);
                else
                    atom_setfloat(&atoms[i], 0.0);
                break;
            }
            case col_type::LONG: {
                auto &col = store.df.get_column<long>(name.c_str());
                if (row_idx < col.size())
                    atom_setlong(&atoms[i], col[row_idx]);
                else
                    atom_setlong(&atoms[i], 0);
                break;
            }
            case col_type::STRING: {
                auto &col = store.df.get_column<std::string>(name.c_str());
                if (row_idx < col.size())
                    atom_setsym(&atoms[i], gensym(col[row_idx].c_str()));
                else
                    atom_setsym(&atoms[i], gensym(""));
                break;
            }
        }
    }

    outlet_list(x->outlet_data, nullptr, num_atoms, atoms.data());
}

static std::string get_extension(const std::string &path) {
    auto dot = path.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    return "";
}

// Split a CSV line into fields, respecting quoted fields.
static std::vector<std::string> split_csv_line(const std::string &line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);
    return fields;
}

// Trim whitespace from both ends.
static std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Try to detect the type of a string value.
// Returns LONG if it's an integer, DOUBLE if it's a float, STRING otherwise.
static col_type detect_value_type(const std::string &val) {
    if (val.empty()) return col_type::STRING;

    // Try long first (integer)
    try {
        size_t pos = 0;
        std::stol(val, &pos);
        if (pos == val.size()) return col_type::LONG;
    } catch (...) {}

    // Try double
    try {
        size_t pos = 0;
        std::stod(val, &pos);
        if (pos == val.size()) return col_type::DOUBLE;
    } catch (...) {}

    return col_type::STRING;
}

// --------------------------------------------------------------------------
// ext_main
// --------------------------------------------------------------------------

extern "C" void ext_main(void *r) {
    t_class *c = class_new("dataframe",
        (method)dataframe_new, (method)dataframe_free,
        (long)sizeof(t_dataframe), 0L, A_GIMME, 0);

    // I/O
    class_addmethod(c, (method)dataframe_read,    "read",    A_GIMME, 0);
    class_addmethod(c, (method)dataframe_write,   "write",   A_SYM, 0);
    class_addmethod(c, (method)dataframe_clear,   "clear",   0);

    // Inspection
    class_addmethod(c, (method)dataframe_bang,     "bang",     0);
    class_addmethod(c, (method)dataframe_columns,  "columns",  0);
    class_addmethod(c, (method)dataframe_shape,    "shape",    0);
    class_addmethod(c, (method)dataframe_head,     "head",     A_LONG, 0);
    class_addmethod(c, (method)dataframe_tail,     "tail",     A_LONG, 0);
    class_addmethod(c, (method)dataframe_getcol,   "getcol",   A_SYM, 0);
    class_addmethod(c, (method)dataframe_describe, "describe", A_SYM, 0);

    // Statistics
    class_addmethod(c, (method)dataframe_mean,   "mean",   A_SYM, 0);
    class_addmethod(c, (method)dataframe_median, "median", A_SYM, 0);
    class_addmethod(c, (method)dataframe_std,    "std",    A_SYM, 0);
    class_addmethod(c, (method)dataframe_var,    "var",    A_SYM, 0);
    class_addmethod(c, (method)dataframe_sum,    "sum",    A_SYM, 0);
    class_addmethod(c, (method)dataframe_min,    "min",    A_SYM, 0);
    class_addmethod(c, (method)dataframe_max,    "max",    A_SYM, 0);
    class_addmethod(c, (method)dataframe_count,  "count",  A_SYM, 0);
    class_addmethod(c, (method)dataframe_corr,     "corr",     A_GIMME, 0);
    class_addmethod(c, (method)dataframe_quantile,  "quantile",  A_GIMME, 0);

    // Filtering
    class_addmethod(c, (method)dataframe_sel,    "sel",    A_GIMME, 0);

    // Sorting
    class_addmethod(c, (method)dataframe_sort,   "sort",   A_GIMME, 0);

    // Groupby
    class_addmethod(c, (method)dataframe_groupby, "groupby", A_GIMME, 0);

    // Join
    class_addmethod(c, (method)dataframe_join,   "join",   A_GIMME, 0);

    // Data manipulation
    class_addmethod(c, (method)dataframe_fill_missing, "fill_missing", A_GIMME, 0);
    class_addmethod(c, (method)dataframe_curvefit,     "curvefit",     A_GIMME, 0);
    class_addmethod(c, (method)dataframe_apply,        "apply",        A_GIMME, 0);

    // Reshape
    class_addmethod(c, (method)dataframe_melt,         "melt",         A_GIMME, 0);
    class_addmethod(c, (method)dataframe_pivot,        "pivot",        A_GIMME, 0);
    class_addmethod(c, (method)dataframe_transpose,    "transpose",    A_GIMME, 0);

    // Dict interop
    class_addmethod(c, (method)dataframe_todict,       "todict",       A_GIMME, 0);

    // Assist
    class_addmethod(c, (method)dataframe_assist, "assist", A_CANT, 0);

    class_register(CLASS_BOX, c);
    dataframe_class = c;
}

// --------------------------------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------------------------------

static void *dataframe_new(t_symbol *s, long argc, t_atom *argv) {
    t_dataframe *x = (t_dataframe *)object_alloc(dataframe_class);
    if (!x) return nullptr;

    // Default name
    x->name = gensym("default");

    // Parse first argument as name (positional, not @attr)
    if (argc > 0 && atom_gettype(argv) == A_SYM) {
        x->name = atom_getsym(argv);
    }

    // Create outlets (right to left)
    x->outlet_info = outlet_new(x, nullptr);
    x->outlet_data = outlet_new(x, nullptr);

    // Named instance registry
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        auto it = g_registry.find(x->name);
        if (it != g_registry.end()) {
            x->store = it->second;
        } else {
            x->store = std::make_shared<DataFrameStore>();
            g_registry[x->name] = x->store;
        }
        x->store->refcount++;
    }

    x->thread = nullptr;

    post_info(x, "dataframe: instance '%s' (refcount: %zu)",
              x->name->s_name, x->store->refcount);

    return x;
}

static void dataframe_free(t_dataframe *x) {
    if (x->thread) {
        unsigned int rv;
        systhread_join(x->thread, &rv);
        x->thread = nullptr;
    }
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    if (x->store) {
        x->store->refcount--;
        if (x->store->refcount == 0) {
            g_registry.erase(x->name);
        }
        x->store.reset();
    }
}

static void dataframe_assist(t_dataframe *x, void *b, long m, long a, char *s) {
    if (m == ASSIST_INLET) {
        snprintf(s, 256, "messages: read, write, clear, bang, columns, shape, "
                 "head, tail, getcol, mean, median, std, var, sum, min, max, "
                 "count, describe, corr, quantile, sel, sort, groupby, join, "
                 "fill_missing, curvefit, apply, melt, pivot, transpose, todict");
    } else {
        switch (a) {
            case 0: snprintf(s, 256, "data output (lists, floats, symbols)"); break;
            case 1: snprintf(s, 256, "info output (bang on completion, errors)"); break;
        }
    }
}

// --------------------------------------------------------------------------
// I/O
// --------------------------------------------------------------------------

// Resolve a filename through Max's search path.
// Returns the absolute system path, or empty string on failure.
static std::string resolve_path(t_dataframe *x, const char *name) {
    char filename[MAX_PATH_CHARS];
    short path_id = 0;
    t_fourcc type = 0;

    strncpy_zero(filename, name, MAX_PATH_CHARS);

    if (locatefile_extended(filename, &path_id, &type, nullptr, 0)) {
        post_error(x, "dataframe: file '%s' not found", name);
        return "";
    }

    char fullpath[MAX_PATH_CHARS];
    if (path_toabsolutesystempath(path_id, filename, fullpath)) {
        post_error(x, "dataframe: could not resolve path for '%s'", name);
        return "";
    }

    return std::string(fullpath);
}

// Worker-thread helpers for read_plain_csv and read_xlsx that operate on
// a standalone ULDataFrame + col_types map (no t_dataframe access).
static bool read_plain_csv_into(const std::string &path, ULDataFrame &df,
                                std::unordered_map<std::string, col_type> &types) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    if (!std::getline(file, line)) return false;
    auto headers = split_csv_line(line);
    size_t ncols = headers.size();
    for (auto &h : headers) h = trim(h);

    std::vector<std::vector<std::string>> rows;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;
        auto fields = split_csv_line(line);
        fields.resize(ncols);
        for (auto &f : fields) f = trim(f);
        rows.push_back(std::move(fields));
    }

    size_t nrows = rows.size();
    if (nrows == 0 || ncols == 0) return false;

    std::vector<col_type> ctypes(ncols, col_type::LONG);
    for (size_t col = 0; col < ncols; col++) {
        for (size_t row = 0; row < nrows; row++) {
            auto vt = detect_value_type(rows[row][col]);
            if (vt == col_type::STRING) { ctypes[col] = col_type::STRING; break; }
            if (vt == col_type::DOUBLE && ctypes[col] == col_type::LONG)
                ctypes[col] = col_type::DOUBLE;
        }
    }

    df = ULDataFrame();
    types.clear();
    std::vector<unsigned long> idx(nrows);
    for (size_t i = 0; i < nrows; i++) idx[i] = static_cast<unsigned long>(i);
    df.load_index(std::move(idx));

    for (size_t col = 0; col < ncols; col++) {
        const auto &name = headers[col];
        switch (ctypes[col]) {
            case col_type::DOUBLE: {
                std::vector<double> vec(nrows);
                for (size_t row = 0; row < nrows; row++) {
                    try { vec[row] = std::stod(rows[row][col]); }
                    catch (...) { vec[row] = 0.0; }
                }
                df.load_column(name.c_str(), std::move(vec));
                types[name] = col_type::DOUBLE;
                break;
            }
            case col_type::LONG: {
                std::vector<long> vec(nrows);
                for (size_t row = 0; row < nrows; row++) {
                    try { vec[row] = std::stol(rows[row][col]); }
                    catch (...) { vec[row] = 0; }
                }
                df.load_column(name.c_str(), std::move(vec));
                types[name] = col_type::LONG;
                break;
            }
            case col_type::STRING: {
                std::vector<std::string> vec(nrows);
                for (size_t row = 0; row < nrows; row++) vec[row] = rows[row][col];
                df.load_column(name.c_str(), std::move(vec));
                types[name] = col_type::STRING;
                break;
            }
        }
    }
    return true;
}

static bool read_xlsx_into(const std::string &path, const std::string &sheet_name,
                           ULDataFrame &df,
                           std::unordered_map<std::string, col_type> &types) {
    OpenXLSX::XLDocument doc;
    doc.open(path);
    auto wb = doc.workbook();
    if (wb.worksheetCount() == 0) return false;

    auto ws = sheet_name.empty() ? wb.worksheet(1) : wb.worksheet(sheet_name);
    uint32_t nrows_total = ws.rowCount();
    uint16_t ncols = ws.columnCount();
    if (nrows_total < 1 || ncols == 0) return false;

    std::vector<std::string> headers(ncols);
    for (uint16_t col = 1; col <= ncols; col++) {
        auto cell = ws.cell(1, col);
        const auto &val = cell.value();
        if (val.type() == OpenXLSX::XLValueType::String)
            headers[col - 1] = val.get<std::string>();
        else
            headers[col - 1] = cell.getString();
    }

    uint32_t nrows = nrows_total - 1;
    df = ULDataFrame();
    types.clear();

    if (nrows == 0) {
        std::vector<unsigned long> idx;
        df.load_index(std::move(idx));
        for (uint16_t col = 0; col < ncols; col++) {
            std::vector<std::string> empty;
            df.load_column(headers[col].c_str(), std::move(empty));
            types[headers[col]] = col_type::STRING;
        }
        doc.close();
        return true;
    }

    std::vector<col_type> ctypes(ncols, col_type::LONG);
    std::vector<std::vector<std::string>> raw(ncols, std::vector<std::string>(nrows));

    for (uint32_t row = 2; row <= nrows_total; row++) {
        for (uint16_t col = 1; col <= ncols; col++) {
            auto cell = ws.cell(row, col);
            const auto &val = cell.value();
            uint16_t ci = col - 1;
            uint32_t ri = row - 2;
            switch (val.type()) {
                case OpenXLSX::XLValueType::Integer:
                    raw[ci][ri] = std::to_string(val.get<int64_t>());
                    break;
                case OpenXLSX::XLValueType::Float:
                    raw[ci][ri] = std::to_string(val.get<double>());
                    if (ctypes[ci] == col_type::LONG) ctypes[ci] = col_type::DOUBLE;
                    break;
                case OpenXLSX::XLValueType::Boolean:
                    raw[ci][ri] = val.get<bool>() ? "1" : "0";
                    break;
                case OpenXLSX::XLValueType::String:
                    raw[ci][ri] = val.get<std::string>();
                    ctypes[ci] = col_type::STRING;
                    break;
                case OpenXLSX::XLValueType::Empty:
                    raw[ci][ri] = "";
                    break;
                default:
                    raw[ci][ri] = "";
                    ctypes[ci] = col_type::STRING;
                    break;
            }
        }
    }
    doc.close();

    std::vector<unsigned long> idx(nrows);
    for (uint32_t i = 0; i < nrows; i++) idx[i] = static_cast<unsigned long>(i);
    df.load_index(std::move(idx));

    for (uint16_t col = 0; col < ncols; col++) {
        const auto &name = headers[col];
        switch (ctypes[col]) {
            case col_type::DOUBLE: {
                std::vector<double> vec(nrows);
                for (uint32_t row = 0; row < nrows; row++) {
                    try { vec[row] = std::stod(raw[col][row]); }
                    catch (...) { vec[row] = 0.0; }
                }
                df.load_column(name.c_str(), std::move(vec));
                types[name] = col_type::DOUBLE;
                break;
            }
            case col_type::LONG: {
                std::vector<long> vec(nrows);
                for (uint32_t row = 0; row < nrows; row++) {
                    try { vec[row] = std::stol(raw[col][row]); }
                    catch (...) { vec[row] = 0; }
                }
                df.load_column(name.c_str(), std::move(vec));
                types[name] = col_type::LONG;
                break;
            }
            case col_type::STRING: {
                std::vector<std::string> vec(nrows);
                for (uint32_t row = 0; row < nrows; row++) vec[row] = raw[col][row];
                df.load_column(name.c_str(), std::move(vec));
                types[name] = col_type::STRING;
                break;
            }
        }
    }
    return true;
}

// Deferred completion callback for read (runs on main thread)
static void dataframe_read_done(t_dataframe *x, t_symbol *, short argc, t_atom *argv) {
    AsyncIOArgs *args = reinterpret_cast<AsyncIOArgs *>(atom_getlong(argv));
    if (args->ok) {
        x->store->df = std::move(args->df);
        x->store->col_types = std::move(args->col_types);
        auto nrows = x->store->df.get_index().size();
        auto ncols = x->store->col_types.size();
        post_info(x, "dataframe: read %zu rows x %zu columns from '%s'",
                  nrows, ncols, args->filename.c_str());
        outlet_bang(x->outlet_info);
    } else {
        if (!args->error_msg.empty())
            post_error(x, "dataframe: read failed: %s", args->error_msg.c_str());
        else
            post_error(x, "dataframe: failed to read '%s'", args->filename.c_str());
    }
    x->thread = nullptr;
    delete args;
}

// Read worker thread entry point
static void *dataframe_read_worker(void *arg) {
    AsyncIOArgs *a = static_cast<AsyncIOArgs *>(arg);
    try {
        std::string ext = get_extension(a->path);
        if (ext == "xlsx") {
            a->ok = read_xlsx_into(a->path, a->sheet, a->df, a->col_types);
        } else if (ext == "json") {
            a->df = ULDataFrame();
            a->col_types.clear();
            a->df.read(a->path.c_str(), io_format::json);
            auto col_info = a->df.get_columns_info<double, long, std::string>();
            for (auto &info : col_info) {
                std::string name(std::get<0>(info).c_str());
                if (name == DF_INDEX_COL_NAME) continue;
                auto ti = std::get<2>(info);
                if (ti == std::type_index(typeid(double)))
                    a->col_types[name] = col_type::DOUBLE;
                else if (ti == std::type_index(typeid(long)))
                    a->col_types[name] = col_type::LONG;
                else if (ti == std::type_index(typeid(std::string)))
                    a->col_types[name] = col_type::STRING;
            }
            a->ok = true;
        } else {
            a->ok = read_plain_csv_into(a->path, a->df, a->col_types);
        }
    } catch (const std::exception &e) {
        a->ok = false;
        a->error_msg = e.what();
    }
    t_atom av;
    atom_setlong(&av, reinterpret_cast<t_atom_long>(a));
    defer_low(a->x, (method)dataframe_read_done, nullptr, 1, &av);
    systhread_exit(0);
    return nullptr;
}

// Deferred completion callback for write (runs on main thread)
static void dataframe_write_done(t_dataframe *x, t_symbol *, short argc, t_atom *argv) {
    AsyncIOArgs *args = reinterpret_cast<AsyncIOArgs *>(atom_getlong(argv));
    if (args->ok) {
        post_info(x, "dataframe: wrote to '%s'", args->filename.c_str());
        outlet_bang(x->outlet_info);
    } else {
        if (!args->error_msg.empty())
            post_error(x, "dataframe: write failed: %s", args->error_msg.c_str());
        else
            post_error(x, "dataframe: failed to write '%s'", args->filename.c_str());
    }
    x->thread = nullptr;
    delete args;
}

// Write worker thread entry point
static void *dataframe_write_worker(void *arg) {
    AsyncIOArgs *a = static_cast<AsyncIOArgs *>(arg);
    try {
        std::string ext = get_extension(a->path);
        if (ext == "xlsx") {
            // We need the store data, which was copied into a->df / a->col_types
            // but write_xlsx works on t_dataframe. Instead, just do the write
            // directly here using the copied data.
            auto &df = a->df;
            auto &col_types = a->col_types;

            // Get sorted column names
            std::vector<std::string> col_names;
            for (auto &pair : col_types) col_names.push_back(pair.first);
            std::sort(col_names.begin(), col_names.end());

            size_t ncols = col_names.size();
            size_t nrows = df.get_index().size();

            lxw_workbook *wb = workbook_new(a->path.c_str());
            if (!wb) { a->ok = false; }
            else {
                lxw_worksheet *ws = workbook_add_worksheet(wb, nullptr);
                for (size_t col = 0; col < ncols; col++)
                    worksheet_write_string(ws, 0, static_cast<lxw_col_t>(col),
                                           col_names[col].c_str(), nullptr);
                for (size_t row = 0; row < nrows; row++) {
                    lxw_row_t r = static_cast<lxw_row_t>(row + 1);
                    for (size_t col = 0; col < ncols; col++) {
                        lxw_col_t c = static_cast<lxw_col_t>(col);
                        const auto &name = col_names[col];
                        switch (col_types[name]) {
                            case col_type::DOUBLE:
                                worksheet_write_number(ws, r, c,
                                    df.get_column<double>(name.c_str())[row], nullptr);
                                break;
                            case col_type::LONG:
                                worksheet_write_number(ws, r, c,
                                    static_cast<double>(df.get_column<long>(name.c_str())[row]),
                                    nullptr);
                                break;
                            case col_type::STRING:
                                worksheet_write_string(ws, r, c,
                                    df.get_column<std::string>(name.c_str())[row].c_str(),
                                    nullptr);
                                break;
                        }
                    }
                }
                a->ok = (workbook_close(wb) == LXW_NO_ERROR);
            }
        } else if (ext == "json") {
            a->df.write<double, long, std::string>(a->path.c_str(), io_format::json);
            a->ok = true;
        } else {
            // CSV
            auto &df = a->df;
            auto &col_types = a->col_types;
            std::vector<std::string> col_names;
            for (auto &pair : col_types) col_names.push_back(pair.first);
            std::sort(col_names.begin(), col_names.end());

            std::ofstream file(a->path);
            if (!file.is_open()) { a->ok = false; }
            else {
                size_t ncols = col_names.size();
                size_t nrows = df.get_index().size();
                for (size_t i = 0; i < ncols; i++) {
                    if (i > 0) file << ',';
                    file << col_names[i];
                }
                file << '\n';
                for (size_t row = 0; row < nrows; row++) {
                    for (size_t col = 0; col < ncols; col++) {
                        if (col > 0) file << ',';
                        const auto &name = col_names[col];
                        switch (col_types[name]) {
                            case col_type::DOUBLE:
                                file << df.get_column<double>(name.c_str())[row];
                                break;
                            case col_type::LONG:
                                file << df.get_column<long>(name.c_str())[row];
                                break;
                            case col_type::STRING: {
                                const auto &val = df.get_column<std::string>(name.c_str())[row];
                                if (val.find(',') != std::string::npos ||
                                    val.find('"') != std::string::npos) {
                                    file << '"';
                                    for (char ch : val) {
                                        if (ch == '"') file << '"';
                                        file << ch;
                                    }
                                    file << '"';
                                } else {
                                    file << val;
                                }
                                break;
                            }
                        }
                    }
                    file << '\n';
                }
                a->ok = true;
            }
        }
    } catch (const std::exception &e) {
        a->ok = false;
        a->error_msg = e.what();
    }

    t_atom av;
    atom_setlong(&av, reinterpret_cast<t_atom_long>(a));
    defer_low(a->x, (method)dataframe_write_done, nullptr, 1, &av);
    systhread_exit(0);
    return nullptr;
}

static void dataframe_read(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 1 || atom_gettype(argv) != A_SYM) {
        post_error(x, "dataframe: read requires <file> [sheet]");
        return;
    }

    if (x->thread) {
        post_error(x, "dataframe: I/O already in progress");
        return;
    }

    t_symbol *file_sym = atom_getsym(argv);
    std::string sheet_name;
    if (argc >= 2 && atom_gettype(argv + 1) == A_SYM)
        sheet_name = atom_getsym(argv + 1)->s_name;

    std::string path = resolve_path(x, file_sym->s_name);
    if (path.empty()) return;

    AsyncIOArgs *a = new AsyncIOArgs();
    a->x = x;
    a->filename = file_sym->s_name;
    a->path = path;
    a->sheet = sheet_name;
    a->is_write = false;
    a->ok = false;

    systhread_create((method)dataframe_read_worker, a, 0, 0, 0, &x->thread);
}

static void dataframe_write(t_dataframe *x, t_symbol *s) {
    if (x->thread) {
        post_error(x, "dataframe: I/O already in progress");
        return;
    }

    std::string path(s->s_name);

    // Snapshot the data for the worker thread
    AsyncIOArgs *a = new AsyncIOArgs();
    a->x = x;
    a->filename = s->s_name;
    a->path = path;
    a->is_write = true;
    a->ok = false;

    // Copy current DataFrame data for thread-safe write
    a->df = x->store->df;
    a->col_types = x->store->col_types;

    systhread_create((method)dataframe_write_worker, a, 0, 0, 0, &x->thread);
}

static void dataframe_clear(t_dataframe *x) {
    x->store->df = ULDataFrame();
    x->store->col_types.clear();
    post_info(x, "dataframe: cleared");
    outlet_bang(x->outlet_info);
}

// --------------------------------------------------------------------------
// Inspection
// --------------------------------------------------------------------------

static void dataframe_bang(t_dataframe *x) {
    auto nrows = x->store->df.get_index().size();
    auto ncols = x->store->col_types.size();

    // Output shape as list from info outlet
    t_atom shape[2];
    atom_setlong(&shape[0], static_cast<long>(nrows));
    atom_setlong(&shape[1], static_cast<long>(ncols));
    outlet_anything(x->outlet_info, gensym("shape"), 2, shape);

    // Output column names
    auto names = get_column_names(x);
    std::vector<t_atom> atoms(names.size());
    for (size_t i = 0; i < names.size(); i++) {
        atom_setsym(&atoms[i], gensym(names[i].c_str()));
    }
    if (!atoms.empty()) {
        outlet_anything(x->outlet_info, gensym("columns"),
                        static_cast<long>(atoms.size()), atoms.data());
    }
}

static void dataframe_columns(t_dataframe *x) {
    auto names = get_column_names(x);
    std::vector<t_atom> atoms(names.size());
    for (size_t i = 0; i < names.size(); i++) {
        atom_setsym(&atoms[i], gensym(names[i].c_str()));
    }
    if (!atoms.empty()) {
        outlet_list(x->outlet_data, nullptr,
                    static_cast<long>(atoms.size()), atoms.data());
    }
}

static void dataframe_shape(t_dataframe *x) {
    auto nrows = x->store->df.get_index().size();
    auto ncols = x->store->col_types.size();

    t_atom shape[2];
    atom_setlong(&shape[0], static_cast<long>(nrows));
    atom_setlong(&shape[1], static_cast<long>(ncols));
    outlet_list(x->outlet_data, nullptr, 2, shape);
}

static void dataframe_head(t_dataframe *x, long n) {
    if (n <= 0) n = 5;
    auto nrows = x->store->df.get_index().size();
    long count = std::min(static_cast<long>(nrows), n);

    for (long i = 0; i < count; i++) {
        output_row(x, static_cast<unsigned long>(i));
    }
    outlet_bang(x->outlet_info);
}

static void dataframe_tail(t_dataframe *x, long n) {
    if (n <= 0) n = 5;
    auto nrows = static_cast<long>(x->store->df.get_index().size());
    long start = std::max(0L, nrows - n);

    for (long i = start; i < nrows; i++) {
        output_row(x, static_cast<unsigned long>(i));
    }
    outlet_bang(x->outlet_info);
}

static void dataframe_getcol(t_dataframe *x, t_symbol *s) {
    const char *col_name = s->s_name;

    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found", col_name);
        return;
    }

    try {
        auto ct = get_col_type(x, col_name);
        switch (ct) {
            case col_type::DOUBLE: {
                auto &col = x->store->df.get_column<double>(col_name);
                std::vector<t_atom> atoms(col.size());
                for (size_t i = 0; i < col.size(); i++) {
                    atom_setfloat(&atoms[i], col[i]);
                }
                outlet_list(x->outlet_data, nullptr,
                            static_cast<long>(atoms.size()), atoms.data());
                break;
            }
            case col_type::LONG: {
                auto &col = x->store->df.get_column<long>(col_name);
                std::vector<t_atom> atoms(col.size());
                for (size_t i = 0; i < col.size(); i++) {
                    atom_setlong(&atoms[i], col[i]);
                }
                outlet_list(x->outlet_data, nullptr,
                            static_cast<long>(atoms.size()), atoms.data());
                break;
            }
            case col_type::STRING: {
                auto &col = x->store->df.get_column<std::string>(col_name);
                std::vector<t_atom> atoms(col.size());
                for (size_t i = 0; i < col.size(); i++) {
                    atom_setsym(&atoms[i], gensym(col[i].c_str()));
                }
                outlet_list(x->outlet_data, nullptr,
                            static_cast<long>(atoms.size()), atoms.data());
                break;
            }
        }
    } catch (const std::exception &e) {
        post_error(x, "dataframe: getcol '%s' failed: %s", col_name, e.what());
    }
}

// --------------------------------------------------------------------------
// Statistics
// --------------------------------------------------------------------------

// Helper: ensure column exists and is numeric (double or long)
static bool require_numeric_col(t_dataframe *x, const char *col_name) {
    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found", col_name);
        return false;
    }
    auto ct = get_col_type(x, col_name);
    if (ct != col_type::DOUBLE && ct != col_type::LONG) {
        post_error(x, "dataframe: column '%s' is not numeric", col_name);
        return false;
    }
    return true;
}

// Helper: ensure column exists and is strictly double (for in-place mutation)
static bool require_double_col(t_dataframe *x, const char *col_name) {
    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found", col_name);
        return false;
    }
    if (get_col_type(x, col_name) != col_type::DOUBLE) {
        post_error(x, "dataframe: column '%s' is not double", col_name);
        return false;
    }
    return true;
}

// Helper: build a temp DataFrame with specified columns promoted to double.
// Used by stat functions to handle long columns transparently.
static ULDataFrame make_numeric_tmp(t_dataframe *x,
                                    std::initializer_list<const char *> col_names) {
    ULDataFrame tmp;
    std::vector<unsigned long> idx(x->store->df.get_index());
    tmp.load_index(std::move(idx));

    for (auto name : col_names) {
        if (get_col_type(x, name) == col_type::DOUBLE) {
            auto &c = x->store->df.get_column<double>(name);
            tmp.load_column(name, std::vector<double>(c.begin(), c.end()));
        } else {
            auto &c = x->store->df.get_column<long>(name);
            tmp.load_column(name, std::vector<double>(c.begin(), c.end()));
        }
    }
    return tmp;
}

// Helper: visit a numeric column with a double-typed visitor.
// Long columns are promoted to double via a temporary DataFrame.
template<typename Visitor>
static void visit_numeric(t_dataframe *x, const char *col_name, Visitor &v) {
    if (get_col_type(x, col_name) == col_type::DOUBLE) {
        x->store->df.visit<double>(col_name, v);
    } else {
        auto tmp = make_numeric_tmp(x, {col_name});
        tmp.visit<double>(col_name, v);
    }
}

static void dataframe_mean(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        MeanVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: mean failed: %s", e.what());
    }
}

static void dataframe_median(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        MedianVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: median failed: %s", e.what());
    }
}

static void dataframe_std(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        StdVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: std failed: %s", e.what());
    }
}

static void dataframe_var(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        VarVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: var failed: %s", e.what());
    }
}

static void dataframe_sum(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        SumVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: sum failed: %s", e.what());
    }
}

static void dataframe_min(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        MinVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: min failed: %s", e.what());
    }
}

static void dataframe_max(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;
    try {
        MaxVisitor<double, unsigned long> v;
        visit_numeric(x, s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: max failed: %s", e.what());
    }
}

static void dataframe_count(t_dataframe *x, t_symbol *s) {
    if (!has_col(x, s->s_name)) {
        post_error(x, "dataframe: column '%s' not found", s->s_name);
        return;
    }
    try {
        auto ct = get_col_type(x, s->s_name);
        long result = 0;
        switch (ct) {
            case col_type::DOUBLE: {
                CountVisitor<double, unsigned long> v;
                x->store->df.visit<double>(s->s_name, v);
                result = static_cast<long>(v.get_result());
                break;
            }
            case col_type::LONG: {
                CountVisitor<long, unsigned long> v;
                x->store->df.visit<long>(s->s_name, v);
                result = static_cast<long>(v.get_result());
                break;
            }
            case col_type::STRING: {
                CountVisitor<std::string, unsigned long> v;
                x->store->df.visit<std::string>(s->s_name, v);
                result = static_cast<long>(v.get_result());
                break;
            }
        }
        outlet_int(x->outlet_data, result);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: count failed: %s", e.what());
    }
}

static void dataframe_describe(t_dataframe *x, t_symbol *s) {
    if (!require_numeric_col(x, s->s_name)) return;

    try {
        const char *col_name = s->s_name;

        MeanVisitor<double, unsigned long> mean_v;
        MedianVisitor<double, unsigned long> median_v;
        StdVisitor<double, unsigned long> std_v;
        VarVisitor<double, unsigned long> var_v;
        MinVisitor<double, unsigned long> min_v;
        MaxVisitor<double, unsigned long> max_v;
        CountVisitor<double, unsigned long> count_v;
        SumVisitor<double, unsigned long> sum_v;

        visit_numeric(x, col_name, mean_v);
        visit_numeric(x, col_name, median_v);
        visit_numeric(x, col_name, std_v);
        visit_numeric(x, col_name, var_v);
        visit_numeric(x, col_name, min_v);
        visit_numeric(x, col_name, max_v);
        visit_numeric(x, col_name, count_v);
        visit_numeric(x, col_name, sum_v);

        // Output as key-value pairs via info outlet
        t_atom av[2];

        atom_setsym(&av[0], gensym("count"));
        atom_setlong(&av[1], static_cast<long>(count_v.get_result()));
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("mean"));
        atom_setfloat(&av[1], mean_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("median"));
        atom_setfloat(&av[1], median_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("std"));
        atom_setfloat(&av[1], std_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("var"));
        atom_setfloat(&av[1], var_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("min"));
        atom_setfloat(&av[1], min_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("max"));
        atom_setfloat(&av[1], max_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        atom_setsym(&av[0], gensym("sum"));
        atom_setfloat(&av[1], sum_v.get_result());
        outlet_anything(x->outlet_info, gensym("describe"), 2, av);

        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: describe failed: %s", e.what());
    }
}

static void dataframe_corr(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 2) {
        post_error(x, "dataframe: corr requires <col1> <col2>");
        return;
    }

    const char *col1 = atom_getsym(argv)->s_name;
    const char *col2 = atom_getsym(argv + 1)->s_name;

    if (!require_numeric_col(x, col1)) return;
    if (!require_numeric_col(x, col2)) return;

    try {
        CorrVisitor<double, unsigned long> v;
        auto ct1 = get_col_type(x, col1);
        auto ct2 = get_col_type(x, col2);
        if (ct1 == col_type::DOUBLE && ct2 == col_type::DOUBLE) {
            x->store->df.visit<double, double>(col1, col2, v);
        } else {
            auto tmp = make_numeric_tmp(x, {col1, col2});
            tmp.visit<double, double>(col1, col2, v);
        }
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: corr failed: %s", e.what());
    }
}

static void dataframe_quantile(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 2) {
        post_error(x, "dataframe: quantile requires <col> <value>");
        return;
    }

    const char *col_name = atom_getsym(argv)->s_name;
    double qt = atom_getfloat(argv + 1);

    if (!require_numeric_col(x, col_name)) return;

    if (qt < 0.0 || qt > 1.0) {
        post_error(x, "dataframe: quantile value must be in [0.0, 1.0]");
        return;
    }

    try {
        QuantileVisitor<double, unsigned long> v(qt);
        visit_numeric(x, col_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: quantile failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Filtering
// --------------------------------------------------------------------------

// sel <col> <op> <value>
// Operators: >, <, >=, <=, ==, !=
// Replaces the current DataFrame with the filtered result.
static void dataframe_sel(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 3) {
        post_error(x, "dataframe: sel requires <col> <op> <value>");
        return;
    }

    t_symbol *col_sym = atom_getsym(argv);
    t_symbol *op_sym = atom_getsym(argv + 1);
    const char *col_name = col_sym->s_name;
    const char *op = op_sym->s_name;

    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found", col_name);
        return;
    }

    auto ct = get_col_type(x, col_name);
    if (ct != col_type::DOUBLE) {
        post_error(x, "dataframe: sel only supports double columns");
        return;
    }

    double threshold = atom_getfloat(argv + 2);

    try {
        ULDataFrame filtered;
        std::string op_str(op);

        if (op_str == ">") {
            auto fn = [threshold](const unsigned long &, const double &val) -> bool {
                return val > threshold;
            };
            filtered = x->store->df.get_data_by_sel<double, decltype(fn),
                double, long, std::string>(col_name, fn);
        } else if (op_str == "<") {
            auto fn = [threshold](const unsigned long &, const double &val) -> bool {
                return val < threshold;
            };
            filtered = x->store->df.get_data_by_sel<double, decltype(fn),
                double, long, std::string>(col_name, fn);
        } else if (op_str == ">=") {
            auto fn = [threshold](const unsigned long &, const double &val) -> bool {
                return val >= threshold;
            };
            filtered = x->store->df.get_data_by_sel<double, decltype(fn),
                double, long, std::string>(col_name, fn);
        } else if (op_str == "<=") {
            auto fn = [threshold](const unsigned long &, const double &val) -> bool {
                return val <= threshold;
            };
            filtered = x->store->df.get_data_by_sel<double, decltype(fn),
                double, long, std::string>(col_name, fn);
        } else if (op_str == "==") {
            auto fn = [threshold](const unsigned long &, const double &val) -> bool {
                return std::abs(val - threshold) < 1e-10;
            };
            filtered = x->store->df.get_data_by_sel<double, decltype(fn),
                double, long, std::string>(col_name, fn);
        } else if (op_str == "!=") {
            auto fn = [threshold](const unsigned long &, const double &val) -> bool {
                return std::abs(val - threshold) >= 1e-10;
            };
            filtered = x->store->df.get_data_by_sel<double, decltype(fn),
                double, long, std::string>(col_name, fn);
        } else {
            post_error(x, "dataframe: unknown operator '%s' "
                       "(use >, <, >=, <=, ==, !=)", op);
            return;
        }

        // Replace current DataFrame with filtered result
        x->store->df = std::move(filtered);
        detect_column_types(x);

        auto nrows = x->store->df.get_index().size();
        post_info(x, "dataframe: sel result: %zu rows", nrows);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: sel failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Sorting
// --------------------------------------------------------------------------

// sort <col> [asc|desc]
// Sorts the DataFrame in-place by the given column.
// Default direction is ascending.
static void dataframe_sort(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 1) {
        post_error(x, "dataframe: sort requires <col> [asc|desc]");
        return;
    }

    t_symbol *col_sym = atom_getsym(argv);
    const char *col_name = col_sym->s_name;

    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found", col_name);
        return;
    }

    sort_spec dir = sort_spec::ascen;
    if (argc >= 2 && atom_gettype(argv + 1) == A_SYM) {
        std::string dir_str(atom_getsym(argv + 1)->s_name);
        if (dir_str == "desc" || dir_str == "descending")
            dir = sort_spec::desce;
    }

    try {
        auto ct = get_col_type(x, col_name);
        switch (ct) {
            case col_type::DOUBLE:
                x->store->df.sort<double, double, long, std::string>(
                    col_name, dir);
                break;
            case col_type::LONG:
                x->store->df.sort<long, double, long, std::string>(
                    col_name, dir);
                break;
            case col_type::STRING:
                x->store->df.sort<std::string, double, long, std::string>(
                    col_name, dir);
                break;
        }

        post_info(x, "dataframe: sorted by '%s' %s", col_name,
                  dir == sort_spec::ascen ? "ascending" : "descending");
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: sort failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Groupby
// --------------------------------------------------------------------------

// groupby <group_col> <agg> <value_col>
// agg: sum, mean, count, min, max
// Groups by group_col, applies agg to value_col.
// Replaces current DataFrame with the grouped result.
// The result has one row per unique group value.
static void dataframe_groupby(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 3) {
        post_error(x, "dataframe: groupby requires <group_col> <agg> <value_col>");
        return;
    }

    const char *group_col = atom_getsym(argv)->s_name;
    const char *agg_name = atom_getsym(argv + 1)->s_name;
    const char *value_col = atom_getsym(argv + 2)->s_name;

    if (!has_col(x, group_col)) {
        post_error(x, "dataframe: column '%s' not found", group_col);
        return;
    }
    if (!has_col(x, value_col)) {
        post_error(x, "dataframe: column '%s' not found", value_col);
        return;
    }

    auto val_ct = get_col_type(x, value_col);
    std::string agg(agg_name);

    // count works on any type; other aggs require numeric
    if (agg != "count" && val_ct != col_type::DOUBLE) {
        post_error(x, "dataframe: groupby %s requires a double column", agg_name);
        return;
    }

    try {
        auto &store = *x->store;
        auto grp_ct = get_col_type(x, group_col);
        size_t nrows = store.df.get_index().size();

        // Build group mapping: group_key -> list of row indices
        // Use string keys for simplicity (works for all types)
        std::vector<std::string> group_keys(nrows);
        for (size_t i = 0; i < nrows; i++) {
            switch (grp_ct) {
                case col_type::DOUBLE: {
                    auto &col = store.df.get_column<double>(group_col);
                    group_keys[i] = std::to_string(col[i]);
                    break;
                }
                case col_type::LONG: {
                    auto &col = store.df.get_column<long>(group_col);
                    group_keys[i] = std::to_string(col[i]);
                    break;
                }
                case col_type::STRING: {
                    auto &col = store.df.get_column<std::string>(group_col);
                    group_keys[i] = col[i];
                    break;
                }
            }
        }

        // Collect unique groups in order of first appearance
        std::vector<std::string> unique_keys;
        std::unordered_map<std::string, std::vector<size_t>> groups;
        for (size_t i = 0; i < nrows; i++) {
            auto &key = group_keys[i];
            if (groups.find(key) == groups.end())
                unique_keys.push_back(key);
            groups[key].push_back(i);
        }

        size_t ngroups = unique_keys.size();

        // Compute aggregation per group
        std::vector<double> results(ngroups);

        if (agg == "count") {
            for (size_t g = 0; g < ngroups; g++)
                results[g] = static_cast<double>(groups[unique_keys[g]].size());
        } else {
            auto &val_col_data = store.df.get_column<double>(value_col);

            for (size_t g = 0; g < ngroups; g++) {
                auto &indices = groups[unique_keys[g]];
                double acc = 0.0;

                if (agg == "sum") {
                    for (auto idx : indices) acc += val_col_data[idx];
                    results[g] = acc;
                } else if (agg == "mean") {
                    for (auto idx : indices) acc += val_col_data[idx];
                    results[g] = acc / static_cast<double>(indices.size());
                } else if (agg == "min") {
                    acc = val_col_data[indices[0]];
                    for (size_t j = 1; j < indices.size(); j++)
                        acc = std::min(acc, val_col_data[indices[j]]);
                    results[g] = acc;
                } else if (agg == "max") {
                    acc = val_col_data[indices[0]];
                    for (size_t j = 1; j < indices.size(); j++)
                        acc = std::max(acc, val_col_data[indices[j]]);
                    results[g] = acc;
                } else {
                    post_error(x, "dataframe: unknown aggregation '%s' "
                               "(use sum, mean, count, min, max)", agg_name);
                    return;
                }
            }
        }

        // Build new DataFrame
        ULDataFrame new_df;
        std::vector<unsigned long> idx(ngroups);
        for (size_t i = 0; i < ngroups; i++) idx[i] = static_cast<unsigned long>(i);
        new_df.load_index(std::move(idx));

        std::unordered_map<std::string, col_type> new_types;

        // Load group column
        switch (grp_ct) {
            case col_type::DOUBLE: {
                std::vector<double> gv(ngroups);
                auto &src = store.df.get_column<double>(group_col);
                for (size_t g = 0; g < ngroups; g++)
                    gv[g] = src[groups[unique_keys[g]][0]];
                new_df.load_column(group_col, std::move(gv));
                new_types[group_col] = col_type::DOUBLE;
                break;
            }
            case col_type::LONG: {
                std::vector<long> gv(ngroups);
                auto &src = store.df.get_column<long>(group_col);
                for (size_t g = 0; g < ngroups; g++)
                    gv[g] = src[groups[unique_keys[g]][0]];
                new_df.load_column(group_col, std::move(gv));
                new_types[group_col] = col_type::LONG;
                break;
            }
            case col_type::STRING: {
                std::vector<std::string> gv(ngroups);
                for (size_t g = 0; g < ngroups; g++)
                    gv[g] = unique_keys[g];
                new_df.load_column(group_col, std::move(gv));
                new_types[group_col] = col_type::STRING;
                break;
            }
        }

        // Load aggregated value column
        new_df.load_column(value_col, std::move(results));
        new_types[value_col] = col_type::DOUBLE;

        store.df = std::move(new_df);
        store.col_types = std::move(new_types);

        post_info(x, "dataframe: groupby '%s' %s '%s': %zu groups",
                  group_col, agg_name, value_col, ngroups);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: groupby failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Join
// --------------------------------------------------------------------------

// join <other_name> <col> [inner|left|right|outer]
// Joins this DataFrame with another named instance by a shared column.
// Default join type is inner.
static void dataframe_join(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 2) {
        post_error(x, "dataframe: join requires <other_name> <col> [inner|left|right|outer]");
        return;
    }

    t_symbol *other_sym = atom_getsym(argv);
    const char *col_name = atom_getsym(argv + 1)->s_name;

    // Look up the other DataFrame in the registry
    std::shared_ptr<DataFrameStore> other_store;
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        auto it = g_registry.find(other_sym);
        if (it == g_registry.end()) {
            post_error(x, "dataframe: no instance named '%s'", other_sym->s_name);
            return;
        }
        other_store = it->second;
    }

    if (other_store.get() == x->store.get()) {
        post_error(x, "dataframe: cannot join with self");
        return;
    }

    // Check column exists in both
    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found in '%s'",
                   col_name, x->name->s_name);
        return;
    }
    if (other_store->col_types.find(col_name) == other_store->col_types.end()) {
        post_error(x, "dataframe: column '%s' not found in '%s'",
                   col_name, other_sym->s_name);
        return;
    }

    // Parse join policy
    join_policy jp = join_policy::inner_join;
    if (argc >= 3 && atom_gettype(argv + 2) == A_SYM) {
        std::string jp_str(atom_getsym(argv + 2)->s_name);
        if (jp_str == "left") jp = join_policy::left_join;
        else if (jp_str == "right") jp = join_policy::right_join;
        else if (jp_str == "outer") jp = join_policy::left_right_join;
    }

    try {
        auto col_ct = get_col_type(x, col_name);
        auto other_col_ct = other_store->col_types.at(col_name);

        if (col_ct != other_col_ct) {
            post_error(x, "dataframe: column '%s' has different types in the two DataFrames",
                       col_name);
            return;
        }

        ULDataFrame joined;

        switch (col_ct) {
            case col_type::DOUBLE:
                joined = x->store->df.join_by_column<ULDataFrame, double,
                    double, long, std::string>(other_store->df, col_name, jp);
                break;
            case col_type::LONG:
                joined = x->store->df.join_by_column<ULDataFrame, long,
                    double, long, std::string>(other_store->df, col_name, jp);
                break;
            case col_type::STRING:
                joined = x->store->df.join_by_column<ULDataFrame, std::string,
                    double, long, std::string>(other_store->df, col_name, jp);
                break;
        }

        x->store->df = std::move(joined);
        detect_column_types(x);

        auto nrows = x->store->df.get_index().size();
        auto ncols = x->store->col_types.size();
        post_info(x, "dataframe: joined with '%s' on '%s': %zu rows x %zu columns",
                  other_sym->s_name, col_name, nrows, ncols);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: join failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Fill Missing
// --------------------------------------------------------------------------

// fill_missing <col> <policy> [value]
// Policies: forward, backward, value <v>, linear, midpoint
static void dataframe_fill_missing(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 2) {
        post_error(x, "dataframe: fill_missing requires <col> <policy> [value]");
        return;
    }

    const char *col_name = atom_getsym(argv)->s_name;
    const char *policy_str = atom_getsym(argv + 1)->s_name;

    if (!require_double_col(x, col_name)) return;

    try {
        std::string policy(policy_str);
        fill_policy fp;
        ULDataFrame::StlVecType<double> values;

        if (policy == "forward") {
            fp = fill_policy::fill_forward;
        } else if (policy == "backward") {
            fp = fill_policy::fill_backward;
        } else if (policy == "value") {
            if (argc < 3) {
                post_error(x, "dataframe: fill_missing value requires a fill value");
                return;
            }
            fp = fill_policy::value;
            values.push_back(atom_getfloat(argv + 2));
        } else if (policy == "linear") {
            fp = fill_policy::linear_interpolate;
        } else if (policy == "midpoint") {
            fp = fill_policy::mid_point;
        } else {
            post_error(x, "dataframe: unknown fill policy '%s' "
                       "(use forward, backward, value, linear, midpoint)", policy_str);
            return;
        }

        ULDataFrame::StlVecType<const char *> col_names = { col_name };
        x->store->df.fill_missing<double>(col_names, fp, values);

        post_info(x, "dataframe: fill_missing '%s' with policy '%s'",
                  col_name, policy_str);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: fill_missing failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Curve Fitting
// --------------------------------------------------------------------------

// curvefit <type> <x_col> <y_col> [degree]
// Types: linear, poly, exp, log, spline
static void dataframe_curvefit(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 3) {
        post_error(x, "dataframe: curvefit requires <type> <x_col> <y_col> [degree]");
        return;
    }

    const char *fit_type = atom_getsym(argv)->s_name;
    const char *x_col = atom_getsym(argv + 1)->s_name;
    const char *y_col = atom_getsym(argv + 2)->s_name;

    if (!require_numeric_col(x, x_col)) return;
    if (!require_numeric_col(x, y_col)) return;

    try {
        std::string type(fit_type);
        std::vector<t_atom> out_atoms;

        // Build a temp DataFrame if either column is long
        auto ct_x = get_col_type(x, x_col);
        auto ct_y = get_col_type(x, y_col);
        bool need_tmp = (ct_x != col_type::DOUBLE || ct_y != col_type::DOUBLE);
        ULDataFrame tmp;
        if (need_tmp) tmp = make_numeric_tmp(x, {x_col, y_col});
        auto &df_ref = need_tmp ? tmp : x->store->df;

        if (type == "linear") {
            LinearFitVisitor<double, unsigned long> v;
            df_ref.single_act_visit<double, double>(x_col, y_col, v);
            out_atoms.resize(2);
            atom_setfloat(&out_atoms[0], v.get_slope());
            atom_setfloat(&out_atoms[1], v.get_intercept());
        } else if (type == "poly") {
            long degree = 2;
            if (argc >= 4) degree = atom_getlong(argv + 3);
            if (degree < 1) degree = 1;
            PolyFitVisitor<double, unsigned long> v(static_cast<size_t>(degree + 1));
            df_ref.single_act_visit<double, double>(x_col, y_col, v);
            auto &coeffs = v.get_result();
            out_atoms.resize(coeffs.size());
            for (size_t i = 0; i < coeffs.size(); i++)
                atom_setfloat(&out_atoms[i], coeffs[i]);
        } else if (type == "exp") {
            ExponentialFitVisitor<double, unsigned long> v;
            df_ref.single_act_visit<double, double>(x_col, y_col, v);
            out_atoms.resize(2);
            atom_setfloat(&out_atoms[0], v.get_slope());
            atom_setfloat(&out_atoms[1], v.get_intercept());
        } else if (type == "log") {
            LogFitVisitor<double, unsigned long> v;
            df_ref.single_act_visit<double, double>(x_col, y_col, v);
            auto &coeffs = v.get_result();
            out_atoms.resize(coeffs.size());
            for (size_t i = 0; i < coeffs.size(); i++)
                atom_setfloat(&out_atoms[i], coeffs[i]);
        } else if (type == "spline") {
            CubicSplineFitVisitor<double, unsigned long> v;
            df_ref.single_act_visit<double, double>(x_col, y_col, v);
            auto &b = v.get_result();
            out_atoms.resize(b.size());
            for (size_t i = 0; i < b.size(); i++)
                atom_setfloat(&out_atoms[i], b[i]);
        } else {
            post_error(x, "dataframe: unknown fit type '%s' "
                       "(use linear, poly, exp, log, spline)", fit_type);
            return;
        }

        outlet_list(x->outlet_data, nullptr,
                    static_cast<long>(out_atoms.size()), out_atoms.data());
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: curvefit failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Apply
// --------------------------------------------------------------------------

// apply <col> <op> <value>
// Operators: +, -, *, /
static void dataframe_apply(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 3) {
        post_error(x, "dataframe: apply requires <col> <op> <value>");
        return;
    }

    const char *col_name = atom_getsym(argv)->s_name;
    const char *op = atom_getsym(argv + 1)->s_name;
    double value = atom_getfloat(argv + 2);

    if (!require_double_col(x, col_name)) return;

    std::string op_str(op);
    if (op_str != "+" && op_str != "-" && op_str != "*" && op_str != "/") {
        post_error(x, "dataframe: unknown operator '%s' (use +, -, *, /)", op);
        return;
    }

    if (op_str == "/" && value == 0.0) {
        post_error(x, "dataframe: division by zero");
        return;
    }

    try {
        auto &col = x->store->df.get_column<double>(col_name);
        for (size_t i = 0; i < col.size(); i++) {
            if (op_str == "+") col[i] += value;
            else if (op_str == "-") col[i] -= value;
            else if (op_str == "*") col[i] *= value;
            else if (op_str == "/") col[i] /= value;
        }

        post_info(x, "dataframe: applied %s %g to '%s' (%zu values)",
                  op, value, col_name, col.size());
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: apply failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Reshape: melt, pivot, transpose
// --------------------------------------------------------------------------

// melt <id_col> [val_col1 val_col2 ...]
// Wide-to-long: unpivot value columns into (variable, values) pairs.
static void dataframe_melt(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 1) {
        post_error(x, "dataframe: melt requires <id_col> [val_col1 val_col2 ...]");
        return;
    }

    const char *id_col = atom_getsym(argv)->s_name;

    if (!has_col(x, id_col)) {
        post_error(x, "dataframe: column '%s' not found", id_col);
        return;
    }

    // Collect optional value column names
    std::vector<const char *> val_cols;
    for (long i = 1; i < argc; i++) {
        const char *vn = atom_getsym(argv + i)->s_name;
        if (!has_col(x, vn)) {
            post_error(x, "dataframe: column '%s' not found", vn);
            return;
        }
        if (get_col_type(x, vn) != col_type::DOUBLE) {
            post_error(x, "dataframe: melt value column '%s' must be double", vn);
            return;
        }
        val_cols.push_back(vn);
    }

    try {
        auto id_ct = get_col_type(x, id_col);
        ULDataFrame result;

        switch (id_ct) {
            case col_type::DOUBLE:
                result = x->store->df.unpivot<double, double>(
                    id_col, std::move(val_cols));
                break;
            case col_type::LONG:
                result = x->store->df.unpivot<long, double>(
                    id_col, std::move(val_cols));
                break;
            case col_type::STRING:
                result = x->store->df.unpivot<std::string, double>(
                    id_col, std::move(val_cols));
                break;
        }

        x->store->df = std::move(result);
        detect_column_types(x);

        auto nrows = x->store->df.get_index().size();
        auto ncols = x->store->col_types.size();
        post_info(x, "dataframe: melt on '%s': %zu rows x %zu columns",
                  id_col, nrows, ncols);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: melt failed: %s", e.what());
    }
}

// pivot <names_col> <val_col1> [val_col2 ...]
// Long-to-wide: spread repeating values in names_col into new columns.
static void dataframe_pivot(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    if (argc < 2) {
        post_error(x, "dataframe: pivot requires <names_col> <val_col1> [val_col2 ...]");
        return;
    }

    const char *names_col = atom_getsym(argv)->s_name;

    if (!has_col(x, names_col)) {
        post_error(x, "dataframe: column '%s' not found", names_col);
        return;
    }
    if (get_col_type(x, names_col) != col_type::STRING) {
        post_error(x, "dataframe: pivot names column '%s' must be string", names_col);
        return;
    }

    std::vector<const char *> val_cols;
    for (long i = 1; i < argc; i++) {
        const char *vn = atom_getsym(argv + i)->s_name;
        if (!has_col(x, vn)) {
            post_error(x, "dataframe: column '%s' not found", vn);
            return;
        }
        if (get_col_type(x, vn) != col_type::DOUBLE) {
            post_error(x, "dataframe: pivot value column '%s' must be double", vn);
            return;
        }
        val_cols.push_back(vn);
    }

    try {
        auto result = x->store->df.pivot<double>(
            names_col, std::move(val_cols));

        x->store->df = std::move(result);
        detect_column_types(x);

        auto nrows = x->store->df.get_index().size();
        auto ncols = x->store->col_types.size();
        post_info(x, "dataframe: pivot on '%s': %zu rows x %zu columns",
                  names_col, nrows, ncols);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: pivot failed: %s", e.what());
    }
}

// transpose (no arguments)
// Transpose all-double DataFrame: rows become columns, columns become rows.
static void dataframe_transpose(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    auto &store = *x->store;

    if (store.col_types.empty()) {
        post_error(x, "dataframe: transpose requires a non-empty DataFrame");
        return;
    }

    // All columns must be double
    for (auto &pair : store.col_types) {
        if (pair.second != col_type::DOUBLE) {
            post_error(x, "dataframe: transpose requires all columns to be double "
                       "(column '%s' is not)", pair.first.c_str());
            return;
        }
    }

    try {
        auto col_names = get_column_names(x);
        size_t nrows = store.df.get_index().size();
        size_t ncols = col_names.size();

        // New index: one entry per old column
        std::vector<unsigned long> new_idx(ncols);
        for (size_t i = 0; i < ncols; i++)
            new_idx[i] = static_cast<unsigned long>(i);

        // New column names: "row_0", "row_1", ...
        std::vector<std::string> new_col_names(nrows);
        for (size_t i = 0; i < nrows; i++)
            new_col_names[i] = "row_" + std::to_string(i);

        auto result = store.df.transpose<double>(
            std::move(new_idx), new_col_names);

        store.df = std::move(result);
        detect_column_types(x);

        auto out_nrows = store.df.get_index().size();
        auto out_ncols = store.col_types.size();
        post_info(x, "dataframe: transposed: %zu rows x %zu columns",
                  out_nrows, out_ncols);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: transpose failed: %s", e.what());
    }
}

// --------------------------------------------------------------------------
// Dict Interop
// --------------------------------------------------------------------------

// todict [name]
// Export DataFrame contents as a Max dict.
static void dataframe_todict(t_dataframe *x, t_symbol *s, long argc, t_atom *argv) {
    try {
        t_dictionary *d = dictionary_new();
        if (!d) {
            post_error(x, "dataframe: failed to create dictionary");
            return;
        }

        auto &store = *x->store;
        auto col_names = get_column_names(x);

        for (auto &name : col_names) {
            auto ct = store.col_types[name];
            switch (ct) {
                case col_type::DOUBLE: {
                    auto &col = store.df.get_column<double>(name.c_str());
                    std::vector<t_atom> atoms(col.size());
                    for (size_t i = 0; i < col.size(); i++)
                        atom_setfloat(&atoms[i], col[i]);
                    dictionary_appendatoms(d, gensym(name.c_str()),
                                           static_cast<long>(atoms.size()), atoms.data());
                    break;
                }
                case col_type::LONG: {
                    auto &col = store.df.get_column<long>(name.c_str());
                    std::vector<t_atom> atoms(col.size());
                    for (size_t i = 0; i < col.size(); i++)
                        atom_setlong(&atoms[i], col[i]);
                    dictionary_appendatoms(d, gensym(name.c_str()),
                                           static_cast<long>(atoms.size()), atoms.data());
                    break;
                }
                case col_type::STRING: {
                    auto &col = store.df.get_column<std::string>(name.c_str());
                    std::vector<t_atom> atoms(col.size());
                    for (size_t i = 0; i < col.size(); i++)
                        atom_setsym(&atoms[i], gensym(col[i].c_str()));
                    dictionary_appendatoms(d, gensym(name.c_str()),
                                           static_cast<long>(atoms.size()), atoms.data());
                    break;
                }
            }
        }

        t_symbol *dict_name = nullptr;
        if (argc >= 1 && atom_gettype(argv) == A_SYM)
            dict_name = atom_getsym(argv);

        dictobj_register(d, &dict_name);

        t_atom name_atom;
        atom_setsym(&name_atom, dict_name);
        outlet_anything(x->outlet_data, gensym("dictionary"), 1, &name_atom);
        outlet_bang(x->outlet_info);

        post_info(x, "dataframe: exported to dict '%s'", dict_name->s_name);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: todict failed: %s", e.what());
    }
}
