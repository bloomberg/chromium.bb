This directory (`platform2/system_api`) contains constants and definitions
like D-Bus service names that are shared between Chromium and Chromium OS.

This directory is only for things like headers and .proto files.
No implementation should be added.

When writting a .proto file make sure to use:

```
option optimize_for = LITE_RUNTIME;
```

This will force usage of a lite protobuf instead of a full/heavy weight
protobuf. The browser only links against the light version, so you will get
cryptic link errors about missing parts of Message if you define a protobuf here
and then try to use it in Chrome. Currently CrOS links against the full
protobuffer library, but that might change in the future.

When declaring a protobuf, avoid use of required unless it is exactly what you
mean. "Required is Forever" and very rarely should actually be used. Consult
[Protocol Buffer Basics: C++](http://code.google.com/apis/protocolbuffers/docs/cpptutorial.html)
for a detailed of this issue.
