# Blink 'common' directory

This directory contains the common Web Platform stuff that needs to be shared
by renderer-side and browser-side code.

Things that live in `third_party/WebKit` can directly depend on this directory,
while the code outside the WebKit directory (e.g. `//content` and `//chrome`)
can only depend on the common stuff via the public headers exposed in
`WebKit/public/common`.

Anything in this directory should **NOT** depend on the non-common stuff
in the WebKit directory. See `DEPS` and `BUILD.gn` files for more details.

Code in this directory would normally use `blink` namespace.

Unlike other directories in WebKit, code in this directory should:

* Use Chromium's common types (e.g. //base ones) rather than Blink's ones
  (e.g. WTF types)

* Use underscore_separated_file_names.cc style rather than CamelCase.cpp.

* Follow [Chromium's common coding style guide](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md)

* Use full-path from src/ for includes (e.g. `third_party/WebKit/common/foo.h`
  rather than `common/foo.h`). Likewise, code outside this directory that
  includes files in this directory should use the full-path.
