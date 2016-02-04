# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import logging
import re
import subprocess
import urllib2
import json

from telemetry.util import command_line


CHROMIUM_CONFIG_FILENAME = 'tools/run-perf-test.cfg'
BLINK_CONFIG_FILENAME = 'Tools/run-perf-test.cfg'
SUCCESS, NO_CHANGES, ERROR = range(3)
# Unsupported Perf bisect bots.
EXCLUDED_BOTS = {
    'win_xp_perf_bisect',  # Goma issues: crbug.com/330900
    'linux_perf_tester',
    'linux_perf_bisector',
    'win_perf_bisect_builder',
    'win64_nv_tester',
    'winx64_bisect_builder',
    'linux_perf_bisect_builder',
    'mac_perf_bisect_builder',
    'android_perf_bisect_builder',
    'android_arm64_perf_bisect_builder',
    # Bisect FYI bots are not meant for testing actual perf regressions.
    # Hardware configuration on these bots is different from actual bisect bot
    # and these bots runs E2E integration tests for auto-bisect
    # using dummy benchmarks.
    'linux_fyi_perf_bisect',
    'mac_fyi_perf_bisect',
    'win_fyi_perf_bisect',
}

INCLUDE_BOTS = [
    'all',
    'all-win',
    'all-mac',
    'all-linux',
    'all-android'
]

# Default try bot to use incase builbot is unreachable.
DEFAULT_TRYBOTS =  [
    'linux_perf_bisect',
    'mac_10_11_perf_bisect',
    'winx64_10_perf_bisect',
    'android_s5_perf_bisect'
]


class TrybotError(Exception):

  def __str__(self):
    return '%s\nError running tryjob.' % self.args[0]


def _GetTrybotList(builders):
  builders = ['%s' % bot.replace('_perf_bisect', '').replace('_', '-')
              for bot in builders]
  builders.extend(INCLUDE_BOTS)
  return sorted(builders)


def _GetBuilderNames(trybot_name, builders):
  """ Return platform and its available bot name as dictionary."""
  if 'all' not in trybot_name:
    bot = ['%s_perf_bisect' % trybot_name.replace('-', '_')]
    bot_platform = trybot_name.split('-')[0]
    if 'x64' in trybot_name:
      bot_platform += '-x64'
    return {bot_platform: bot}

  platform_and_bots = {}
  for os_name in ['linux', 'android', 'mac', 'win']:
    platform_and_bots[os_name] = [bot for bot in builders if os_name in bot]

  # Special case for Windows x64, consider it as separate platform
  # config config should contain target_arch=x64 and --browser=release_x64.
  win_x64_bots = [
      platform_and_bots['win'].pop(i)
      for i, win_bot in enumerate(platform_and_bots['win'])
      if 'x64' in win_bot]
  platform_and_bots['win-x64'] = win_x64_bots

  if 'all-win' in trybot_name:
    return {'win': platform_and_bots['win'],
            'win-x64': platform_and_bots['win-x64']}
  if 'all-mac' in trybot_name:
    return {'mac': platform_and_bots['mac']}
  if 'all-android' in trybot_name:
    return {'android': platform_and_bots['android']}
  if 'all-linux' in trybot_name:
    return {'linux': platform_and_bots['linux']}

  return platform_and_bots


def _RunProcess(cmd):
  logging.debug('Running process: "%s"', ' '.join(cmd))
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  out, err = proc.communicate()
  returncode = proc.poll()
  return (returncode, out, err)


