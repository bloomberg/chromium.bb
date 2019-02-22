#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging


def VerboseLevel(count):
  if count == 0:
    return logging.WARNING
  elif count == 1:
    return logging.INFO
  else:
    return logging.DEBUG


def ConfigureLogging(verbose_count):
  logging.basicConfig(level=VerboseLevel(verbose_count))
