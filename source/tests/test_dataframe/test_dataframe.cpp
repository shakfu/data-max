#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameStatsVisitors.h>
#include <OpenXLSX/OpenXLSX.hpp>

extern "C" {
#include "xlsxwriter.h"
}

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace hmdf;
using ULDataFrame = StdDataFrame<unsigned long>;

static bool approx(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) < tol;
}

static void test_load_and_shape() {
    std::cout << "test_load_and_shape... ";

    ULDataFrame df;

    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> vals = {10.0, 20.0, 30.0, 40.0, 50.0};
    std::vector<std::string> names = {"a", "b", "c", "d", "e"};

    df.load_index(std::move(idx));
    df.load_column("values", std::move(vals));
    df.load_column("names", std::move(names));

    assert(df.get_index().size() == 5);
    assert(df.get_column<double>("values").size() == 5);
    assert(df.get_column<std::string>("names")[0] == "a");

    std::cout << "PASSED" << std::endl;
}

static void test_csv_io() {
    std::cout << "test_csv_io... ";

    ULDataFrame df;

    std::vector<unsigned long> idx = {0, 1, 2};
    std::vector<double> scores = {92.5, 87.3, 95.1};
    std::vector<long> ages = {25, 30, 22};

    df.load_index(std::move(idx));
    df.load_column("score", std::move(scores));
    df.load_column("age", std::move(ages));

    // Write to CSV
    df.write<double, long>("/tmp/test_dataframe_out.csv", io_format::csv2);

    // Read it back
    ULDataFrame df2;
    df2.read("/tmp/test_dataframe_out.csv", io_format::csv2);

    assert(df2.get_index().size() == 3);
    assert(approx(df2.get_column<double>("score")[0], 92.5));
    assert(df2.get_column<long>("age")[1] == 30);

    std::cout << "PASSED" << std::endl;
}

static void test_statistics() {
    std::cout << "test_statistics... ";

    ULDataFrame df;

    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> vals = {10.0, 20.0, 30.0, 40.0, 50.0};

    df.load_index(std::move(idx));
    df.load_column("values", std::move(vals));

    // Mean
    MeanVisitor<double, unsigned long> mean_v;
    df.visit<double>("values", mean_v);
    assert(approx(mean_v.get_result(), 30.0));

    // Sum
    SumVisitor<double, unsigned long> sum_v;
    df.visit<double>("values", sum_v);
    assert(approx(sum_v.get_result(), 150.0));

    // Min
    MinVisitor<double, unsigned long> min_v;
    df.visit<double>("values", min_v);
    assert(approx(min_v.get_result(), 10.0));

    // Max
    MaxVisitor<double, unsigned long> max_v;
    df.visit<double>("values", max_v);
    assert(approx(max_v.get_result(), 50.0));

    // Variance
    VarVisitor<double, unsigned long> var_v;
    df.visit<double>("values", var_v);
    double expected_var = 250.0; // sample variance: sum((x-mean)^2)/(n-1)
    assert(approx(var_v.get_result(), expected_var));

    // Std
    StdVisitor<double, unsigned long> std_v;
    df.visit<double>("values", std_v);
    assert(approx(std_v.get_result(), std::sqrt(expected_var)));

    // Median
    MedianVisitor<double, unsigned long> median_v;
    df.visit<double>("values", median_v);
    assert(approx(median_v.get_result(), 30.0));

    // Count
    CountVisitor<double, unsigned long> count_v;
    df.visit<double>("values", count_v);
    assert(count_v.get_result() == 5);

    std::cout << "PASSED" << std::endl;
}

static void test_filter() {
    std::cout << "test_filter... ";

    ULDataFrame df;

    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> vals = {10.0, 20.0, 30.0, 40.0, 50.0};
    std::vector<std::string> names = {"a", "b", "c", "d", "e"};

    df.load_index(std::move(idx));
    df.load_column("values", std::move(vals));
    df.load_column("names", std::move(names));

    // Filter values > 25
    auto filter_fn = [](const unsigned long &, const double &val) -> bool {
        return val > 25.0;
    };
    auto filtered = df.get_data_by_sel<double, decltype(filter_fn),
                                        double, std::string>(
        "values", filter_fn);

    assert(filtered.get_index().size() == 3);
    assert(approx(filtered.get_column<double>("values")[0], 30.0));
    assert(filtered.get_column<std::string>("names")[2] == "e");

    std::cout << "PASSED" << std::endl;
}

