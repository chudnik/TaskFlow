from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class TaskFlowDependencies(ConanFile):
    required_conan_version = ">=2.28,<3"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("drogon/1.9.13")
        self.requires("libpq/17.7")
        self.requires("redis-plus-plus/1.3.15")
        self.requires("spdlog/1.17.0")
        self.requires("nlohmann_json/3.12.0")
        self.requires("jwt-cpp/0.7.2")
        self.requires("argon2/20190702")
        self.requires("gtest/1.17.0")

    def configure(self):
        self.options["drogon"].shared = False
        self.options["drogon"].with_orm = True
        self.options["drogon"].with_postgres = True
        self.options["drogon"].with_redis = False
        self.options["redis-plus-plus"].shared = False
        self.options["spdlog"].shared = False
        self.options["gtest"].shared = False

    def layout(self):
        cmake_layout(self)
        self.folders.generators = "."

    def generate(self):
        CMakeDeps(self).generate()

        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        toolchain.generate()
