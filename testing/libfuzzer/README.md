# Libfuzzer in Chrome

[go/libfuzzer-chrome](https://goto.google.com/libfuzzer-chrome)

*** aside
[Getting Started](getting_started.md)
| [Buildbot](https://goto.google.com/libfuzzer-clusterfuzz-buildbot)
| [ClusterFuzz Status](https://goto.google.com/libfuzzer-clusterfuzz-status)
***

This directory contains integration between [LibFuzzer] and Chrome.
Libfuzzer is an in-process coverage-driven evolutionary fuzzer. It helps
engineers to uncover potential security & stability problems earlier.

*** note
**Requirements:** libfuzzer in chrome is supported with GN on Linux only. 
***

## Integration Status

Fuzzer tests are well-integrated with Chrome build system & distributed 
ClusterFuzz fuzzing system. Cover bug: [crbug.com/539572].

## Documentation

* [Getting Started Guide] walks you through all the steps necessary to create
your fuzzer and submit it to ClusterFuzz.
* [Efficient Fuzzer Guide] explains how to measure fuzzer effectiveness and
ways to improve it.
* [ClusterFuzz Integration] describes integration between ClusterFuzz and 
libfuzzer.
* [Reference] contains detailed references for different integration parts.


[LibFuzzer]: http://llvm.org/docs/LibFuzzer.html
[crbug.com/539572]: https://bugs.chromium.org/p/chromium/issues/detail?id=539572
[Getting Started Guide]: getting_started.md
[Efficient Fuzzer Guide]: efficient_fuzzer.md
[ClusterFuzz Integration]: clusterfuzz.md
[Reference]: reference.md