static void test_read_sample_csv() {
    std::cout << "test_read_sample_csv... ";

    // Read the sample CSV from the project
    ULDataFrame df;
    df.read("examples/sample.csv", io_format::csv2);

    assert(df.get_index().size() == 15);

    const auto &scores = df.get_column<double>("score");
    assert(scores.size() == 15);
    assert(approx(scores[0], 92.5));

    const auto &ages = df.get_column<long>("age");
    assert(ages.size() == 15);
    assert(ages[0] == 25);

    std::cout << "PASSED" << std::endl;
}

static void test_sort() {
    std::cout << "test_sort... ";

    ULDataFrame df;

    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> vals = {30.0, 10.0, 50.0, 20.0, 40.0};
    std::vector<std::string> names = {"c", "a", "e", "b", "d"};

    df.load_index(std::move(idx));
    df.load_column("values", std::move(vals));
    df.load_column("names", std::move(names));

    // Sort ascending by values
    df.sort<double, double, std::string>("values", sort_spec::ascen);

    auto &sorted_vals = df.get_column<double>("values");
    assert(approx(sorted_vals[0], 10.0));
    assert(approx(sorted_vals[1], 20.0));
    assert(approx(sorted_vals[2], 30.0));
    assert(approx(sorted_vals[3], 40.0));
    assert(approx(sorted_vals[4], 50.0));

    // Names should follow the sort
    auto &sorted_names = df.get_column<std::string>("names");
    assert(sorted_names[0] == "a");
    assert(sorted_names[4] == "e");

    // Sort descending by names
    df.sort<std::string, double, std::string>("names", sort_spec::desce);

    auto &desc_names = df.get_column<std::string>("names");
    assert(desc_names[0] == "e");
    assert(desc_names[4] == "a");

    // Values should follow
    auto &desc_vals = df.get_column<double>("values");
    assert(approx(desc_vals[0], 50.0));
    assert(approx(desc_vals[4], 10.0));

    std::cout << "PASSED" << std::endl;
}

static void test_groupby() {
    std::cout << "test_groupby... ";

    // Simulate what dataframe_groupby does manually
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4, 5};
    std::vector<std::string> categories = {"A", "B", "A", "B", "A", "B"};
    std::vector<double> values = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0};

    df.load_index(std::move(idx));
    df.load_column("cat", std::move(categories));
    df.load_column("val", std::move(values));

    // Manual groupby sum
    auto &cats = df.get_column<std::string>("cat");
    auto &vals = df.get_column<double>("val");

    std::vector<std::string> unique_keys;
    std::unordered_map<std::string, std::vector<size_t>> groups;
    for (size_t i = 0; i < cats.size(); i++) {
        if (groups.find(cats[i]) == groups.end())
            unique_keys.push_back(cats[i]);
        groups[cats[i]].push_back(i);
    }

    assert(unique_keys.size() == 2);
    assert(unique_keys[0] == "A");
    assert(unique_keys[1] == "B");

    // Sum for A: 10+30+50 = 90, B: 20+40+60 = 120
    double sum_a = 0, sum_b = 0;
    for (auto i : groups["A"]) sum_a += vals[i];
    for (auto i : groups["B"]) sum_b += vals[i];
    assert(approx(sum_a, 90.0));
    assert(approx(sum_b, 120.0));

    // Mean for A: 30, B: 40
    double mean_a = sum_a / groups["A"].size();
    double mean_b = sum_b / groups["B"].size();
    assert(approx(mean_a, 30.0));
    assert(approx(mean_b, 40.0));

    // Count for A: 3, B: 3
    assert(groups["A"].size() == 3);
    assert(groups["B"].size() == 3);

    std::cout << "PASSED" << std::endl;
}

