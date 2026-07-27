# syntax=docker/dockerfile:1.7

FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG CONAN_VERSION=2.30.0

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
      build-essential \
      ca-certificates \
      cmake \
      git \
      ninja-build \
      pkg-config \
      python3 \
      python3-pip \
    && python3 -m pip install --break-system-packages --no-cache-dir "conan==${CONAN_VERSION}" \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --uid 10001 taskflow
USER taskflow
WORKDIR /workspace

COPY --chown=taskflow:taskflow conanfile.py CMakeLists.txt CMakePresets.json ./
COPY --chown=taskflow:taskflow cmake ./cmake
RUN --mount=type=cache,target=/home/taskflow/.conan2,uid=10001,gid=10001 \
    conan profile detect --force \
    && conan install . \
      --output-folder=build/conan/release \
      --build=missing \
      -s build_type=Release \
      -s compiler.cppstd=20

COPY --chown=taskflow:taskflow apps ./apps
COPY --chown=taskflow:taskflow include ./include
COPY --chown=taskflow:taskflow src ./src
COPY --chown=taskflow:taskflow tests ./tests
COPY --chown=taskflow:taskflow openapi ./openapi
RUN --mount=type=cache,target=/home/taskflow/.conan2,uid=10001,gid=10001 \
    cmake --preset ci-release \
    && cmake --build --preset ci-release

FROM build AS api
EXPOSE 8080
ENTRYPOINT ["/workspace/build/ci-release/apps/taskflow-api"]

FROM build AS worker
ENTRYPOINT ["/workspace/build/ci-release/apps/taskflow-worker"]

FROM build AS integration-tests
ENTRYPOINT ["ctest", "--test-dir", "/workspace/build/ci-release", "--output-on-failure"]
