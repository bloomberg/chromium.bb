# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bash auto completion support.

Contains the special mode that returns lists of possible completions for the
current command line.
"""

import cr


def Complete(context):
  """Attempts to build a completion list for the current command line.

  COMP_WORD contains the word that is being completed, and COMP_CWORD has
  the index of that word on the command line.

  Args:
    context: The cr context object, of type cr.context.Context.
  """

  _ = context
  # TODO(iancottrell): support auto complete of more than just the command
  # try to parse the command line using parser
  print ' '.join(command.name for command in cr.Command.Plugins())