static void test_join() {
    std::cout << "test_join... ";

    // LHS: people with department IDs
    ULDataFrame lhs;
    std::vector<unsigned long> idx1 = {0, 1, 2, 3};
    std::vector<std::string> names = {"Alice", "Bob", "Carol", "Dave"};
    std::vector<long> dept_ids = {1, 2, 1, 3};

    lhs.load_index(std::move(idx1));
    lhs.load_column("name", std::move(names));
    lhs.load_column("dept_id", std::move(dept_ids));

    // RHS: departments
    ULDataFrame rhs;
    std::vector<unsigned long> idx2 = {0, 1, 2};
    std::vector<long> dept_ids2 = {1, 2, 3};
    std::vector<std::string> dept_names = {"Engineering", "Marketing", "Sales"};

    rhs.load_index(std::move(idx2));
    rhs.load_column("dept_id", std::move(dept_ids2));
    rhs.load_column("dept_name", std::move(dept_names));

    // Inner join by dept_id
    auto joined = lhs.join_by_column<ULDataFrame, long,
        std::string, long>(rhs, "dept_id", join_policy::inner_join);

    // Should have 4 rows (all dept_ids match)
    assert(joined.get_index().size() == 4);

    // Should have columns from both sides
    auto &j_names = joined.get_column<std::string>("name");
    auto &j_depts = joined.get_column<std::string>("dept_name");
    auto &j_ids = joined.get_column<long>("dept_id");

    // Verify join correctness: each name maps to correct department
    for (size_t i = 0; i < j_names.size(); i++) {
        if (j_names[i] == "Alice" || j_names[i] == "Carol")
            assert(j_depts[i] == "Engineering");
        else if (j_names[i] == "Bob")
            assert(j_depts[i] == "Marketing");
        else if (j_names[i] == "Dave")
            assert(j_depts[i] == "Sales");
    }

    std::cout << "PASSED" << std::endl;
}

static void test_xlsx_write_read_roundtrip() {
    std::cout << "test_xlsx_write_read_roundtrip... ";

    // Write an xlsx file using libxlsxwriter
    const char *xlsx_path = "/tmp/test_dataframe_roundtrip.xlsx";

    lxw_workbook *wb = workbook_new(xlsx_path);
    lxw_worksheet *ws = workbook_add_worksheet(wb, nullptr);

    // Header
    worksheet_write_string(ws, 0, 0, "name", nullptr);
    worksheet_write_string(ws, 0, 1, "age", nullptr);
    worksheet_write_string(ws, 0, 2, "score", nullptr);

    // Data rows
    worksheet_write_string(ws, 1, 0, "Alice", nullptr);
    worksheet_write_number(ws, 1, 1, 25, nullptr);
    worksheet_write_number(ws, 1, 2, 92.5, nullptr);

    worksheet_write_string(ws, 2, 0, "Bob", nullptr);
    worksheet_write_number(ws, 2, 1, 30, nullptr);
    worksheet_write_number(ws, 2, 2, 87.3, nullptr);

    worksheet_write_string(ws, 3, 0, "Carol", nullptr);
    worksheet_write_number(ws, 3, 1, 22, nullptr);
    worksheet_write_number(ws, 3, 2, 95.1, nullptr);

    lxw_error err = workbook_close(wb);
    assert(err == LXW_NO_ERROR);

    // Read it back using OpenXLSX
    OpenXLSX::XLDocument doc;
    doc.open(xlsx_path);

    auto workbook = doc.workbook();
    assert(workbook.worksheetCount() >= 1);

    auto worksheet = workbook.worksheet(1);
    assert(worksheet.rowCount() == 4);   // 1 header + 3 data
    assert(worksheet.columnCount() == 3);

    // Check header
    {
        auto c = worksheet.cell(1, 1);
        const auto &v = c.value();
        assert(v.type() == OpenXLSX::XLValueType::String);
        assert(v.get<std::string>() == "name");
    }

    // Check data types and values
    {
        auto c = worksheet.cell(2, 1);
        const auto &v = c.value();
        assert(v.type() == OpenXLSX::XLValueType::String);
        assert(v.get<std::string>() == "Alice");
    }

    {
        auto c = worksheet.cell(2, 2);
        const auto &v = c.value();
        // libxlsxwriter writes all numbers as doubles
        assert(v.type() == OpenXLSX::XLValueType::Float ||
               v.type() == OpenXLSX::XLValueType::Integer);
        assert(approx(v.get<double>(), 25.0));
    }

    {
        auto c = worksheet.cell(2, 3);
        const auto &v = c.value();
        assert(v.type() == OpenXLSX::XLValueType::Float ||
               v.type() == OpenXLSX::XLValueType::Integer);
        assert(approx(v.get<double>(), 92.5));
    }

    doc.close();
    std::cout << "PASSED" << std::endl;
}

