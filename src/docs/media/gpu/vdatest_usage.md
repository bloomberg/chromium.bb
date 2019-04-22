# Using the Video Decode/Encode Accelerator Unittests Manually

VDAtest (or `video_decode_accelerator_unittest`) and VEAtest (or
`video_encode_accelerator_unittest`) are unit tests that embeds the Chrome video
decoding/encoding stack without requiring the whole browser, meaning they can
work in a headless environment. They includes a variety of tests to validate the
decoding and encoding stacks with h264, vp8 and vp9.

Running these tests manually can be very useful when bringing up a new codec, or
in order to make sure that new code does not break hardware decoding and/or
encoding. This document is a walk though the prerequisites for running these
programs, as well as their most common options.

## Prerequisites

The required kernel drivers should be loaded, and there should exist a
`/dev/video-dec0` symbolic link pointing to the decoder device node (e.g.
`/dev/video-dec0` → `/dev/video0`). Similarly, a `/dev/video-enc0` symbolic
link should point to the encoder device node.

The unittests can be built by specifying the `video_decode_accelerator_unittest`
and `video_encode_accelerator_unittest` targets to `ninja`. If you are building
for an ARM board that is not yet supported by the
[simplechrome](https://chromium.googlesource.com/chromiumos/docs/+/master/simple_chrome_workflow.md)
workflow, use `arm-generic` as the board. It should work across all ARM targets.

For unlisted Intel boards, any other Intel target (preferably with the same
chipset) should be usable with libva. AMD targets can use `amd64-generic`.

## Basic VDA usage

The `media/test/data` folder in Chromium's source tree contains files with
encoded video data (`test-25fps.h264`, `test-25fps.vp8` and `test-25fps.vp9`).
Each of these files also has a `.md5` counterpart, which contains the md5
checksums of valid thumbnails.

Running the VDAtest can be done as follows:

    ./video_decode_accelerator_unittest --disable_rendering --single-process-tests --test_video_data=test_video

Where test_video is of the form

    filename:width:height:numframes:numfragments:minFPSwithRender:minFPSnoRender:profile

The correct value of test_video for each test file follows:

* __H264__: `test-25fps.h264:320:240:250:258:35:150:1`
* __VP8__: `test-25fps.vp8:320:240:250:250:35:150:11`
* __VP9__: `test-25fps.vp9:320:240:250:250:35:150:12`

So in order to run all h264 tests, one would invoke

    ./video_decode_accelerator_unittest --disable_rendering --single-process-tests --test_video_data=test-25fps.h264:320:240:250:258:35:150:1

## Test filtering options

`./video_decode_accelerator_unittest --help` will list all valid options.

The list of available tests can be retrieved using the `--gtest_list_tests`
option.

By default, all tests are run, which can be a bit too much, especially when
bringing up a new codec. The `--gtest_filter` option can be used to specify a
pattern of test names to run. For instance, to only run the
`TestDecodeTimeMedian` test, one can specify
`--gtest_filter="*TestDecodeTimeMedian*"`.

So the complete command line to test vp9 decoding with the
`TestDecodeTimeMedian` test only (a good starting point for bringup) would be

    ./video_decode_accelerator_unittest --disable_rendering --single-process-tests --test_video_data=test-25fps.vp9:320:240:250:250:35:150:12 --gtest_filter="*TestDecodeTimeMedian*"

## Verbosity options

The `--vmodule` options allows to specify a set of source files that should be
more verbose about what they are doing. For basic usage, a useful set of vmodule
options could be:

    --vmodule=*/media/gpu/*=4

## Testing performance

Use the `--disable_rendering --rendering_fps=0 --gtest_filter="DecodeVariations/*/0"`
options to max the decoder output and measure its performance.

## Testing parallel decoding

Use `--gtest_filter="ResourceExhaustion*/0"` to run 3 decoders in parallel, and
`--gtest_filter="ResourceExhaustion*/1"` to run 4 decoders in parallel.

## Wrap-up

Using all these options together, we can invoke VDAtest in the following way for
a verbose H264 decoding test:

    ./video_decode_accelerator_unittest --single-process-tests --disable_rendering --gtest_filter="*TestDecodeTimeMedian*" --vmodule=*/media/gpu/*=4 --test_video_data=test-25fps.h264:320:240:250:258:35:150:1

## Import mode

There are two modes in which VDA runs, ALLOCATE and IMPORT. In ALLOCATE mode,
the video decoder is responsible for allocating the buffers containing the
decoded frames itself. In IMPORT mode, the buffers are allocated by the client
and provided to the decoder during decoding. ALLOCATE mode is used during
playback within Chrome (e.g. HTML5 videos), while IMPORT mode is used by ARC++
when Android applications require accelerated decoding.\\
VDAtest runs VDA in ALLOCATE mode by default. Use `--test_import` to run VDA in
IMPORT mode. VDA cannot run in IMPORT mode on platforms too old for ARC++ to be
enabled.

## (Recommended) Frame validator

Use `--frame_validator=check` to verify the correctness of frames decoded by
VideoDecodeAccelerator in all test cases. This validator is based on the fact
that a decoded content is deterministic in H.264, VP8 and VP9. It reads the
expected md5 value of each frame from `*.frames.md5`, for example, `test-25fps.h264.frames.md5`
for `test-25fps.h264`.\\
VDATest is able to read the memory of a decoded frame only if VDA runs in IMPORT
mode. Therefore, if `--frame_validator=check` is specified, VDATest runs as if
`--test_import` is specified. See [Import mode](#import-mode) about IMPORT mode.

### Dump mode

Use `--frame_validator=dump` to write down all the decoded frames. The output
format will be I420 and the saved file name will be `frame_%{frame-num}_%{width}x%{height}_I420.yuv`
in the specified directory or a directory whose name is the test file + `.frames`
if unspecified. Here, width and height are visible width and height. For
instance, they will be `test-25fps.h264.frames/frame_%{frame-num}_320x180_I420.yuv.`

### How to generate md5 values of decoded frames for a new video stream

It is necessary to generate md5 values of decoded frames for new test streams.
ffmpeg with `-f framemd5` can be used for this purpose. For instance,
`ffmpeg -i test-25fps.h264 -f framemd5 test-25fps.frames.md5`

## Basic VEA usage

The VEA works in a similar fashion to the VDA, taking raw YUV files in I420
format as input and producing e.g. a H.264 Annex-B byte stream. Sample raw YUV
files can be found at the following locations:

* [1080 Crowd YUV](http://commondatastorage.googleapis.com/chromiumos-test-assets-public/crowd/crowd1080-96f60dd6ff87ba8b129301a0f36efc58.yuv)
* [320x180 Bear YUV](http://commondatastorage.googleapis.com/chromiumos-test-assets-public/bear/bear-320x180-c60a86c52ba93fa7c5ae4bb3156dfc2a.yuv)

It is recommended to rename these files after downloading them to e.g.
`crowd1080.yuv` and `bear-320x180.yuv`.

The VEA can then be tested as follows:

    ./video_encode_accelerator_unittest --single-process-tests --disable_flush --gtest_filter=SimpleEncode/VideoEncodeAcceleratorTest.TestSimpleEncode/0 --test_stream_data=bear-320x180.yuv:320:180:1:bear.mp4:100000:30

for the `bear` file, and

    ./video_encode_accelerator_unittest --single-process-tests --disable_flush --gtest_filter=SimpleEncode/VideoEncodeAcceleratorTest.TestSimpleEncode/0 --test_stream_data=crowd1080.yuv:1920:1080:1:crowd.mp4:4000000:30

for the larger `crowd` file. These commands will put the encoded output into
`bear.mp4` and `crowd.mp4` respectively. They can then be copied on the host and
played with `mplayer -fps 25`.

## Source code

The VDAtest's source code can be consulted here: [https://cs.chromium.org/chromium/src/media/gpu/video_decode_accelerator_unittest.cc](https://cs.chromium.org/chromium/src/media/gpu/video_decode_accelerator_unittest.cc).

V4L2 support: [https://cs.chromium.org/chromium/src/media/gpu/v4l2/](https://cs.chromium.org/chromium/src/media/gpu/v4l2/).

VAAPI support: [https://cs.chromium.org/chromium/src/media/gpu/vaapi/](https://cs.chromium.org/chromium/src/media/gpu/vaapi/).
