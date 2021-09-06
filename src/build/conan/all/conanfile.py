import os
from typing import Optional

from conans import CMake, ConanFile, tools
from conans.util.runners import check_output_runner
from conans.errors import ConanInvalidConfiguration


class BLPWTK2Conan(ConanFile):
    """Conanfile to produce a VS Build Tools binary package."""

    name = "blpwtk2"
    description = "blpwtk2 libraries and headers"
    url = "https://bbgithub.dev.bloomberg.com/buildbot/sotr-conan-index"
    settings = "os", "arch"
    build_requires = ("p7zip/19.00",)
    exports_sources = ["resources/CMakeLists.txt"]

    def configure(self):
        if self.settings.os != "Windows":
            raise ConanInvalidConfiguration("Only Windows supported")

    def source(self):
        tools.download(self.conan_data["sources"][self.version]["url"],
                       "archive.7z",
                       sha256=self.conan_data["sources"][self.version]["sha256"])

    def build(self):
        self.run("7za.exe x -y -mmt -o\".\" \"archive.7z\"")

    def package(self):
        bitness_path_suffix = "64" if "64" in str(self.settings.arch) else ""
        self.copy(f"{self.version}/lib{bitness_path_suffix}/*", dst="lib", keep_path=False)
        self.copy(f"{self.version}/include/blpwtk2/*", dst="include/blpwtk2", keep_path=False)
        self.copy(f"{self.version}/include/v8/*", dst="include/v8", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = [f"blpwtk2.{self.version}.dll.lib"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.defines = ["USING_BLPWTK2_SHARED", "USING_V8_SHARED", "USING_BLPWTK2V8"]
        if "64" in str(self.settings.arch):
            self.cpp_info.defines.append("V8_COMPRESS_POINTERS")

        self.cpp_info.includedirs = ["include/blpwtk2", "include/v8"]
        self.cpp_info.requires = [f"{x.split('/')[0]}::{x.split('/')[0]}" for x in self.requires]
        self.cpp_info.builddirs = [os.path.join(self.package_folder, "lib", "cmake")]

        cmake_prefix = os.path.join(self.package_folder, "lib", "cmake")
        self.env_info.CMAKE_PREFIX_PATH.append(cmake_prefix)
        self.output.info(f"CMake Prefix Path: [{cmake_prefix}]")
