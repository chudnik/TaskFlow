# TaskFlow

C++20 modular-monolith backend scaffold for a multi-user task management service.

## Build

```sh
cmake -S . -B build/bootstrap -DTASKFLOW_BUILD_TESTS=ON
cmake --build build/bootstrap
ctest --test-dir build/bootstrap --output-on-failure
```

## Full Conan build

Install Conan 2, create a default profile once, and install the pinned dependencies:

```sh
conan profile detect --force
conan install . --output-folder=build/conan/debug --build=missing -s build_type=Debug -s compiler.cppstd=20
cmake -S . -B build/conan-build -DCMAKE_TOOLCHAIN_FILE=build/conan/debug/conan_toolchain.cmake
cmake --build build/conan-build
ctest --test-dir build/conan-build --output-on-failure
```
