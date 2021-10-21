# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys
import json

from gpu_tests import gpu_integration_test
from gpu_tests import path_util

html_path = os.path.join(path_util.GetChromiumSrcDir(), 'content', 'test',
                         'data', 'gpu', 'webcodecs')
data_path = os.path.join(path_util.GetChromiumSrcDir(), 'media', 'test', 'data')
four_colors_img_path = os.path.join(data_path, 'four-colors.y4m')

frame_sources = ["camera", "capture", "offscreen", "hw_decoder", "sw_decoder"]
codecs = ["avc1.42001E", "vp8", "vp09.00.10.08"]
accelerations = ["prefer-hardware", "prefer-software"]


class WebCodecsIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls):
    return 'webcodecs'

  @classmethod
  def GenerateGpuTests(cls, options):
    for source_type in frame_sources:
      yield ('WebCodecs_DrawImage_' + source_type, 'draw-image.html', ({
          "source_type":
          source_type
      }))
      yield ('WebCodecs_TexImage2d_' + source_type, 'tex-image-2d.html', ({
          "source_type":
          source_type
      }))
      yield ('WebCodecs_copyTo_' + source_type, 'copyTo.html', ({
          "source_type":
          source_type
      }))

    for codec in codecs:
      yield ('WebCodecs_EncodeDecode_' + codec, 'encode-decode.html', ({
          "codec":
          codec
      }))

    for source_type in frame_sources:
      for codec in codecs:
        for acc in accelerations:
          args = (source_type, codec, acc)
          yield ('WebCodecs_Encode_%s_%s_%s' % args, 'encode.html', ({
              "source_type":
              source_type,
              "codec":
              codec,
              "acceleration":
              acc
          }))

    for codec in codecs:
      for acc in accelerations:
        args = ("camera", codec, acc)
        yield ('WebCodecs_Realtime_%s_%s_%s' % args, 'realtime.html', ({
            "source_type":
            source_type,
            "codec":
            codec,
            "acceleration":
            acc
        }))

    for codec in codecs:
      for layers in [2, 3]:
        args = (codec, layers)
        yield ('WebCodecs_SVC_%s_layers_%d' % args, 'svc.html', ({
            "codec":
            codec,
            "layers":
            layers
        }))

  def RunActualGpuTest(self, test_path, *args):
    url = self.UrlOfStaticFilePath(html_path + '/' + test_path)
    tab = self.tab
    arg_obj = args[0]
    os_name = self.platform.GetOSName()
    arg_obj["validate_camera_frames"] = self.CameraCanShowFourColors(os_name)
    tab.Navigate(url)
    tab.action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')
    tab.EvaluateJavaScript('TEST.run(' + json.dumps(arg_obj) + ')')
    tab.action_runner.WaitForJavaScriptCondition('TEST.finished', timeout=60)
    if not tab.EvaluateJavaScript('TEST.success'):
      self.fail('Test failure:' + tab.EvaluateJavaScript('TEST.summary()'))

  @staticmethod
  def CameraCanShowFourColors(os_name):
    return os_name != 'android' and os_name != 'chromeos'

  @classmethod
  def SetUpProcess(cls):
    super(WebCodecsIntegrationTest, cls).SetUpProcess()
    args = [
        '--use-fake-device-for-media-stream',
        '--use-fake-ui-for-media-stream',
    ]

    # If we don't call CustomizeBrowserArgs cls.platform is None
    cls.CustomizeBrowserArgs(args)
    platform = cls.platform

    if cls.CameraCanShowFourColors(platform.GetOSName()):
      args.append('--use-file-for-fake-video-capture=' + four_colors_img_path)
      cls.CustomizeBrowserArgs(args)

    cls.StartBrowser()
    cls.SetStaticServerDirs([html_path, data_path])

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webcodecs_expectations.txt')
    ]


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
