![Build Status](https://github.com/tud-zih-energy/FIRESTARTER/workflows/Build/badge.svg)

# FIRESTARTER - A Processor Stress Test Utility

FIRESTARTER can be build under Linux, Windows and macOS with CMake.

GCC (>=7) or Clang (>=9) is supported.

CMake option | Description
:--- | :---
FIRESTARTER_LINK_STATIC | Link FIRESTARTER as a static binary. Note, dlopen is not supported in static binaries. Default ON
FIRESTARTER_CUDA | Build FIRESTARTER with CUDA support. This will result in a shared linked binary. Default OFF
FIRESTARTER_BUILD_HWLOC | Build hwloc dependency. Default ON
FIRESTARTER_THREAD_AFFINITY | Enable FIRESTARTER to set affinity to hardware threads. Default ON

# Reference

A detailed description can be found in the following paper. Please cite this if you use FIRESTARTER for scientific work.

Daniel Hackenberg, Roland Oldenburg, Daniel Molka, and Robert Schöne
[Introducing FIRESTARTER: A processor stress test utility](http://dx.doi.org/10.1109/IGCC.2013.6604507) (IGCC 2013)

Additional information: https://tu-dresden.de/zih/forschung/projekte/firestarter

# License

This program contains a slightly modified version of the implementation of the NSGA2 algorithm from [pagmo](https://github.com/esa/pagmo2) licensed under LGPL or GPL v3.

# Contact

Daniel Hackenberg < daniel dot hackenberg at tu-dresden.de >
