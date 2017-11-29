# Efficient Fuzzer

This document describes ways to determine your fuzzer efficiency and ways
to improve it.

## Overview

Being a coverage-driven fuzzer, libFuzzer considers a certain input *interesting*
if it results in new code coverage. The set of all interesting inputs is called
*corpus*.

Items in corpus are constantly mutated in search of new interesting inputs.
Corpus can be shared across fuzzer runs and grows over time as new code is
reached.

The following things are extremely effective for improving fuzzer efficiency, so we
*strongly recommend* them for any fuzzer:

* [Seed Corpus](#Seed-Corpus)
* [Fuzzer Dictionary](#Fuzzer-Dictionary)

There are several metrics you should look at to determine your fuzzer effectiveness:

* [Fuzzer Speed](#Fuzzer-Speed)
* [Corpus Size](#Corpus-Size)
* [Code Coverage](#Code-Coverage)

You can collect these metrics manually or take them from [ClusterFuzz status]
pages after a fuzzer is checked in Chromium repository.

## Seed Corpus

Seed corpus is a set of *valid* and *interesting* inputs that serve as starting points
for a fuzzer. If one is not provided, a fuzzer would have to guess these inputs
from scratch, which can take an indefinite amount of time depending of the size
of inputs.

Seed corpus works especially well for strictly defined file formats and data
transmission protocols.

* For file format parsers, add valid files from your test suite.
* For protocol parsers, add valid raw streams from test suite into separate files.

Other examples include a graphics library seed corpus, which would be a variety of
small PNG/JPG/GIF files.

If you are running the fuzzer locally, you can pass a corpus directory as an argument:

```
./out/libfuzzer/my_fuzzer ~/tmp/my_fuzzer_corpus
```

While libFuzzer can start with an empty corpus, most fuzzers require a seed corpus
to be useful. The fuzzer would store all the interesting items it finds in that directory.

ClusterFuzz uses seed corpus defined in Chromium source repository. You need to
add a `seed_corpus` attribute to your fuzzer definition in BUILD.gn file:

```
fuzzer_test("my_protocol_fuzzer") {
  ...
  seed_corpus = "test/fuzz/testcases"
  ...
}
```

You may specify multiple seed corpus directories via `seed_corpuses` attribute:

```
fuzzer_test("my_protocol_fuzzer") {
  ...
  seed_corpuses = [ "test/fuzz/testcases", "test/unittest/data" ]
  ...
}
```

All files found in these directories and their subdirectories will be archived
into a `<my_fuzzer_name>_seed_corpus.zip` output archive.

If you can't store seed corpus in Chromium repository (e.g. it is too large, has
licensing issues, etc), you can upload the corpus to Google Cloud Storage bucket
used by ClusterFuzz:

1) Go to [Corpus GCS Bucket].
2) Open directory named `<my_fuzzer_name>_static`. If the directory does not
exist, please create it.
3) Upload corpus files into the directory.

Alternative and faster way is to use [gsutil] command line tool:
```bash
gsutil -m rsync <path_to_corpus> gs://clusterfuzz-corpus/libfuzzer/<my_fuzzer_name>_static
```

## Fuzzer Dictionary

It is very useful to provide fuzzer a set of *common words or values* that you
expect to find in the input. Adding a dictionary highly improves the efficiency of
finding new units and works especially well in certain usecases (e.g. fuzzing file
format decoders).

To add a dictionary, first create a dictionary file. Dictionary file is a flat text file
where tokens are listed one per line in the format of name="value". The
alphanumeric name is ignored and can be omitted, although it is a convenient
way to document the meaning of a particular token. The value must appear in
quotes, with hex escaping (\xNN) applied to all non-printable, high-bit, or
otherwise problematic characters (\\ and \" shorthands are recognized too).
This syntax is similar to the one used by [AFL] fuzzing engine (-x option).

An examples dictionary looks like:

```
# Lines starting with '#' and empty lines are ignored.

# Adds "blah" word (w/o quotes) to the dictionary.
kw1="blah"
# Use \\ for backslash and \" for quotes.
kw2="\"ac\\dc\""
# Use \xAB for hex values.
kw3="\xF7\xF8"
# Key name before '=' can be omitted:
"foo\x0Abar"
```

Make sure to test your dictionary by running your fuzzer locally:

```bash
./out/libfuzzer/my_protocol_fuzzer -dict=<path_to_dict> <path_to_corpus>
```

If the dictionary is effective, you should see new units discovered in fuzzer output.

To submit a dictionary to Chromium repository:

1) Add the dictionary file in the same directory as your fuzz target, with name
`<my_fuzzer>.dict`.
2) Add `dict` attribute to fuzzer definition in BUILD.gn file:

```
fuzzer_test("my_protocol_fuzzer") {
  ...
  dict = "my_protocol_fuzzer.dict"
}
```

The dictionary will be used automatically by ClusterFuzz once it picks up a new
revision build.

### Corpus Minimization

It's important to minimize seed corpus to a *small set of interesting inputs* before
uploading. The reason being that seed corpus is synced to all fuzzing bots for every
iteration, so it is important to keep it small both for fuzzing efficiency and to prevent
our bots from running out of disk space (should not exceed 1 Gb).

The minimization can be done using `-merge=1` option of libFuzzer:

```bash
# Create an empty directory.
mkdir seed_corpus_minimized

# Run the fuzzer with -merge=1 flag.
./my_fuzzer -merge=1 ./seed_corpus_minimized ./seed_corpus
```

