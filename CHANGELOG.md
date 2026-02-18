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
