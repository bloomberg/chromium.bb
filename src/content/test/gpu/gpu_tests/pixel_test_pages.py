# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is more akin to a .pyl/JSON file, so it's expected to be long.
# pylint: disable=too-many-lines

import datetime

from gpu_tests import skia_gold_matching_algorithms as algo

_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS = [
    {
        'comment': 'top left video, yellow',
        'location': [5, 5],
        'size': [110, 57],
        'color': [255, 255, 15],
    },
    {
        'comment': 'top right video, red',
        'location': [125, 5],
        'size': [110, 57],
        'color': [255, 17, 24],
    },
    {
        'comment': 'bottom left video, blue',
        'location': [5, 72],
        'size': [110, 57],
        'color': [12, 12, 255],
    },
    {
        'comment': 'bottom right video, green',
        'location': [125, 72],
        'size': [110, 57],
        'color': [44, 255, 16],
    },
]

CRASH_TYPE_GPU = 'gpu'

# Meant to be used when we know a test is going to be noisy, and we want any
# images it generates to be auto-triaged until we have enough data to calculate
# more suitable/less permissive parameters.
VERY_PERMISSIVE_SOBEL_ALGO = algo.SobelMatchingAlgorithm(
    max_different_pixels=10000, pixel_delta_threshold=255, edge_threshold=0)


class PixelTestPage(object):
  """A wrapper class mimicking the functionality of the PixelTestsStorySet
  from the old-style GPU tests.
  """

  def __init__(self,
               url,
               name,
               test_rect,
               tolerance=2,
               browser_args=None,
               expected_colors=None,
               gpu_process_disabled=False,
               optional_action=None,
               restart_browser_after_test=False,
               other_args=None,
               grace_period_end=None,
               expected_per_process_crashes=None,
               matching_algorithm=None):
    super(PixelTestPage, self).__init__()
    self.url = url
    self.name = name
    self.test_rect = test_rect
    # The tolerance when comparing against the reference image.
    self.tolerance = tolerance
    self.browser_args = browser_args
    # The expected colors can be specified as a list of dictionaries,
    # in which case these specific pixels will be sampled instead of
    # comparing the entire image snapshot. The format is only defined
    # by contract with _CompareScreenshotSamples in
    # cloud_storage_integration_test_base.py.
    self.expected_colors = expected_colors
    # Only a couple of tests run with the GPU process completely
    # disabled. To prevent regressions, only allow the GPU information
    # to be incomplete in these cases.
    self.gpu_process_disabled = gpu_process_disabled
    # Some of the tests require custom actions to be run. These are
    # specified as a string which is the name of a method to call in
    # PixelIntegrationTest. For example if the action here is
    # "CrashGpuProcess" then it would be defined in a
    # "_CrashGpuProcess" method in PixelIntegrationTest.
    self.optional_action = optional_action
    # Whether the browser should be forcibly restarted after the test
    # runs. The browser is always restarted after running tests with
    # optional_actions.
    self.restart_browser_after_test = restart_browser_after_test
    # These are used to pass additional arguments to the test harness.
    # VideoPathTraceTest and OverlayModeTest support the following boolean
    # arguments: expect_yuy2, zero_copy, video_is_rotated, and no_overlay.
    self.other_args = other_args
    # This allows a newly added test to be exempted from failures for a
    # (hopefully) short period after being added. This is so that any slightly
    # different but valid images that get produced by the waterfall bots can
    # be triaged without turning the bots red.
    # This should be a datetime.date object.
    self.grace_period_end = grace_period_end
    # This lets the test runner know that one or more crashes are expected as
    # part of the test. Should be a map of process type (str) to expected number
    # of crashes (int).
    self.expected_per_process_crashes = expected_per_process_crashes or {}
    # This should be a child of
    # skia_gold_matching_algorithms.SkiaGoldMatchingAlgorithm. This specifies
    # which matching algorithm Skia Gold should use for the test.
    self.matching_algorithm = (matching_algorithm
                               or algo.ExactMatchingAlgorithm())

  def CopyWithNewBrowserArgsAndSuffix(self, browser_args, suffix):
    return PixelTestPage(self.url, self.name + suffix, self.test_rect,
                         self.tolerance, browser_args, self.expected_colors)

  def CopyWithNewBrowserArgsAndPrefix(self, browser_args, prefix):
    # Assuming the test name is 'Pixel'.
    split = self.name.split('_', 1)
    return PixelTestPage(self.url, split[0] + '_' + prefix + split[1],
                         self.test_rect, self.tolerance, browser_args,
                         self.expected_colors)


def CopyPagesWithNewBrowserArgsAndSuffix(pages, browser_args, suffix):
  return [
      p.CopyWithNewBrowserArgsAndSuffix(browser_args, suffix) for p in pages
  ]


def CopyPagesWithNewBrowserArgsAndPrefix(pages, browser_args, prefix):
  return [
      p.CopyWithNewBrowserArgsAndPrefix(browser_args, prefix) for p in pages
  ]


