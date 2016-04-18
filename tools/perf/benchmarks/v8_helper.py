# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shlex


def EnableIgnition(options):
  existing_js_flags = []
  for extra_arg in options.extra_browser_args:
    if extra_arg.startswith('--js-flags='):
      existing_js_flags.extend(shlex.split(extra_arg[len('--js-flags='):]))
  options.AppendExtraBrowserArgs([
      # This overrides any existing --js-flags, hence we have to include the
      # previous flags as well.
      '--js-flags=--ignition %s' % (' '.join(existing_js_flags))
  ])