static void test_xlsx_to_dataframe() {
    std::cout << "test_xlsx_to_dataframe... ";

    // Write a test xlsx
    const char *xlsx_path = "/tmp/test_dataframe_load.xlsx";

    lxw_workbook *wb = workbook_new(xlsx_path);
    lxw_worksheet *ws = workbook_add_worksheet(wb, nullptr);

    worksheet_write_string(ws, 0, 0, "city", nullptr);
    worksheet_write_string(ws, 0, 1, "population", nullptr);
    worksheet_write_string(ws, 0, 2, "area", nullptr);

    worksheet_write_string(ws, 1, 0, "Tokyo", nullptr);
    worksheet_write_number(ws, 1, 1, 13960000, nullptr);
    worksheet_write_number(ws, 1, 2, 2194.0, nullptr);

    worksheet_write_string(ws, 2, 0, "London", nullptr);
    worksheet_write_number(ws, 2, 1, 8982000, nullptr);
    worksheet_write_number(ws, 2, 2, 1572.0, nullptr);

    workbook_close(wb);

    // Read into DataFrame using OpenXLSX (simulating what read_xlsx does)
    OpenXLSX::XLDocument doc;
    doc.open(xlsx_path);
    auto worksheet = doc.workbook().worksheet(1);

    uint32_t nrows_total = worksheet.rowCount();
    uint16_t ncols = worksheet.columnCount();
    assert(nrows_total == 3);  // 1 header + 2 data
    assert(ncols == 3);

    // Read headers
    std::vector<std::string> headers(ncols);
    for (uint16_t col = 1; col <= ncols; col++) {
        auto c = worksheet.cell(1, col);
        headers[col - 1] = c.value().get<std::string>();
    }
    assert(headers[0] == "city");
    assert(headers[1] == "population");
    assert(headers[2] == "area");

    // Build a DataFrame from the xlsx data
    ULDataFrame df;
    uint32_t nrows = nrows_total - 1;
    std::vector<unsigned long> idx(nrows);
    for (uint32_t i = 0; i < nrows; i++) idx[i] = i;
    df.load_index(std::move(idx));

    // city column (strings)
    std::vector<std::string> cities(nrows);
    for (uint32_t r = 0; r < nrows; r++) {
        auto c = worksheet.cell(r + 2, 1);
        cities[r] = c.value().get<std::string>();
    }
    df.load_column("city", std::move(cities));

    // population column (numbers)
    std::vector<double> pops(nrows);
    for (uint32_t r = 0; r < nrows; r++) {
        auto c = worksheet.cell(r + 2, 2);
        pops[r] = c.value().get<double>();
    }
    df.load_column("population", std::move(pops));

    // area column (numbers)
    std::vector<double> areas(nrows);
    for (uint32_t r = 0; r < nrows; r++) {
        auto c = worksheet.cell(r + 2, 3);
        areas[r] = c.value().get<double>();
    }
    df.load_column("area", std::move(areas));

    doc.close();

    // Verify
    assert(df.get_index().size() == 2);
    assert(df.get_column<std::string>("city")[0] == "Tokyo");
    assert(approx(df.get_column<double>("population")[0], 13960000.0));
    assert(approx(df.get_column<double>("area")[1], 1572.0));

    // Compute stats
    MeanVisitor<double, unsigned long> mean_v;
    df.visit<double>("area", mean_v);
    assert(approx(mean_v.get_result(), (2194.0 + 1572.0) / 2.0));

    std::cout << "PASSED" << std::endl;
}

static void test_fill_missing_forward() {
    std::cout << "test_fill_missing_forward... ";

    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> vals = {1.0, std::numeric_limits<double>::quiet_NaN(),
                                3.0, std::numeric_limits<double>::quiet_NaN(), 5.0};

    df.load_index(std::move(idx));
    df.load_column("v", std::move(vals));

    ULDataFrame::StlVecType<const char *> col_names = {"v"};
    df.fill_missing<double>(col_names, fill_policy::fill_forward);

    auto &filled = df.get_column<double>("v");
    assert(approx(filled[0], 1.0));
    assert(approx(filled[1], 1.0));  // forward-filled from index 0
    assert(approx(filled[2], 3.0));
    assert(approx(filled[3], 3.0));  // forward-filled from index 2
    assert(approx(filled[4], 5.0));

    std::cout << "PASSED" << std::endl;
}

