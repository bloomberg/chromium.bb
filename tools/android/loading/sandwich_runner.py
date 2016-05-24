# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import sys
import tempfile

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

import chrome_cache
import controller
import devtools_monitor
import device_setup
import loading_trace


# Standard filenames in the sandwich runner's output directory.
TRACE_FILENAME = 'trace.json'
VIDEO_FILENAME = 'video.mp4'
WPR_LOG_FILENAME = 'wpr.log'

# Memory dump category used to get memory metrics.
MEMORY_DUMP_CATEGORY = 'disabled-by-default-memory-infra'

# Devtools timeout of 1 minute to avoid websocket timeout on slow
# network condition.
_DEVTOOLS_TIMEOUT = 60


def _CleanArtefactsFromPastRuns(output_directories_path):
  """Cleans artifacts generated from past run in the output directory.

  Args:
    output_directories_path: The output directory path where to clean the
        previous traces.
  """
  for dirname in os.listdir(output_directories_path):
    directory_path = os.path.join(output_directories_path, dirname)
    if not os.path.isdir(directory_path):
      continue
    try:
      int(dirname)
    except ValueError:
      continue
    shutil.rmtree(directory_path)


class CacheOperation(object):
  CLEAR, SAVE, PUSH = range(3)


class SandwichRunner(object):
  """Sandwich runner.

  This object is meant to be configured first and then run using the Run()
  method.
  """
  _ATTEMPT_COUNT = 3

  def __init__(self):
    """Configures a sandwich runner out of the box.

    Public members are meant to be configured before calling Run().
    """
    # Cache operation to do before doing the chrome navigation.
    self.cache_operation = CacheOperation.CLEAR

    # The cache archive's path to save to or push from. Is str or None.
    self.cache_archive_path = None

    # Controls whether the WPR server should do script injection.
    self.disable_wpr_script_injection = False

    # Number of times to repeat the url.
    self.repeat = 1

    # Network conditions to emulate. None if no emulation.
    self.network_condition = None

    # Network condition emulator. Can be: browser,wpr
    self.network_emulator = 'browser'

    # Output directory where to save the traces, videos, etc. Is str or None.
    self.output_dir = None

    # URL to navigate to.
    self.url = None

    # Configures whether to record speed-index video.
    self.record_video = False

    # Configures whether to record memory dumps.
    self.record_memory_dumps = False

    # Path to the WPR archive to load or save. Is str or None.
    self.wpr_archive_path = None

    # Configures whether the WPR archive should be read or generated.
    self.wpr_record = False

    # The android DeviceUtils to run sandwich on or None to run it locally.
    self.android_device = None

    self._chrome_ctl = None
    self._local_cache_directory_path = None

  def _CleanTraceOutputDirectory(self):
    assert self.output_dir
    if not os.path.isdir(self.output_dir):
      try:
        os.makedirs(self.output_dir)
      except OSError:
        logging.error('Cannot create directory for results: %s',
            self.output_dir)
        raise
    else:
      _CleanArtefactsFromPastRuns(self.output_dir)

  def _GetEmulatorNetworkCondition(self, emulator):
    if self.network_emulator == emulator:
      return self.network_condition
    return None

  def _RunNavigation(self, clear_cache, run_id=None):
    """Run a page navigation to the given URL.

    Args:
      clear_cache: Whether if the cache should be cleared before navigation.
      run_id: Id of the run in the output directory. If it is None, then no
        trace or video will be saved.
    """
    run_path = None
    if self.output_dir is not None and run_id is not None:
      run_path = os.path.join(self.output_dir, str(run_id))
      if not os.path.isdir(run_path):
        os.makedirs(run_path)
    self._chrome_ctl.SetNetworkEmulation(
        self._GetEmulatorNetworkCondition('browser'))
    additional_categories = []
    if self.record_memory_dumps:
      additional_categories = [MEMORY_DUMP_CATEGORY]
    # TODO(gabadie): add a way to avoid recording a trace.
    with self._chrome_ctl.Open() as connection:
      if clear_cache:
        connection.ClearCache()
      if run_path is not None and self.record_video:
        device = self._chrome_ctl.GetDevice()
        if device is None:
          raise RuntimeError('Can only record video on a remote device.')
        video_recording_path = os.path.join(run_path, VIDEO_FILENAME)
        with device_setup.RemoteSpeedIndexRecorder(device, connection,
                                                   video_recording_path):
          trace = loading_trace.LoadingTrace.RecordUrlNavigation(
              url=self.url,
              connection=connection,
              chrome_metadata=self._chrome_ctl.ChromeMetadata(),
              additional_categories=additional_categories,
              timeout_seconds=_DEVTOOLS_TIMEOUT)
      else:
        trace = loading_trace.LoadingTrace.RecordUrlNavigation(
            url=self.url,
            connection=connection,
            chrome_metadata=self._chrome_ctl.ChromeMetadata(),
            additional_categories=additional_categories,
            timeout_seconds=_DEVTOOLS_TIMEOUT)
    if run_path is not None:
      trace_path = os.path.join(run_path, TRACE_FILENAME)
      trace.ToJsonFile(trace_path)

  def _RunUrl(self, run_id):
    for attempt_id in xrange(self._ATTEMPT_COUNT):
      try:
        self._chrome_ctl.ResetBrowserState()
        clear_cache = False
        if self.cache_operation == CacheOperation.CLEAR:
          clear_cache = True
        elif self.cache_operation == CacheOperation.PUSH:
          self._chrome_ctl.PushBrowserCache(self._local_cache_directory_path)
        elif self.cache_operation == CacheOperation.SAVE:
          clear_cache = run_id == 0
        self._RunNavigation(clear_cache=clear_cache, run_id=run_id)
        break
      except controller.ChromeControllerError as error:
        if not error.IsIntermittent():
          raise
        if self.output_dir is not None:
          dump_path = os.path.join(self.output_dir, str(run_id),
                                   'error{}'.format(attempt_id))
          with open(dump_path, 'w') as dump_output:
            error.Dump(dump_output)
    else:
      logging.error('Failed to navigate to %s after %d attemps' % \
                    (self.url, self._ATTEMPT_COUNT))
      raise

  def _PullCacheFromDevice(self):
    assert self.cache_operation == CacheOperation.SAVE
    assert self.cache_archive_path, 'Need to specify where to save the cache'

    cache_directory_path = self._chrome_ctl.PullBrowserCache()
    chrome_cache.ZipDirectoryContent(
        cache_directory_path, self.cache_archive_path)
    shutil.rmtree(cache_directory_path)

  def Run(self):
    """SandwichRunner main entry point meant to be called once configured."""
    assert self._chrome_ctl == None
    assert self._local_cache_directory_path == None
    if self.output_dir:
      self._CleanTraceOutputDirectory()

    if self.android_device:
      self._chrome_ctl = controller.RemoteChromeController(self.android_device)
    else:
      self._chrome_ctl = controller.LocalChromeController()
    self._chrome_ctl.AddChromeArgument('--disable-infobars')
    if self.cache_operation == CacheOperation.SAVE:
      self._chrome_ctl.SetSlowDeath()

    wpr_log_path = None
    if self.output_dir:
      wpr_log_path = os.path.join(self.output_dir, WPR_LOG_FILENAME)

    try:
      if self.cache_operation == CacheOperation.PUSH:
        assert os.path.isfile(self.cache_archive_path)
        self._local_cache_directory_path = tempfile.mkdtemp(suffix='.cache')
        chrome_cache.UnzipDirectoryContent(
            self.cache_archive_path, self._local_cache_directory_path)

      with self._chrome_ctl.OpenWprHost(self.wpr_archive_path,
          record=self.wpr_record,
          network_condition_name=self._GetEmulatorNetworkCondition('wpr'),
          disable_script_injection=self.disable_wpr_script_injection,
          out_log_path=wpr_log_path):
        for repeat_id in xrange(self.repeat):
          self._RunUrl(run_id=repeat_id)
    finally:
      if self._local_cache_directory_path:
        shutil.rmtree(self._local_cache_directory_path)
        self._local_cache_directory_path = None
    if self.cache_operation == CacheOperation.SAVE:
      self._PullCacheFromDevice()

    self._chrome_ctl = None
