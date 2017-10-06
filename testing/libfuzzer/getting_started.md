# Getting Started with libFuzzer in Chrome

*** note
**Prerequisites:** libFuzzer in Chrome is supported with GN on Linux only. 
***

This document will walk you through:

* setting up your build enviroment.
* creating your first fuzzer.
* running the fuzzer and verifying its vitals.

## Configure Build

Use `use_libfuzzer` GN argument together with sanitizer to generate build files:

*Notice*: current implementation also supports `use_afl` argument, but it is
recommended to use libFuzzer for development. Running libFuzzer locally doesn't
require any special configuration and quickly gives meaningful output for speed,
coverage and other parameters.

```bash
# With address sanitizer
gn gen out/libfuzzer '--args=use_libfuzzer=true is_asan=true is_debug=false enable_nacl=false' --check
```

Supported sanitizer configurations are:

| GN Argument | Description |
|--------------|----|
| `is_asan=true` | enables [Address Sanitizer] to catch problems like buffer overruns. |
| `is_msan=true` | enables [Memory Sanitizer] to catch problems like uninitialed reads<sup>\[[*](reference.md#MSan)\]</sup>. |
| `is_ubsan_security=true` | enables [Undefined Behavior Sanitizer] to catch<sup>\[[*](reference.md#UBSan)\]</sup> undefined behavior like integer overflow. |
| | it is possible to run libfuzzer without any sanitizers; *probably not what you want*.|

To get the exact GN configuration that are used on our builders, see
[Build Config].

## Write Fuzzer Function

Create a new .cc file and define a `LLVMFuzzerTestOneInput` function:

```cpp
#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // put your fuzzing code here and use data+size as input.
  return 0;
}
```

[url_parse_fuzzer.cc] is a simple example of real-world fuzzer.

## Define GN Target

Define `fuzzer_test` GN target:

```python
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
./out/libfuzzer/url_parse_fuzzer
```

Your fuzzer should produce output like this:

```
INFO: Seed: 1787335005
INFO: -max_len is not provided, using 64
INFO: PreferSmall: 1
#0      READ   units: 1 exec/s: 0
#1      INITED cov: 2361 bits: 95 indir: 29 units: 1 exec/s: 0
#2      NEW    cov: 2710 bits: 359 indir: 36 units: 2 exec/s: 0 L: 64 MS: 0
```

The `... NEW ...` line appears when libFuzzer finds new and interesting input.
The efficient fuzzer should be able to finds lots of them rather quickly.
The `... pulse ...` line will appear periodically to show the current status.

### Symbolize Stacktrace

If your fuzzer crashes when running locally and you see non-symbolized
stacktrace, make sure that you have directory containing `llvm-symbolizer`
binary added in `$PATH`. The symbolizer binary is included in Chromium's Clang
package located at `third_party/llvm-build/Release+Asserts/bin/` directory.

Alternatively, you can set `external_symbolizer_path` option via `ASAN_OPTIONS`
env variable:

```bash
$ ASAN_OPTIONS=external_symbolizer_path=/my/local/llvm/build/llvm-symbolizer \
    ./fuzzer ./crash-input
```

The same approach works with other sanitizers (e.g. `MSAN_OPTIONS` and others).

## Improving Your Fuzzer

Your fuzzer may immediately discover interesting (i.e. crashing) inputs.
To make it more efficient, several small steps can take you really far:

* Create seed corpus. Add `seed_corpus = "src/fuzz-testcases/"` attribute
to your fuzzer targets and add example files in appropriate folder. Read more
in [Seed Corpus] section of efficient fuzzer guide.
*Make sure corpus files are appropriately licensed.*
* Create mutation dictionary. With a `dict = "protocol.dict"` attribute and
`key=value` dicitionary file format, mutations can be more effective.
See [Fuzzer Dictionary].
* Specify maximum testcase length. By default libFuzzer uses `-max_len=64`
 (or takes the longest testcase in a corpus). ClusterFuzz takes
random value in range from `1` to `10000` for each fuzzing session and passes
that value to libFuzzers. If corpus contains testcases of size greater than
`max_len`, libFuzzer will use only first `max_len` bytes of such testcases. 
See [Maximum Testcase Length].

## Disable noisy error message logging

If the code that you are a fuzzing generates error messages when encountering
incorrect or invalid data then you need to silence those errors in the fuzzer.

If the target uses the Chromium logging APIs, the best way to do that is to
override the environment used for logging in your fuzzer:

```cpp
struct Environment {
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

Environment* env = new Environment();
```

## Submitting Fuzzer to ClusterFuzz

ClusterFuzz builds and executes all `fuzzer_test` targets in the source tree.
The only thing you should do is to submit a fuzzer into Chrome.

## Next Steps

* After your fuzzer is submitted, you should check its [ClusterFuzz status] in
a day or two.
* Check the [Efficient Fuzzer Guide] to better understand your fuzzer
performance and for optimization hints.


[Address Sanitizer]: http://clang.llvm.org/docs/AddressSanitizer.html
[ClusterFuzz status]: clusterfuzz.md#Status-Links
[Efficient Fuzzer Guide]: efficient_fuzzer.md
[Fuzzer Dictionary]: efficient_fuzzer.md#Fuzzer-Dictionary
[Maximum Testcase Length]: efficient_fuzzer.md#Maximum-Testcase-Length
[Memory Sanitizer]: http://clang.llvm.org/docs/MemorySanitizer.html
[Seed Corpus]: efficient_fuzzer.md#Seed-Corpus
[Undefined Behavior Sanitizer]: http://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
[crbug/598448]: https://bugs.chromium.org/p/chromium/issues/detail?id=598448
[url_parse_fuzzer.cc]: https://code.google.com/p/chromium/codesearch#chromium/src/testing/libfuzzer/fuzzers/url_parse_fuzzer.cc
[Build Config]: reference.md#Builder-configurations
