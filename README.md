# TaskFlow

C++20 modular-monolith backend scaffold for a multi-user task management service.

## Bootstrap build

The bootstrap preset verifies the target graph without downloading third-party packages:

```sh
cmake --preset developer
cmake --build --preset developer
ctest --preset developer
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

The `conan-debug` preset can be used after dependency installation:

```sh
cmake --preset conan-debug
cmake --build --preset conan-debug
ctest --preset conan-debug
```

Release CI uses the same flow with `build/conan/release`, `build_type=Release`, and the
`ci-release` configure/build/test preset.

Quality targets are `format`, `format-check`, and `tidy`. Sanitizer builds use the `asan` and
`ubsan` presets.
