# Reproducing libFuzzer and AFL crashes

libFuzzer and AFL crashes can be reproduced easily with the [ClusterFuzz Reproduce Tool]. However,
if you are unable to use the tool (e.g. unsupported platform, some other tool issue), you can still
reproduce the crash manually using the steps below:

*** note
**Requirements:** For Windows, you must convert the forward slashes (/) to backslashes (\\) in
the commands below and use `set` command instead of `export` to set the environment variable (step 4).
Note that these commands are intended to be used with cmd.exe, not PowerShell.
***

1. Download the testcase from ClusterFuzz. If you are CCed on an issue filed by
   ClusterFuzz, a link to it is next to "Reproducer testcase" in the bug description.

   For the rest of this walkthrough, we call the path of this
   file: `$TESTCASE_PATH` and the fuzz target you want to reproduce a
   crash on: `$FUZZER_NAME` (provided as "Fuzz target binary" in the bug description).

2. Generate gn build configuration:

```
gn args out/fuzz
```

This will open up an editor. Copy the gn configuration paramaters from the values
provided in `GN Config` section in the ClusterFuzz testcase report.


3. Build the fuzzer:

```
autoninja -C out/fuzz $FUZZER_NAME
```

4. Set the `*SAN_OPTIONS` environment variable as provided in the `Crash Stacktrace` section in the
testcase report.
Here is an example value of `ASAN_OPTIONS` that is similar to its value on
ClusterFuzz:

```
export ASAN_OPTIONS=redzone=256:print_summary=1:handle_sigill=1:allocator_release_to_os_interval_ms=500:print_suppressions=0:strict_memcmp=1:allow_user_segv_handler=0:use_sigaltstack=1:handle_sigfpe=1:handle_sigbus=1:detect_stack_use_after_return=0:alloc_dealloc_mismatch=0:detect_leaks=0:print_scariness=1:allocator_may_return_null=1:handle_abort=1:check_malloc_usable_size=0:detect_container_overflow=0:quarantine_size_mb=256:detect_odr_violation=0:symbolize=1:handle_segv=1:fast_unwind_on_fatal=1
```

5. Run the fuzz target:

```
out/fuzz/$FUZZER_NAME -runs=100 $TESTCASE_PATH
```

If you see an un-symbolized stacktrace, please see the instructions [here].

[File a bug] if you run into any issues.

[ClusterFuzz Reproduce Tool]: https://github.com/google/clusterfuzz-tools
[File a bug]: https://bugs.chromium.org/p/chromium/issues/entry?component=Tools%3EStability%3ElibFuzzer&comment=What%20problem%20are%20you%20seeing
[here]: getting_started.md#symbolize-stacktrace
