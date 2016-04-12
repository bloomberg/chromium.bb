# libFuzzer Integration Reference

## fuzzer_test GN Template

Use `fuzzer_test` to define libFuzzer targets:

```
fuzzer_test("my_fuzzer") {
  ...
}
```

Following arguments are supported:

| Argument | Description |
|----------|-------------|
| sources | **required** list of fuzzer test source files. |
| deps | fuzzer dependencies |
| additional_configs | additional GN configurations to be used for compilation |
| dict | a dictionary file for the fuzzer |
| libfuzzer_options | runtime options file for the fuzzer. See [Fuzzer Runtime Options](Fuzzer-Options) |


## Fuzzer Runtime Options

There are many different runtime options supported by libFuzzer. Options
are passed as command line arguments:

```
./fuzzer [-flag1=val1 [-flag2=val2 ...] ] [dir1 [dir2 ...] ]
```

Most common flags are:

| Flag | Description |
|------|-------------|
| max_len | Maximum length of test input. |
| timeout | Timeout of seconds. Units slower than this value will be reported as bugs. |

A fuller list of options can be found at [libFuzzer Usage] page and by running
the binary with `-help=1`.

To specify these options for ClusterFuzz, list all parameters in
`libfuzzer_options` target attribute:

```
fuzzer_test("my_fuzzer") {
  ...
  libfuzzer_options = [
    "max_len=2048",
    "use_traces=1",
  ]
}
```

[libFuzzer Usage]: http://llvm.org/docs/LibFuzzer.html#usage


