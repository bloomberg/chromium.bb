# Caspian

[TOC]

## What is it?

Caspian is the name for the WebAssembly portion of the SuperSize Tiger Viewer.
It also contains a Linux command-line test binary.

## Building the WASM Module

### 1. Install Emscripten

This step needs to be only once.

1. `git clone` emscripten:
https://emscripten.org/docs/getting_started/downloads.html
2. Install the known working version:
   ```sh
   cd $EMSDK; ./emsdk install 3.1.1 && ./emsdk activate 3.1.1
   ```
3. Activate it via environment variables:
   ```sh
   cd $EMSDK; source emsdk_env.sh
   ```

### 2. Apply Local Patches

Caspian needs some minor edits that we don't want to commit:

```sh
git apply -3 tools/binary_size/libsupersize/viewer/caspian/wasmbuild.patch
```

To re-create .patch file:
```sh
git add ...files to include in patch...
git diff --staged > tools/binary_size/libsupersize/viewer/caspian/wasmbuild.patch
# Double check that only expected files are included in the patch:
grep +++ tools/binary_size/libsupersize/viewer/caspian/wasmbuild.patch
```

### 3. Build

```sh
# Omit is_official_build=true if developing locally.
gn gen out/caspian --args='is_official_build=true treat_warnings_as_errors=false fatal_linker_warnings=false chrome_pgo_phase=0'
# Build and copy into static/ directory:
( cd out/caspian; autoninja caspian_web && cp wasm/caspian_web.* ../../tools/binary_size/libsupersize/viewer/static/ )
```

## Building the Linux Test App & Unit Tests

There is a test and a binary that you can run to help with development (and
allows debugging outside of the browser).

```sh
OUT=$out/linux_debug
autoninja -C $OUT caspian_cli caspian_unittests
$OUT/caspian_cli --help
$OUT/caspian_unittests
# To debug a crash:
lldb $OUT/caspian_cli validatediff supersize_diff.sizediff
```

## Debugging WASM

Follow this article: https://developer.chrome.com/blog/wasm-debugging-2020/

Make sure to:

 * Run Chrome on the same machine you are serving from
 * Use Chrome Dev or Canary (`sudo apt-get install google-chrome-unstable`)
 * Install the reference extension
 * Enabled DWARF debugging in DevTools' experiments panel

## Updating Emscripten Version

1. Run:
   ```
   cd $PATH_TO_EMSDK
   git pull origin main --tags
   ```
2.  Update this README's Emscripten version above, then follow its steps.
