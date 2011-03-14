# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite base class.

This module maintains information about the build environemnt, including
paths and defaults. It includes methods for querying the environemnt.

(For now this is just a placeholder to illustrate the concept)
"""

import os
import signal
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
  """
  def __init__(self):
    # We have at least a single overall operation always, so set it up.
    self._oper = operation.Operation('operation')

  def __del__(self):
    del self._oper

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
