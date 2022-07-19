import os
import shutil

from conan import ConanFile
from conan.tools.files import download
from conan.errors import ConanInvalidConfiguration


class BLPWTK2Conan(ConanFile):
    """Conanfile to produce a BLPWTK2 binary package."""

    default_user = "devkit"
    default_channel = "stable"

    name = "blpwtk2"
    description = "blpwtk2 libraries and headers"
    url = "bbgithub:uiinf/chromium.bb"
    settings = "os", "arch"
    generators = ("VirtualBuildEnv",)

    tool_requires = ("p7zip/19.00@devkit/stable",)

    def configure(self):
        if self.settings.os != "Windows":
            raise ConanInvalidConfiguration("Only Windows supported")

    def source(self):
        download(
            self,
            f"{self.conan_data['sources']['url'][0]}{self.version}.7z",
            "archive.7z",
        )

    def build(self):
        self.run('7za.exe x -y -mmt -o"." "archive.7z"')
        file_names = os.listdir(self.version)

        for file_name in file_names:
            shutil.move(os.path.join(self.version, file_name), ".")

    def package(self):
        bitness_path_suffix = "64" if "64" in str(self.settings.arch) else ""
        self.copy(f"bin/release{bitness_path_suffix}/*", keep_path=True)
        self.copy(f"lib/release{bitness_path_suffix}/*", keep_path=True)
        self.copy("include/blpwtk2/*", keep_path=True)
        self.copy("include/v8/*", keep_path=True)

    def package_info(self):
        bitness_path_suffix = "64" if "64" in str(self.settings.arch) else ""
        bindir = f"bin/release{bitness_path_suffix}"
        libdir = f"lib/release{bitness_path_suffix}"

        self.cpp_info.components["blpwtk2-all"].requires = [
            f"{x.split('/')[0]}::{x.split('/')[0]}" for x in self.requires
        ]

        ## BLPWTK2 ##
        label = "blpwtk2-component"
        self.cpp_info.components[label].libs = [f"blpwtk2.{self.version}.dll.lib"]
        self.cpp_info.components[label].requires = ["icudt_from_blpwtk2"]
        self.cpp_info.components[label].bindirs = [bindir]
        self.cpp_info.components[label].libdirs = [libdir]
        self.cpp_info.components[label].defines = [
            "USING_BLPWTK2_SHARED",
            "USING_V8_SHARED",
            "USING_BLPWTK2V8",
        ]
        if "64" in str(self.settings.arch):
            self.cpp_info.components[label].defines.append("V8_COMPRESS_POINTERS")
        self.cpp_info.components[label].includedirs = [
            "include/blpwtk2",
            "include/v8",
        ]
        self.user_info.version = self.version

        ## ICUDT_FROM_BLPWTK2 ##
        self.cpp_info.components["icudt_from_blpwtk2"].bindirs = [bindir]
        self.cpp_info.components["icudt_from_blpwtk2"].libdirs = [libdir]

        ## V8 ##
        self.cpp_info.components["v8"].libs = [f"blpwtk2.{self.version}.dll.lib"]
        self.cpp_info.components["v8"].bindirs = [bindir]
        self.cpp_info.components["v8"].libdirs = [libdir]
        self.cpp_info.components["v8"].defines = [
            "USING_V8_SHARED",
            "USING_BLPWTK2V8",
        ]
        if "64" in str(self.settings.arch):
            self.cpp_info.components["v8"].defines.append("V8_COMPRESS_POINTERS")
        self.cpp_info.components["v8"].includedirs = [
            "include/blpwtk2",
            "include/v8",
        ]
        self.cpp_info.components["v8"].requires = [
            "blpwtk2-component",
            "icudt_from_blpwtk2",
        ]
        self.buildenv_info.append_path("PATH", os.path.join(self.package_folder, bindir))
