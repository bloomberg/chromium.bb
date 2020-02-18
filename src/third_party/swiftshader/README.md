# SwiftShader

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0) [![Build Status](https://travis-ci.org/google/swiftshader.svg?branch=master)](https://travis-ci.org/google/swiftshader) [![Build status](https://ci.appveyor.com/api/projects/status/yrmyvb34j22jg1uj?svg=true)](https://ci.appveyor.com/project/c0d1f1ed/swiftshader)

Introduction
------------

SwiftShader is a high-performance CPU-based implementation of the Vulkan, OpenGL ES, and Direct3D 9 graphics APIs<sup>1</sup><sup>2</sup>. Its goal is to provide hardware independence for advanced 3D graphics.

Building
--------

SwiftShader libraries can be built for Windows, Linux, and Mac OS X.\
Android and Chrome (OS) build environments are also supported.

* **Visual Studio**
\
  For building the Vulkan ICD library, use [Visual Studio 2019](https://visualstudio.microsoft.com/vs/community/) to open the project folder and wait for it to run CMake. Open the [CMake Targets View](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=vs-2019#ide-integration) in the Solution Explorer and select the vk_swiftshader project to [build](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=vs-2019#building-cmake-projects) it.

  There is also a legacy [SwiftShader.sln](SwiftShader.sln) file for Visual Studio 2017 for building OpenGL ES and Direct3D libraries. Output DLLs will be placed in the _out_ subfolder. Sample executables such as _OGLES3ColourGrading_ can be found under the Tests solution folder and can be run from the IDE.

* **CMake**

  [Install CMake](https://cmake.org/download/) for Linux, Mac OS X, or Windows and use either [the IDE](https://cmake.org/runningcmake/) or run the following terminal commands:

      cd build
      cmake ..
      make --jobs=8

      ./gles-unittests
      ./OGLES2HelloAPI

Usage
-----

The SwiftShader libraries act as drop-in replacements for graphics drivers.

On Windows, most applications can be made to use SwiftShader's DLLs by placing them in the same folder as the executable. On Linux, the LD\_LIBRARY\_PATH environment variable or -rpath linker option can be used to direct applications to search for shared libraries in the indicated directory first.

Contributing
------------

See [CONTRIBUTING.txt](CONTRIBUTING.txt) for important contributing requirements.

The canonical repository for SwiftShader is hosted at:
https://swiftshader.googlesource.com/SwiftShader

All changes must be reviewed and approved in the [Gerrit](https://www.gerritcodereview.com/) review tool at:
https://swiftshader-review.googlesource.com

Authenticate your account here:
https://swiftshader-review.googlesource.com/new-password

All changes require a [Change-ID](https://gerrit-review.googlesource.com/Documentation/user-changeid.html) tag in the commit message. A commit hook may be used to add this tag automatically, and can be found at:
https://gerrit-review.googlesource.com/tools/hooks/commit-msg. To clone the repository and install the commit hook in one go:

    git clone https://swiftshader.googlesource.com/SwiftShader && (cd SwiftShader && curl -Lo `git rev-parse --git-dir`/hooks/commit-msg https://gerrit-review.googlesource.com/tools/hooks/commit-msg ; chmod +x `git rev-parse --git-dir`/hooks/commit-msg)

Changes are uploaded to Gerrit by executing:

    git push origin HEAD:refs/for/master

Testing
-------

SwiftShader's OpenGL ES implementation can be tested using the [dEQP](https://github.com/KhronosGroup/VK-GL-CTS) test suite.

See [docs/dEQP.md](docs/dEQP.md) for details.

Third-Party Dependencies
------------------------

The [third_party](third_party/) directory contains projects which originated outside of SwiftShader:

[subzero](third_party/subzero/) contains a fork of the [Subzero](https://chromium.googlesource.com/native_client/pnacl-subzero/) project. It is part of Google Chrome's (Portable) [Native Client](https://developer.chrome.com/native-client) project. Its authoritative source is at [https://chromium.googlesource.com/native_client/pnacl-subzero/](https://chromium.googlesource.com/native_client/pnacl-subzero/). The fork was made using [git-subtree](https://github.com/git/git/blob/master/contrib/subtree/git-subtree.txt) to include all of Subzero's history, and until further notice it should **not** diverge from the upstream project. Contributions must be tested using the [README](third_party/subzero/docs/README.rst) instructions, reviewed at [https://chromium-review.googlesource.com](https://chromium-review.googlesource.com/q/project:native_client%252Fpnacl-subzero), and then pulled into the SwiftShader repository.

[llvm-subzero](third_party/llvm-subzero/) contains a minimized set of LLVM dependencies of the Subzero project.

[PowerVR_SDK](third_party/PowerVR_SDK/) contains a subset of the [PowerVR Graphics Native SDK](https://github.com/powervr-graphics/Native_SDK) for running several sample applications.

[googletest](third_party/googletest/) contains the [Google Test](https://github.com/google/googletest) project, as a Git submodule. It is used for running unit tests for Chromium, and Reactor unit tests. Run `git submodule update --init` to obtain/update the code. Any contributions should be made upstream.

Documentation
-------------

See [docs/Index.md](docs/Index.md).

Contact
-------

Public mailing list: [swiftshader@googlegroups.com](https://groups.google.com/forum/#!forum/swiftshader)

General bug tracker:  https://g.co/swiftshaderbugs\
Chrome specific bugs: https://bugs.chromium.org/p/swiftshader

License
-------

The SwiftShader project is licensed under the Apache License Version 2.0. You can find a copy of it in [LICENSE.txt](LICENSE.txt).

Files in the third_party folder are subject to their respective license.

Authors and Contributors
------------------------

The legal authors for copyright purposes are listed in [AUTHORS.txt](AUTHORS.txt).

[CONTRIBUTORS.txt](CONTRIBUTORS.txt) contains a list of names of individuals who have contributed to SwiftShader. If you're not on the list, but you've signed the [Google CLA](https://cla.developers.google.com/clas) and have contributed more than a formatting change, feel free to request to be added.

Disclaimer
----------

1. Trademarks are the property of their respective owners.
2. We do not claim official conformance with the Direct3D and OpenGL graphics APIs at this moment.
3. This is not an official Google product.
