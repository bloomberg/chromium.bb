# Efficient Fuzzer

This document describes ways to determine your fuzzer efficiency and ways 
to improve it.

## Overview

Being a coverage-driven fuzzer, Libfuzzer considers a certain input *interesting*
if it results in new coverage. The set of all interesting inputs is called 
*corpus*. 
Items in corpus are constantly mutated in search of new interesting input.
Corpus is usually maintained between multiple fuzzer runs.

There are several metrics you should look at to determine your fuzzer effectiveness:

* fuzzer speed (exec/s)
* corpus size
* coverage

You can collect these metrics manually or take them from [ClusterFuzz status]
pages.

## Fuzzer Speed

Fuzzer speed is printed while fuzzer runs:

```
#19346  NEW    cov: 2815 bits: 1082 indir: 43 units: 150 exec/s: 19346 L: 62
```

Because Libfuzzer performs randomized search, it is critical to have it as fast
as possible. You should try to get to at least 1,000 exec/s. Profile the fuzzer
using any standard tool to see where it spends its time.

### Initialization/Cleanup

Try to keep your fuzzing function as simple as possible. Prefer to use static
initialization and shared resources rather than bringing environment up and down
every single run.

Fuzzers don't have to shutdown gracefully (we either kill them or they crash
because sanitizer has found a problem). You can skip freeing static resource.

Of course all resources allocated withing `LLVMFuzzerTestOneInput` function
should be deallocated since this function is called millions of times during
one fuzzing session.


## Corpus Size

After running for a while the fuzzer would reach a plateau and won't discover
new interesting input. Corpus for a reasonably complex functionality
should contain hundreds (if not thousands) of items.

Too small corpus size indicates some code barrier that
libfuzzer is having problems penetrating. Common cases include: checksums,
magic numbers etc. The easiest way to diagnose this problem is to generate a 
[coverage report](#Coverage). To fix the issue you can:

* change the code (e.g. disable crc checks while fuzzing)
* prepare fuzzer dictionary
* prepare [corpus seed](#Corpus-Seed).

## Coverage

You can easily generate source-level coverage report for a given corpus:

```
ASAN_OPTIONS=coverage=1:html_cov_report=1:sancov_path=./third_party/llvm-build/Release+Asserts/bin/sancov \
  ./out/libfuzzer/my_fuzzer -runs=0 ~/tmp/my_fuzzer_corpus
```

This will produce an .html file with colored source-code. It can be used to
determine where your fuzzer is "stuck".

## Corpus Seed

You can pass a corpus directory to a fuzzer that you run manually:

```
./out/libfuzzer/my_fuzzer ~/tmp/my_fuzzer_corpus
```

The directory can initially be empty. The fuzzer would store all the interesting
items it finds in the directory. You can help the fuzzer by "seeding" the corpus:
simply copy interesting inputs for your function to the corpus directory before
running. This works especially well for file-parsing functionality: just
use some valid files from your test suite.

After discovering new and interesting items, [upload corpus to Clusterfuzz].


[ClusterFuzz status]: ./clusterfuzz.md#Fuzzer-Status
[upload corpus to Clusterfuzz]: ./clusterfuzz.md#Upload-Corpus
