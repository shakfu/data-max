#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameStatsVisitors.h>

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
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

int main() {
    std::cout << "DataFrame integration tests" << std::endl;
    std::cout << "===========================" << std::endl;

    test_load_and_shape();
    test_csv_io();
    test_statistics();
    test_filter();
    // test_read_sample_csv requires running from project root
    // test_read_sample_csv();

    std::cout << std::endl;
    std::cout << "All tests passed." << std::endl;
    return EXIT_SUCCESS;
}
