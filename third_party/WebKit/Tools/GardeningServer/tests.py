#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest


_gae_sdk_not_on_python_path_message = '''
    You must include the google_appengine SDK directory on PYTHONPATH.
'''


_webtest_not_installed_message = '''
    Could not load webtest python module. You may need to:
    sudo apt-get python-webtest
'''


def main():
    try:
        import dev_appserver
    except ImportError:
        print >> sys.stderr, _gae_sdk_not_on_python_path_message
        raise

    dev_appserver.fix_sys_path()

    try:
        import webtest
    except ImportError:
        print >> sys.stderr, _webtest_not_installed_message
        raise

    tests_path = os.path.dirname(sys.modules[__name__].__file__)
    suite = unittest.loader.TestLoader().discover(tests_path,
                                                  pattern='*_test.py')
    unittest.TextTestRunner(verbosity=2).run(suite)


if __name__ == '__main__':
    main()
