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

## Optimization of NEAT
This additional feature allows the the resource usage of NEAT to be
compared before and after changes to optimize NEAT.

The experiments are run with scripts written in bash. These are
added to the scripts folder. 

The files in the folder .../neat should be added to the NEAT GitHub repository. 


## Compilation
```shell
$ cd <path-to-repository-root>
$ mkdir build && cd build
$ cmake ..
$ make
```
