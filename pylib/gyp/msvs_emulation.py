# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module helps emulate Visual Studio 2008 behavior on top of other
build systems, primarily ninja.
"""

import re

windows_quoter_regex = re.compile(r'(\\*)"')

def QuoteCmdExeArgument(arg):
  """Quote a command line argument so that it appears as one argument when
  processed via cmd.exe and parsed by CommandLineToArgvW (as is typical for
  Windows programs)."""
  # See http://goo.gl/cuFbX and http://goo.gl/dhPnp including the comment
  # threads. This is actually the quoting rules for CommandLineToArgvW, not
  # for the shell, because the shell doesn't do anything in Windows. This
  # works more or less because most programs (including the compiler, etc.)
  # use that function to handle command line arguments.
  #
  # For a literal quote, CommandLineToArgvW requires 2n+1 backslashes
  # preceding it, and results in n backslashes + the quote. So we substitute
  # in 2* what we match, +1 more, plus the quote.
  tmp = windows_quoter_regex.sub(lambda mo: 2 * mo.group(1) + '\\"', arg)

  # Now, we need to escape some things that are actually for the shell.
  # ^-escape various characters that are otherwise interpreted by the shell.
  tmp = re.sub(r'([&|^])', r'^\1', tmp)

  # %'s also need to be doubled otherwise they're interpreted as batch
  # positional arguments. Also make sure to escape the % so that they're
  # passed literally through escaping so they can be singled to just the
  # original %. Otherwise, trying to pass the literal representation that
  # looks like an environment variable to the shell (e.g. %PATH%) would fail.
  tmp = tmp.replace('%', '^%^%')

  # Finally, wrap the whole thing in quotes so that the above quote rule
  # applies and whitespace isn't a word break.
  return '"' + tmp + '"'

def EncodeCmdExeList(args):
  """Process a list of arguments using QuoteCmdExeArgument."""
  return ' '.join(QuoteCmdExeArgument(arg) for arg in args)
