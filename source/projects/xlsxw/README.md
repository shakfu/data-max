# xlsxw - a max external to .xlsx files

This early stage external wraps a subset of [libxlsxwriter](https://github.com/jmcnamara/libxlsxwriter) -- a C library for creating Excel XLSX files.


## Exchanging data with Max Datastructures

- `dict` 			<-> 2 cols or 2 rows

- `array` 			<-> 1 column or 1 row

- `table`			<-> 1 column or 1 row

- `coll` 			<-> xl table

- `jit.cellblock` 	<-> xl table

- `jit.matrix` 		<-> xl workbook (plane <-> worksheet)

