# Description
This directory contains tools related to src/chrome/predictors, and especially
`resource_prefetch_predictor`.

# Prerequisites
The following assumes a Chrome for Android setup, on Ubuntu 14.04LTS.

To use these tools, you need:
* Python protocol buffer library
* Protocol buffer Python modules to read the database
* SQLite >= 3.9

# Installation
* Install an updated protobuf library: `pip install --user protobuf`
* Generated protocol buffers in the path: Assuming that the build directory is
`out/Release` and chromium's `src/` is `$CHROMIUM_SRC`, `export
PYTHONPATH=$PYTHONPATH:${CHROMIUM_SRC}/out/Release/pyproto/chrome/browser/predictors/`
* SQLite from the Android SDK is recent enough, make sure that `${CHROMIUM_SRC}/third_party/android_tools/sdk/platform-tools` is in the path.
