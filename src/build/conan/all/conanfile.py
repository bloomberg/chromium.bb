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
        self.run("7za.exe x -y -mmt -o\".\" \"archive.7z\"")

    def build(self):
        chromium_version, bb_version = self.version.split("_")
        bitness_path_suffix = "64" if "64" in str(self.settings.arch) else ""
        os.rename("resources/CMakeLists.txt", "CMakeLists.txt")
        cmake = CMake(self, generator="Ninja")
        cmake.configure(defs={
            "BLPWTK2_VERSION": self.version,
            "BLPWTK2_CR_VERSION": chromium_version,
            "BLPWTK2_BB_VERSION": bb_version,
            "BITNESS_PATH_SUFFIX": bitness_path_suffix
        })
        cmake.build()
        cmake.install()

    # def package(self):
        # self.copy(pattern="*", src=os.path.join("3.16.1"), keep_path=True)

    def package_info(self):
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.libs = [f"blpwtk2.{self.version}.dll.lib",]
        self.output.info(f"Libs: [{', '.join(self.cpp_info.libs)}]")
        self.cpp_info.includedirs = ["include/blpwtk2", "include/v8"]
        self.output.info(f"Include Dirs: [{', '.join(self.cpp_info.includedirs)}]")
        cmake_prefix = os.path.join(self.package_folder, "lib", "cmake")
        self.env_info.CMAKE_PREFIX_PATH.append(cmake_prefix)
        self.output.info(f"CMake Prefix Path: [{cmake_prefix}]")
        self.cpp_info.defines = [f"BLPWTK2_VERSION={self.version}",]
