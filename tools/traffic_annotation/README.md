# Running the traffic annotation checkers

The traffic annotation checkers ensure that every operation in the
code base that talks to the network is properly annotated in the
source code, so that we can produce reports of what Chromium talks to
over the network and why.

To run the checkers, you need a populated build directory, and then
you do:

```
$ python tools/annotation_checker/presubmit_checks.py --build-path out/Default
```

## Building the annotation checker.

The annotation checkers are built as Clang tools. We do not want every
developer to have to build clang, and so we store pre-built binaries
in a Google Cloud Storage bucket and retrieve them via gclient hooks.

To roll new versions of the binaries, assuming you have write access
to the chromium-tools-traffic_annotation bucket, run:

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
sed -i '/^CLANG_REVISION=/d' tools/traffic_annotation/README.md
sed -i '/^LASTCHANGE=/d' tools/traffic_annotation/README.md
grep '^CLANG_REVISION=' tools/clang/scripts/update.py >> tools/traffic_annotation/README.md
cat build/util/LASTCHANGE >> tools/traffic_annotation/README.md
git commit -a -m 'Roll traffic_annotation checkers'
git cl upload

```

and land the resulting CL.

The following two lines will be updated by the above script, and the modified
README should be committed along with the updated .sha1 checksums.

In the event that clang changes something that requires this tool to be
rebuilt (or for some other reason the tests don't work correctly), please
disable this test by setting the `test_is_enabled` flag to False in
//tools/traffic_annotation/scripts_check_annotation.py, and file a bug
and cc the people listed in OWNERS; they'll be on the hook to rebuild and
re-enable the test.

CLANG_REVISION = '308728'
LASTCHANGE=e684c0cebda9d00d4b5d655920c516f79d857103-refs/heads/master@{#507132}
