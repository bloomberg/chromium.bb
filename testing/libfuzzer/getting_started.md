# Getting Started with Libfuzzer in Chrome

*** note
**Prerequisites:** libfuzzer in chrome is supported with GN on Linux only. 
***

This document will walk you through:

* setting up your build enviroment.
* creating your first fuzzer.
* running the fuzzer and verifying its vitals.

## Check Out ToT Clang

Libfuzzer relies heavily on compile-time instrumentation. Because it is still
under heavy development you need to use tot clang with libfuzzer:

```bash
# In chrome/src
LLVM_FORCE_HEAD_REVISION=1 ./tools/clang/scripts/update.py --force-local-build --without-android
```

To revert this run the same script without specifying `LLVM_FORCE_HEAD_REVISION`.

## Configure Build

Use `use_libfuzzer` GN argument together with sanitizer to generate build files:

```bash
# With address sanitizer
gn gen out/libfuzzer '--args=use_libfuzzer=true is_asan=true enable_nacl=false' --check
```

Supported sanitizer configurations are:

| GN Argument | Description |
|--------------|----|
| is_asan=true | enables [Address Sanitizer] to catch problems like buffer overruns. |
| is_msan=true | enables [Memory Sanitizer] to catch problems like uninitialed reads. |


## Write Fuzzer Function

Create a new .cc file and define a `LLVMFuzzerTestOneInput` function:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  // put your fuzzing code here and use data+size as input.
  return 0;
}
```

[url_parse_fuzzer.cc] is a simple example of real-world fuzzer.

## Define GN Target

Define `fuzzer_test` GN target:

```
import("//testing/libfuzzer/fuzzer_test.gni")
fuzzer_test("my_fuzzer") {
  sources = [ "my_fuzzer.cc" ]
  deps = [ ... ]
}
```

## Build and Run Fuzzer Locally

Build with ninja as usual and run:

```bash
ninja -C out/libfuzzer url_parse_fuzzer
./out/libfuzzer url_parse_fuzzer
```

Your fuzzer should produce output like this:

```
INFO: Seed: 1787335005
INFO: -max_len is not provided, using 64
INFO: PreferSmall: 1
#0      READ   units: 1 exec/s: 0
#1      INITED cov: 2361 bits: 95 indir: 29 units: 1 exec/s: 0
#2      NEW    cov: 2710 bits: 359 indir: 36 units: 2 exec/s: 0 L: 64 MS: 0 
#3      NEW    cov: 2715 bits: 371 indir: 37 units: 3 exec/s: 0 L: 64 MS: 1 ShuffleBytes-
#5      NEW    cov: 2728 bits: 375 indir: 38 units: 4 exec/s: 0 L: 63 MS: 3 ShuffleBytes-ShuffleBytes-EraseByte-
#6      NEW    cov: 2729 bits: 384 indir: 38 units: 5 exec/s: 0 L: 10 MS: 4 ShuffleBytes-ShuffleBytes-EraseByte-CrossOver-
#7      NEW    cov: 2733 bits: 424 indir: 39 units: 6 exec/s: 0 L: 63 MS: 1 ShuffleBytes-
#8      NEW    cov: 2733 bits: 426 indir: 39 units: 7 exec/s: 0 L: 63 MS: 2 ShuffleBytes-ChangeByte-
#11     NEW    cov: 2733 bits: 447 indir: 39 units: 8 exec/s: 0 L: 33 MS: 5 ShuffleBytes-ChangeByte-ChangeASCIIInt-ChangeBit-CrossOver-
#12     NEW    cov: 2733 bits: 451 indir: 39 units: 9 exec/s: 0 L: 62 MS: 1 CrossOver-
#16     NEW    cov: 2733 bits: 454 indir: 39 units: 10 exec/s: 0 L: 61 MS: 5 CrossOver-ChangeBit-ChangeBit-EraseByte-ChangeBit-
#18     NEW    cov: 2733 bits: 458 indir: 39 units: 11 exec/s: 0 L: 24 MS: 2 CrossOver-CrossOver-
```

The `... NEW ...` line appears when libfuzzer finds new and interesting input. The 
efficient fuzzer should be able to finds lots of them rather quickly.

The `... pulse ...` line will appear periodically to show the current status.


## Submitting Fuzzer to ClusterFuzz

ClusterFuzz builds and executes all `fuzzer_test` targets in the source tree.
The only thing you should do is to submit a fuzzer into Chrome.

## Next Steps

* After your fuzzer is submitted, you should check its [ClusterFuzz status] in
a day or two.
* Check the [Efficient Fuzzer Guide] to better understand your fuzzer
performance and for optimization hints.


[Address Sanitizer]: http://clang.llvm.org/docs/AddressSanitizer.html
[Memory Sanitizer]: http://clang.llvm.org/docs/MemorySanitizer.html
[url_parser_fuzzer.cc]: https://code.google.com/p/chromium/codesearch#chromium/src/testing/libfuzzer/fuzzers/url_parse_fuzzer.cc
[ClusterFuzz status]: clusterfuzz.md#Status-Links
[Efficient Fuzzer Guide]: efficient_fuzzer.md
