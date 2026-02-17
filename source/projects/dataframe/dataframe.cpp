// dataframe - Max external wrapping the hmdf DataFrame library
// Provides pandas-like columnar data manipulation in Max.
// Uses named instances (like coll/buffer~) for shared data.

extern "C" {
#include "ext.h"
#include "ext_obex.h"
}

#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameStatsVisitors.h>

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
} t_dataframe;

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
static void dataframe_read(t_dataframe *x, t_symbol *s);
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

// Filtering
static void dataframe_sel(t_dataframe *x, t_symbol *s, long argc, t_atom *argv);

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

// Read a plain CSV file into the DataFrame.
static bool read_plain_csv(t_dataframe *x, const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;

    // Read header
    if (!std::getline(file, line)) return false;
    auto headers = split_csv_line(line);
    size_t ncols = headers.size();
    for (auto &h : headers) h = trim(h);

    // Read all rows as strings first
    std::vector<std::vector<std::string>> rows;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;
        auto fields = split_csv_line(line);
        // Pad or truncate to match header count
        fields.resize(ncols);
        for (auto &f : fields) f = trim(f);
        rows.push_back(std::move(fields));
    }

    size_t nrows = rows.size();
    if (nrows == 0 || ncols == 0) return false;

    // Detect column types by scanning all values in each column.
    // A column is LONG if all values parse as long, DOUBLE if all parse as
    // double (or long, which promotes), STRING otherwise.
    std::vector<col_type> types(ncols, col_type::LONG);
    for (size_t col = 0; col < ncols; col++) {
        for (size_t row = 0; row < nrows; row++) {
            auto vt = detect_value_type(rows[row][col]);
            if (vt == col_type::STRING) {
                types[col] = col_type::STRING;
                break;
            }
            if (vt == col_type::DOUBLE && types[col] == col_type::LONG) {
                types[col] = col_type::DOUBLE;
            }
        }
    }

    // Build the DataFrame
    auto &store = *x->store;
    store.df = ULDataFrame();
    store.col_types.clear();

    // Create index: 0..nrows-1
    std::vector<unsigned long> idx(nrows);
    for (size_t i = 0; i < nrows; i++) idx[i] = static_cast<unsigned long>(i);
    store.df.load_index(std::move(idx));

    // Load each column
    for (size_t col = 0; col < ncols; col++) {
        const auto &name = headers[col];
        switch (types[col]) {
            case col_type::DOUBLE: {
                std::vector<double> vec(nrows);
                for (size_t row = 0; row < nrows; row++) {
                    try { vec[row] = std::stod(rows[row][col]); }
                    catch (...) { vec[row] = 0.0; }
                }
                store.df.load_column(name.c_str(), std::move(vec));
                store.col_types[name] = col_type::DOUBLE;
                break;
            }
            case col_type::LONG: {
                std::vector<long> vec(nrows);
                for (size_t row = 0; row < nrows; row++) {
                    try { vec[row] = std::stol(rows[row][col]); }
                    catch (...) { vec[row] = 0; }
                }
                store.df.load_column(name.c_str(), std::move(vec));
                store.col_types[name] = col_type::LONG;
                break;
            }
            case col_type::STRING: {
                std::vector<std::string> vec(nrows);
                for (size_t row = 0; row < nrows; row++) {
                    vec[row] = rows[row][col];
                }
                store.df.load_column(name.c_str(), std::move(vec));
                store.col_types[name] = col_type::STRING;
                break;
            }
        }
    }

    return true;
}