class Trybot(command_line.ArgParseCommand):
  """ Run telemetry perf benchmark on trybot """

  usage = 'botname benchmark_name [<benchmark run options>]'
  _builders = None

  def __init__(self):
    self._builder_names = None

  @classmethod
  def _GetBuilderList(cls):
    if not cls._builders:
      try:
        f = urllib2.urlopen(
            ('https://build.chromium.org/p/tryserver.chromium.perf/json/'
             'builders'),
            timeout=5)
      # In case of any kind of exception, allow tryjobs to use default trybots.
      # Possible exception are ssl.SSLError, urllib2.URLError,
      # socket.timeout, socket.error.
      except Exception:
        # Incase of any exception return default trybots.
        print ('WARNING: Unable to reach builbot to retrieve trybot '
            'information, tryjob will use default trybots.')
        cls._builders = DEFAULT_TRYBOTS
      else:
        builders = json.loads(f.read()).keys()
        # Exclude unsupported bots like win xp and some dummy bots.
        cls._builders = [bot for bot in builders if bot not in EXCLUDED_BOTS]

    return cls._builders

  def _InitializeBuilderNames(self, trybot):
    self._builder_names = _GetBuilderNames(trybot, self._GetBuilderList())

  @classmethod
  def CreateParser(cls):
    parser = argparse.ArgumentParser(
        ('Run telemetry benchmarks on trybot. You can add all the benchmark '
         'options available except the --browser option'),
        formatter_class=argparse.RawTextHelpFormatter)
    return parser

  @classmethod
  def ProcessCommandLineArgs(cls, parser, options, extra_args, environment):
    del options, environment  # unused
    for arg in extra_args:
      if arg == '--browser' or arg.startswith('--browser='):
        parser.error('--browser=... is not allowed when running trybot.')

  @classmethod
  def AddCommandLineArgs(cls, parser, environment):
    del environment  # unused
    available_bots = _GetTrybotList(cls._GetBuilderList())
    parser.add_argument(
        'trybot', choices=available_bots,
        help=('specify which bots to run telemetry benchmarks on. '
              ' Allowed values are:\n' + '\n'.join(available_bots)),
        metavar='<trybot name>')
    parser.add_argument(
        'benchmark_name', type=str,
        help=('specify which benchmark to run. To see all available benchmarks,'
              ' run `run_benchmark list`'),
        metavar='<benchmark name>')

  def Run(self, options, extra_args=None):
    """Sends a tryjob to a perf trybot.

    This creates a branch, telemetry-tryjob, switches to that branch, edits
    the bisect config, commits it, uploads the CL to rietveld, and runs a
    tryjob on the given bot.
    """
    if extra_args is None:
      extra_args = []
    self._InitializeBuilderNames(options.trybot)

    arguments = [options.benchmark_name] + extra_args

    # First check if there are chromium changes to upload.
    status = self._AttemptTryjob(CHROMIUM_CONFIG_FILENAME, arguments)
    if status not in [SUCCESS, ERROR]:
      # If we got here, there are no chromium changes to upload. Try blink.
      os.chdir('third_party/WebKit/')
      status = self._AttemptTryjob(BLINK_CONFIG_FILENAME, arguments)
      os.chdir('../..')
      if status not in [SUCCESS, ERROR]:
        logging.error('No local changes found in chromium or blink trees. '
                      'browser=%s argument sends local changes to the '
                      'perf trybot(s): %s.', options.trybot,
                      self._builder_names.values())
        return 1
    return 0

  def _UpdateConfigAndRunTryjob(self, bot_platform, cfg_file_path, arguments):
    """Updates perf config file, uploads changes and excutes perf try job.

    Args:
      bot_platform: Name of the platform to be generated.
      cfg_file_path: Perf config file path.

    Returns:
      (result, msg) where result is one of:
          SUCCESS if a tryjob was sent
          NO_CHANGES if there was nothing to try,
          ERROR if a tryjob was attempted but an error encountered
          and msg is an error message if an error was encountered, or rietveld
          url if success, otherwise throws TrybotError exception.
    """
    config = self._GetPerfConfig(bot_platform, arguments)
    try:
      config_file = open(cfg_file_path, 'w')
    except IOError:
      msg = 'Cannot find %s. Please run from src dir.' % cfg_file_path
      return (ERROR, msg)
    config_file.write('config = %s' % json.dumps(
        config, sort_keys=True, indent=2, separators=(',', ': ')))
    config_file.close()
    # Commit the config changes locally.
    returncode, out, err = _RunProcess(
        ['git', 'commit', '-a', '-m', 'bisect config: %s' % bot_platform])
    if returncode:
      raise TrybotError('Could not commit bisect config change for %s,'
                        ' error %s' % (bot_platform, err))
    # Upload the CL to rietveld and run a try job.
    returncode, out, err = _RunProcess([
        'git', 'cl', 'upload', '-f', '--bypass-hooks', '-m',
        'CL for perf tryjob on %s' % bot_platform
    ])
    if returncode:
      raise TrybotError('Could upload to rietveld for %s, error %s' %
                        (bot_platform, err))

    match = re.search(r'https://codereview.chromium.org/[\d]+', out)
    if not match:
      raise TrybotError('Could not upload CL to rietveld for %s! Output %s' %
                        (bot_platform, out))
    rietveld_url = match.group(0)
    # Generate git try command for available bots.
    git_try_command = ['git', 'cl', 'try', '-m', 'tryserver.chromium.perf']
    for bot in self._builder_names[bot_platform]:
      git_try_command.extend(['-b', bot])
    returncode, out, err = _RunProcess(git_try_command)
    if returncode:
      raise TrybotError('Could not try CL for %s, error %s' %
                        (bot_platform, err))

    return (SUCCESS, rietveld_url)

  def _GetPerfConfig(self, bot_platform, arguments):
    """Generates the perf config for try job.

    Args:
      bot_platform: Name of the platform to be generated.

    Returns:
      A dictionary with perf config parameters.
    """
    # To make sure that we don't mutate the original args
    arguments = arguments[:]

    # Generate the command line for the perf trybots
    target_arch = 'ia32'
    if any(arg == '--chrome-root' or arg.startswith('--chrome-root=') for arg
           in arguments):
      raise ValueError(
          'Trybot does not suport --chrome-root option set directly '
          'through command line since it may contain references to your local '
          'directory')
    if bot_platform in ['win', 'win-x64']:
      arguments.insert(0, 'python tools\\perf\\run_benchmark')
    else:
      arguments.insert(0, './tools/perf/run_benchmark')

    if bot_platform == 'android':
      arguments.insert(1, '--browser=android-chromium')
    elif any('x64' in bot for bot in self._builder_names[bot_platform]):
      arguments.insert(1, '--browser=release_x64')
      target_arch = 'x64'
    else:
      arguments.insert(1, '--browser=release')

    command = ' '.join(arguments)

    return {
        'command': command,
        'repeat_count': '1',
        'max_time_minutes': '120',
        'truncate_percent': '0',
        'target_arch': target_arch,
    }

  def _AttemptTryjob(self, cfg_file_path, arguments):
    """Attempts to run a tryjob from the current directory.

    This is run once for chromium, and if it returns NO_CHANGES, once for blink.

    Args:
      cfg_file_path: Path to the config file for the try job.

    Returns:
      Returns SUCCESS if a tryjob was sent, NO_CHANGES if there was nothing to
      try, ERROR if a tryjob was attempted but an error encountered.
    """
    source_repo = 'chromium'
    if cfg_file_path == BLINK_CONFIG_FILENAME:
      source_repo = 'blink'

    # TODO(prasadv): This method is quite long, we should consider refactor
    # this by extracting to helper methods.
    returncode, original_branchname, err = _RunProcess(
        ['git', 'rev-parse', '--abbrev-ref', 'HEAD'])
    if returncode:
      msg = 'Must be in a git repository to send changes to trybots.'
      if err:
        msg += '\nGit error: %s' % err
      logging.error(msg)
      return ERROR

    original_branchname = original_branchname.strip()

    # Check if the tree is dirty: make sure the index is up to date and then
    # run diff-index
    _RunProcess(['git', 'update-index', '--refresh', '-q'])
    returncode, out, err = _RunProcess(['git', 'diff-index', 'HEAD'])
    if out:
      logging.error(
          'Cannot send a try job with a dirty tree. Commit locally first.')
      return ERROR

    # Make sure the tree does have local commits.
    returncode, out, err = _RunProcess(
        ['git', 'log', 'origin/master..HEAD'])
    if not out:
      return NO_CHANGES

    # Create/check out the telemetry-tryjob branch, and edit the configs
    # for the tryjob there.
    returncode, out, err = _RunProcess(
        ['git', 'checkout', '-b', 'telemetry-tryjob'])
    if returncode:
      logging.error('Error creating branch telemetry-tryjob. '
                    'Please delete it if it exists.\n%s', err)
      return ERROR
    try:
      returncode, out, err = _RunProcess(
          ['git', 'branch', '--set-upstream-to', 'origin/master'])
      if returncode:
        logging.error('Error in git branch --set-upstream-to: %s', err)
        return ERROR
      for bot_platform in self._builder_names:
        if not self._builder_names[bot_platform]:
          logging.warning('No builder is found for %s', bot_platform)
          continue
        try:
          results, output = self._UpdateConfigAndRunTryjob(
              bot_platform, cfg_file_path, arguments)
          if results == ERROR:
            logging.error(output)
            return ERROR
          print ('Uploaded %s try job to rietveld for %s platform. '
                 'View progress at %s' % (source_repo, bot_platform, output))
        except TrybotError, err:
          print err
          logging.error(err)
    finally:
      # Checkout original branch and delete telemetry-tryjob branch.
      # TODO(prasadv): This finally block could be extracted out to be a
      # separate function called _CleanupBranch.
      returncode, out, err = _RunProcess(
          ['git', 'checkout', original_branchname])
      if returncode:
        logging.error('Could not check out %s. Please check it out and '
                      'manually delete the telemetry-tryjob branch. '
                      ': %s', original_branchname, err)
        return ERROR  # pylint: disable=lost-exception
      logging.info('Checked out original branch: %s', original_branchname)
      returncode, out, err = _RunProcess(
          ['git', 'branch', '-D', 'telemetry-tryjob'])
      if returncode:
        logging.error('Could not delete telemetry-tryjob branch. '
                      'Please delete it manually: %s', err)
        return ERROR  # pylint: disable=lost-exception
      logging.info('Deleted temp branch: telemetry-tryjob')
    return SUCCESS
