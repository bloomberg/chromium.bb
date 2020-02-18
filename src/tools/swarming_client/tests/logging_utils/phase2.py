#!/usr/bin/env python
# Copyright 2019 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import logging
import os
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
CLIENT_DIR = os.path.dirname(os.path.dirname(THIS_DIR))
sys.path.insert(0, CLIENT_DIR)
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party'))

from utils import logging_utils

dst = sys.argv[1]

# Simulate rotating the log.
logging_utils.prepare_logging(os.path.join(dst, 'shared.log'))
logging.info('Child1')
os.rename(os.path.join(dst, 'shared.log'), os.path.join(dst, 'shared.1.log'))
logging.info('Child2')
