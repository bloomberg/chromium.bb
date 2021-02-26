# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil
import stat
import tempfile

from py_utils import atexit_with_log

from telemetry.internal.platform.tracing_agent import chrome_tracing_agent

_DESKTOP_OS_NAMES = ['linux', 'mac', 'win']

# The trace config file path should be the same as specified in
# src/components/tracing/trace_config_file.[h|cc]
_CHROME_TRACE_CONFIG_DIR_ANDROID = '/data/local/'
_CHROME_TRACE_CONFIG_DIR_CROS = '/tmp/'
_CHROME_TRACE_CONFIG_FILE_NAME = 'chrome-trace-config.json'


def ClearStarupTracingStateIfNeeded(platform_backend):
  # Trace config file has fixed path on Android and temporary path on desktop.
  if platform_backend.GetOSName() == 'android':
    trace_config_file = os.path.join(_CHROME_TRACE_CONFIG_DIR_ANDROID,
                                     _CHROME_TRACE_CONFIG_FILE_NAME)
    platform_backend.device.RunShellCommand(
        ['rm', '-f', trace_config_file], check_return=True, as_root=True)


# A class that uses ReturnAsStream mode for chrome tracing.
class ChromeReturnAsStreamTracingAgent(chrome_tracing_agent.ChromeTracingAgent):
  def __init__(self, platform_backend, config):
    super(ChromeReturnAsStreamTracingAgent, self).__init__(
        platform_backend, config)

  @classmethod
  def IsSupported(cls, platform_backend):
    return platform_backend.GetOSName() != 'fuchsia'

  def _GetTransferMode(self):
    return 'ReturnAsStream'

  def _StartStartupTracing(self, config):
    self._CreateTraceConfigFile(config)
    logging.info('Created startup trace config file in: %s',
                 self._trace_config_file)
    return True

  def _CreateTraceConfigFileString(self, config):
    # See src/components/tracing/trace_config_file.h for the format
    result = {
        'trace_config':
        config.chrome_trace_config.GetChromeTraceConfigForStartupTracing()
    }
    return json.dumps(result, sort_keys=True)

  def _CreateTraceConfigFile(self, config):
    assert not self._trace_config_file
    os_name = self._platform_backend.GetOSName()
    if os_name == 'android':
      self._trace_config_file = os.path.join(_CHROME_TRACE_CONFIG_DIR_ANDROID,
                                             _CHROME_TRACE_CONFIG_FILE_NAME)
      self._platform_backend.device.WriteFile(
          self._trace_config_file,
          self._CreateTraceConfigFileString(config), as_root=True)
      # The config file has fixed path on Android. We need to ensure it is
      # always cleaned up.
      atexit_with_log.Register(self._RemoveTraceConfigFile)
    elif os_name == 'chromeos':
      self._trace_config_file = os.path.join(_CHROME_TRACE_CONFIG_DIR_CROS,
                                             _CHROME_TRACE_CONFIG_FILE_NAME)
      cri = self._platform_backend.cri
      cri.PushContents(self._CreateTraceConfigFileString(config),
                       self._trace_config_file)
      cri.Chown(self._trace_config_file)
      # The config file has fixed path on CrOS. We need to ensure it is
      # always cleaned up.
      atexit_with_log.Register(self._RemoveTraceConfigFile)
    elif os_name in _DESKTOP_OS_NAMES:
      self._trace_config_file = os.path.join(tempfile.mkdtemp(),
                                             _CHROME_TRACE_CONFIG_FILE_NAME)
      with open(self._trace_config_file, 'w') as f:
        trace_config_string = self._CreateTraceConfigFileString(config)
        logging.info('Trace config file string: %s', trace_config_string)
        f.write(trace_config_string)
      os.chmod(self._trace_config_file,
               os.stat(self._trace_config_file).st_mode | stat.S_IROTH)
    else:
      raise NotImplementedError('Tracing not supported on: %s' % os_name)

  def _RemoveTraceConfigFile(self):
    if not self._trace_config_file:
      return
    logging.info('Remove trace config file in %s', self._trace_config_file)
    if self._platform_backend.GetOSName() == 'android':
      self._platform_backend.device.RemovePath(
          self._trace_config_file, force=True, rename=True, as_root=True)
    elif self._platform_backend.GetOSName() == 'chromeos':
      self._platform_backend.cri.RmRF(self._trace_config_file)
    elif self._platform_backend.GetOSName() in _DESKTOP_OS_NAMES:
      if os.path.exists(self._trace_config_file):
        os.remove(self._trace_config_file)
      shutil.rmtree(os.path.dirname(self._trace_config_file))
    else:
      raise NotImplementedError
    self._trace_config_file = None
