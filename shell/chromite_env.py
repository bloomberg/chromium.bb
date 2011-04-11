# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite base class.

This module maintains information about the build environemnt, including
paths and defaults. It includes methods for querying the environemnt.

(For now this is just a placeholder to illustrate the concept)
"""

import imp
import os
import signal
import sys
import utils
from chromite.lib import operation
from chromite.lib import cros_subprocess


class ChromiteError(Exception):
  """A Chromite exception, such as a build error or missing file.

  We define this as an exception type so that we can report proper
  meaningful errors to upper layers rather than just the OS 'File not
  found' low level message.
  """
  pass


class ChromiteEnv:
  """Chromite environment class.

  This holds information about a Chromite environment, including its
  chroot, builds, images and so on. It is intended to understand the paths
  to use for each object, and provide methods to accessing and querying
  the various things in the environment.

  Variables:
    subcmds: A dictionary of sub-commands, with the key being the module name,
        and the value being the module.

        For each sub-command xxx there must exist
        chromite.shell.subcmds.xxx_cmd.py containing a class XxxCmd.
  """
  def __init__(self):
    # We have at least a single overall operation always, so set it up.
    self._oper = operation.Operation('operation')

    # Figure out where chromite sub-commands are located.
    self._subcmd_dir = os.path.join(os.path.dirname(__file__), 'subcmds')

    if not os.path.exists(self._subcmd_dir):
      raise ChromiteError('Cannot find sub-command directory')

    self.subcmds = {}
    self._ScanSubcmds()

  def __del__(self):
    del self._oper

  def _ScanSubcmds(self):
    """Scan for sub-commands that we can execute.

    Sets self.subcmds to a dictionary of module names, with the value of each
    being the module.
    """
    for file_name in os.listdir(self._subcmd_dir):
      if file_name.endswith('_cmd.py'):
        mod_name = os.path.splitext(file_name)[0]
        base_name = mod_name.rsplit('_', 1)[0]
        cls_name = '%sCmd' % base_name.title()

        mod = imp.load_module(mod_name,
            *imp.find_module(mod_name, [self._subcmd_dir]))
        cls = getattr(mod, cls_name)
        assert hasattr(cls, '__doc__'), \
              ('All handlers must have docstrings: %s' % name)
        self.subcmds[base_name] = cls

  def GetOperation(self):
    """Returns the current operation in progress
    """
    return self._oper

  def RunScript(self, name, cmd, args, env=None, shell_vars=None):
    """Run a legacy script in src/scripts.

    This is the only place in chromite where legacy scripts are invoked. We
    aim to replace all calls to this method with a proper python alternative
    complete with unit tests. For now this allows chromite to do its job.

    We'll ignore signal.SIGINT before calling the child.  This is the desired
    behavior if we know our child will handle Ctrl-C.  If we don't do this, I
    think we and the child will both get Ctrl-C at the same time, which means
    we'll forcefully kill the child.

    The specified command will be executed through the shell.

    We'll put CWD as src/scripts when running the commands, since many of these
    legacy commands live in src/scripts.

    This code has come from RunCommand but we have removed the so-far unused
    extra features like input, entering chroot and various controls.

    Also removed is argument handling. This function does not handle arguments
    with spaces in them. Also it does not escape any special characters that
    are passed in. The command is executed with 'sh -c ...' so the limitations
    of that must be kept in mind.

    Args:
      name: A name for the script, as might be displayed to the user.
      cmd: Command to run.  Should be input to subprocess.Popen.
      args: list of arguments to pass to command.
      env: If non-None, this is the environment for the new process.
      shell_vars: If non-None, this is the shell variables to define before
          running the script.

    Raises:
      ChromiteError(msg) if subprocess return code is not 0.
      Args:
        msg: Error message

    TODO(sjg): Can we remove shell_vars and require callers to use env?
        And _SplitEnvFromArgs already does this!
    """
    #TODO(sjg): Make printing the operation name optional
    self._oper.Outline(name)
    if shell_vars is None:
      shell_vars = ''

    # In verbose mode we don't try to separate stdout and stderr since we
    # can't be sure we will interleave output correctly for verbose
    # subprocesses.
    stderr = cros_subprocess.PIPE_PTY
    if self._oper.verbose:
      stderr = cros_subprocess.STDOUT

    child = cros_subprocess.Popen(
        ["sh", "-c", shell_vars + ' ' + cmd + ' ' + ' '.join(args)],
        stderr=stderr,
        env=env,
        cwd=os.path.join(utils.SRCROOT_PATH, 'src', 'scripts'))
    old_sigint = signal.signal(signal.SIGINT, signal.SIG_IGN)

    try:
      child.CommunicateFilter(self._oper.Output)
    finally:
      signal.signal(signal.SIGINT, old_sigint)

    if child.returncode != 0:
      msg = ("Error: command '%s' exited with error code %d" %
          (cmd, child.returncode))
      self._oper.Outline(msg)
      raise ChromiteError(msg)
    elif self._oper.WereErrorsDetected():
      self._oper.Outline("Error output detected, but command indicated "\
          "success.")

    self._oper.FinishOutput()