# TODO(kbr): consider refactoring this into pixel_integration_test.py.
SCALE_FACTOR_OVERRIDES = {
    "comment":
    "scale factor overrides",
    "scale_factor_overrides": [
        {
            "device_type": "Nexus 5",
            "scale_factor": 1.105
        },
        {
            "device_type": "Nexus 5X",
            "scale_factor": 1.105
        },
        {
            "device_type": "Nexus 6",
            "scale_factor": 1.47436
        },
        {
            "device_type": "Nexus 6P",
            "scale_factor": 1.472
        },
        {
            "device_type": "Nexus 9",
            "scale_factor": 1.566
        },
        {
            "comment": "NVIDIA Shield",
            "device_type": "sb_na_wf",
            "scale_factor": 1.226
        },
        {
            "device_type": "Pixel 2",
            "scale_factor": 1.1067
        },
    ]
}


class PixelTestPages(object):
  @staticmethod
  def DefaultPages(base_name):
    sw_compositing_args = ['--disable-gpu-compositing']

    # Tolerance of 10% is required for all the formats to match gold/pixel
    # expectations on all the platforms for pixel video tests. Hence setting it
    # to 20.
    # Bug filed on MacOSX to investigate the tolerance -
    # https://crbug.com/911895.
    tolerance = 20
    tolerance_vp9 = 20

    return [
        PixelTestPage('pixel_background_image.html',
                      base_name + '_BackgroundImage',
                      test_rect=[20, 20, 370, 370]),
        PixelTestPage('pixel_reflected_div.html',
                      base_name + '_ReflectedDiv',
                      test_rect=[0, 0, 100, 300]),
        PixelTestPage('pixel_canvas2d.html',
                      base_name + '_Canvas2DRedBox',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_canvas2d_untagged.html',
                      base_name + '_Canvas2DUntagged',
                      test_rect=[0, 0, 257, 257]),
        PixelTestPage('pixel_css3d.html',
                      base_name + '_CSS3DBlueBox',
                      test_rect=[0, 0, 300, 300],
                      matching_algorithm=algo.SobelMatchingAlgorithm(
                          max_different_pixels=0,
                          pixel_delta_threshold=0,
                          edge_threshold=100)),
        PixelTestPage('pixel_webgl_aa_alpha.html',
                      base_name + '_WebGLGreenTriangle_AA_Alpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noaa_alpha.html',
                      base_name + '_WebGLGreenTriangle_NoAA_Alpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_aa_noalpha.html',
                      base_name + '_WebGLGreenTriangle_AA_NoAlpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noaa_noalpha.html',
                      base_name + '_WebGLGreenTriangle_NoAA_NoAlpha',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_noalpha_implicit_clear.html',
                      base_name +
                      '_WebGLTransparentGreenTriangle_NoAlpha_ImplicitClear',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_webgl_sad_canvas.html',
                      base_name + '_WebGLSadCanvas',
                      test_rect=[0, 0, 300, 300],
                      optional_action='CrashGpuProcess'),
        PixelTestPage('pixel_scissor.html',
                      base_name + '_ScissorTestWithPreserveDrawingBuffer',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_canvas2d_webgl.html',
                      base_name + '_2DCanvasWebGL',
                      test_rect=[0, 0, 300, 300]),
        PixelTestPage('pixel_background.html',
                      base_name + '_SolidColorBackground',
                      test_rect=[500, 500, 100, 100]),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_Video_MP4',
            test_rect=[0, 0, 240, 135],
            tolerance=tolerance,
            expected_colors=_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_Video_MP4_DXVA',
            browser_args=['--disable-features=D3D11VideoDecoder'],
            test_rect=[0, 0, 240, 135],
            tolerance=tolerance,
            expected_colors=_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS),
        PixelTestPage('pixel_video_mp4_four_colors_aspect_4x3.html',
                      base_name + '_Video_MP4_FourColors_Aspect_4x3',
                      test_rect=[0, 0, 240, 135],
                      tolerance=tolerance,
                      expected_colors=[
                          {
                              'comment':
                              'outside video content, left side, white',
                              'location': [1, 1],
                              'size': [28, 133],
                              'color': [255, 255, 255],
                          },
                          {
                              'comment':
                              'outside video content, right side, white',
                              'location': [211, 1],
                              'size': [28, 133],
                              'color': [255, 255, 255],
                          },
                          {
                              'comment': 'top left video, yellow',
                              'location': [35, 5],
                              'size': [80, 57],
                              'color': [255, 255, 15],
                          },
                          {
                              'comment': 'top right video, red',
                              'location': [125, 5],
                              'size': [80, 57],
                              'color': [255, 17, 24],
                          },
                          {
                              'comment': 'bottom left video, blue',
                              'location': [35, 73],
                              'size': [80, 57],
                              'color': [12, 12, 255],
                          },
                          {
                              'comment': 'bottom right video, green',
                              'location': [125, 73],
                              'size': [80, 57],
                              'color': [44, 255, 16],
                          },
                      ]),
        PixelTestPage('pixel_video_mp4_four_colors_rot_90.html',
                      base_name + '_Video_MP4_FourColors_Rot_90',
                      test_rect=[0, 0, 270, 240],
                      tolerance=tolerance,
                      expected_colors=[
                          {
                              'comment':
                              'outside video content, left side, white',
                              'location': [1, 1],
                              'size': [60, 238],
                              'color': [255, 255, 255],
                          },
                          {
                              'comment':
                              'outside video content, right side, white',
                              'location': [210, 1],
                              'size': [60, 238],
                              'color': [255, 255, 255],
                          },
                          {
                              'comment': 'top left video, red',
                              'location': [73, 5],
                              'size': [55, 110],
                              'color': [255, 17, 24],
                          },
                          {
                              'comment': 'top right video, green',
                              'location': [141, 5],
                              'size': [55, 110],
                              'color': [44, 255, 16],
                          },
                          {
                              'comment': 'bottom left video, yellow',
                              'location': [73, 125],
                              'size': [55, 110],
                              'color': [255, 255, 15],
                          },
                          {
                              'comment': 'bottom right video, blue',
                              'location': [141, 125],
                              'size': [55, 110],
                              'color': [12, 12, 255],
                          },
                      ]),
        PixelTestPage('pixel_video_mp4_four_colors_rot_180.html',
                      base_name + '_Video_MP4_FourColors_Rot_180',
                      test_rect=[0, 0, 240, 135],
                      tolerance=tolerance,
                      expected_colors=[
                          {
                              'comment': 'top left video, green',
                              'location': [5, 5],
                              'size': [110, 57],
                              'color': [44, 255, 16],
                          },
                          {
                              'comment': 'top right video, blue',
                              'location': [125, 5],
                              'size': [110, 57],
                              'color': [12, 12, 255],
                          },
                          {
                              'comment': 'bottom left video, red',
                              'location': [5, 72],
                              'size': [110, 57],
                              'color': [255, 17, 24],
                          },
                          {
                              'comment': 'bottom right video, yellow',
                              'location': [125, 72],
                              'size': [110, 57],
                              'color': [255, 255, 15],
                          },
                      ]),
        PixelTestPage('pixel_video_mp4_four_colors_rot_270.html',
                      base_name + '_Video_MP4_FourColors_Rot_270',
                      test_rect=[0, 0, 270, 240],
                      tolerance=tolerance,
                      expected_colors=[
                          {
                              'comment':
                              'outside video content, left side, white',
                              'location': [1, 1],
                              'size': [60, 238],
                              'color': [255, 255, 255],
                          },
                          {
                              'comment':
                              'outside video content, right side, white',
                              'location': [210, 1],
                              'size': [60, 238],
                              'color': [255, 255, 255],
                          },
                          {
                              'comment': 'top left video, blue',
                              'location': [73, 5],
                              'size': [55, 110],
                              'color': [12, 12, 255],
                          },
                          {
                              'comment': 'top right video, yellow',
                              'location': [141, 5],
                              'size': [55, 110],
                              'color': [255, 255, 15],
                          },
                          {
                              'comment': 'bottom left video, green',
                              'location': [73, 125],
                              'size': [55, 110],
                              'color': [44, 255, 16],
                          },
                          {
                              'comment': 'bottom right video, red',
                              'location': [141, 125],
                              'size': [55, 110],
                              'color': [255, 17, 24],
                          },
                      ]),
        PixelTestPage('pixel_video_mp4_rounded_corner.html',
                      base_name + '_Video_MP4_Rounded_Corner',
                      test_rect=[0, 0, 240, 135],
                      tolerance=tolerance),
        PixelTestPage(
            'pixel_video_vp9.html',
            base_name + '_Video_VP9',
            test_rect=[0, 0, 240, 135],
            tolerance=tolerance_vp9,
            expected_colors=_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS),
        PixelTestPage(
            'pixel_video_vp9.html',
            base_name + '_Video_VP9_DXVA',
            browser_args=['--disable-features=D3D11VideoDecoder'],
            test_rect=[0, 0, 240, 135],
            tolerance=tolerance_vp9,
            expected_colors=_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS),

        # The MP4 contains H.264 which is primarily hardware decoded on bots.
        PixelTestPage(
            'pixel_video_context_loss.html?src='
            '/media/test/data/four-colors.mp4',
            base_name + '_Video_Context_Loss_MP4',
            test_rect=[0, 0, 240, 135],
            tolerance=tolerance,
            expected_colors=_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS,
            expected_per_process_crashes={
                CRASH_TYPE_GPU: 1,
            }),

        # The VP9 test clip is primarily software decoded on bots.
        PixelTestPage(('pixel_video_context_loss.html'
                       '?src=/media/test/data/four-colors-vp9.webm'),
                      base_name + '_Video_Context_Loss_VP9',
                      test_rect=[0, 0, 240, 135],
                      tolerance=tolerance_vp9,
                      expected_colors=_FOUR_COLOR_VIDEO_240x135_EXPECTED_COLORS,
                      expected_per_process_crashes={
                          CRASH_TYPE_GPU: 1,
                      }),
        PixelTestPage('pixel_video_backdrop_filter.html',
                      base_name + '_Video_BackdropFilter',
                      test_rect=[0, 0, 240, 135],
                      tolerance=tolerance),
        PixelTestPage('pixel_webgl_premultiplied_alpha_false.html',
                      base_name + '_WebGL_PremultipliedAlpha_False',
                      test_rect=[0, 0, 150, 150]),
        PixelTestPage('pixel_webgl2_blitframebuffer_result_displayed.html',
                      base_name + '_WebGL2_BlitFramebuffer_Result_Displayed',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_webgl2_clearbufferfv_result_displayed.html',
                      base_name + '_WebGL2_ClearBufferfv_Result_Displayed',
                      test_rect=[0, 0, 200, 200]),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D',
                      test_rect=[0, 0, 256, 256]),
        PixelTestPage('pixel_repeated_webgl_to_2d.html',
                      base_name + '_RepeatedWebGLTo2D_SoftwareCompositing',
                      test_rect=[0, 0, 256, 256],
                      browser_args=sw_compositing_args),
        PixelTestPage('pixel_canvas2d_tab_switch.html',
                      base_name + '_Canvas2DTabSwitch',
                      test_rect=[0, 0, 100, 100],
                      optional_action='SwitchTabs'),
        PixelTestPage('pixel_canvas2d_tab_switch.html',
                      base_name + '_Canvas2DTabSwitch_SoftwareCompositing',
                      test_rect=[0, 0, 100, 100],
                      browser_args=sw_compositing_args,
                      optional_action='SwitchTabs'),
        PixelTestPage('pixel_webgl_copy_image.html',
                      base_name + '_WebGLCopyImage',
                      test_rect=[0, 0, 200, 100]),
        PixelTestPage('pixel_webgl_read_pixels_tab_switch.html',
                      base_name + '_WebGLReadPixelsTabSwitch',
                      test_rect=[0, 0, 100, 100],
                      optional_action='SwitchTabs',
                      tolerance=3,
                      expected_colors=[
                          {
                              'comment': 'top left, red',
                              'location': [5, 5],
                              'size': [40, 40],
                              'color': [255, 0, 0],
                          },
                          {
                              'comment': 'bottom right, red',
                              'location': [55, 55],
                              'size': [40, 40],
                              'color': [255, 0, 0],
                          },
                          {
                              'comment': 'top right, blue',
                              'location': [55, 5],
                              'size': [40, 40],
                              'color': [0, 0, 255],
                          },
                          {
                              'comment': 'bottom left, green',
                              'location': [5, 55],
                              'size': [40, 40],
                              'color': [0, 255, 0],
                          },
                      ]),
        PixelTestPage('pixel_webgl_read_pixels_tab_switch.html',
                      base_name +
                      '_WebGLReadPixelsTabSwitch_SoftwareCompositing',
                      test_rect=[0, 0, 100, 100],
                      browser_args=sw_compositing_args,
                      optional_action='SwitchTabs',
                      tolerance=3,
                      expected_colors=[
                          {
                              'comment': 'top left, red',
                              'location': [5, 5],
                              'size': [40, 40],
                              'color': [255, 0, 0],
                          },
                          {
                              'comment': 'bottom right, red',
                              'location': [55, 55],
                              'size': [40, 40],
                              'color': [255, 0, 0],
                          },
                          {
                              'comment': 'top right, blue',
                              'location': [55, 5],
                              'size': [40, 40],
                              'color': [0, 0, 255],
                          },
                          {
                              'comment': 'bottom left, green',
                              'location': [5, 55],
                              'size': [40, 40],
                              'color': [0, 255, 0],
                          },
                      ]),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_main.html',
                      base_name + '_OffscreenCanvasIBRCWebGLMain',
                      test_rect=[0, 0, 300, 300],
                      tolerance=3,
                      expected_colors=[{
                          'comment': 'solid green',
                          'location': [100, 100],
                          'size': [100, 100],
                          'color': [0, 255, 0],
                      }],
                      optional_action='RunOffscreenCanvasIBRCWebGLTest'),
        PixelTestPage('pixel_offscreen_canvas_ibrc_webgl_worker.html',
                      base_name + '_OffscreenCanvasIBRCWebGLWorker',
                      test_rect=[0, 0, 300, 300],
                      tolerance=3,
                      expected_colors=[{
                          'comment': 'solid green',
                          'location': [100, 100],
                          'size': [100, 100],
                          'color': [0, 255, 0],
                      }],
                      optional_action='RunOffscreenCanvasIBRCWebGLTest'),
    ]

  # Pages that should be run with GPU rasterization enabled.
  @staticmethod
  def GpuRasterizationPages(base_name):
    browser_args = [
        '--force-gpu-rasterization', '--disable-software-compositing-fallback'
    ]
    return [
        PixelTestPage(
            'pixel_background.html',
            base_name + '_GpuRasterization_BlueBox',
            test_rect=[0, 0, 220, 220],
            browser_args=browser_args,
            tolerance=0,
            expected_colors=[
                {
                    'comment': 'body-t',
                    'location': [5, 5],
                    'size': [1, 1],
                    'color': [0, 128, 0],
                },
                {
                    'comment': 'body-r',
                    'location': [215, 5],
                    'size': [1, 1],
                    'color': [0, 128, 0],
                },
                {
                    'comment': 'body-b',
                    'location': [215, 215],
                    'size': [1, 1],
                    'color': [0, 128, 0],
                },
                {
                    'comment': 'body-l',
                    'location': [5, 215],
                    'size': [1, 1],
                    'color': [0, 128, 0],
                },
                {
                    'comment': 'background-t',
                    'location': [30, 30],
                    'size': [1, 1],
                    'color': [0, 0, 0],
                },
                {
                    'comment': 'background-r',
                    'location': [170, 30],
                    'size': [1, 1],
                    'color': [0, 0, 0],
                },
                {
                    'comment': 'background-b',
                    'location': [170, 170],
                    'size': [1, 1],
                    'color': [0, 0, 0],
                },
                {
                    'comment': 'background-l',
                    'location': [30, 170],
                    'size': [1, 1],
                    'color': [0, 0, 0],
                },
                {
                    'comment': 'box-t',
                    'location': [70, 70],
                    'size': [1, 1],
                    'color': [0, 0, 255],
                },
                {
                    'comment': 'box-r',
                    'location': [140, 70],
                    'size': [1, 1],
                    'color': [0, 0, 255],
                },
                {
                    'comment': 'box-b',
                    'location': [140, 140],
                    'size': [1, 1],
                    'color': [0, 0, 255],
                },
                {
                    'comment': 'box-l',
                    'location': [70, 140],
                    'size': [1, 1],
                    'color': [0, 0, 255],
                },
            ]),
        PixelTestPage(
            'concave_paths.html',
            base_name + '_GpuRasterization_ConcavePaths',
            test_rect=[0, 0, 100, 100],
            browser_args=browser_args,
            tolerance=0,
            expected_colors=[
                {
                    'comment': 'outside',
                    'location': [80, 60],
                    'size': [1, 1],
                    'color': [255, 255, 255],
                },
                {
                    'comment': 'outside',
                    'location': [28, 20],
                    'size': [1, 1],
                    'color': [255, 255, 255],
                },
                {
                    'comment': 'inside',
                    'location': [32, 25],
                    'size': [1, 1],
                    'color': [255, 215, 0],
                },
                {
                    'comment': 'inside',
                    'location': [80, 80],
                    'size': [1, 1],
                    'color': [255, 215, 0],
                },
            ]),
        PixelTestPage(
            'pixel_precision_rounded_corner.html',
            base_name + '_PrecisionRoundedCorner',
            test_rect=[0, 0, 400, 400],
            browser_args=browser_args,
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=10,
                pixel_delta_threshold=30,
                edge_threshold=100)),
    ]

  # Pages that should be run with off-thread paint worklet flags.
  @staticmethod
  def PaintWorkletPages(base_name):
    browser_args = [
        '--enable-blink-features=OffMainThreadCSSPaint',
        '--enable-gpu-rasterization', '--enable-oop-rasterization'
    ]

    return [
        PixelTestPage(
            'pixel_paintWorklet_transform.html',
            base_name + '_PaintWorkletTransform',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args),
    ]

  # Pages that should be run with experimental canvas features.
  @staticmethod
  def ExperimentalCanvasFeaturesPages(base_name):
    browser_args = [
        '--enable-experimental-web-platform-features',  # For lowLatency.
    ]
    accelerated_args = [
        '--disable-software-compositing-fallback',
    ]
    unaccelerated_args = [
        '--disable-accelerated-2d-canvas',
        '--disable-gpu-compositing',
    ]

    return [
        PixelTestPage(
            'pixel_offscreenCanvas_transfer_after_style_resize.html',
            base_name + '_OffscreenCanvasTransferAfterStyleResize',
            test_rect=[0, 0, 350, 350],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_transfer_before_style_resize.html',
            base_name + '_OffscreenCanvasTransferBeforeStyleResize',
            test_rect=[0, 0, 350, 350],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_paint_after_resize.html',
            base_name + '_OffscreenCanvasWebGLPaintAfterResize',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args,
            tolerance=0,
            expected_colors=[
                SCALE_FACTOR_OVERRIDES,
                {
                    'comment': 'resized area',
                    'location': [1, 1],
                    'size': [48, 98],
                    'color': [0, 255, 0],
                },
                {
                    'comment': 'outside resized area',
                    'location': [51, 1],
                    'size': [48, 98],
                    'color': [255, 255, 255],
                },
            ]),
        PixelTestPage(
            'pixel_offscreenCanvas_transferToImageBitmap_main.html',
            base_name + '_OffscreenCanvasTransferToImageBitmap',
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_transferToImageBitmap_worker.html',
            base_name + '_OffscreenCanvasTransferToImageBitmapWorker',
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_commit_main.html',
            base_name + '_OffscreenCanvasWebGLDefault',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_commit_worker.html',
            base_name + '_OffscreenCanvasWebGLDefaultWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_commit_main.html',
            base_name + '_OffscreenCanvasWebGLSoftwareCompositing',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + ['--disable-gpu-compositing']),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_commit_worker.html',
            base_name + '_OffscreenCanvasWebGLSoftwareCompositingWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + ['--disable-gpu-compositing']),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_main.html',
            base_name + '_OffscreenCanvasAccelerated2D',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + accelerated_args),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_worker.html',
            base_name + '_OffscreenCanvasAccelerated2DWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + accelerated_args),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_main.html',
            base_name + '_OffscreenCanvasUnaccelerated2D',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_worker.html',
            base_name + '_OffscreenCanvasUnaccelerated2DWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_main.html',
            base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositing',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + ['--disable-accelerated-2d-canvas']),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_commit_worker.html',
            base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
            test_rect=[0, 0, 360, 200],
            browser_args=browser_args + ['--disable-accelerated-2d-canvas']),
        PixelTestPage(
            'pixel_offscreenCanvas_2d_resize_on_worker.html',
            base_name + '_OffscreenCanvas2DResizeOnWorker',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_offscreenCanvas_webgl_resize_on_worker.html',
            base_name + '_OffscreenCanvasWebglResizeOnWorker',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_canvas_display_linear-rgb.html',
            base_name + '_CanvasDisplayLinearRGBAccelerated2D',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + accelerated_args),
        PixelTestPage(
            'pixel_canvas_display_linear-rgb.html',
            base_name + '_CanvasDisplayLinearRGBUnaccelerated2D',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_canvas_display_linear-rgb.html',
            base_name + '_CanvasDisplayLinearRGBUnaccelerated2DGPUCompositing',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + ['--disable-accelerated-2d-canvas']),
        PixelTestPage(
            'pixel_canvas_display_srgb.html',
            base_name + '_CanvasDisplaySRGBAccelerated2D',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + accelerated_args),
        PixelTestPage(
            'pixel_canvas_display_srgb.html',
            base_name + '_CanvasDisplaySRGBUnaccelerated2D',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_canvas_display_srgb.html',
            base_name + '_CanvasDisplaySRGBUnaccelerated2DGPUCompositing',
            test_rect=[0, 0, 140, 140],
            browser_args=browser_args + ['--disable-accelerated-2d-canvas']),
        PixelTestPage(
            'pixel_canvas_low_latency_2d.html',
            base_name + '_CanvasLowLatency2D',
            test_rect=[0, 0, 100, 100],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_canvas_low_latency_2d.html',
            base_name + '_CanvasUnacceleratedLowLatency2D',
            test_rect=[0, 0, 100, 100],
            browser_args=browser_args + unaccelerated_args),
        PixelTestPage(
            'pixel_canvas_low_latency_webgl.html',
            base_name + '_CanvasLowLatencyWebGL',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args,
            tolerance=0,
            expected_colors=[
                SCALE_FACTOR_OVERRIDES,
                {
                    'comment': 'green',
                    'location': [1, 1],
                    'size': [98, 98],
                    'color': [0, 255, 0],
                },
            ]),
    ]

  @staticmethod
  def LowLatencySwapChainPages(base_name):
    browser_args = [
        '--enable-features=LowLatencyWebGLSwapChain,LowLatencyCanvas2dSwapChain'
    ]
    return [
        PixelTestPage(
            'pixel_canvas_low_latency_2d.html',
            base_name + '_CanvasLowLatency2DSwapChain',
            test_rect=[0, 0, 100, 100],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_canvas_low_latency_webgl.html',
            base_name + '_CanvasLowLatencyWebGLSwapChain',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args,
            tolerance=0,
            expected_colors=[
                SCALE_FACTOR_OVERRIDES,
                {
                    'comment': 'green',
                    'location': [1, 1],
                    'size': [98, 98],
                    'color': [0, 255, 0],
                },
            ]),
        PixelTestPage(
            'pixel_canvas_low_latency_webgl_alpha_false.html',
            base_name + '_CanvasLowLatencyWebGLSwapChainAlphaFalse',
            test_rect=[0, 0, 200, 200],
            browser_args=browser_args,
            tolerance=0,
            expected_colors=[
                SCALE_FACTOR_OVERRIDES,
                {
                    'comment': 'green',
                    'location': [1, 1],
                    'size': [98, 98],
                    'color': [0, 255, 0, 255],
                },
            ]),
    ]

  # Only add these tests on platforms where SwiftShader is enabled.
  # Currently this is Windows and Linux.
  @staticmethod
  def SwiftShaderPages(base_name):
    browser_args = ['--disable-gpu']
    suffix = "_SwiftShader"
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_css3d.html',
            base_name + '_CSS3DBlueBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_webgl_aa_alpha.html',
            base_name + '_WebGLGreenTriangle_AA_Alpha' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args),
        PixelTestPage(
            'pixel_repeated_webgl_to_2d.html',
            base_name + '_RepeatedWebGLTo2D' + suffix,
            test_rect=[0, 0, 256, 256],
            browser_args=browser_args,
            tolerance=3,
            expected_colors=[
                SCALE_FACTOR_OVERRIDES,
                {
                    'comment': 'green',
                    # 64x64 rectangle around the center at (128,128)
                    'location': [96, 96],
                    'size': [64, 64],
                    'color': [0, 255, 0],
                },
            ]),
    ]

  # Test rendering where GPU process is blocked.
  @staticmethod
  def NoGpuProcessPages(base_name):
    browser_args = ['--disable-gpu', '--disable-software-rasterizer']
    suffix = "_NoGpuProcess"
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args,
            gpu_process_disabled=True),
        PixelTestPage(
            'pixel_css3d.html',
            base_name + '_CSS3DBlueBox' + suffix,
            test_rect=[0, 0, 300, 300],
            browser_args=browser_args,
            gpu_process_disabled=True),
    ]

  # Pages that should be run with various macOS specific command line
  # arguments.
  @staticmethod
  def MacSpecificPages(base_name):
    iosurface_2d_canvas_args = ['--enable-accelerated-2d-canvas']

    non_chromium_image_args = ['--disable-webgl-image-chromium']

    # This disables the Core Animation compositor, falling back to the
    # old GLRenderer path, but continuing to allocate IOSurfaces for
    # WebGL's back buffer.
    no_overlays_args = ['--disable-mac-overlays']

    return [
        # On macOS, test the IOSurface 2D Canvas compositing path.
        PixelTestPage(
            'pixel_canvas2d_accelerated.html',
            base_name + '_IOSurface2DCanvas',
            test_rect=[0, 0, 400, 400],
            browser_args=iosurface_2d_canvas_args),
        PixelTestPage(
            'pixel_canvas2d_webgl.html',
            base_name + '_IOSurface2DCanvasWebGL',
            test_rect=[0, 0, 300, 300],
            browser_args=iosurface_2d_canvas_args),

        # On macOS, test WebGL non-Chromium Image compositing path.
        PixelTestPage(
            'pixel_webgl_aa_alpha.html',
            base_name + '_WebGLGreenTriangle_NonChromiumImage_AA_Alpha',
            test_rect=[0, 0, 300, 300],
            browser_args=non_chromium_image_args),
        PixelTestPage(
            'pixel_webgl_noaa_alpha.html',
            base_name + '_WebGLGreenTriangle_NonChromiumImage_NoAA_Alpha',
            test_rect=[0, 0, 300, 300],
            browser_args=non_chromium_image_args),
        PixelTestPage(
            'pixel_webgl_aa_noalpha.html',
            base_name + '_WebGLGreenTriangle_NonChromiumImage_AA_NoAlpha',
            test_rect=[0, 0, 300, 300],
            browser_args=non_chromium_image_args),
        PixelTestPage(
            'pixel_webgl_noaa_noalpha.html',
            base_name + '_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
            test_rect=[0, 0, 300, 300],
            browser_args=non_chromium_image_args),

        # On macOS, test CSS filter effects with and without the CA compositor.
        PixelTestPage(
            'filter_effects.html',
            base_name + '_CSSFilterEffects',
            test_rect=[0, 0, 300, 300],
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=10,
                pixel_delta_threshold=5,
                edge_threshold=245)),
        PixelTestPage(
            'filter_effects.html',
            base_name + '_CSSFilterEffects_NoOverlays',
            test_rect=[0, 0, 300, 300],
            tolerance=10,
            browser_args=no_overlays_args,
            matching_algorithm=algo.SobelMatchingAlgorithm(
                max_different_pixels=240,
                pixel_delta_threshold=5,
                edge_threshold=250)),

        # Test WebGL's premultipliedAlpha:false without the CA compositor.
        PixelTestPage(
            'pixel_webgl_premultiplied_alpha_false.html',
            base_name + '_WebGL_PremultipliedAlpha_False_NoOverlays',
            test_rect=[0, 0, 150, 150],
            browser_args=no_overlays_args,
            tolerance=3,
            expected_colors=[
                SCALE_FACTOR_OVERRIDES,
                {
                    'comment': 'brown',
                    'location': [1, 1],
                    'size': [148, 148],
                    # This is the color on an NVIDIA based MacBook Pro if the
                    # sRGB profile's applied correctly.
                    'color': [102, 77, 0],
                    # This is the color if it isn't.
                    # 'color': [101, 76, 12],
                },
            ]),
    ]

  # Pages that should be run only on dual-GPU MacBook Pros (at the
  # present time, anyway).
  @staticmethod
  def DualGPUMacSpecificPages(base_name):
    return [
        PixelTestPage(
            'pixel_webgl_high_to_low_power.html',
            base_name + '_WebGLHighToLowPower',
            test_rect=[0, 0, 300, 300],
            tolerance=3,
            expected_colors=[{
                'comment': 'solid green',
                'location': [100, 100],
                'size': [100, 100],
                'color': [0, 255, 0],
            }],
            optional_action='RunTestWithHighPerformanceTab'),
        PixelTestPage(
            'pixel_webgl_low_to_high_power.html',
            base_name + '_WebGLLowToHighPower',
            test_rect=[0, 0, 300, 300],
            tolerance=3,
            expected_colors=[{
                'comment': 'solid green',
                'location': [100, 100],
                'size': [100, 100],
                'color': [0, 255, 0],
            }],
            optional_action='RunLowToHighPowerTest'),
        PixelTestPage(
            'pixel_webgl_low_to_high_power_alpha_false.html',
            base_name + '_WebGLLowToHighPowerAlphaFalse',
            test_rect=[0, 0, 300, 300],
            tolerance=3,
            expected_colors=[{
                'comment': 'solid green',
                'location': [100, 100],
                'size': [100, 100],
                'color': [0, 255, 0],
            }],
            optional_action='RunLowToHighPowerTest'),
        PixelTestPage(
            'pixel_offscreen_canvas_ibrc_webgl_main.html',
            base_name + '_OffscreenCanvasIBRCWebGLHighPerfMain',
            test_rect=[0, 0, 300, 300],
            tolerance=3,
            expected_colors=[{
                'comment': 'solid green',
                'location': [100, 100],
                'size': [100, 100],
                'color': [0, 255, 0],
            }],
            optional_action='RunOffscreenCanvasIBRCWebGLHighPerfTest'),
        PixelTestPage(
            'pixel_offscreen_canvas_ibrc_webgl_worker.html',
            base_name + '_OffscreenCanvasIBRCWebGLHighPerfWorker',
            test_rect=[0, 0, 300, 300],
            tolerance=3,
            expected_colors=[{
                'comment': 'solid green',
                'location': [100, 100],
                'size': [100, 100],
                'color': [0, 255, 0],
            }],
            optional_action='RunOffscreenCanvasIBRCWebGLHighPerfTest'),
    ]

  @staticmethod
  def DirectCompositionPages(base_name):
    browser_args = [
        '--enable-direct-composition-video-overlays',
        # All bots are connected with a power source, however, we want to to
        # test with the code path that's enabled with battery power.
        '--disable_vp_scaling=1'
    ]
    browser_args_Nonroot = browser_args + [
        '--enable-features=DirectCompositionNonrootOverlays,' +
        'DirectCompositionUnderlays'
    ]
    browser_args_Complex = browser_args + [
        '--enable-features=DirectCompositionComplexOverlays,' +
        'DirectCompositionNonrootOverlays,' + 'DirectCompositionUnderlays'
    ]
    browser_args_YUY2 = browser_args + [
        '--disable-features=DirectCompositionPreferNV12Overlays'
    ]
    browser_args_DXVA = browser_args + ['--disable-features=D3D11VideoDecoder']

    return [
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_DirectComposition_Video_MP4',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_DirectComposition_Video_MP4_DXVA',
            browser_args=browser_args_DXVA,
            test_rect=[0, 0, 240, 135],
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4_fullsize.html',
            base_name + '_DirectComposition_Video_MP4_Fullsize',
            browser_args=browser_args,
            test_rect=[0, 0, 960, 540],
            other_args={'zero_copy': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_DirectComposition_Video_MP4_YUY2',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args_YUY2,
            other_args={'expect_yuy2': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_aspect_4x3.html',
            base_name + '_DirectComposition_Video_MP4_FourColors_Aspect_4x3',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_90.html',
            base_name + '_DirectComposition_Video_MP4_FourColors_Rot_90',
            test_rect=[0, 0, 270, 240],
            browser_args=browser_args,
            other_args={'video_is_rotated': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_180.html',
            base_name + '_DirectComposition_Video_MP4_FourColors_Rot_180',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            other_args={'video_is_rotated': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_mp4_four_colors_rot_270.html',
            base_name + '_DirectComposition_Video_MP4_FourColors_Rot_270',
            test_rect=[0, 0, 270, 240],
            browser_args=browser_args,
            other_args={'video_is_rotated': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_vp9.html',
            base_name + '_DirectComposition_Video_VP9',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_vp9.html',
            base_name + '_DirectComposition_Video_VP9_DXVA',
            browser_args=browser_args_DXVA,
            test_rect=[0, 0, 240, 135],
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_vp9_fullsize.html',
            base_name + '_DirectComposition_Video_VP9_Fullsize',
            test_rect=[0, 0, 960, 540],
            browser_args=browser_args,
            other_args={'zero_copy': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_vp9.html',
            base_name + '_DirectComposition_Video_VP9_YUY2',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args_YUY2,
            other_args={'expect_yuy2': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_vp9_i420a.html',
            base_name + '_DirectComposition_Video_VP9_I420A',
            test_rect=[0, 0, 240, 135],
            browser_args=browser_args,
            other_args={'no_overlay': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_underlay.html',
            base_name + '_DirectComposition_Underlay',
            test_rect=[0, 0, 240, 136],
            browser_args=browser_args,
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_underlay.html',
            base_name + '_DirectComposition_Underlay_DXVA',
            test_rect=[0, 0, 240, 136],
            browser_args=browser_args_DXVA,
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_underlay_fullsize.html',
            base_name + '_DirectComposition_Underlay_Fullsize',
            test_rect=[0, 0, 960, 540],
            browser_args=browser_args,
            other_args={'zero_copy': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_nonroot.html',
            base_name + '_DirectComposition_Nonroot',
            test_rect=[0, 0, 240, 136],
            browser_args=browser_args_Nonroot,
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage(
            'pixel_video_complex_overlays.html',
            base_name + '_DirectComposition_ComplexOverlays',
            test_rect=[0, 0, 240, 136],
            browser_args=browser_args_Complex,
            other_args={'video_is_rotated': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
        PixelTestPage('pixel_video_mp4_rounded_corner.html',
                      base_name + '_DirectComposition_Video_MP4_Rounded_Corner',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True}),
        PixelTestPage('pixel_video_backdrop_filter.html',
                      base_name + '_DirectComposition_Video_BackdropFilter',
                      test_rect=[0, 0, 240, 135],
                      browser_args=browser_args,
                      other_args={'no_overlay': True}),
        PixelTestPage(
            'pixel_video_mp4.html',
            base_name + '_DirectComposition_Video_Disable_Overlays',
            test_rect=[0, 0, 240, 135],
            browser_args=['--disable-direct-composition-video-overlays'],
            other_args={'no_overlay': True},
            # Part of the expected color migration, crbug.com/1078914.
            grace_period_end=datetime.date(2020, 5, 19),
            matching_algorithm=VERY_PERMISSIVE_SOBEL_ALGO),
    ]

  @staticmethod
  def HdrTestPages(base_name):
    return [
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBoxScrgbLinear',
            test_rect=[0, 0, 300, 300],
            browser_args=['--force-color-profile=scrgb-linear']),
        PixelTestPage(
            'pixel_canvas2d.html',
            base_name + '_Canvas2DRedBoxHdr10',
            test_rect=[0, 0, 300, 300],
            browser_args=['--force-color-profile=hdr10']),
    ]
