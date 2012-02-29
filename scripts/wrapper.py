#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

# We want to use correct version of libraries even when executed through symlink.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))

if __name__ == '__main__':
  target = os.path.basename(sys.argv[0])
  # Compatibility for badly named scripts that can't yet be fixed.
  if target.endswith('.py'):
    target = target[:-3]
  target = 'chromite.scripts.%s' % target
  module = __import__(target)
  # __import__ gets us the root of the namespace import; walk our way up.
  for attr in target.split('.')[1:]:
    module = getattr(module, attr)

  if hasattr(module, 'main'):
    ret = module.main(sys.argv[1:])
  else:
    print >>sys.stderr, ("Internal error detected in wrapper.py: no main "
                         "functor found in module %r." % (target,))
    sys.exit(100)
  if ret is None:
    ret = 0
  sys.exit(ret)
