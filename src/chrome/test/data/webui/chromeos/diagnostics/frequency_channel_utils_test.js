// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChannelBand} from 'chrome://diagnostics/diagnostics_types.js';
import {convertFrequencyToChannel, getFrequencyChannelBand} from 'chrome://diagnostics/frequency_channel_utils.js';

import {assertEquals} from '../../chai_assert.js';

export function frequencyChannelUtilsTestSuite() {
  test('ConvertFrequencyToChannel', () => {
    // Frequency not in map.
    assertEquals(null, convertFrequencyToChannel(0));
    assertEquals(null, convertFrequencyToChannel(2411));
    assertEquals(null, convertFrequencyToChannel(2413));
    assertEquals(null, convertFrequencyToChannel(2494));
    assertEquals(null, convertFrequencyToChannel(2496));
    assertEquals(null, convertFrequencyToChannel(5059));
    assertEquals(null, convertFrequencyToChannel(5061));
    assertEquals(null, convertFrequencyToChannel(5979));
    assertEquals(null, convertFrequencyToChannel(5981));
    // Frequency in map.
    assertEquals(1, convertFrequencyToChannel(2412));
    assertEquals(14, convertFrequencyToChannel(2484));
    assertEquals(32, convertFrequencyToChannel(5160));
    assertEquals(196, convertFrequencyToChannel(5980));
  });

  test('GetFrequencyChannelBand', () => {
    assertEquals(ChannelBand.UNKNOWN, getFrequencyChannelBand(0));
    assertEquals(ChannelBand.TWO_DOT_FOUR_GHZ, getFrequencyChannelBand(2412));
    assertEquals(ChannelBand.FIVE_GHZ, getFrequencyChannelBand(5160));
  });
}
