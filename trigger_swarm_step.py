#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Temporary script to be deleted once the Try Server to be restarted."""

import sys

import swarm_trigger_step


if __name__ == '__main__':
  sys.exit(swarm_trigger_step.main())
