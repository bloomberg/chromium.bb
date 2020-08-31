# libgav1 -- an AV1 decoder

libgav1 is a Main profile (0) & High profile (1) compliant AV1 decoder. More
information on the AV1 video format can be found at
[aomedia.org](https://aomedia.org).

[TOC]

## Building

### Prerequisites

1.  A C++11 compiler. gcc 6+, clang 7+ or Microsoft Visual Studio 2017+ are
    recommended.

2.  [CMake >= 3.7.1](https://cmake.org/download/)

3.  [Abseil](https://abseil.io)

    From within the libgav1 directory:

    ```shell
      $ git clone https://github.com/abseil/abseil-cpp.git third_party/abseil-cpp
    ```

### Compile

```shell
  $ mkdir build && cd build
  $ cmake -G "Unix Makefiles" ..
  $ make
```

Configuration options:

*   `LIBGAV1_MAX_BITDEPTH`: defines the maximum supported bitdepth (8, 10;
    default: 10).
*   `LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS`: define to a non-zero value to disable
    [symbol reduction](#symbol-reduction) in an optimized build to keep all
    versions of dsp functions available. Automatically defined in
    `src/dsp/dsp.h` if unset.
*   `LIBGAV1_ENABLE_NEON`: define to a non-zero value to enable NEON
    optimizations. Automatically defined in `src/dsp/dsp.h` if unset.
*   `LIBGAV1_ENABLE_SSE4_1`: define to a non-zero value to enable sse4.1
    optimizations. Automatically defined in `src/dsp/dsp.h` if unset.
*   `LIBGAV1_ENABLE_LOGGING`: define to 0/1 to control debug logging.
    Automatically defined in `src/utils/logging.h` if unset.
*   `LIBGAV1_EXAMPLES_ENABLE_LOGGING`: define to 0/1 to control error logging in
    the examples. Automatically defined in `examples/logging.h` if unset.
*   `LIBGAV1_ENABLE_TRANSFORM_RANGE_CHECK`: define to 1 to enable transform
    coefficient range checks.
*   `LIBGAV1_LOG_LEVEL`: controls the maximum allowed log level, see `enum
    LogSeverity` in `src/utils/logging.h`. Automatically defined in
    `src/utils/logging.cc` if unset.
*   `LIBGAV1_THREADPOOL_USE_STD_MUTEX`: controls use of std::mutex and
    absl::Mutex in ThreadPool. Defining this to 1 will remove any Abseil
    dependency from the core library. Automatically defined in
    `src/utils/threadpool.h` if unset.

For additional options see:

```shell
  $ cmake .. -LH
```

## Testing

*   `gav1_decode` can be used to decode IVF files, see `gav1_decode --help` for
    options. Note: tools like [FFmpeg](https://ffmpeg.org) can be used to
    convert other container formats to IVF.

## Development

### Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to submit patches.

### Style

libgav1 follows the
[Google C++ style guide](https://google.github.io/styleguide/cppguide.html) with
formatting enforced by `clang-format`.

### Comments

Comments of the form '`// X.Y(.Z).`', '`Section X.Y(.Z).`' or '`... in the
spec`' reference the relevant section(s) in the
[AV1 specification](http://aomediacodec.github.io/av1-spec/av1-spec.pdf).

### DSP structure

*   `src/dsp/dsp.cc` defines the main entry point: `libgav1::dsp::DspInit()`.
    This handles cpu-detection and initializing each logical unit which populate
    `libgav1::dsp::Dsp` function tables.
*   `src/dsp/dsp.h` contains function and type definitions for all logical units
    (e.g., intra-predictors)
*   `src/utils/cpu.h` contains definitions for cpu-detection
*   base implementations are located in `src/dsp/*.{h,cc}` with platform
    specific optimizations in sub-folders
*   unit tests define `DISABLED_Speed` test(s) to allow timing of individual
    functions

#### Symbol reduction

Based on the build configuration unneeded lesser optimizations are removed using
a hierarchical include and define system. Each logical unit in `src/dsp` should
include all platform specific headers in descending order to allow higher level
optimizations to disable lower level ones. See `src/dsp/loop_filter.h` for an
example.

Each function receives a new define which can be checked in platform specific
headers. The format is: `LIBGAV1_<Dsp-table>_FunctionName` or
`LIBGAV1_<Dsp-table>_[sub-table-index1][...-indexN]`, e.g.,
`LIBGAV1_Dsp8bpp_AverageBlend`,
`LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDc`. The Dsp-table name is of
the form `Dsp<bitdepth>bpp` e.g. `Dsp10bpp` for bitdepth == 10 (bpp stands for
bits per pixel). The indices correspond to enum values used as lookups with
leading 'k' removed. Platform specific headers then should first check if the
symbol is defined and if not set the value to the corresponding
`LIBGAV1_CPU_<arch>` value from `src/utils/cpu.h`.

```
  #ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDc
  #define LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDc LIBGAV1_CPU_SSE4_1
  #endif
```

Within each module the code should check if the symbol is defined to its
specific architecture or forced via `LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS` before
defining the function. The `DSP_ENABLED_(8|10)BPP_*` macros are available to
simplify this check for optimized code.

```
  #if DSP_ENABLED_8BPP_SSE4_1(TransformSize4x4_IntraPredictorDc)
  ...

  // In unoptimized code use the following structure; there's no equivalent
  // define for LIBGAV1_CPU_C as it would require duplicating the function
  // defines used in optimized code for only a small benefit to this
  // boilerplate.
  #if LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  ...
  #else  // !LIBGAV1_ENABLE_ALL_DSP_FUNCTIONS
  #ifndef LIBGAV1_Dsp8bpp_TransformSize4x4_IntraPredictorDcFill
  ...
```

## Bugs

Please report all bugs to the issue tracker:
https://issuetracker.google.com/issues/new?component=750480&template=1355007

## Discussion

Email: gav1-devel@googlegroups.com

Web: https://groups.google.com/forum/#!forum/gav1-devel
