# libFuzzer and ClusterFuzz Integration

*** note
Most links on this page are private.
***

ClusterFuzz is a distributed fuzzing infrastructure 
([go/clusterfuzz](https://goto.google.com/clusterfuzz)) that automatically
executes libFuzzer tests on scale.

## Status Links

* [Buildbot] - status of all libFuzzer builds.
* [ClusterFuzz Fuzzer Status] - fuzzing metrics, links to crashes and coverage 
reports.
* [Corpus GCS Bucket] - current corpus for each fuzzer. Can be used to upload
bootstrapped corpus.

## Integration Details

The integration between libFuzzer and ClusterFuzz consists of:

* Build rules definition in [fuzzer_test.gni].
* [Buildbot] that automatically discovers fuzzers using `gn refs` facility, 
builds fuzzers with multiple sanitizers and uploads binaries to a special
GCS bucket. Build bot recipe is defined in [chromium_libfuzzer.py].
* ClusterFuzz downloads new binaries once a day and runs fuzzers continuously.
* Fuzzing corpus is maintained for each fuzzer in [Corpus GCS Bucket]. Once a day
corpus is minimized to reduce number of duplicates and/or reduce effect of 
parasitic coverage. 
* [ClusterFuzz Fuzzer Status] displays fuzzer runtime 
metrics as well as provides links to crashes and coverage reports. The information
is collected every 30 minutes.


[Buildbot]: https://goto.google.com/libfuzzer-clusterfuzz-buildbot
[fuzzer_test.gni]: https://code.google.com/p/chromium/codesearch#chromium/src/testing/libfuzzer/fuzzer_test.gni
[chromium_libfuzzer.py]: https://code.google.com/p/chromium/codesearch#chromium/build/scripts/slave/recipes/chromium_libfuzzer.py
[ClusterFuzz Fuzzer Status]: https://goto.google.com/libfuzzer-clusterfuzz-status
[Corpus GCS Bucket]: https://goto.google.com/libfuzzer-clusterfuzz-corpus
