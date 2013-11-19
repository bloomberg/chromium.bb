# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium cr tool main module.

Holds the main function and all it's support code.
"""

import os
import sys
import cr
import cr.auto.user
import cr.autocomplete
import cr.loader

_CONTACT = 'iancottrell@chromium.org'


def Main():
  """Chromium cr tool main function.

  This is the main entry point of the cr tool, it finds and loads all the
  plugins, creates the context and then activates and runs the specified
  command.
  """

  # Add the users plugin dir to the cr.auto.user package scan
  user_path = os.path.expanduser(os.path.join('~', '.config', 'cr'))
  cr.auto.user.__path__.append(user_path)

  cr.loader.Scan()

  # Build the command context
  context = cr.Context(
      description='The chrome dev build tool.',
      epilog='Contact ' + _CONTACT + ' if you have issues with this tool.',
  )
  # Install the sub-commands
  for command in cr.Command.Plugins():
    context.AddSubParser(command)

  # test for the special autocomplete command
  if context.autocompleting:
    # After plugins are loaded so pylint: disable=g-import-not-at-top
    cr.autocomplete.Complete(context)
    return
  # Speculative argument processing to add config specific args
  context.ParseArgs(True)
  cr.plugin.Activate(context)
  # At this point we should know what command we are going to use
  command = cr.Command.GetActivePlugin(context)
  # Do some early processing, in case it changes the build dir
  if command:
    command.EarlyArgProcessing(context)
  # Update the activated set again, in case the early processing changed it
  cr.plugin.Activate(context)
  # Load the build specific configuration
  found_build_dir = cr.base.client.LoadConfig(context)
  # Final processing or arguments
  context.ParseArgs()
  cr.plugin.Activate(context)
  # If we did not get a command before, it might have been fixed.
  if command is None:
    command = cr.Command.GetActivePlugin(context)
  # If the verbosity level is 3 or greater, then print the environment here
  if context.verbose >= 3:
    context.DumpValues(context.verbose > 3)
  if command is None:
    print context.Substitute('No command specified.')
    exit(1)
  if command.requires_build_dir:
    if not found_build_dir:
      if not context.Find('CR_OUT_FULL'):
        print context.Substitute(
            'No build directory specified. Please use cr init to make one.')
      else:
        print context.Substitute(
            'Build {CR_BUILD_DIR} not a valid build directory')
      exit(1)
    if context.Find('CR_VERSION') != cr.base.client.VERSION:
      print context.Substitute(
          'Build {CR_BUILD_DIR} is for the wrong version of cr')
      print 'Please run cr init to reset it'
      exit(1)
    cr.Platform.Prepare(context)
  if context.verbose >= 1:
    print context.Substitute(
        'Running cr ' + command.name + ' for {CR_BUILD_DIR}')
  # Invoke the given command
  command.Run(context)

if __name__ == '__main__':
  sys.exit(Main())
