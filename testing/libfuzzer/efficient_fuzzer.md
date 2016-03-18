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
* prepare [fuzzer dictionary](#Fuzzer-Dictionary)
* prepare [corpus seed](#Corpus-Seed).

## Coverage

You can easily generate source-level coverage report for a given corpus:

```
ASAN_OPTIONS=coverage=1:html_cov_report=1:sancov_path=./third_party/llvm-build/Release+Asserts/bin/sancov \
  ./out/libfuzzer/my_fuzzer -runs=0 ~/tmp/my_fuzzer_corpus
```

This will produce an .html file with colored source-code. It can be used to
determine where your fuzzer is "stuck".

## Fuzzer Dictionary

It is very useful to provide fuzzer a set of common words/values that you expect
to find in the input. This greatly improves efficiency of finding new units and
works especially well while fuzzing file format decoders.

To add a dictionary, first create a dictionary file.
Dictionary syntax is similar to that used by [AFL] for its -x option:
```
# Lines starting with '#' and empty lines are ignored.

# Adds "blah" (w/o quotes) to the dictionary.
kw1="blah"
# Use \\ for backslash and \" for quotes.
kw2="\"ac\\dc\""
# Use \xAB for hex values
kw3="\xF7\xF8"
# the name of the keyword followed by '=' may be omitted:
"foo\x0Abar"
```

Test your dictionary by running your fuzzer locally:
```bash
./out/libfuzzer/my_protocol_fuzzer -dict <path_to_dict> <path_to_corpus>
```
You should see lots of new units discovered.

Add `dict` attribute to fuzzer target:

```
fuzzer_test("my_protocol_fuzzer") {
  ...
  dict = "protocol.dict"
}
```

Make sure to submit dictionary file to git. The dictionary will be used
automatically by ClusterFuzz once it picks up new fuzzer version (once a day).



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


[ClusterFuzz status]: ./clusterfuzz.md#Status-Links
[upload corpus to Clusterfuzz]: ./clusterfuzz.md#Upload-Corpus
[AFL]: http://lcamtuf.coredump.cx/afl/
