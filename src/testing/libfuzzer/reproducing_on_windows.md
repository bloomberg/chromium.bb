# Reproducing libFuzzer Crashes on Windows

Since the [ClusterFuzz Reproduce Tool] does not work on Windows, you will need
to reproduce crashes found by ClusterFuzz manually. Luckily, this process is
usually very simple. Below are the steps:

*** note
**Requirements:** These commands are intended to work in cmd.exe, not
powershell.
***

1. Download the test case from ClusterFuzz (if you are CCed on an issue filed by
   ClusterFuzz, a link to it is next to "Reproducer testcase" in the
   bug description). For the rest of this walkthrough, we call the path of this
   file: `$TESTCASE_PATH`. We will call the fuzz target you want to reproduce a
   crash on: `$FUZZER_NAME`.

2. Generate gn build configuration for libFuzzer:

```
python tools\mb\mb.py gen -m chromium.fuzz -b "Libfuzzer Upload Windows ASan" out\libFuzzer
```

3. Build the fuzzer:

```
autoninja -C .\out\libFuzzer\ $FUZZER_NAME
```

4. Set the `ASAN_OPTIONS` environment variable to be the same as ClusterFuzz.
Note that this may not be necessary.
Here is an example value of `ASAN_OPTIONS` that is similar to its value on
ClusterFuzz:

```
set ASAN_OPTIONS=redzone=256:print_summary=1:handle_sigill=1:allocator_release_to_os_interval_ms=500:print_suppressions=0:strict_memcmp=1:allow_user_segv_handler=0:use_sigaltstack=1:handle_sigfpe=1:handle_sigbus=1:detect_stack_use_after_return=0:alloc_dealloc_mismatch=0:detect_leaks=0:print_scariness=1:allocator_may_return_null=1:handle_abort=1:check_malloc_usable_size=0:detect_container_overflow=0:quarantine_size_mb=256:detect_odr_violation=0:symbolize=1:handle_segv=1:fast_unwind_on_fatal=1
```

5. Run the fuzzer:

```
.\out\libFuzzer\$FUZZER_NAME -runs=100 $TESTCASE_PATH
```

Send an email to fuzzing@chromium.org if you run into any issues.

[ClusterFuzz Reproduce Tool]: https://github.com/google/clusterfuzz-tools
