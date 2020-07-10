#!/usr/bin/env python
# Copyright 2019 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import logging
import os
import socket
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
CLIENT_DIR = os.path.dirname(os.path.dirname(THIS_DIR))
sys.path.insert(0, CLIENT_DIR)
sys.path.insert(0, os.path.join(
    CLIENT_DIR, 'third_party', 'httplib2', 'python%d' % sys.version_info.major))
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party', 'pyasn1'))
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party', 'rsa'))
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party'))

from utils import on_error

# third_party/
from depot_tools import fix_encoding
import urllib3


# This is not useful here, since it's accessing a local host for the test.
urllib3.disable_warnings()


def run_shell_out(url, mode):
  # Enable 'report_on_exception_exit' even though main file is *_test.py.
  on_error._is_in_test = lambda: False

  # Hack it out so registering works.
  on_error._ENABLED_DOMAINS = (socket.getfqdn(),)

  # Don't try to authenticate into localhost.
  on_error.authenticators.OAuthAuthenticator = lambda *_: None

  if not on_error.report_on_exception_exit(url):
    print 'Failure to register the handler'
    return 83

  # Hack out certificate verification because we are using a self-signed
  # certificate here. In practice, the SSL certificate is signed to guard
  # against MITM attacks.
  on_error._SERVER.engine.session.verify = False

  if mode == 'crash':
    # Sadly, net is a bit overly verbose, which breaks
    # test_shell_out_crash_server_down.
    logging.error = lambda *_, **_kwargs: None
    logging.warning = lambda *_, **_kwargs: None
    raise ValueError('Oops')

  if mode == 'report':
    # Generate a manual report without an exception frame. Also set the version
    # value.
    setattr(sys.modules['__main__'], '__version__', '123')
    on_error.report('Oh dang')

  if mode == 'exception':
    # Report from inside an exception frame.
    try:
      raise TypeError('You are not my type')
    except TypeError:
      on_error.report('Really')

  if mode == 'exception_no_msg':
    # Report from inside an exception frame.
    try:
      raise TypeError('You are not my type #2')
    except TypeError:
      on_error.report(None)
  return 0


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  # pylint: disable=no-value-for-parameter
  sys.exit(run_shell_out(*sys.argv[1:]))
