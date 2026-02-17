# Ideas for New Externals

## Data I/O Externals

**`jsonio`** -- Structured JSON read/write with query support (JSONPath or jq-like selectors). Max has `dict` but its JSON support is limited. This would allow importing complex nested JSON (e.g., from APIs or sensor logs) and extracting specific fields into Max messages. Could use [simdjson](https://github.com/simdjson/simdjson) or [nlohmann/json](https://github.com/nlohmann/json).

## Data Transformation Externals

**`pivot`** -- Reshape tabular data: pivot (long-to-wide), melt (wide-to-long), stack/unstack. A focused external that pairs with `dataframe` or works standalone on `coll`/`dict` data.

**`norm`** -- Normalize and scale data: min-max, z-score, log-transform, quantile normalization. Common preprocessing step before visualization or ML. Takes lists or buffer~ data.

**`interp`** -- Interpolation external: linear, cubic spline, Lagrange, Akima. Input a set of control points, query arbitrary positions. Useful for mapping curves, transfer functions, or resampling irregular data to regular grids. DataFrame has `CubicSplineFitVisitor` but a standalone interpolation object would be more flexible for real-time use.

## `dataframe` v2 Enhancements

The v1 `dataframe` external is implemented and provides: named instances, plain CSV/JSON I/O, column access, descriptive statistics (mean, median, std, var, sum, min, max, count, describe), and row filtering (sel). Potential v2 additions:

- **xlsx I/O** -- Read/write `.xlsx` directly, leveraging the existing libxlsxwriter and OpenXLSX dependencies. Subsumes the standalone `xlsxr` idea.
- **sort** -- Sort by one or more columns (ascending/descending). DataFrame library supports multi-column sort.
- **groupby** -- Group-by with aggregation (sum, mean, count per group). Maps to DataFrame's `groupby1`/`groupby2`.
- **merge/join** -- Join two named DataFrames by a shared column. DataFrame supports inner/outer/left/right joins.
- **apply** -- Apply a Max expression or lambda to each row or column (e.g., `apply score * 1.1`).
- **corr** -- Pairwise correlation between two columns. DataFrame has `CorrVisitor`.
- **quantile** -- Arbitrary quantile queries (e.g., `quantile score 0.25`). DataFrame has `QuantileVisitor`.
- **fill_missing** -- Fill NaN/missing values (forward fill, backward fill, constant). DataFrame has `fill_missing`.
- **curvefit** -- Fit standard curve families to column data. DataFrame has `PolyFitVisitor`, `ExponentialFitVisitor`, `LinearFitVisitor`, `LogFitVisitor`, `CubicSplineFitVisitor`.
- **defer/threading** -- Move read/compute operations off the main thread for large datasets.
- **dict interop** -- Export DataFrame contents as a Max `dict` for integration with `dict.view`, `dict.serialize`, etc.

## Visualization Externals

**`svgplot`** -- Generate SVG plots from data: scatter, line, bar, histogram, heatmap. SVG is a lighter approach than matplotx. Output SVG files or render to `jit.matrix` for display in `jit.pwindow`. No heavy graphics dependencies.

**`sparkline`** -- Minimal inline visualization as a Max UI object. Draws a small time-series sparkline in the patcher, useful for monitoring data streams at a glance. Lighter than a full plotting external.

## Signal Analysis Externals

**`features~`** -- Audio feature extraction: RMS, spectral centroid, spectral flux, zero-crossing rate, MFCCs, onset detection. Bridges audio signals into data-domain analysis. Could output to `dataframe` or lists for downstream statistical processing.

**`dtw`** -- Dynamic Time Warping distance between two sequences. Useful for comparing gesture data, audio segments, or any time-series. Returns distance and alignment path.

## Prioritization

Ranked by impact-to-effort ratio and coherence with existing work:

1. **`dataframe` v2** -- xlsx I/O, sort, groupby, join (extend the working foundation)
2. **`svgplot`** -- fills the visualization gap
3. **`interp`** -- broadly useful for mapping and resampling
4. **`features~`** -- bridges audio and data domains

## Design Consideration

The `dataframe` external validates the "richer object" approach -- bundling I/O, query, statistics, and filtering into one named-instance object (like `coll` or `buffer~`). This is more discoverable than scattering functionality across many small externals. Future externals should consider whether they belong as messages on an existing object (e.g., adding `corr` to `dataframe`) or as standalone objects that interoperate via shared naming.
