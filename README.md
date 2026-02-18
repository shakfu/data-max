# data-max

A [Max](https://cycling74.com/products/max) package providing data-oriented externals for tabular data manipulation, spreadsheet I/O, and numerical computation.

## Externals

### dataframe

Pandas-like columnar data manipulation in Max. Uses named instances (like `coll` and `buffer~`) so multiple objects sharing a name share the same data.

**Supported column types:** double, long, string (auto-detected on import).

| Category | Messages | Description |
|---|---|---|
| I/O | `read <file>`, `write <file>`, `clear` | Read/write CSV, JSON, or XLSX. File lookup uses the Max search path. |
| Inspection | `bang`, `columns`, `shape`, `head <n>`, `tail <n>`, `getcol <col>` | Query structure and contents. |
| Statistics | `mean <col>`, `median <col>`, `std <col>`, `var <col>`, `sum <col>`, `min <col>`, `max <col>`, `count <col>`, `describe <col>` | Descriptive statistics on numeric columns. |
| Filtering | `sel <col> <op> <value>` | Filter rows in-place. Operators: `>`, `<`, `>=`, `<=`, `==`, `!=`. |
| Sorting | `sort <col> [asc\|desc]` | Sort rows by column (default ascending). |
| Groupby | `groupby <col> <agg> <val_col>` | Group by column, aggregate with `sum`, `mean`, `count`, `min`, `max`. |
| Join | `join <name> <col> [inner\|left\|right\|outer]` | Join with another named dataframe by a shared column. |

**Outlets:** left = data (lists, floats, symbols), right = info (bang on completion, shape, errors).

Built on the [DataFrame](https://github.com/hosseinmoein/DataFrame) C++ library (C++23).

### xlsxw

Write `.xlsx` spreadsheet files from Max. Built on [libxlsxwriter](https://github.com/jmcnamara/libxlsxwriter).

## Building

Requires Xcode command line tools on macOS (arm64 or x86_64).

```sh
make setup    # init submodules, symlink package into Max 9
make          # build all externals and tests
```

The build installs third-party dependencies automatically, produces `.mxo` bundles in `externals/`, and compiles standalone tests in `build/tests/`.

## Project Structure

```
source/
  projects/
    dataframe/      # dataframe external
    xlsxw/          # xlsx writer external
  tests/
    test_dataframe/ # DataFrame library integration test
  thirdparty/
    DataFrame/      # vendored DataFrame library (C++23)
  max-sdk-base/     # Max SDK (git submodule)
examples/
  sample.csv        # student scores (CSV)
  sample.xlsx       # world cities (XLSX)
help/
  dataframe.maxhelp # help patch for dataframe
  xlsxw.maxhelp     # help patch for xlsxw
```

## Third-Party Libraries

| Library | Purpose | Integration |
|---|---|---|
| [DataFrame](https://github.com/hosseinmoein/DataFrame) | Columnar data, statistics, filtering | Vendored, compiled into `dataframe` external |
| [libxlsxwriter](https://github.com/jmcnamara/libxlsxwriter) | `.xlsx` file writing | Built by `install_deps.sh`, linked into `dataframe` and `xlsxw` |
| [OpenXLSX](https://github.com/troldal/OpenXLSX) | `.xlsx` file reading | Built by `install_deps.sh`, linked into `dataframe` |