// Write the DataFrame as a plain CSV file.
static bool write_plain_csv(t_dataframe *x, const std::string &path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    auto &store = *x->store;
    auto col_names = get_column_names(x);
    size_t ncols = col_names.size();
    size_t nrows = store.df.get_index().size();

    // Header
    for (size_t i = 0; i < ncols; i++) {
        if (i > 0) file << ',';
        file << col_names[i];
    }
    file << '\n';

    // Rows
    for (size_t row = 0; row < nrows; row++) {
        for (size_t col = 0; col < ncols; col++) {
            if (col > 0) file << ',';
            const auto &name = col_names[col];
            switch (store.col_types[name]) {
                case col_type::DOUBLE: {
                    auto &c = store.df.get_column<double>(name.c_str());
                    file << c[row];
                    break;
                }
                case col_type::LONG: {
                    auto &c = store.df.get_column<long>(name.c_str());
                    file << c[row];
                    break;
                }
                case col_type::STRING: {
                    auto &c = store.df.get_column<std::string>(name.c_str());
                    // Quote if the value contains commas or quotes
                    const auto &val = c[row];
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

    return true;
}

// --------------------------------------------------------------------------
// ext_main
// --------------------------------------------------------------------------

extern "C" void ext_main(void *r) {
    t_class *c = class_new("dataframe",
        (method)dataframe_new, (method)dataframe_free,
        (long)sizeof(t_dataframe), 0L, A_GIMME, 0);

    // I/O
    class_addmethod(c, (method)dataframe_read,    "read",    A_SYM, 0);
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

    // Filtering
    class_addmethod(c, (method)dataframe_sel,    "sel",    A_GIMME, 0);

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

    post_info(x, "dataframe: instance '%s' (refcount: %zu)",
              x->name->s_name, x->store->refcount);

    return x;
}

static void dataframe_free(t_dataframe *x) {
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
                 "count, describe, sel");
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

static void dataframe_read(t_dataframe *x, t_symbol *s) {
    try {
        std::string path = resolve_path(x, s->s_name);
        if (path.empty()) return;

        std::string ext = get_extension(path);

        bool ok = false;
        if (ext == "json") {
            x->store->df = ULDataFrame();
            x->store->col_types.clear();
            x->store->df.read(path.c_str(), io_format::json);
            detect_column_types(x);
            ok = true;
        } else {
            ok = read_plain_csv(x, path);
        }

        if (!ok) {
            post_error(x, "dataframe: failed to read '%s'", s->s_name);
            return;
        }

        auto nrows = x->store->df.get_index().size();
        auto ncols = x->store->col_types.size();
        post_info(x, "dataframe: read %zu rows x %zu columns from '%s'",
                  nrows, ncols, s->s_name);

        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: read failed: %s", e.what());
    }
}

static void dataframe_write(t_dataframe *x, t_symbol *s) {
    try {
        std::string path(s->s_name);
        std::string ext = get_extension(path);

        bool ok = false;
        if (ext == "json") {
            x->store->df.write<double, long, std::string>(
                path.c_str(), io_format::json);
            ok = true;
        } else {
            ok = write_plain_csv(x, path);
        }

        if (!ok) {
            post_error(x, "dataframe: failed to write '%s'", s->s_name);
            return;
        }

        post_info(x, "dataframe: wrote to '%s'", s->s_name);
        outlet_bang(x->outlet_info);
    } catch (const std::exception &e) {
        post_error(x, "dataframe: write failed: %s", e.what());
    }
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

// Helper: ensure column exists and is numeric (double)
static bool require_double_col(t_dataframe *x, const char *col_name) {
    if (!has_col(x, col_name)) {
        post_error(x, "dataframe: column '%s' not found", col_name);
        return false;
    }
    if (get_col_type(x, col_name) != col_type::DOUBLE) {
        post_error(x, "dataframe: column '%s' is not numeric (double)", col_name);
        return false;
    }
    return true;
}

static void dataframe_mean(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        MeanVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: mean failed: %s", e.what());
    }
}

static void dataframe_median(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        MedianVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: median failed: %s", e.what());
    }
}

static void dataframe_std(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        StdVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: std failed: %s", e.what());
    }
}

static void dataframe_var(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        VarVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: var failed: %s", e.what());
    }
}

static void dataframe_sum(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        SumVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: sum failed: %s", e.what());
    }
}

static void dataframe_min(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        MinVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
        outlet_float(x->outlet_data, v.get_result());
    } catch (const std::exception &e) {
        post_error(x, "dataframe: min failed: %s", e.what());
    }
}

static void dataframe_max(t_dataframe *x, t_symbol *s) {
    if (!require_double_col(x, s->s_name)) return;
    try {
        MaxVisitor<double, unsigned long> v;
        x->store->df.visit<double>(s->s_name, v);
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
    if (!require_double_col(x, s->s_name)) return;

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

        x->store->df.visit<double>(col_name, mean_v);
        x->store->df.visit<double>(col_name, median_v);
        x->store->df.visit<double>(col_name, std_v);
        x->store->df.visit<double>(col_name, var_v);
        x->store->df.visit<double>(col_name, min_v);
        x->store->df.visit<double>(col_name, max_v);
        x->store->df.visit<double>(col_name, count_v);
        x->store->df.visit<double>(col_name, sum_v);

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
