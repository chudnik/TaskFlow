# TaskFlow

C++20 modular-monolith backend scaffold for a multi-user task management service.

## Build

```sh
cmake -S . -B build/bootstrap -DTASKFLOW_BUILD_TESTS=ON
cmake --build build/bootstrap
ctest --test-dir build/bootstrap --output-on-failure
```
