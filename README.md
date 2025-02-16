# data-max -- data-related Max external(s)

This is a Max package for prototyping date-related max externals:

- **xlsxw** - uses [libxlsxwriter](https://github.com/jmcnamara/libxlsxwriter) to generate `.xlsx` files (working)

- **matplotx** - tries to use [matplotplusplus](https://github.com/alandefreitas/matplotplusplus) to generate static plots (currently not working)

## Building

Development is being done on an M1 Macbook Air. To build for  macos `arm64`:

```sh
make
```


To make install this project into Max either copy it into `~/Documents/Max 9/Packages`, or better still, run the following to symlink this package to `~/Documents/Max 9/Packages`

```sh
make setup
```

