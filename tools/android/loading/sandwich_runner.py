# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
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

# Memory dump category used to get memory metrics.
MEMORY_DUMP_CATEGORY = 'disabled-by-default-memory-infra'

_JOB_SEARCH_PATH = 'sandwich_jobs'

# Devtools timeout of 1 minute to avoid websocket timeout on slow
# network condition.
_DEVTOOLS_TIMEOUT = 60


def _ReadUrlsFromJobDescription(job_name):
  """Retrieves the list of URLs associated with the job name."""
  try:
    # Extra sugar: attempt to load from a relative path.
    json_file_name = os.path.join(os.path.dirname(__file__), _JOB_SEARCH_PATH,
        job_name)
    with open(json_file_name) as f:
      json_data = json.load(f)
  except IOError:
    # Attempt to read by regular file name.
    with open(job_name) as f:
      json_data = json.load(f)

  key = 'urls'
  if json_data and key in json_data:
    url_list = json_data[key]
    if isinstance(url_list, list) and len(url_list) > 0:
      return url_list
  raise Exception('Job description does not define a list named "urls"')


def _CleanPreviousTraces(output_directories_path):
  """Cleans previous traces from the output directory.

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


class SandwichRunner(object):
  """Sandwich runner.

  This object is meant to be configured first and then run using the Run()
  method. The runner can configure itself conveniently with parsed arguement
  using the PullConfigFromArgs() method. The only job is to make sure that the
  command line flags have `dest` parameter set to existing runner members.
  """

  def __init__(self):
    """Configures a sandwich runner out of the box.

    Public members are meant to be configured as wished before calling Run().

    Args:
      job_name: The job name to get the associated urls.
    """
    # Cache operation to do before doing the chrome navigation.
    #   Can be: clear,save,push,reload
    self.cache_operation = 'clear'

    # The cache archive's path to save to or push from. Is str or None.
    self.cache_archive_path = None

    # Controls whether the WPR server should do script injection.
    self.disable_wpr_script_injection = False

    # The job name. Is str.
    self.job_name = '__unknown_job'

    # Number of times to repeat the job.
    self.job_repeat = 1

    # Network conditions to emulate. None if no emulation.
    self.network_condition = None

    # Network condition emulator. Can be: browser,wpr
    self.network_emulator = 'browser'

    # Output directory where to save the traces. Is str or None.
    self.trace_output_directory = None

    # List of urls to run.
    self.urls = []

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

  def LoadJob(self, job_name):
    self.job_name = job_name
    self.urls = _ReadUrlsFromJobDescription(job_name)

  def PullConfigFromArgs(self, args):
    """Configures the sandwich runner from parsed command line argument.

    Args:
      args: The command line parsed argument.
    """
    for config_name in self.__dict__.keys():
      if config_name in args.__dict__:
        self.__dict__[config_name] = args.__dict__[config_name]

  def PrintConfig(self):
    """Print the current sandwich runner configuration to stdout. """
    for config_name in sorted(self.__dict__.keys()):
      if config_name[0] != '_':
        print '{} = {}'.format(config_name, self.__dict__[config_name])

  def _CleanTraceOutputDirectory(self):
    assert self.trace_output_directory
    if not os.path.isdir(self.trace_output_directory):
      try:
        os.makedirs(self.trace_output_directory)
      except OSError:
        logging.error('Cannot create directory for results: %s',
            self.trace_output_directory)
        raise
    else:
      _CleanPreviousTraces(self.trace_output_directory)

  def _GetEmulatorNetworkCondition(self, emulator):
    if self.network_emulator == emulator:
      return self.network_condition
    return None

  def _RunNavigation(self, url, clear_cache, run_id=None):
    """Run a page navigation to the given URL.

    Args:
      url: The URL to navigate to.
      clear_cache: Whether if the cache should be cleared before navigation.
      run_id: Id of the run in the output directory. If it is None, then no
        trace or video will be saved.
    """
    run_path = None
    if self.trace_output_directory is not None and run_id is not None:
      run_path = os.path.join(self.trace_output_directory, str(run_id))
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
              url=url,
              connection=connection,
              chrome_metadata=self._chrome_ctl.ChromeMetadata(),
              additional_categories=additional_categories,
              timeout_seconds=_DEVTOOLS_TIMEOUT)
      else:
        trace = loading_trace.LoadingTrace.RecordUrlNavigation(
            url=url,
            connection=connection,
            chrome_metadata=self._chrome_ctl.ChromeMetadata(),
            additional_categories=additional_categories,
            timeout_seconds=_DEVTOOLS_TIMEOUT)
    if run_path is not None:
      trace_path = os.path.join(run_path, TRACE_FILENAME)
      trace.ToJsonFile(trace_path)

  def _RunUrl(self, url, run_id):
    clear_cache = False
    if self.cache_operation == 'clear':
      clear_cache = True
    elif self.cache_operation == 'push':
      self._chrome_ctl.PushBrowserCache(self._local_cache_directory_path)
    elif self.cache_operation == 'reload':
      self._RunNavigation(url, clear_cache=True)
    elif self.cache_operation == 'save':
      clear_cache = run_id == 0
    self._RunNavigation(url, clear_cache=clear_cache, run_id=run_id)

  def _PullCacheFromDevice(self):
    assert self.cache_operation == 'save'
    assert self.cache_archive_path, 'Need to specify where to save the cache'

    cache_directory_path = self._chrome_ctl.PullBrowserCache()
    chrome_cache.ZipDirectoryContent(
        cache_directory_path, self.cache_archive_path)
    shutil.rmtree(cache_directory_path)

  def Run(self):
    """SandwichRunner main entry point meant to be called once configured."""
    assert self._chrome_ctl == None
    assert self._local_cache_directory_path == None
    if self.trace_output_directory:
      self._CleanTraceOutputDirectory()

    if self.android_device:
      self._chrome_ctl = controller.RemoteChromeController(self.android_device)
    else:
      self._chrome_ctl = controller.LocalChromeController()
    self._chrome_ctl.AddChromeArgument('--disable-infobars')
    if self.cache_operation == 'save':
      self._chrome_ctl.SetSlowDeath()

    if self.cache_operation == 'push':
      assert os.path.isfile(self.cache_archive_path)
      self._local_cache_directory_path = tempfile.mkdtemp(suffix='.cache')
      chrome_cache.UnzipDirectoryContent(
          self.cache_archive_path, self._local_cache_directory_path)

    ran_urls = []
    with self._chrome_ctl.OpenWprHost(self.wpr_archive_path,
        record=self.wpr_record,
        network_condition_name=self._GetEmulatorNetworkCondition('wpr'),
        disable_script_injection=self.disable_wpr_script_injection
        ):
      for _ in xrange(self.job_repeat):
        for url in self.urls:
          self._RunUrl(url, run_id=len(ran_urls))
          ran_urls.append(url)

    if self._local_cache_directory_path:
      shutil.rmtree(self._local_cache_directory_path)
      self._local_cache_directory_path = None
    if self.cache_operation == 'save':
      self._PullCacheFromDevice()

    self._chrome_ctl = None
