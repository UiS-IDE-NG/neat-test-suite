# neat-test-suite
This test suite contains different tools to measure the
resource usage of NEAT. It contains source code for
NEAT, libuv, and kqueue servers and clients. The
resource usage of these applications are compared to
quantify the resource overhead of NEAT.

This test suite is based on running experiments with
TEACUP. Included in this test suite is an extended
version of the TEACUP code that enables running
performance experiments for NEAT. Also, TEACUP
config files for different kinds of experiments are
included.

The test suite also includes different scripts that
can be used to parse experiment data and produce
graphs.

## Compilation
```shell
$ cd <path-to-repository-root>
$ mkdir build && cd build
$ cmake ..
$ make
```