#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import json5_generator
import make_names
import media_feature_symbol


class MakeMediaFeatureNamesWriter(make_names.MakeNamesWriter):
    pass

MakeMediaFeatureNamesWriter.filters['symbol'] = media_feature_symbol.getMediaFeatureSymbolWithSuffix('MediaFeature')

if __name__ == "__main__":
    json5_generator.Maker(MakeMediaFeatureNamesWriter).main()