static void test_fill_missing_backward() {
    std::cout << "test_fill_missing_backward... ";

    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> vals = {1.0, std::numeric_limits<double>::quiet_NaN(),
                                3.0, std::numeric_limits<double>::quiet_NaN(), 5.0};

    df.load_index(std::move(idx));
    df.load_column("v", std::move(vals));

    ULDataFrame::StlVecType<const char *> col_names = {"v"};
    df.fill_missing<double>(col_names, fill_policy::fill_backward);

    auto &filled = df.get_column<double>("v");
    assert(approx(filled[0], 1.0));
    assert(approx(filled[1], 3.0));  // backward-filled from index 2
    assert(approx(filled[2], 3.0));
    assert(approx(filled[3], 5.0));  // backward-filled from index 4
    assert(approx(filled[4], 5.0));

    std::cout << "PASSED" << std::endl;
}

static void test_fill_missing_value() {
    std::cout << "test_fill_missing_value... ";

    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2};
    std::vector<double> vals = {std::numeric_limits<double>::quiet_NaN(),
                                10.0,
                                std::numeric_limits<double>::quiet_NaN()};

    df.load_index(std::move(idx));
    df.load_column("v", std::move(vals));

    ULDataFrame::StlVecType<const char *> col_names = {"v"};
    ULDataFrame::StlVecType<double> fill_vals = {-1.0};
    df.fill_missing<double>(col_names, fill_policy::value, fill_vals);

    auto &filled = df.get_column<double>("v");
    assert(approx(filled[0], -1.0));
    assert(approx(filled[1], 10.0));
    assert(approx(filled[2], -1.0));

    std::cout << "PASSED" << std::endl;
}

static void test_curvefit_linear() {
    std::cout << "test_curvefit_linear... ";

    // y = 2x + 1
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {3.0, 5.0, 7.0, 9.0, 11.0};

    df.load_index(std::move(idx));
    df.load_column("x", std::move(x));
    df.load_column("y", std::move(y));

    LinearFitVisitor<double, unsigned long> v;
    df.single_act_visit<double, double>("x", "y", v);

    assert(approx(v.get_slope(), 2.0));
    assert(approx(v.get_intercept(), 1.0));

    std::cout << "PASSED" << std::endl;
}

static void test_curvefit_poly() {
    std::cout << "test_curvefit_poly... ";

    // y = 1 + 2x + 3x^2 => coeffs [1, 2, 3] (degree+1 = 3)
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<double> y(5);
    for (int i = 0; i < 5; i++) {
        double xi = static_cast<double>(i);
        y[i] = 1.0 + 2.0 * xi + 3.0 * xi * xi;
    }

    df.load_index(std::move(idx));
    df.load_column("x", std::move(x));
    df.load_column("y", std::move(y));

    PolyFitVisitor<double, unsigned long> v(3);  // degree+1 = 3
    df.single_act_visit<double, double>("x", "y", v);

    auto &coeffs = v.get_result();
    assert(coeffs.size() == 3);
    assert(approx(coeffs[0], 1.0, 1e-4));  // constant
    assert(approx(coeffs[1], 2.0, 1e-4));  // linear
    assert(approx(coeffs[2], 3.0, 1e-4));  // quadratic

    std::cout << "PASSED" << std::endl;
}

static void test_curvefit_exp() {
    std::cout << "test_curvefit_exp... ";

    // y = exp(0.5 * x) -- ExponentialFitVisitor fits in log-linear space
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4, 5, 6, 7};
    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    std::vector<double> y(8);
    for (int i = 0; i < 8; i++) {
        y[i] = std::exp(0.5 * x[i]);
    }

    df.load_index(std::move(idx));
    df.load_column("x", std::move(x));
    df.load_column("y", std::move(y));

    ExponentialFitVisitor<double, unsigned long> v;
    df.single_act_visit<double, double>("x", "y", v);

    // slope should be ~0.5, intercept ~0.0 (since y = exp(0) * exp(0.5*x))
    assert(approx(v.get_slope(), 0.5, 1e-3));
    assert(approx(v.get_intercept(), 0.0, 1e-3));

    std::cout << "PASSED" << std::endl;
}

