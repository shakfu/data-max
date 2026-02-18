# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and [Commons Changelog](https://common-changelog.org). This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Types of Changes

- Added: for new features.
- Changed: for changes in existing functionality.
- Deprecated: for soon-to-be removed features.
- Removed: for now removed features.
- Fixed: for any bug fixes.
- Security: in case of vulnerabilities.

---

## [Unreleased]

### Added

- `corr <col1> <col2>` message for Pearson correlation between two numeric columns
- `quantile <col> <value>` message for arbitrary quantile queries (value in [0.0, 1.0])
- Multi-sheet XLSX: `read <file> [sheet]` accepts an optional sheet name
- `fill_missing <col> <policy> [value]` message for filling NaN/missing values (forward, backward, value, linear, midpoint)
- `curvefit <type> <x_col> <y_col> [degree]` message for curve fitting (linear, poly, exp, log, spline); outputs coefficients as list
- `apply <col> <op> <value>` message for in-place arithmetic on a column (+, -, *, /)
- `todict [name]` message to export DataFrame as a Max dict
- `melt <id_col> [val_col1 ...]` message for wide-to-long reshape (unpivot)
- `pivot <names_col> <val_col1> [val_col2 ...]` message for long-to-wide reshape
- `transpose` message to transpose an all-double DataFrame

### Changed

- Statistics functions (`mean`, `median`, `std`, `var`, `sum`, `min`, `max`, `describe`, `corr`, `quantile`, `curvefit`) now accept both `double` and `long` columns; long values are promoted to double transparently
- `read` and `write` now run on a background thread (systhread + defer_low), keeping the Max scheduler responsive during file I/O
- Removed legacy synchronous `read_plain_csv`, `write_plain_csv`, `read_xlsx`, `write_xlsx` functions superseded by threaded I/O

## [0.1.2]

### Added

- XLSX read/write support in `dataframe` external via OpenXLSX and libxlsxwriter
- `examples/sample.xlsx` (cities dataset, distinct from `sample.csv`)
- `sort <col> [asc|desc]` message for in-place sorting by any column type
- `groupby <col> <agg> <val_col>` message for group-by with aggregation (sum, mean, count, min, max)
- `join <name> <col> [inner|left|right|outer]` message for joining two named DataFrames by a shared column

## [0.1.1]

### Added

- `dataframe` external: pandas-like columnar data manipulation with named instances
  - I/O: `read`/`write` for plain CSV and JSON (format auto-detected by extension)
  - Inspection: `bang`, `columns`, `shape`, `head`, `tail`, `getcol`
  - Statistics: `mean`, `median`, `std`, `var`, `sum`, `min`, `max`, `count`, `describe`
  - Filtering: `sel` with comparison operators (`>`, `<`, `>=`, `<=`, `==`, `!=`)
  - Column types: double, long, string (auto-detected on import)
- `examples/sample.csv` (student scores dataset)
- `help/dataframe.maxhelp`
- DataFrame library integration tests (`test_dataframe`)

### Removed

- `matplotx` external (unused)
- Eigen and NumCpp dependencies (unused)

## [0.1.0]

### Added

- `xlsxw` external: write `.xlsx` spreadsheet files from Max
