# cfossil

Single-header C [fossil](github.com/dburkart/fossil) library. While fossil is a go-based
database, there are many scenarios where clients might like to append data from somewhere
go is not available, such as embedded devices. Since the main use case is for appending
data, this library does not yet support querying data from a fossil database.

## Using

To use `cfossil`, you can either copy "fossil.h" into your project, or you can build
`libcfossil.a` using the CMake project.

## Examples

For examples on how to use `cfossil`, see [the examples directory](./examples)