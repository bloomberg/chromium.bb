# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium cr tool module.

This is the root module of all the cr code.
Commonly accessed elements, including all plugins, are promoted into this
module.
"""
import sys

from cr.config import Config
from cr.loader import AutoExport
import cr.plugin
