# tbb-python

This repository contains an example of how to call into python from tbb flow nodes.

## Requirements

- `cmake`

## Compiling

Generate and build a project using cmake, for example using `ninja` with Debug configuration.
Dependencies wil be downloaded automatically.

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -GNinja ..
ninja
```

This will create an executable that should finish without errors.
```bash
./bin/tbb-python-example
```