static void test_curvefit_spline() {
    std::cout << "test_curvefit_spline... ";

    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<double> y = {0.0, 1.0, 4.0, 9.0, 16.0};

    df.load_index(std::move(idx));
    df.load_column("x", std::move(x));
    df.load_column("y", std::move(y));

    CubicSplineFitVisitor<double, unsigned long> v;
    df.single_act_visit<double, double>("x", "y", v);

    // b-coefficients vector should have same size as data
    auto &b = v.get_result();
    assert(b.size() == 5);
    // b[0] should be the first derivative at x=0 for the spline
    // Just verify we got non-trivial output
    bool any_nonzero = false;
    for (auto val : b) {
        if (std::abs(val) > 1e-10) { any_nonzero = true; break; }
    }
    assert(any_nonzero);

    std::cout << "PASSED" << std::endl;
}

static void test_apply_arithmetic() {
    std::cout << "test_apply_arithmetic... ";

    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3};
    std::vector<double> vals = {10.0, 20.0, 30.0, 40.0};

    df.load_index(std::move(idx));
    df.load_column("v", std::move(vals));

    // Add 5
    auto &col = df.get_column<double>("v");
    for (size_t i = 0; i < col.size(); i++) col[i] += 5.0;
    assert(approx(col[0], 15.0));
    assert(approx(col[3], 45.0));

    // Multiply by 2
    for (size_t i = 0; i < col.size(); i++) col[i] *= 2.0;
    assert(approx(col[0], 30.0));
    assert(approx(col[3], 90.0));

    // Subtract 10
    for (size_t i = 0; i < col.size(); i++) col[i] -= 10.0;
    assert(approx(col[0], 20.0));
    assert(approx(col[3], 80.0));

    // Divide by 4
    for (size_t i = 0; i < col.size(); i++) col[i] /= 4.0;
    assert(approx(col[0], 5.0));
    assert(approx(col[3], 20.0));

    std::cout << "PASSED" << std::endl;
}

static void test_xlsx_multi_sheet() {
    std::cout << "test_xlsx_multi_sheet... ";

    const char *xlsx_path = "/tmp/test_dataframe_multisheet.xlsx";

    // Write a multi-sheet xlsx using libxlsxwriter
    lxw_workbook *wb = workbook_new(xlsx_path);

    lxw_worksheet *ws1 = workbook_add_worksheet(wb, "Sheet1");
    worksheet_write_string(ws1, 0, 0, "a", nullptr);
    worksheet_write_number(ws1, 1, 0, 100, nullptr);
    worksheet_write_number(ws1, 2, 0, 200, nullptr);

    lxw_worksheet *ws2 = workbook_add_worksheet(wb, "Sheet2");
    worksheet_write_string(ws2, 0, 0, "b", nullptr);
    worksheet_write_number(ws2, 1, 0, 999, nullptr);
    worksheet_write_number(ws2, 2, 0, 888, nullptr);

    lxw_error err = workbook_close(wb);
    assert(err == LXW_NO_ERROR);

    // Read Sheet1 (default, first sheet)
    {
        OpenXLSX::XLDocument doc;
        doc.open(xlsx_path);
        auto ws = doc.workbook().worksheet(1);
        assert(ws.rowCount() == 3);
        auto c1 = ws.cell(1, 1);
        assert(c1.value().get<std::string>() == "a");
        auto c2 = ws.cell(2, 1);
        assert(approx(c2.value().get<double>(), 100.0));
        doc.close();
    }

    // Read Sheet2 by name
    {
        OpenXLSX::XLDocument doc;
        doc.open(xlsx_path);
        auto ws = doc.workbook().worksheet("Sheet2");
        assert(ws.rowCount() == 3);
        auto c1 = ws.cell(1, 1);
        assert(c1.value().get<std::string>() == "b");
        auto c2 = ws.cell(2, 1);
        assert(approx(c2.value().get<double>(), 999.0));
        doc.close();
    }

    std::cout << "PASSED" << std::endl;
}

