# Caspian

## What is it?

Caspian is the name for the WebAssembly portion of the SuperSize Tiger Viewer.

## How do I build it?

Install emscripten from:
https://emscripten.org/docs/getting_started/downloads.html

```sh
git apply tools/binary_size/libsupersize/caspian/wasmbuild.patch
gn gen out/caspian --args='is_official_build=true treat_warnings_as_errors=false fatal_linker_warnings=false'
( cd out/caspian; autoninja tools/binary_size:caspian_web && cp wasm/caspian_web.* ../../tools/binary_size/libsupersize/static/ )
```

Then run locally via:
```sh
tools/binary_size/supersize start_server out.size
```

or upload to hosted site via:
```sh
tools/binary_size/libsupersize/upload_html_viewer.py --sync
```
