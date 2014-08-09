#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run Performance Test Bisect Tool

This script is used by a trybot to run the src/tools/bisect-perf-regression.py
script with the parameters specified in run-bisect-perf-regression.cfg. It will
check out a copy of the depot in a subdirectory 'bisect' of the working
directory provided, and run the bisect-perf-regression.py script there.

"""

import imp
import optparse
import os
import platform
import subprocess
import sys
import traceback

from auto_bisect import bisect_utils
from auto_bisect import math_utils

bisect = imp.load_source('bisect-perf-regression',
    os.path.join(os.path.abspath(os.path.dirname(sys.argv[0])),
        'bisect-perf-regression.py'))


CROS_BOARD_ENV = 'BISECT_CROS_BOARD'
CROS_IP_ENV = 'BISECT_CROS_IP'

# Default config file names.
BISECT_REGRESSION_CONFIG = 'run-bisect-perf-regression.cfg'
RUN_TEST_CONFIG = 'run-perf-test.cfg'
WEBKIT_RUN_TEST_CONFIG = os.path.join(
    '..', 'third_party', 'WebKit', 'Tools', 'run-perf-test.cfg')

class Goma(object):

  def __init__(self, path_to_goma):
    self._abs_path_to_goma = None
    self._abs_path_to_goma_file = None
    if not path_to_goma:
      return
    self._abs_path_to_goma = os.path.abspath(path_to_goma)
    filename = 'goma_ctl.bat' if os.name == 'nt' else 'goma_ctl.sh'
    self._abs_path_to_goma_file = os.path.join(self._abs_path_to_goma, filename)

  def __enter__(self):
    if self._HasGomaPath():
      self._SetupAndStart()
    return self

  def __exit__(self, *_):
    if self._HasGomaPath():
      self._Stop()

  def _HasGomaPath(self):
    return bool(self._abs_path_to_goma)

  def _SetupEnvVars(self):
    if os.name == 'nt':
      os.environ['CC'] = (os.path.join(self._abs_path_to_goma, 'gomacc.exe') +
          ' cl.exe')
      os.environ['CXX'] = (os.path.join(self._abs_path_to_goma, 'gomacc.exe') +
          ' cl.exe')
    else:
      os.environ['PATH'] = os.pathsep.join([self._abs_path_to_goma,
          os.environ['PATH']])

  def _SetupAndStart(self):
    """Sets up goma and launches it.

    Args:
      path_to_goma: Path to goma directory.

    Returns:
      True if successful."""
    self._SetupEnvVars()

    # Sometimes goma is lingering around if something went bad on a previous
    # run. Stop it before starting a new process. Can ignore the return code
    # since it will return an error if it wasn't running.
    self._Stop()

    if subprocess.call([self._abs_path_to_goma_file, 'start']):
      raise RuntimeError('Goma failed to start.')

  def _Stop(self):
    subprocess.call([self._abs_path_to_goma_file, 'stop'])


def _LoadConfigFile(config_file_path):
  """Attempts to load the specified config file as a module
  and grab the global config dict.

  Args:
    config_file_path: Path to the config file.

  Returns:
    If successful, returns the config dict loaded from the file. If no
    such dictionary could be loaded, returns the empty dictionary.
  """
  try:
    local_vars = {}
    execfile(config_file_path, local_vars)
    return local_vars['config']
  except Exception:
    print
    traceback.print_exc()
    print
    return {}


def _ValidateConfigFile(config_contents, valid_parameters):
  """Validates the config file contents, checking whether all values are
  non-empty.

  Args:
    config_contents: A config dictionary.
    valid_parameters: A list of parameters to check for.

  Returns:
    True if valid.
  """
  for parameter in valid_parameters:
    if parameter not in config_contents:
      return False
    value = config_contents[parameter]
    if not value or type(value) is not str:
      return False
  return True


def _ValidatePerfConfigFile(config_contents):
  """Validates the perf config file contents.

  This is used when we're doing a perf try job, rather than a bisect.
  The config file is called run-perf-test.cfg by default.

  The parameters checked are the required parameters; any additional optional
  parameters won't be checked and validation will still pass.

  Args:
    config_contents: A config dictionary.

  Returns:
    True if valid.
  """
  valid_parameters = [
      'command',
      'metric',
      'repeat_count',
      'truncate_percent',
      'max_time_minutes',
  ]
  return _ValidateConfigFile(config_contents, valid_parameters)


def _ValidateBisectConfigFile(config_contents):
  """Validates the bisect config file contents.

  The parameters checked are the required parameters; any additional optional
  parameters won't be checked and validation will still pass.

  Args:
    config_contents: A config dictionary.

  Returns:
    True if valid.
  """
  valid_params = [
      'command',
      'good_revision',
      'bad_revision',
      'metric',
      'repeat_count',
      'truncate_percent',
      'max_time_minutes',
  ]
  return _ValidateConfigFile(config_contents, valid_params)


def _OutputFailedResults(text_to_print):
  bisect_utils.OutputAnnotationStepStart('Results - Failed')
  print
  print text_to_print
  print
  bisect_utils.OutputAnnotationStepClosed()


def _CreateBisectOptionsFromConfig(config):
  print config['command']
  opts_dict = {}
  opts_dict['command'] = config['command']
  opts_dict['metric'] = config['metric']

  if config['repeat_count']:
    opts_dict['repeat_test_count'] = int(config['repeat_count'])

  if config['truncate_percent']:
    opts_dict['truncate_percent'] = int(config['truncate_percent'])

  if config['max_time_minutes']:
    opts_dict['max_time_minutes'] = int(config['max_time_minutes'])

  if config.has_key('use_goma'):
    opts_dict['use_goma'] = config['use_goma']
  if config.has_key('goma_dir'):
    opts_dict['goma_dir'] = config['goma_dir']

  opts_dict['build_preference'] = 'ninja'
  opts_dict['output_buildbot_annotations'] = True

  if '--browser=cros' in config['command']:
    opts_dict['target_platform'] = 'cros'

    if os.environ[CROS_BOARD_ENV] and os.environ[CROS_IP_ENV]:
      opts_dict['cros_board'] = os.environ[CROS_BOARD_ENV]
      opts_dict['cros_remote_ip'] = os.environ[CROS_IP_ENV]
    else:
      raise RuntimeError('Cros build selected, but BISECT_CROS_IP or'
          'BISECT_CROS_BOARD undefined.')
  elif 'android' in config['command']:
    if 'android-chrome-shell' in config['command']:
      opts_dict['target_platform'] = 'android'
    elif 'android-chrome' in config['command']:
      opts_dict['target_platform'] = 'android-chrome'
    else:
      opts_dict['target_platform'] = 'android'

  return bisect.BisectOptions.FromDict(opts_dict)


def _RunPerformanceTest(config, path_to_file):
  """Runs a performance test with and without the current patch.

  Args:
    config: Contents of the config file, a dictionary.
    path_to_file: Path to the bisect-perf-regression.py script.

  Attempts to build and run the current revision with and without the
  current patch, with the parameters passed in.
  """
  # Bisect script expects to be run from the src directory
  os.chdir(os.path.join(path_to_file, '..'))

  bisect_utils.OutputAnnotationStepStart('Building With Patch')

  opts = _CreateBisectOptionsFromConfig(config)
  b = bisect.BisectPerformanceMetrics(None, opts)

  if bisect_utils.RunGClient(['runhooks']):
    raise RuntimeError('Failed to run gclient runhooks')

  if not b.BuildCurrentRevision('chromium'):
    raise RuntimeError('Patched version failed to build.')

  bisect_utils.OutputAnnotationStepClosed()
  bisect_utils.OutputAnnotationStepStart('Running With Patch')

  results_with_patch = b.RunPerformanceTestAndParseResults(
      opts.command, opts.metric, reset_on_first_run=True, results_label='Patch')

  if results_with_patch[1]:
    raise RuntimeError('Patched version failed to run performance test.')

  bisect_utils.OutputAnnotationStepClosed()

  bisect_utils.OutputAnnotationStepStart('Reverting Patch')
  # TODO: When this is re-written to recipes, this should use bot_update's
  # revert mechanism to fully revert the client. But for now, since we know that
  # the perf trybot currently only supports src/ and src/third_party/WebKit, we
  # simply reset those two directories.
  bisect_utils.CheckRunGit(['reset', '--hard'])
  bisect_utils.CheckRunGit(['reset', '--hard'],
                           os.path.join('third_party', 'WebKit'))
  bisect_utils.OutputAnnotationStepClosed()

  bisect_utils.OutputAnnotationStepStart('Building Without Patch')

  if bisect_utils.RunGClient(['runhooks']):
    raise RuntimeError('Failed to run gclient runhooks')

  if not b.BuildCurrentRevision('chromium'):
    raise RuntimeError('Unpatched version failed to build.')

  bisect_utils.OutputAnnotationStepClosed()
  bisect_utils.OutputAnnotationStepStart('Running Without Patch')

  results_without_patch = b.RunPerformanceTestAndParseResults(
      opts.command, opts.metric, upload_on_last_run=True, results_label='ToT')

  if results_without_patch[1]:
    raise RuntimeError('Unpatched version failed to run performance test.')

  # Find the link to the cloud stored results file.
  output = results_without_patch[2]
  cloud_file_link = [t for t in output.splitlines()
      if 'storage.googleapis.com/chromium-telemetry/html-results/' in t]
  if cloud_file_link:
    # What we're getting here is basically "View online at http://..." so parse
    # out just the url portion.
    cloud_file_link = cloud_file_link[0]
    cloud_file_link = [t for t in cloud_file_link.split(' ')
        if 'storage.googleapis.com/chromium-telemetry/html-results/' in t]
    assert cloud_file_link, "Couldn't parse url from output."
    cloud_file_link = cloud_file_link[0]
  else:
    cloud_file_link = ''

  # Calculate the % difference in the means of the 2 runs.
  percent_diff_in_means = (results_with_patch[0]['mean'] /
      max(0.0001, results_without_patch[0]['mean'])) * 100.0 - 100.0
  std_err = math_utils.PooledStandardError(
      [results_with_patch[0]['values'], results_without_patch[0]['values']])

  bisect_utils.OutputAnnotationStepClosed()
  bisect_utils.OutputAnnotationStepStart('Results - %.02f +- %0.02f delta' %
      (percent_diff_in_means, std_err))
  print ' %s %s %s' % (''.center(10, ' '), 'Mean'.center(20, ' '),
      'Std. Error'.center(20, ' '))
  print ' %s %s %s' % ('Patch'.center(10, ' '),
      ('%.02f' % results_with_patch[0]['mean']).center(20, ' '),
      ('%.02f' % results_with_patch[0]['std_err']).center(20, ' '))
  print ' %s %s %s' % ('No Patch'.center(10, ' '),
      ('%.02f' % results_without_patch[0]['mean']).center(20, ' '),
      ('%.02f' % results_without_patch[0]['std_err']).center(20, ' '))
  if cloud_file_link:
    bisect_utils.OutputAnnotationStepLink('HTML Results', cloud_file_link)
  bisect_utils.OutputAnnotationStepClosed()


def _SetupAndRunPerformanceTest(config, path_to_file, path_to_goma):
  """Attempts to build and run the current revision with and without the
  current patch, with the parameters passed in.

  Args:
    config: The config read from run-perf-test.cfg.
    path_to_file: Path to the bisect-perf-regression.py script.
    path_to_goma: Path to goma directory.

  Returns:
    The exit code of bisect-perf-regression.py: 0 on success, otherwise 1.
  """
  try:
    with Goma(path_to_goma) as _:
      config['use_goma'] = bool(path_to_goma)
      config['goma_dir'] = os.path.abspath(path_to_goma)
      _RunPerformanceTest(config, path_to_file)
    return 0
  except RuntimeError, e:
    bisect_utils.OutputAnnotationStepClosed()
    _OutputFailedResults('Error: %s' % e.message)
    return 1


def _RunBisectionScript(
    config, working_directory, path_to_file, path_to_goma, path_to_extra_src,
    dry_run):
  """Attempts to execute bisect-perf-regression.py with the given parameters.

  Args:
    config: A dict containing the parameters to pass to the script.
    working_directory: A working directory to provide to the
      bisect-perf-regression.py script, where it will store it's own copy of
      the depot.
    path_to_file: Path to the bisect-perf-regression.py script.
    path_to_goma: Path to goma directory.
    path_to_extra_src: Path to extra source file.
    dry_run: Do a dry run, skipping sync, build, and performance testing steps.

  Returns:
    An exit status code: 0 on success, otherwise 1.
  """
  _PrintConfigStep(config)

  cmd = ['python', os.path.join(path_to_file, 'bisect-perf-regression.py'),
         '-c', config['command'],
         '-g', config['good_revision'],
         '-b', config['bad_revision'],
         '-m', config['metric'],
         '--working_directory', working_directory,
         '--output_buildbot_annotations']

  if config['repeat_count']:
    cmd.extend(['-r', config['repeat_count']])

  if config['truncate_percent']:
    cmd.extend(['-t', config['truncate_percent']])

  if config['max_time_minutes']:
    cmd.extend(['--max_time_minutes', config['max_time_minutes']])

  if config.has_key('bisect_mode'):
    cmd.extend(['--bisect_mode', config['bisect_mode']])

  cmd.extend(['--build_preference', 'ninja'])

  if '--browser=cros' in config['command']:
    cmd.extend(['--target_platform', 'cros'])

    if os.environ[CROS_BOARD_ENV] and os.environ[CROS_IP_ENV]:
      cmd.extend(['--cros_board', os.environ[CROS_BOARD_ENV]])
      cmd.extend(['--cros_remote_ip', os.environ[CROS_IP_ENV]])
    else:
      print ('Error: Cros build selected, but BISECT_CROS_IP or'
             'BISECT_CROS_BOARD undefined.\n')
      return 1

  if 'android' in config['command']:
    if 'android-chrome-shell' in config['command']:
      cmd.extend(['--target_platform', 'android'])
    elif 'android-chrome' in config['command']:
      cmd.extend(['--target_platform', 'android-chrome'])
    else:
      cmd.extend(['--target_platform', 'android'])

  if path_to_goma:
    # For Windows XP platforms, goma service is not supported.
    # Moreover we don't compile chrome when gs_bucket flag is set instead
    # use builds archives, therefore ignore goma service for Windows XP.
    # See http://crbug.com/330900.
    if config.get('gs_bucket') and platform.release() == 'XP':
      print ('Goma doesn\'t have a win32 binary, therefore it is not supported '
             'on Windows XP platform. Please refer to crbug.com/330900.')
      path_to_goma = None
    cmd.append('--use_goma')

  if path_to_extra_src:
    cmd.extend(['--extra_src', path_to_extra_src])

  # These flags are used to download build archives from cloud storage if
  # available, otherwise will post a try_job_http request to build it on
  # tryserver.
  if config.get('gs_bucket'):
    if config.get('builder_host') and config.get('builder_port'):
      cmd.extend(['--gs_bucket', config['gs_bucket'],
                  '--builder_host', config['builder_host'],
                  '--builder_port', config['builder_port']
                 ])
    else:
      print ('Error: Specified gs_bucket, but missing builder_host or '
             'builder_port information in config.')
      return 1

  if dry_run:
    cmd.extend(['--debug_ignore_build', '--debug_ignore_sync',
        '--debug_ignore_perf_test'])
  cmd = [str(c) for c in cmd]

  with Goma(path_to_goma) as _:
    return_code = subprocess.call(cmd)

  if return_code:
    print ('Error: bisect-perf-regression.py returned with error %d\n'
           % return_code)

  return return_code


def _PrintConfigStep(config):
  """Prints out the given config, along with Buildbot annotations."""
  bisect_utils.OutputAnnotationStepStart('Config')
  print
  for k, v in config.iteritems():
    print '  %s : %s' % (k, v)
  print
  bisect_utils.OutputAnnotationStepClosed()


def _OptionParser():
  """Returns the options parser for run-bisect-perf-regression.py."""
  usage = ('%prog [options] [-- chromium-options]\n'
           'Used by a trybot to run the bisection script using the parameters'
           ' provided in the run-bisect-perf-regression.cfg file.')
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-w', '--working_directory',
                    type='str',
                    help='A working directory to supply to the bisection '
                    'script, which will use it as the location to checkout '
                    'a copy of the chromium depot.')
  parser.add_option('-p', '--path_to_goma',
                    type='str',
                    help='Path to goma directory. If this is supplied, goma '
                    'builds will be enabled.')
  parser.add_option('--path_to_config',
                    type='str',
                    help='Path to the config file to use. If this is supplied, '
                    'the bisect script will use this to override the default '
                    'config file path. The script will attempt to load it '
                    'as a bisect config first, then a perf config.')
  parser.add_option('--extra_src',
                    type='str',
                    help='Path to extra source file. If this is supplied, '
                    'bisect script will use this to override default behavior.')
  parser.add_option('--dry_run',
                    action="store_true",
                    help='The script will perform the full bisect, but '
                    'without syncing, building, or running the performance '
                    'tests.')
  return parser


def main():
  """Entry point for run-bisect-perf-regression.py.

  Reads the config file, and then tries to either bisect a regression or
  just run a performance test, depending on the particular config parameters
  specified in the config file.
  """

  parser = _OptionParser()
  opts, _ = parser.parse_args()

  current_dir = os.path.abspath(os.path.dirname(sys.argv[0]))

  # Use the default config file path unless one was specified.
  config_path = os.path.join(current_dir, BISECT_REGRESSION_CONFIG)
  if opts.path_to_config:
    config_path = opts.path_to_config
  config = _LoadConfigFile(config_path)

  # Check if the config is valid for running bisect job.
  config_is_valid = _ValidateBisectConfigFile(config)

  if config and config_is_valid:
    if not opts.working_directory:
      print 'Error: missing required parameter: --working_directory\n'
      parser.print_help()
      return 1

    return _RunBisectionScript(
        config, opts.working_directory, current_dir,
        opts.path_to_goma, opts.extra_src, opts.dry_run)

  # If it wasn't valid for running a bisect, then maybe the user wanted
  # to run a perf test instead of a bisect job. Try reading any possible
  # perf test config files.
  perf_cfg_files = [RUN_TEST_CONFIG, WEBKIT_RUN_TEST_CONFIG]
  for current_perf_cfg_file in perf_cfg_files:
    if opts.path_to_config:
      path_to_perf_cfg = opts.path_to_config
    else:
      path_to_perf_cfg = os.path.join(
          os.path.abspath(os.path.dirname(sys.argv[0])),
          current_perf_cfg_file)

    config = _LoadConfigFile(path_to_perf_cfg)
    config_is_valid = _ValidatePerfConfigFile(config)

    if config and config_is_valid:
      return _SetupAndRunPerformanceTest(
          config, current_dir, opts.path_to_goma)

  print ('Error: Could not load config file. Double check your changes to '
         'run-bisect-perf-regression.cfg or run-perf-test.cfg for syntax '
         'errors.\n')
  return 1


if __name__ == '__main__':
  sys.exit(main())
