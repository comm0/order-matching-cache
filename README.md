# Order Cache

## Requirements

- Visual Studio 2022 with the C++ workload
- CMake 3.10+
- Google Test (installed via vcpkg)

## Installing Google Test

```
vcpkg install gtest:x64-windows-static-md
```

The `x64-windows-static-md` triplet builds a static library with the dynamic CRT (`/MD`), matching the project's runtime.

## Generating the Visual Studio solution

Run `generate_vs2022.bat`. It creates `vs/order_cache.sln`. `order_cache_test` is the startup project, so F5 builds and runs the tests.

Set `VCPKG_DIR` at the top of the script to your vcpkg location. Any extra arguments are forwarded to CMake.
