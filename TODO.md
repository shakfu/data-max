# TODO

## `dataframe` v2 Enhancements

### Done

- [x] XLSX read/write (OpenXLSX + libxlsxwriter)
- [x] `sort <col> [asc|desc]` -- single-column sort, ascending/descending
- [x] `groupby <col> <agg> <val_col>` -- group-by with aggregation (sum, mean, count, min, max)
- [x] `join <name> <col> [inner|left|right|outer]` -- join two named DataFrames by a shared column

### Open

- [ ] Multi-sheet XLSX: accept optional sheet name in `read` (requires changing from `A_SYM` to `A_GIMME`)
- [ ] `corr` -- pairwise correlation between two columns
- [ ] `quantile` -- arbitrary quantile queries (e.g., `quantile score 0.25`)
- [ ] `fill_missing` -- fill NaN/missing values (forward fill, backward fill, constant)
- [ ] `curvefit` -- fit curves to column data (polynomial, exponential, linear, log, cubic spline)
- [ ] `defer`/threading -- move read/compute operations off the main thread
- [ ] `dict` interop -- export DataFrame contents as a Max `dict`
