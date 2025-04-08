# data-max - data-related Max external(s)

This is an early-stage exploratory Max project for prototyping data-related Max externals.

The current task is to research relevant external libraries, which should be largely self-contained with minimal external dependencies and test how to build them and whether they are suitable to be incorporated into a useful Max external.

Tests are created in the `data-max/source/tests` folder, these include tests for 

- [eigen](https://eigen.tuxfamily.org) - C++ template library for linear algebra: matrices, vectors, numerical solvers, and related algorithms

- [numcpp](https://github.com/dpilger26/NumCpp) - C++ implementation of the Python Numpy library


Additionally, the following proof-of-concept externals were created with the following libraries:

- **xlsxw** - uses [libxlsxwriter](https://github.com/jmcnamara/libxlsxwriter) to generate `.xlsx` files (working)


## Building

Development is being done on an M1 Macbook Air. To build for  macos `arm64`:

```sh
make
```


To make install this project into Max either copy it into `~/Documents/Max 9/Packages`, or better still, run the following to symlink this package to `~/Documents/Max 9/Packages`

```sh
make setup
```


## Research

- [openxlsx](https://github.com/troldal/OpenXLSX) - A C++ library for reading, writing, creating and modifying `.xlsx` files

- [eigen](https://eigen.tuxfamily.org) - C++ template library for linear algebra: matrices, vectors, numerical solvers, and related algorithms

- [ndarray](https://github.com/ndarray/ndarray) - NumPy-compatible multidimensional arrays in C++

- [numcpp](https://github.com/dpilger26/NumCpp) - C++ implementation of the Python Numpy library

- [xtensor](https://github.com/xtensor-stack/xtensor) - C++ tensors with broadcasting and lazy computing

- [stdlib](https://github.com/stdlib-js/stdlib) - Standard numerical computation library for JavaScript and Node.js

- [dataframe](https://github.com/hosseinmoein/DataFrame) - C++ DataFrame for statistical, Financial, and ML analysis -- in modern C++ using native types and contiguous memory storage

- [apache arrow for c++](https://arrow.apache.org/docs/cpp/index.html) - Apache Arrow is a universal columnar format and multi-language toolbox for fast data interchange and in-memory analytics.

- [lunasvg](https://github.com/sammycage/lunasvg) - SVG rendering and manipulation library in C++



