# TODO

## `dataframe` v2 Enhancements

### Done

- [x] XLSX read/write (OpenXLSX + libxlsxwriter)
- [x] `sort <col> [asc|desc]` -- single-column sort, ascending/descending
- [x] `groupby <col> <agg> <val_col>` -- group-by with aggregation (sum, mean, count, min, max)
- [x] `join <name> <col> [inner|left|right|outer]` -- join two named DataFrames by a shared column
- [x] `corr` -- pairwise correlation between two columns
- [x] `quantile` -- arbitrary quantile queries (e.g., `quantile score 0.25`)
- [x] Multi-sheet XLSX: accept optional sheet name in `read` (e.g., `read file.xlsx Sheet2`)
- [x] `fill_missing` -- fill NaN/missing values (forward, backward, value, linear, midpoint)
- [x] `curvefit` -- fit curves to column data (linear, poly, exp, log, spline)
- [x] `defer`/threading -- read/write operations run off the main thread
- [x] `apply` -- apply arithmetic operation to each value in a column (e.g., `apply score * 1.1`)
- [x] `dict` interop -- export DataFrame contents as a Max `dict` via `todict`
