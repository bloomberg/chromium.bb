<!-- Copyright 2019 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# Performance tools

This directory contains a variety of command line tools that can be used to run
benchmarks, interact with speed services, and manage performance waterfall
configurations.

Note you can also read the higher level [Chrome Speed][speed] documentation to
learn more about the team organization and, in particular, the top level view
of [How Chrome Measures Performance][chrome_perf].

[speed]: /docs/speed/README.md
[chrome_perf]: /docs/speed/how_does_chrome_measure_performance.md

## run_benchmark

This command allows running benchmarks defined in the chromium repository,
specifically in [tools/perf/benchmarks][benchmarks_dir]. If you need it,
documentation is available on how to [run benchmarks locally][run_locally]
and how to properly [set up your device][device_setup].

[benchmarks_dir]: https://cs.chromium.org/chromium/src/tools/perf/benchmarks/
[run_locally]: https://chromium.googlesource.com/catapult.git/+/HEAD/telemetry/docs/run_benchmarks_locally.md
[device_setup]: /docs/speed/benchmark/telemetry_device_setup.md


## TODO

Documentation for other commands in this directory is also coming.
