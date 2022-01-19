# LZ4F decompression API extention

The LZ4F decompression API does not expose/implement functions to that allow to
decompress up to 'size' bytes. This project implements such functions.

## Description

It was needed in another project of mine to implement functions similar to
fread(), fgets(), fgetc, gzread(), gzgets(), gzgetc(). The LZ4F API does not
implement this functionality.

This projects provides a similar API around an opaque struct, Cfp. The main
functions are
* cfopen(), to open a file for decompressing
* cfclose(), when done decompressing
* cfread(), to get up to size decompressed bytes in a provided buffer, similar
  to fread()
* cfgets(), to get a NULL terminated string up to new line or up to size,
  similar to fgets()
* cfgets(), to get a decompressed byte, similar to fgets()

## Getting Started

### Dependencies

* Linux/Unix
* gcc
* lz4 library
* make
* diff (for tests)
* valgrind, gcc (for debugging)

### Installing

To compile and install in ./build simply run make
 ` make `

To run some rundimentary tests run
 `make check`

To clean up
 `make clean`


## Help

For help run
 ` ./build/lz4_decompress_size h`

For example, to decompress file 'foo' using the fgets equivalent use:
 ` ./build/lz4_decompress_size l foo`

## Authors

Georgios

## Version History

* 0.1
    * Initial Release

## License

This project is licensed under the BSD License - see the LICENSE.md file
for details

## Acknowledgments

The testing file was taken from postgres source.  The LZ4F implementation can be
found in github.

* [wal.sgml](https://github.com/postgres/postgres/blob/master/doc/src/sgml/wal.sgml)
* [LZ4 source](https://github.com/lz4/lz4)
