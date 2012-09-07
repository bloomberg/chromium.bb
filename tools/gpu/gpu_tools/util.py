# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import math

def RoundTo3DecimalPlaces(f):
  return math.floor(f * 1000) / 1000.0

def RoundTo1DecimalPlace(f):
  return math.floor(f * 10) / 10.0
