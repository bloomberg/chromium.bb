## Building the annotation checker.
We do not want every developer to have to build clang, and so we store pre-built
binaries in a Google Cloud Storage bucket and retrieve them via gclient hooks.

To roll new versions of the binaries, assuming you have write access
to the chromium-tools-traffic_annotation bucket, run:

# On Linux:
```bash
git new-branch roll_traffic_annotation_tools
python tools/clang/scripts/update.py --bootstrap --force-local-build \
    --without-android --extra-tools traffic_annotation_extractor
cp third_party/llvm-build/Release+Asserts/bin/traffic_annotation_extractor \
    tools/traffic_annotation/bin/linux64/

# These GN flags produce an optimized, stripped binary that has no dependency
# on glib.
gn gen --args='is_official_build=true use_ozone=true' out/Default

ninja -C out/Default traffic_annotation_auditor
cp -p out/Default/traffic_annotation_auditor \
    tools/traffic_annotation/bin/linux64

strip tools/traffic_annotation/bin/linux64/traffic_annotation_{auditor,extractor}

third_party/depot_tools/upload_to_google_storage.py \
    -b chromium-tools-traffic_annotation \
    tools/traffic_annotation/bin/linux64/traffic_annotation_{auditor,extractor}
sed -i '/^CLANG_REVISION =/d' tools/traffic_annotation/bin/README.md
sed -i '/^LASTCHANGE=/d' tools/traffic_annotation/bin/README.md
grep '^CLANG_REVISION =' tools/clang/scripts/update.py >> tools/traffic_annotation/bin/README.md
cat build/util/LASTCHANGE >> tools/traffic_annotation/bin/README.md
git commit -a -m 'Roll traffic_annotation checkers'
git cl upload

```

# On Windows:
```bash
git new-branch roll_traffic_annotation_tools
python tools/clang/scripts/update.py --bootstrap --force-local-build ^
    --without-android --extra-tools traffic_annotation_extractor
cp third_party/llvm-build/Release+Asserts/bin/traffic_annotation_extractor.exe ^
    tools/traffic_annotation/bin/win32/

# These GN flags produce an optimized, stripped binary that has no dependency
# on glib.
gn gen --args="is_official_build=true" out/Default

ninja -C out/Default traffic_annotation_auditor
cp -p out/Default/traffic_annotation_auditor.exe ^
    tools/traffic_annotation/bin/win32

python third_party/depot_tools/upload_to_google_storage.py ^
    -b chromium-tools-traffic_annotation ^
    tools/traffic_annotation/bin/win32/traffic_annotation_auditor.exe
python third_party/depot_tools/upload_to_google_storage.py ^
    -b chromium-tools-traffic_annotation ^
    tools/traffic_annotation/bin/win32/traffic_annotation_extractor.exe
sed -i "/^CLANG_REVISION =/d" tools/traffic_annotation/bin/README.md
sed -i "/^LASTCHANGE=/d" tools/traffic_annotation/bin/README.md
grep "^CLANG_REVISION =" tools/clang/scripts/update.py >> ^
    tools/traffic_annotation/bin/README.md
cat build/util/LASTCHANGE >> tools/traffic_annotation/bin/README.md
dos2unix tools/traffic_annotation/bin/README.md
git commit -a -m 'Roll traffic_annotation checkers'
git cl upload

```

and land the resulting CL.

The following two lines will be updated by the above script, and the modified
README should be committed along with the updated .sha1 checksums.

CLANG_REVISION = '336424'
LASTCHANGE=410d3d75c58860d371551c79045f35b7b277a183-refs/heads/master@{#574126}