static void test_curvefit_log() {
    std::cout << "test_curvefit_log... ";

    // y = 2 + 3*log(x) => LogFit uses PolyFit(1) on (log(x), y)
    // So coeffs[0] = intercept = 2, coeffs[1] = slope = 3
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4, 5, 6, 7};
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    std::vector<double> y(8);
    for (int i = 0; i < 8; i++) {
        y[i] = 2.0 + 3.0 * std::log(x[i]);
    }

    df.load_index(std::move(idx));
    df.load_column("x", std::move(x));
    df.load_column("y", std::move(y));

    LogFitVisitor<double, unsigned long> v;
    df.single_act_visit<double, double>("x", "y", v);

    auto &coeffs = v.get_result();
    assert(coeffs.size() == 2);
    assert(approx(coeffs[0], 2.0, 1e-3));  // intercept
    assert(approx(coeffs[1], 3.0, 1e-3));  // slope (coefficient of log(x))

    std::cout << "PASSED" << std::endl;
}

static void test_stats_on_long_columns() {
    std::cout << "test_stats_on_long_columns... ";

    // Verify that stats visitors work on long data promoted to double
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4};
    std::vector<long> ages = {20, 30, 40, 50, 60};

    df.load_index(std::move(idx));
    df.load_column("age", std::move(ages));

    // Promote to double and compute stats
    auto &lcol = df.get_column<long>("age");
    std::vector<double> dcol(lcol.begin(), lcol.end());

    ULDataFrame tmp;
    std::vector<unsigned long> tidx = {0, 1, 2, 3, 4};
    tmp.load_index(std::move(tidx));
    tmp.load_column("age", std::move(dcol));

    MeanVisitor<double, unsigned long> mean_v;
    tmp.visit<double>("age", mean_v);
    assert(approx(mean_v.get_result(), 40.0));

    SumVisitor<double, unsigned long> sum_v;
    tmp.visit<double>("age", sum_v);
    assert(approx(sum_v.get_result(), 200.0));

    MinVisitor<double, unsigned long> min_v;
    tmp.visit<double>("age", min_v);
    assert(approx(min_v.get_result(), 20.0));

    MaxVisitor<double, unsigned long> max_v;
    tmp.visit<double>("age", max_v);
    assert(approx(max_v.get_result(), 60.0));

    // Two-column: corr between long and double
    std::vector<double> scores = {60.0, 70.0, 80.0, 90.0, 100.0};
    auto &lcol2 = df.get_column<long>("age");
    std::vector<double> dcol2(lcol2.begin(), lcol2.end());

    ULDataFrame tmp2;
    std::vector<unsigned long> tidx2 = {0, 1, 2, 3, 4};
    tmp2.load_index(std::move(tidx2));
    tmp2.load_column("age", std::move(dcol2));
    tmp2.load_column("score", std::move(scores));

    CorrVisitor<double, unsigned long> corr_v;
    tmp2.visit<double, double>("age", "score", corr_v);
    assert(approx(corr_v.get_result(), 1.0));  // perfect positive correlation

    std::cout << "PASSED" << std::endl;
}

static void test_melt() {
    std::cout << "test_melt... ";

    // Wide format: name (string), jan (double), feb (double)
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2};
    std::vector<std::string> names = {"Alice", "Bob", "Carol"};
    std::vector<double> jan = {10.0, 20.0, 30.0};
    std::vector<double> feb = {40.0, 50.0, 60.0};

    df.load_index(std::move(idx));
    df.load_column("name", std::move(names));
    df.load_column("jan", std::move(jan));
    df.load_column("feb", std::move(feb));

    // Melt on "name", unpivoting jan and feb
    std::vector<const char *> val_cols = {"jan", "feb"};
    auto melted = df.unpivot<std::string, double>("name", std::move(val_cols));

    // Should have 6 rows (3 names x 2 value columns)
    assert(melted.get_index().size() == 6);

    // Columns: name, variable, values
    auto &m_names = melted.get_column<std::string>("name");
    auto &m_var = melted.get_column<std::string>("variable");
    auto &m_vals = melted.get_column<double>("values");

    assert(m_names.size() == 6);
    assert(m_var.size() == 6);
    assert(m_vals.size() == 6);

    // First 3 rows should be jan values, next 3 feb values
    // (or interleaved -- check by matching name+variable pairs)
    bool found_alice_jan = false;
    bool found_bob_feb = false;
    for (size_t i = 0; i < 6; i++) {
        if (m_names[i] == "Alice" && m_var[i] == "jan") {
            assert(approx(m_vals[i], 10.0));
            found_alice_jan = true;
        }
        if (m_names[i] == "Bob" && m_var[i] == "feb") {
            assert(approx(m_vals[i], 50.0));
            found_bob_feb = true;
        }
    }
    assert(found_alice_jan);
    assert(found_bob_feb);

    std::cout << "PASSED" << std::endl;
}

