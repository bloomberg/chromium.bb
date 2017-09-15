# Getting Started with libprotobuf-mutator in Chrome

*** note
**Note:** libprotobuf-mutator is new to Chromium and does not (yet) have a
long track record of success. Also, writing fuzzers with libprotobuf-mutator
will probably require more effort than writing fuzzers with libFuzzer alone.
If you run into problems, send an email to [fuzzing@chromium.org] for help.

**Prerequisites:** Knowledge of [libFuzzer in Chromium] and basic understanding of
[Protocol Buffers].
***

This document will walk you through:

* An overview of libprotobuf-mutator and how it's used.
* Writing and building your first fuzzer using libprotobuf-mutator.

## Overview of libprotobuf-mutator
libprotobuf-mutator is a package that allows libFuzzer’s mutation engine to
manipulate protobufs. This allows libFuzzer's mutations to be more specific
to the format it is fuzzing and less arbitrary. Below are some good use cases
for libprotobuf-mutator:

* Fuzzing targets that accept Protocol Buffers as input. Note that if you are
fuzzing a target that accepts protobuffers, your protobuf definition *cannot*
be optimized for `LITE_RUNTIME`, as is the case with almost all protobuf
definitions in Chromium. To get around this you can copy the file without the
optimization.
* Fuzzing targets that accept other highly structured input. To do this you
must write code that converts data from a protobuf-based format to a format the
target accepts. url_parse_proto_fuzzer is a working example of this and is
commented extensively. Readers may wish to consult its code, which is located in
`testing/libfuzzer/fuzzers/url_parse_proto_fuzzer.cc`, and
`testing/libfuzzer/fuzzers/url.proto`. Its build configuration can be found
in `testing/libfuzzer/fuzzers/BUILD.gn`.
* Fuzzing targets that accept more than one argument (such as data and flags).
In this case, you can define each argument as its own field in your protobuf
definition.

In the next two sections, we will discuss how to write and build fuzzers using
libprotobuf-mutator. Interested readers may also want to look at [this] example
of a libprotobuf-mutator fuzzer that is even more trivial than
url_parse_proto_fuzzer.

## Write a libprotobuf-mutator fuzzer

Once you have in mind the code you want to fuzz and the format it accepts, you
are ready to start writing a libprotobuf-mutator fuzzer. Writing the fuzzer
will have three steps:

* Define the fuzzed format (not required for protobuf formats, unless the
original definition is optimized for `LITE_RUNTIME`).
* Write the fuzzer target and conversion code (for non-protobuf formats).
* Define the GN target

### Define the Fuzzed Format
Create a new .proto using `proto2` or `proto3` syntax and define a message that
you want libFuzzer to mutate.

``` cpp
syntax = "proto2";

package my_fuzzer;

message MyFormat {
    // Define a format for libFuzzer to mutate here.
}
```

See `testing/libfuzzer/fuzzers/url.proto` for an example of this in practice.
That example has extensive comments on URL syntax and how that influenced
the definition of the Url message.

### Write the Fuzzer Target and Conversion Code
Create a new .cc and write a `DEFINE_BINARY_PROTO_FUZZER` function:

```cpp
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
// Assuming the .proto file is path/to/your/proto_file/my_format.proto.
#include "path/to/your/proto_file/my_format.pb.h"

// Silence logging from the protobuf library.
protobuf_mutator::protobuf::LogSilencer log_silencer;

DEFINE_BINARY_PROTO_FUZZER(const my_fuzzer::MyFormat& my_format) {
  // Put your conversion code here (if needed) and then pass the result to
  // your fuzzing code (or just pass "my_format", if your target accepts
  // protobufs).
}
```

This is very similar to the same step in writing a standard libFuzzer fuzzer.
The only real differences are accepting protobufs rather than raw data and
converting them to the desired format. Conversion code can't really be explored
in this guide since it is format-specific. However, a good example of conversion
code (and a fuzzer target) can be found in
`testing/libfuzzer/fuzzers/url_parse_proto_fuzzer.cc`. That example thoroughly
documents how it converts the Url protobuf message into a real URL string.
Note that `DEFINE_TEXT_PROTO_FUZZER` may be used instead of
`DEFINE_BINARY_PROTO_FUZZER`. The former may make it easier to debug how
libFuzzer is mutating inputs, since those inputs will be in text format before
it is passed to our code, rather than binary.

### Define the GN Target
Define a fuzzer_test target and include your protobuf definition and
libprotobuf-mutator as dependencies.

```python
import("//testing/libfuzzer/fuzzer_test.gni")
import("//third_party/protobuf/proto_library.gni")

fuzzer_test("my_fuzzer") {
  sources = [ "my_fuzzer.cc" ]
  deps = [
    :my_format_proto
    "//third_party/libprotobuf-mutator"
    ...
  ]
}

proto_library("my_format_proto") {
  sources = [ "my_format.proto" ]
}
```

See `testing/libfuzzer/fuzzers/BUILD.gn` for an example of this in practice.

### Wrapping Up
Once you have written a fuzzer with libprotobuf-mutator, building and running
it is pretty much the same as if the fuzzer were a standard libFuzzer-based
fuzzer (with minor exceptions, like your seed corpus must be in protobuf
format).

[libFuzzer in Chromium]: getting_started.md
[Protocol Buffers]: https://developers.google.com/protocol-buffers/docs/cpptutorial
[fuzzing@chromium.org]: mailto:fuzzing@chromium.org
[this]: https://github.com/google/libprotobuf-mutator/tree/master/examples/libfuzzer/libfuzzer_example.cc
