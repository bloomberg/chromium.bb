# Video Decoder performance tests
The video decoder performance tests are a set of tests used to measure the
performance of various video decoder implementations. These tests run directly
on top of the video decoder implementation, and don't require the full Chrome
browser stack. They are build on top of the
[GoogleTest](https://github.com/google/googletest/blob/master/README.md)
framework.

[TOC]

## Running from Tast
__Note:__ The video decoder performance tests are in the progress of being added
to the Tast framework. This section will be updated once this is done.

## Running manually
To run the video decoder performance tests manually the
_video_decode_accelerator_perf_tests_ target needs to be built and deployed to
the device being tested. Running the video decoder performance tests can be done
by executing:

    ./video_decode_accelerator_perf_tests [<video path>] [<video metadata path>]

e.g.: `./video_decode_accelerator_perf_tests test-25fps.h264`

__Test videos:__ Test videos are present for multiple codecs in the
[_media/test/data_](https://cs.chromium.org/chromium/src/media/test/data/)
folder in Chromium's source tree (e.g.
[_test-25fps.vp8_](https://cs.chromium.org/chromium/src/media/test/data/test-25fps.vp8)).
If no video is specified _test-25fps.h264_ will be used.

__Video Metadata:__ These videos also have an accompanying metadata _.json_ file
that needs to be deployed alongside the test video. They can also be found in
the _media/test/data_ folder (e.g.
[_test-25fps.h264.json_](https://cs.chromium.org/chromium/src/media/test/data/test-25fps.h264.json)).
If no metadata file is specified _\<video path\>.json_ will be used. The video
metadata file contains info about the video such as its codec profile,
dimensions and number of frames.

__Note:__ Various CPU scaling and thermal throttling mechanisms might be active
on the device (e.g. _scaling_governor_, _intel_pstate_). As these can influence
test results it's advised to disable them.

## Performance metrics
Currently uncapped decoder performance is measured by playing the specified test
video from start to finish. Various performance metrics (e.g. the number of
frames decoded per second) are collected, and written to a file called
_perf_metrics/<test_name>.txt_. Individual frame decode times can be found in
_perf_metrics/<test_name>.frame_times.txt_.

__Note:__ A capped performance test is in the process of being added. This test
will simulate a more realistic real-time decoding environment.

## Command line options
Multiple command line arguments can be given to the command:

     -v                  enable verbose mode, e.g. -v=2.
    --vmodule            enable verbose mode for the specified module,
                         e.g. --vmodule=*media/gpu*=2.
    --use_vd             use the new VD-based video decoders, instead of
                         the default VDA-based video decoders.
    --gtest_help         display the gtest help and exit.
    --help               display this help and exit.

## Source code
See the video decoder performance tests [source code](https://cs.chromium.org/chromium/src/media/gpu/video_decode_accelerator_perf_tests.cc).

