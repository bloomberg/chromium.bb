# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants

class Options(object):
  """Global options repository.

  ParseArgs must be called before use. See _ARGS for common members, these will
  be available as instance attributes (eg, OPTIONS.clear_cache).
  """
  # Tuples of (argument name, default value, help string).
  _ARGS = [ ('clear_cache', True,
             'clear browser cache before loading'),
            ('chrome_package_name', 'chrome',
             'build/android/pylib/constants package description'),
            ('devtools_hostname', 'localhost',
             'hostname for devtools websocket connection'),
            ('devtools_port', 9222,
             'port for devtools websocket connection'),
            ('local_binary', 'out/Release/chrome',
             'chrome binary for local runs'),
            ('local_profile_dir', '',
             'profile directory to use for local runs'),
            ('no_sandbox', False,
             'pass --no-sandbox to browser (local run only; see also '
             'https://chromium.googlesource.com/chromium/src/+/master/'
             'docs/linux_suid_sandbox_development.md)'),
          ]


  def __init__(self):
    self._arg_set = set()
    self._parsed_args = None

  def AddGlobalArgument(self, arg_name, default, help_str):
    """Add a global argument.

    Args:
      arg_name: the name of the argument. This will be used as an optional --
        argument.
      default: the default value for the argument. The type of this default will
        be used as the type of the argument.
      help_str: the argument help string.
    """
    self._ARGS.append((arg_name, default, help_str))

  def ParseArgs(self, arg_str, description=None, extra=None):
    """Parse command line arguments.

    Args:
      arg_str: command line argument string.
      description: description to use in argument parser.
      extra: additional required arguments to add. These will be exposed as
        instance attributes. This is either a list of extra arguments, or a
        single string or tuple. If a tuple, the first item is the argument and
        the second a default, otherwise the argument is required. Arguments are
        used as in argparse, ie those beginning with -- are named, and those
        without a dash are positional. Don't use a single dash.
    """
    parser = argparse.ArgumentParser(description=description)
    for arg, default, help_str in self._ARGS:
      # All global options are named.
      arg = '--' + arg
      self._AddArg(parser, arg, default, help_str=help_str)
    if extra is not None:
      if type(extra) is not list:
        extra = [extra]
      for arg in extra:
        if type(arg) is tuple:
          argname, default = arg
          self._AddArg(parser, argname, default)
        else:
          self._AddArg(parser, arg, None, required=True)
    self._parsed_args = parser.parse_args(arg_str)

  def _AddArg(self, parser, arg, default, required=False, help_str=None):
    assert not arg.startswith('-') or arg.startswith('--'), \
        "Single dash arguments aren't supported: %s" % arg
    arg_name = arg
    if arg.startswith('--'):
      arg_name = arg[2:]
    assert arg_name not in self._arg_set, \
      '%s extra arg is a duplicate' % arg_name
    self._arg_set.add(arg_name)

    kwargs = {}
    if required and arg.startswith('--'):
      kwargs['required'] = required
    if help_str is not None:
      kwargs['help'] = help_str
    if default is not None:
      if type(default) is bool:
        # If the default of a switch is true, setting the flag stores false.
        if default:
          kwargs['action'] = 'store_false'
        else:
          kwargs['action'] = 'store_true'
      else:
        kwargs['default'] = default
        kwargs['type'] = type(default)

    parser.add_argument(arg, **kwargs)

  def __getattr__(self, name):
    if name in self._arg_set:
      assert self._parsed_args, 'Option requested before ParseArgs called'
      return getattr(self._parsed_args, name)
    raise AttributeError(name)

  def ChromePackage(self):
    return constants.PACKAGE_INFO[self.chrome_package_name]

OPTIONS = Options()
