# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import fieldtrial_util
import tempfile


class FieldTrialUtilUnittest(unittest.TestCase):

  def test_GenArgsEmptyPaths(self):
    args = fieldtrial_util.GenerateArgs('', '')
    self.assertEqual([], args)

  def test_GenArgsOneConfig(self):
    with tempfile.NamedTemporaryFile('w') as base_file:
      config = '''{
        "BrowserBlackList": [
          { "group_name": "Enabled" }
        ],
        "c": [
          {
            "group_name": "d.",
            "params": {"url": "http://www.google.com"}
          }
        ],
        "SimpleParams": [
          {
            "group_name": "Default",
            "params": {"id": "abc"}
          }
        ]
      }'''
      base_file.write(config)
      base_file.flush()

      result = fieldtrial_util.GenerateArgs(base_file.name)
      self.assertEqual(['--force-fieldtrials='
          'BrowserBlackList/Enabled/c/d./SimpleParams/Default',
          '--force-fieldtrial-params='
          'c.d%2E:url/http%3A%2F%2Fwww%2Egoogle%2Ecom,'
          'SimpleParams.Default:id/abc'], result)

  def test_GenArgsOverrideConfig(self):
    with tempfile.NamedTemporaryFile('w') as base_file:
      config = '''{
        "a": [
          { "group_name": "b" }
        ],
        "c": [
          { "group_name": "d" }
        ]
      }'''
      base_file.write(config)
      base_file.flush()
      with tempfile.NamedTemporaryFile('w') as platform_file:
        config = '''{
          "a": [
            { "group_name": "1" }
          ],
          "b": [
            { "group_name": "bgroup" }
          ]
        }'''
        platform_file.write(config)
        platform_file.flush()
        result = fieldtrial_util.GenerateArgs(base_file.name,
            platform_file.name)
        self.assertEqual(['--force-fieldtrials='
            'a/1/c/d/b/bgroup'], result)

if __name__ == '__main__':
  unittest.main()