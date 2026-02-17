# TODO

## XLSX Support for `dataframe`

### Read (OpenXLSX)

OpenXLSX (already built and installed via `install_deps.sh`) provides C++17 read/write access. The read path:

```
XLDocument -> .workbook() -> .worksheet("Sheet1") -> .cell(row, col) -> .value()
```

Cell values are typed: `Empty`, `Boolean`, `Integer`, `Float`, `Error`, `String`.

Tasks:

- [ ] Add `read` support for `.xlsx` extension in `dataframe_read`
- [ ] Open XLDocument, iterate first worksheet's rows/columns
- [ ] Map XLValueType to col_type: `Float` -> DOUBLE, `Integer` -> LONG, `String`/`Boolean` -> STRING
- [ ] First row = column headers (same assumption as CSV reader)
- [ ] Type detection: scan all rows per column, promote LONG -> DOUBLE if mixed, fall back to STRING if any non-numeric
- [ ] Load columns into DataFrame via `load_index()` / `load_column()` (same pattern as `read_plain_csv`)
- [ ] Link OpenXLSX: add `${THIRDPARTY_LIB}/libOpenXLSX.a` to `target_link_libraries` and `${THIRDPARTY_INCLUDE}` to `target_include_directories` in `source/projects/dataframe/CMakeLists.txt`
- [ ] Handle multi-sheet workbooks: default to first sheet, optionally accept sheet name (e.g., `read file.xlsx Sheet2` -- would require changing `read` from `A_SYM` to `A_GIMME`)
- [ ] Test with sample `.xlsx` file

### Write (libxlsxwriter)

libxlsxwriter (already built and installed) is a C library for writing `.xlsx`. The API:

```c
lxw_workbook  *wb = workbook_new("path.xlsx");
lxw_worksheet *ws = workbook_add_worksheet(wb, "Sheet1");
worksheet_write_string(ws, row, col, "text", NULL);
worksheet_write_number(ws, row, col, 123.456, NULL);
workbook_close(wb);
```

Tasks:

- [ ] Add `write` support for `.xlsx` extension in `dataframe_write`
- [ ] Write header row from column names
- [ ] Iterate DataFrame rows, dispatch to `worksheet_write_number` (DOUBLE/LONG) or `worksheet_write_string` (STRING)
- [ ] Link libxlsxwriter: add `${THIRDPARTY_LIB}/libxlsxwriter.a` and `-lz` to `target_link_libraries`
- [ ] Wrap libxlsxwriter C calls inside `extern "C"` or include its header appropriately from C++
- [ ] Test round-trip: write xlsx from dataframe, read it back, verify data integrity

### Testing

- [ ] Create `examples/sample.xlsx` (same data as `sample.csv`) for help patch and testing
- [ ] Add xlsx read/write test cases to `test_dataframe` (or create separate test if OpenXLSX linkage is complex)

## Dependency Cleanup

### Eigen -- Remove

Eigen provides linear algebra (matrix decomposition, sparse matrices, solvers). Assessment:

- DataFrame's `Matrix<T>` already covers inverse, transpose, SVD, LU, eigendecomposition
- Eigen's unique advantages (sparse matrices, iterative solvers, expression templates) are for graph signal processing or FEM -- not relevant to tabular data work
- The only test (`test_eigen`) is a 14-line hello-world constructing a 2x2 matrix
- No Max external uses Eigen
- No planned external in IDEAS.md requires Eigen

Tasks:

- [ ] Remove `source/thirdparty/include/eigen3/`
- [ ] Remove `source/tests/test_eigen/`
- [ ] Update README.md to remove Eigen from the third-party libraries table

### NumCpp -- Remove

NumCpp provides NumPy-like N-dimensional arrays, nonlinear least squares, numerical integration, and signal filters. Assessment:

- Significant overlap with DataFrame on 1D statistics (mean, std, variance, etc.)
- Unique capabilities: `gaussNewtonNlls` (arbitrary nonlinear curve fitting), 2D array ops (meshgrid, slicing), numerical integration, signal filters
- `test_numcpp2` demonstrates real curve-fitting use cases (Gauss-Newton NLLS)
- However, DataFrame already has `PolyFitVisitor`, `LogFitVisitor`, `ExponentialFitVisitor`, `LinearFitVisitor`, `CubicSplineFitVisitor` covering standard curve-fitting families
- No Max external uses NumCpp, and no planned external in IDEAS.md requires it
- The 2D array / integration / filter capabilities, while interesting, don't align with the current direction of columnar tabular data

Tasks:

- [ ] Remove `source/thirdparty/include/numcpp/`
- [ ] Remove `source/tests/test_numcpp/`
- [ ] Remove `source/tests/test_numcpp2/`
- [ ] Update README.md to remove NumCpp from the third-party libraries table