static void test_pivot() {
    std::cout << "test_pivot... ";

    // Long format: 6 rows, variable column repeating "jan", "feb"
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2, 3, 4, 5};
    std::vector<std::string> variable = {"jan", "feb", "jan", "feb", "jan", "feb"};
    std::vector<double> temp = {10.0, 40.0, 20.0, 50.0, 30.0, 60.0};

    df.load_index(std::move(idx));
    df.load_column("variable", std::move(variable));
    df.load_column("temp", std::move(temp));

    // Pivot: variable column becomes new column names, temp is the value
    std::vector<const char *> val_cols = {"temp"};
    auto pivoted = df.pivot<double>("variable", std::move(val_cols));

    // Should have 3 rows (6 / 2 repeating values)
    assert(pivoted.get_index().size() == 3);

    // Should have columns "jan" and "feb"
    auto &jan_col = pivoted.get_column<double>("jan");
    auto &feb_col = pivoted.get_column<double>("feb");

    assert(jan_col.size() == 3);
    assert(feb_col.size() == 3);

    assert(approx(jan_col[0], 10.0));
    assert(approx(jan_col[1], 20.0));
    assert(approx(jan_col[2], 30.0));
    assert(approx(feb_col[0], 40.0));
    assert(approx(feb_col[1], 50.0));
    assert(approx(feb_col[2], 60.0));

    std::cout << "PASSED" << std::endl;
}

static void test_transpose() {
    std::cout << "test_transpose... ";

    // 3 rows x 2 columns, all double
    ULDataFrame df;
    std::vector<unsigned long> idx = {0, 1, 2};
    std::vector<double> a = {1.0, 2.0, 3.0};
    std::vector<double> b = {4.0, 5.0, 6.0};

    df.load_index(std::move(idx));
    df.load_column("a", std::move(a));
    df.load_column("b", std::move(b));

    // Transpose: 2 rows x 3 columns
    std::vector<unsigned long> new_idx = {0, 1};
    std::vector<std::string> new_col_names = {"row_0", "row_1", "row_2"};

    auto transposed = df.transpose<double>(std::move(new_idx), new_col_names);

    assert(transposed.get_index().size() == 2);

    auto &r0 = transposed.get_column<double>("row_0");
    auto &r1 = transposed.get_column<double>("row_1");
    auto &r2 = transposed.get_column<double>("row_2");

    assert(r0.size() == 2);
    assert(r1.size() == 2);
    assert(r2.size() == 2);

    // Original: a=[1,2,3], b=[4,5,6] (columns sorted alphabetically)
    // Transposed row 0 = column "a" values: 1, 2, 3
    // Transposed row 1 = column "b" values: 4, 5, 6
    assert(approx(r0[0], 1.0));
    assert(approx(r1[0], 2.0));
    assert(approx(r2[0], 3.0));
    assert(approx(r0[1], 4.0));
    assert(approx(r1[1], 5.0));
    assert(approx(r2[1], 6.0));

    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "DataFrame integration tests" << std::endl;
    std::cout << "===========================" << std::endl;

    test_load_and_shape();
    test_csv_io();
    test_statistics();
    test_filter();
    test_sort();
    test_groupby();
    test_join();
    test_xlsx_write_read_roundtrip();
    test_xlsx_to_dataframe();
    test_fill_missing_forward();
    test_fill_missing_backward();
    test_fill_missing_value();
    test_curvefit_linear();
    test_curvefit_poly();
    test_curvefit_exp();
    test_curvefit_log();
    test_curvefit_spline();
    test_apply_arithmetic();
    test_xlsx_multi_sheet();
    test_stats_on_long_columns();
    test_melt();
    test_pivot();
    test_transpose();

    std::cout << std::endl;
    std::cout << "All tests passed." << std::endl;
    return EXIT_SUCCESS;
}