After running the command above, `seed_corpus_minimized` directory will contain
a minimized corpus that gives the same code coverage as the initial
`seed_corpus` directory.

## Fuzzer Speed

Fuzzer speed is calculated in executions per second. It is printed while the fuzzer
is running:

```
#19346  NEW    cov: 2815 bits: 1082 indir: 43 units: 150 exec/s: 19346 L: 62
```

Because libFuzzer performs randomized mutations, it is critical to have it run as
fast as possible to navigate the large search space efficiently and find interesting
code paths. You should try to get to at least 1,000 exec/s from your fuzzer runs
locally before submitting the fuzzer to Chromium repository. Profile the fuzzer
using any standard tool to see where it spends its time.


### Initialization/Cleanup

Try to keep `LLVMFuzzerTestOneInput` function as simple as possible. If your fuzzing
function is too complex, it can bring down fuzzer execution speed OR it might target
very specific usecases and fail to account for unexpected scenarios.

Prefer to use static initialization and shared resources rather than bringing the
environment up and down on every single run. Otherwise, it will slow down
fuzzer speed on every run and its ability to find new interesting inputs.
Checkout example on [startup initialization] in libFuzzer documentation.

Fuzzers don't have to shutdown gracefully. We either kill them or they crash
because memory sanitizer tool found a problem. You can skip freeing static
resources.

All resources allocated within `LLVMFuzzerTestOneInput` function should be
de-allocated since this function is called millions of times during a fuzzing session.
Otherwise, we will hit OOMs frequently and reduce overall fuzzing efficiency.


### Memory Usage

Avoid allocation of dynamic memory wherever possible. Memory instrumentation
works faster for stack-based and static objects, than for heap allocated ones.

It is always a good idea to try different variants for your fuzzer locally, and then
submit the fastest implementation.


### Maximum Testcase Length

You can control the maximum length of a test input using `-max_len` parameter
(see [custom options](#Custom-Options)). This parameter can often significantly
improve execution speed. Beware that you might miss coverage and unexpected
scenarios happening from longer size inputs.

1) Define which `-max_len` value is reasonable for your target. For example, it
may be useless to fuzz an image decoder with too small value of testcase length.

2) Increase the value defined on previous step. Check its influence on execution
speed of fuzzer. If speed doesn't drop significantly for long inputs, it is fine
to have some bigger value for `-max_len` or even skip it completely.

In general, bigger `-max_len` value gives better coverage which is the main
priority for fuzzing. However, low execution speed may result in waste of
fuzzing resources and being unable to find interesting inputs in reasonable time.
If large inputs make the fuzzer too slow, you should adjust the value of `-max_len`
and find a trade-off between coverage and execution speed.

*Note:* ClusterFuzz runs two different fuzzing engines (**LibFuzzer** and
**AFL**) using the same target functions. AFL doesn't support `-max_len`
parameter and may provide input of any length to the target. If your target has
an input length limit that you would like to *strictly enforce*, it's
recommended to add a sanity check to the beginning of your target function:

```
if (size > kSizeLimit)
  return 0;
```

For more information check out the discussion in [issue 638836].


## Code Coverage

[ClusterFuzz status] page provides fuzzer source-level coverage report from
recent runs. Looking at the report might provide an insight to improve fuzzer
coverage.

You can also generate source-level coverage report locally by running the
[coverage script] stored in Chromium repository. The script provides detailed
instructions as well as an usage example.

We encourage you to try out the script, as it usually generates a better code
coverage visualization compared to the coverage report hosted on ClusterFuzz.
*NOTE: This is an experimental feature and an active area of work. We are
working on improving this process.*


## Corpus Size

After running for a while, the fuzzer would reach a plateau and won't discover
new interesting inputs. Corpus for a reasonably complex functionality should
contain hundreds (if not thousands) of items.

Too small of a corpus size indicates fuzzer is hitting a code barrier and is unable
to get past it. Common cases of such issues include: checksums, magic numbers,
etc. The easiest way to diagnose this problem is to generate and analyze a
[coverage report](#Coverage). To fix the issue, you can:

* Change the code (e.g. disable crc checks while fuzzing).
* Prepare or improve [seed corpus](#Seed-Corpus).
* Prepare or improve [fuzzer dictionary](#Fuzzer-Dictionary).
* Add [custom options](#Custom-Options).

### Custom Options

Custom options help to fine tune libFuzzer execution parameters and will also
override the default values used by ClusterFuzz. Please read [libFuzzer options]
page for detailed documentation on how these work.

Add the options needed in `libfuzzer_options` attribute to your fuzzer definition in
BUILD.gn file:

```
fuzzer_test("my_protocol_fuzzer") {
  ...
  libfuzzer_options = [
    "max_len=2048",
    "use_traces=1",
  ]
}
```

Please note that `dict` parameter should be provided [separately](#Fuzzer-Dictionary).
All other options can be passed using `libfuzzer_options` property.

[AFL]: http://lcamtuf.coredump.cx/afl/
[ClusterFuzz status]: clusterfuzz.md#Status-Links
[Corpus GCS Bucket]: https://console.cloud.google.com/storage/clusterfuzz-corpus/libfuzzer
[issue 638836]: https://bugs.chromium.org/p/chromium/issues/detail?id=638836
[coverage script]: https://cs.chromium.org/chromium/src/testing/libfuzzer/coverage.py
[gsutil]: https://cloud.google.com/storage/docs/gsutil
[libFuzzer options]: https://llvm.org/docs/LibFuzzer.html#options
[startup initialization]: https://llvm.org/docs/LibFuzzer.html#startup-initialization
