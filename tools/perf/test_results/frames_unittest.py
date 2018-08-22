# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_results import frames


@unittest.skipIf(frames.pandas is None, 'pandas module not available')
class TestDataFrames(unittest.TestCase):
  def testBuildersDataFrame(self):
    sample_data = {
        'masters': [
            {
                'name': 'chromium.perf',
                'tests': {
                    'all_platforms_test': {
                        'builders': [
                            'my-mac-bot',
                            'my-linux-bot',
                            'my-android-bot'
                        ]
                    },
                    'desktop_test': {
                        'builders': [
                            'my-mac-bot',
                            'my-linux-bot'
                        ]
                    },
                    'mobile_test': {
                        'builders': [
                            'my-android-bot'
                        ]
                    }
                }
            },
            {
                'name': 'chromium.perf.fyi',
                'tests': {
                    'mobile_test': {
                        'builders': [
                            'my-new-android-bot'
                        ]
                    }
                }
            }
        ]
    }
    df = frames.BuildersDataFrame(sample_data)
    # Poke and check a few simple facts about our sample data.
    # There are two masters: chromium.perf, chromium.perf.fyi.
    self.assertItemsEqual(
        df['master'].unique(), ['chromium.perf', 'chromium.perf.fyi'])
    # The 'desktop_test' runs on desktop builders only.
    self.assertItemsEqual(
        df[df['test_type'] == 'desktop_test']['builder'].unique(),
        ['my-mac-bot', 'my-linux-bot'])
    # The 'mobile_test' runs on mobile builders only.
    self.assertItemsEqual(
        df[df['test_type'] == 'mobile_test']['builder'].unique(),
        ['my-android-bot', 'my-new-android-bot'])
    # The new android bot is on the chromium.perf.fyi waterfall.
    self.assertItemsEqual(
        df[df['builder'] == 'my-new-android-bot']['master'].unique(),
        ['chromium.perf.fyi'])
