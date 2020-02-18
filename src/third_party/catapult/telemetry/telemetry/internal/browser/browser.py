# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from py_utils import cloud_storage
from py_utils import exc_util

from telemetry.core import exceptions
from telemetry import decorators
from telemetry.internal import app
from telemetry.internal.backends import browser_backend
from telemetry.internal.backends.chrome_inspector import tracing_backend
from telemetry.internal.browser import extension_dict
from telemetry.internal.browser import tab_list
from telemetry.internal.browser import web_contents
from telemetry.testing import test_utils


class Browser(app.App):
  """A running browser instance that can be controlled in a limited way.

  To create a browser instance, use browser_finder.FindBrowser, e.g:

    possible_browser = browser_finder.FindBrowser(finder_options)
    with possible_browser.BrowserSession(
        finder_options.browser_options) as browser:
      # Do all your operations on browser here.

  See telemetry.internal.browser.possible_browser for more details and use
  cases.
  """
  def __init__(self, backend, platform_backend, startup_args,
               find_existing=False):
    super(Browser, self).__init__(app_backend=backend,
                                  platform_backend=platform_backend)
    try:
      self._browser_backend = backend
      self._platform_backend = platform_backend
      self._tabs = tab_list.TabList(backend.tab_list_backend)
      self._browser_backend.SetBrowser(self)
      if find_existing:
        self._browser_backend.BindDevToolsClient()
      else:
        self._browser_backend.Start(startup_args)
      self._LogBrowserInfo()
    except Exception:
      self.DumpStateUponFailure()
      self.Close()
      raise

  @property
  def browser_type(self):
    return self.app_type

  @property
  def supports_extensions(self):
    return self._browser_backend.supports_extensions

  @property
  def supports_tab_control(self):
    return self._browser_backend.supports_tab_control

  @property
  def tabs(self):
    return self._tabs

  @property
  def foreground_tab(self):
    for i in xrange(len(self._tabs)):
      # The foreground tab is the first (only) one that isn't hidden.
      # This only works through luck on Android, due to crbug.com/322544
      # which means that tabs that have never been in the foreground return
      # document.hidden as false; however in current code the Android foreground
      # tab is always tab 0, which will be the first one that isn't hidden
      if self._tabs[i].EvaluateJavaScript('!document.hidden'):
        return self._tabs[i]
    raise exceptions.TabMissingError("No foreground tab found")

  @property
  @decorators.Cache
  def extensions(self):
    if not self.supports_extensions:
      raise browser_backend.ExtensionsNotSupportedException(
          'Extensions not supported')
    return extension_dict.ExtensionDict(self._browser_backend.extension_backend)

  def GetTypExpectationsTags(self):
    tags = self.platform.GetTypExpectationsTags()
    return tags + test_utils.sanitizeTypExpectationsTags([self.browser_type])

  def _LogBrowserInfo(self):
    trim_logs = self._browser_backend.browser_options.trim_logs
    logs = []
    logs.append(' Browser started (pid=%s).' % self._browser_backend.GetPid())
    logs.append(' OS: %s %s' % (
        self._platform_backend.platform.GetOSName(),
        self._platform_backend.platform.GetOSVersionName()))
    os_detail = self._platform_backend.platform.GetOSVersionDetailString()
    if os_detail:
      logs.append(' Detailed OS version: %s' % os_detail)
    system_info = self.GetSystemInfo()
    if system_info:
      if system_info.model_name:
        logs.append(' Model: %s' % system_info.model_name)
      if not trim_logs:
        if system_info.command_line:
          logs.append(' Browser command line: %s' % system_info.command_line)
      if system_info.gpu:
        for i, device in enumerate(system_info.gpu.devices):
          logs.append(' GPU device %d: %s' % (i, device))
        if not trim_logs:
          if system_info.gpu.aux_attributes:
            logs.append(' GPU Attributes:')
            for k, v in sorted(system_info.gpu.aux_attributes.iteritems()):
              logs.append('  %-20s: %s' % (k, v))
          if system_info.gpu.feature_status:
            logs.append(' Feature Status:')
            for k, v in sorted(system_info.gpu.feature_status.iteritems()):
              logs.append('  %-20s: %s' % (k, v))
          if system_info.gpu.driver_bug_workarounds:
            logs.append(' Driver Bug Workarounds:')
            for workaround in system_info.gpu.driver_bug_workarounds:
              logs.append('  %s' % workaround)
      else:
        logs.append(' No GPU devices')
    else:
      logging.warning('System info not supported')
    logging.info('Browser information:\n%s', '\n'.join(logs))

  @exc_util.BestEffort
  def Close(self):
    """Closes this browser."""
    try:
      if self._browser_backend.IsBrowserRunning():
        logging.info(
            'Closing browser (pid=%s) ...', self._browser_backend.GetPid())

      if self._browser_backend.supports_uploading_logs:
        try:
          self._browser_backend.UploadLogsToCloudStorage()
        except cloud_storage.CloudStorageError as e:
          logging.error('Cannot upload browser log: %s' % str(e))
    finally:
      self._browser_backend.Close()
      if self._browser_backend.IsBrowserRunning():
        logging.error(
            'Browser is still running (pid=%s).'
            , self._browser_backend.GetPid())
      else:
        logging.info('Browser is closed.')

  def Foreground(self):
    """Ensure the browser application is moved to the foreground."""
    return self._browser_backend.Foreground()

  def Background(self):
    """Ensure the browser application is moved to the background."""
    return self._browser_backend.Background()

  def GetStandardOutput(self):
    return self._browser_backend.GetStandardOutput()

  def GetLogFileContents(self):
    return self._browser_backend.GetLogFileContents()

  def GetStackTrace(self):
    return self._browser_backend.GetStackTrace()

  def GetMostRecentMinidumpPath(self):
    """Returns the path to the most recent minidump."""
    return self._browser_backend.GetMostRecentMinidumpPath()

  def GetAllMinidumpPaths(self):
    """Returns all minidump paths available in the backend."""
    return self._browser_backend.GetAllMinidumpPaths()

  def GetAllUnsymbolizedMinidumpPaths(self):
    """Returns paths to all minidumps that have not already been
    symbolized."""
    return self._browser_backend.GetAllUnsymbolizedMinidumpPaths()

  def SymbolizeMinidump(self, minidump_path):
    """Given a minidump path, this method returns a tuple with the
    first value being whether or not the minidump was able to be
    symbolized and the second being that symbolized dump when true
    and error message when false."""
    return self._browser_backend.SymbolizeMinidump(minidump_path)

  @property
  def supports_app_ui_interactions(self):
    """True if the browser supports Android app UI interactions."""
    return self._browser_backend.supports_app_ui_interactions

  def GetAppUi(self):
    """Returns an AppUi object to interact with app's UI.

       See devil.android.app_ui for the documentation of the API provided."""
    assert self.supports_app_ui_interactions
    return self._browser_backend.GetAppUi()

  def GetSystemInfo(self):
    """Returns low-level information about the system, if available.

       See the documentation of the SystemInfo class for more details."""
    return self._browser_backend.GetSystemInfo()

  @property
  def supports_memory_dumping(self):
    return self._browser_backend.supports_memory_dumping

  def DumpMemory(self, timeout=None):
    try:
      return self._browser_backend.DumpMemory(timeout=timeout)
    except tracing_backend.TracingUnrecoverableException:
      logging.exception('Failed to record memory dump due to exception:')
      # Re-raise as an AppCrashException to obtain further debug information
      # about the browser state.
      raise exceptions.AppCrashException(
          app=self, msg='Browser failed to record memory dump.')

  @property
  def supports_java_heap_garbage_collection( # pylint: disable=invalid-name
      self):
    return hasattr(self._browser_backend, 'ForceJavaHeapGarbageCollection')

  def ForceJavaHeapGarbageCollection(self):
    """Forces java heap GC on supported platforms."""
    return self._browser_backend.ForceJavaHeapGarbageCollection()

  @property
  # pylint: disable=invalid-name
  def supports_overriding_memory_pressure_notifications(self):
    # pylint: enable=invalid-name
    return (
        self._browser_backend.supports_overriding_memory_pressure_notifications)

  def SetMemoryPressureNotificationsSuppressed(
      self, suppressed, timeout=web_contents.DEFAULT_WEB_CONTENTS_TIMEOUT):
    self._browser_backend.SetMemoryPressureNotificationsSuppressed(
        suppressed, timeout)

  def SimulateMemoryPressureNotification(
      self, pressure_level, timeout=web_contents.DEFAULT_WEB_CONTENTS_TIMEOUT):
    self._browser_backend.SimulateMemoryPressureNotification(
        pressure_level, timeout)

  @property
  def supports_overview_mode(self): # pylint: disable=invalid-name
    return self._browser_backend.supports_overview_mode

  def EnterOverviewMode(
      self, timeout=web_contents.DEFAULT_WEB_CONTENTS_TIMEOUT):
    self._browser_backend.EnterOverviewMode(timeout)

  def ExitOverviewMode(
      self, timeout=web_contents.DEFAULT_WEB_CONTENTS_TIMEOUT):
    self._browser_backend.ExitOverviewMode(timeout)

  @property
  def supports_cpu_metrics(self):
    return self._browser_backend.supports_cpu_metrics

  @property
  def supports_memory_metrics(self):
    return self._browser_backend.supports_memory_metrics

  def CollectDebugData(self, log_level):
    """Attempts to symbolize all currently unsymbolized minidumps and log them.

    Platforms may override this to provide other crash information in addition
    to the symbolized minidumps.

    Args:
      log_level: The logging level to use from the logging module, e.g.
          logging.ERROR.
    """
    self._browser_backend.CollectDebugData(log_level)

  @exc_util.BestEffort
  def DumpStateUponFailure(self):
    logging.info('*************** BROWSER STANDARD OUTPUT ***************')
    try:
      logging.info(self.GetStandardOutput())
    except Exception: # pylint: disable=broad-except
      logging.exception('Failed to get browser standard output:')
    logging.info('*********** END OF BROWSER STANDARD OUTPUT ************')

    logging.info('********************* BROWSER LOG *********************')
    try:
      logging.info(self.GetLogFileContents())
    except Exception: # pylint: disable=broad-except
      logging.exception('Failed to get browser log:')
    logging.info('***************** END OF BROWSER LOG ******************')

    logging.info(
        '********************* SYMBOLIZED MINIDUMP *********************')
    try:
      self.CollectDebugData(logging.INFO)
    except Exception: # pylint: disable=broad-except
      logging.exception('Failed to get symbolized minidump:')
    logging.info(
        '***************** END OF SYMBOLIZED MINIDUMP ******************')
