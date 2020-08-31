# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A wrapper for common operations on a Fuchsia device"""

import logging
import os
import subprocess

from telemetry.core import util

FUCHSIA_BROWSERS = ['web-engine-shell']
_LLVM_SYMBOLIZER_PATH = os.path.join(
    util.GetCatapultDir(), '..', 'llvm-build', 'Release+Asserts', 'bin',
    'llvm-symbolizer')


class CommandRunner(object):
  """Helper class used to execute commands on Fuchsia devices on a remote host
  over SSH."""

  def __init__(self, config_path, host, port):
    """Creates a CommandRunner that connects to the specified |host| and |port|
    using the ssh config at the specified |config_path|.

    Args:
      config_path: Full path to SSH configuration.
      host: The hostname or IP address of the remote host.
      port: The port to connect to."""
    self._config_path = config_path
    self._host = host
    self._port = port

  def _GetSshCommandLinePrefix(self):
    return ['ssh', '-F', self._config_path, self._host, '-p', str(self._port)]

  def RunCommandPiped(self, command=None, ssh_args=None, **kwargs):
    """Executes an SSH command on the remote host and returns a process object
    with access to the command's stdio streams. Does not block.

    Args:
      command: A list of strings containing the command and its arguments.
      ssh_args: Arguments that will be passed to SSH.
      kwargs: A dictionary of parameters to be passed to subprocess.Popen().
          The parameters can be used to override stdin and stdout, for example.

    Returns:
      A subprocess.Popen object for the command."""
    if not command:
      command = []
    if not ssh_args:
      ssh_args = []

    # Having control master causes weird behavior in port_forwarding.
    ssh_args.append('-oControlMaster=no')
    ssh_command = self._GetSshCommandLinePrefix() + ssh_args + ['--'] + command
    logging.debug(' '.join(ssh_command))
    return subprocess.Popen(ssh_command, **kwargs)


def StartSymbolizerForProcessIfPossible(input_file, output_file, build_id_file):
  """Starts an llvm symbolizer process if possible.

    Args:
      input_file: Input file to be symbolized.
      output_file: Output file for symbolizer stdout and stderr.
      build_id_file: Path to the ids.txt file which maps build IDs to
          unstripped binaries on the filesystem.

    Returns:
      A subprocess.Popen object for the started process, None if symbolizer
      fails to start."""
  if (os.path.isfile(_LLVM_SYMBOLIZER_PATH) and
      os.path.isfile(build_id_file)):
    sdk_root = os.path.join(util.GetCatapultDir(), '..', 'fuchsia-sdk', 'sdk')
    symbolizer = os.path.join(sdk_root, 'tools', 'x64', 'symbolize')
    symbolizer_cmd = [symbolizer,
                      '-ids-rel', '-llvm-symbolizer', _LLVM_SYMBOLIZER_PATH,
                      '-build-id-dir', os.path.join(sdk_root, '.build-id')]
    symbolizer_cmd.extend(['-ids', build_id_file])

    logging.debug('Running "%s".' % ' '.join(symbolizer_cmd))
    return subprocess.Popen(symbolizer_cmd,
                            stdin=input_file,
                            stdout=output_file,
                            stderr=subprocess.STDOUT,
                            close_fds=True)
  else:
    logging.info('Symbolizer cannot be started.')
    return None
