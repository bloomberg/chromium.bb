# Running tests

The testing strategy for Perfetto is rather complex due to the wide variety
of build configurations and embedding targets.

Common test targets (all platforms / checkouts):

`perfetto_unittests`:  
Platform-agnostic unit-tests.

`perfetto_integrationtests`:  
End-to-end tests, involving the protobuf-based IPC transport and ftrace
integration (Linux/Android only).

`perfetto_benchmarks`:  
Benchmarks tracking the performance of: (i) trace writing, (ii) trace readback
and (iii) ftrace raw pipe -> protobuf translation.

Running tests on Linux / MacOS
------------------------------

```bash
tools/ninja -C out/default perfetto_{unittests,integrationtests,benchmarks}
out/default/perfetto_unittests --gtest_help
```

`perfetto_integrationtests` requires that the ftrace debugfs directory is
is readable/writable by the current user on Linux:
```bash
sudo chown  -R $USER /sys/kernel/debug/tracing
```

Running tests on Android
------------------------
1A) Connect a device through `adb`  
1B) Start the build-in emulator (supported on Linux and MacOS):

```bash
tools/install-build-deps --android
tools/run_android_emulator &
```

2) Run the tests (either on the emulator or physical device):  

```bash
tools/run_android_test out/default perfetto_unittests
```

Continuous testing
------------------
Perfetto is tested in a variety of locations:

**Perfetto CI**: https://ci.perfetto.dev/  
Builds and runs perfetto_{unittests,integrationtests,benchmarks} from the
standalone checkout. Benchmarks are ran in a reduced form for smoke testing.
See [this doc](/docs/design-docs/continuous-integration.md) for more details.

**Android CI** (see go/apct and go/apct-guide):  
runs only `perfetto_integrationtests`

**Android presubmits (TreeHugger)**:  
Runs before submission of every AOSP CL of `external/perfetto`.

**Android CTS** (Android test suite used run to ensure API compatibility):   
Rolling runs internally.

Note that Perfetto CI uses the standalone build system and the others build as
part of the Android tree.

Unit tests
----------
Unit tests exist for most of the code in Perfetto on the class level. They
ensure that each class broadly works as expected.

Unit tests are currently ran on ci.perfetto.dev and build.chromium.org.
Running unit tests on APCT and Treehugger is WIP.

Integration tests
-----------------
Integration tests ensure that subsystems (importantly ftrace and the IPC layer)
and Perfetto as a whole is working correctly end-to-end.

There are two configurations in which integration tests can be run:

**1. Production mode** (Android-only)  
This mode assumes that both the tracing service (`traced`) and the OS probes
service (`traced_probes`) are already running. In this mode the test enables
only the consumer endpoint and tests the interaction with the production
services. This is the way our Android CTS and APCT tests work.

**2. Standalone mode**:  
Starting up the daemons in the test itself and then testing against them.
This is how standalone builds are tested. This is the only supported way to
run integration tests on Linux and MacOS.

Trace Processor diff tests
-----------------
Trace processor is mainly tested using so called "diff tests".

For these tests, trace processor parses a known trace and executes a query
file. The output of these queries is then compared (i.e. "diff"ed) against
an expected output file and discrepencies are highlighted.

Similar diff tests are also available when writing metrics - instead of a
query file, the metric name is used and the expected output file contains
the expected result of computing the metric.

These tests (for both queries and metrics) can be run as follows:
```bash
tools/ninja -C <out directory>
tools/diff_test_trace_processor.py <out directory>/trace_processor_shell
```

To add a new diff test (for query tests only), the `tools/add_tp_diff_test.py`
script is useful. It will automatically create the query and expected output
files and adds them both to the index. For metrics, this has to be done
manually for now.

TIP: Query diff tests are expected to only have a single query which produces
output in the whole file (usually at the end). Calling
`SELECT RUN_METRIC('metric file')` can trip up this check as this query
generates some hidden output. To address this issue, if a query only has
column is named `suppress_query_output`, even if it has output, this will
be ignored (for example,
`SELECT RUN_METRIC('metric file') as suppress_query_output`)

UI pixel diff tests
-----------------
The pixel tests are used to ensure core user journeys work by verifying they
are the same pixel to pixel against a golden screenshot. They use a headless
chrome to load the webpage and take a screenshot and compare pixel by pixel a
golden screenshot. You can run these tests by using `ui/run-integrationtests`.


These test fail when a certain number of pixels are different. If these tests
fail, you'll need to investigate the diff and determine if its intentional. If
its a desired change you will need to update the screenshots on a linux machine
to get the CI to pass. You can update them by generating and uploading a new
baseline (this requires access to a google bucket through gcloud which only
googlers have access to, googlers can install gcloud
[here](https://g3doc.corp.google.com/cloud/sdk/g3doc/index.md#installing-and-using-the-cloud-sdk)).

```
ui/run-integrationtests --rebaseline
tools/test_data upload
```

Once finished you can commit and upload as part of your CL to cause the CI to
use your new screenshots.

NOTE: If you see a failing diff test you can see the pixel differences on the CI
by using the link to the UI and replace `/ui/index.html` with
`/ui-test-artifacts/<name_of_failing_png_test_from_logs>.png`. This allows you
to tell where in the picture the change was introduced.

Android CTS tests
-----------------
CTS tests ensure that any vendors who modify Android remain compliant with the
platform API.

These tests include a subset of the integration tests above as well as adding
more complex tests which ensure interaction between platform (e.g. Android apps
etc.) and Perfetto is not broken.

The relevant targets are `CtsPerfettoProducerApp` and `CtsPerfettoTestCases`. Once these are built, the following commands should be run:

```bash
adb push $ANDROID_HOST_OUT/cts/android-cts/testcases/CtsPerfettoTestCases64 /data/local/tmp/
adb install -r $ANDROID_HOST_OUT/cts/android-cts/testcases/CtsPerfettoProducerApp.apk
```

Next, the app named `android.perfetto.producer` should be run on the device.

Finally, the following command should be run:

```bash
adb shell /data/local/tmp/CtsPerfettoTestCases64
```

Chromium waterfall
------------------
Perfetto is constantly rolled into chromium's //third_party/perfetto via
[this autoroller](https://autoroll.skia.org/r/perfetto-chromium-autoroll).

The [Chromium CI](https://build.chromium.org) runs the `perfetto_unittests`
target, as defined in the [buildbot config][chromium_buildbot].

[chromium_buildbot]: https://cs.chromium.org/search/?q=perfetto_.*tests+f:%5Esrc/testing.*json$&sq=package:chromium&type=cs
