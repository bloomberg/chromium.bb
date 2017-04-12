#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool for finding the cause of APK bloat.

Run diagnose_apk_bloat.py -h for detailed usage help.
"""

import argparse
import collections
import itertools
import json
import multiprocessing
import os
import shutil
import subprocess
import sys

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
_DEFAULT_OUT_DIR = os.path.join(_SRC_ROOT, 'out', 'diagnose-apk-bloat')
_DEFAULT_TARGET = 'monochrome_public_apk'
_DEFAULT_ARCHIVE_DIR = os.path.join(_SRC_ROOT, 'binary-size-bloat')

# Global variable for storing the initial branch before the script was launched
# so that it doesn't need to be passed everywhere in case we fail and exit.
_initial_branch = None


class BaseDiff(object):
  """Base class capturing binary size diffs."""
  def __init__(self, name):
    self.name = name
    self.banner = '\n' + '*' * 30 + name + '*' * 30
    self.RunDiff()

  def AppendResults(self, logfile):
    """Print and write diff results to an open |logfile|."""
    _PrintAndWriteToFile(logfile, self.banner)
    _PrintAndWriteToFile(logfile, 'Summary:')
    _PrintAndWriteToFile(logfile, self.Summary())
    _PrintAndWriteToFile(logfile, '\nDetails:')
    for l in self.DetailedResults():
      _PrintAndWriteToFile(logfile, l)

  def Summary(self):
    """A short description that summarizes the source of binary size bloat."""
    raise NotImplementedError()

  def DetailedResults(self):
    """An iterable description of the cause of binary size bloat."""
    raise NotImplementedError()

  def ProduceDiff(self):
    """Prepare a binary size diff with ready to print results."""
    raise NotImplementedError()

  def RunDiff(self):
    _Print('Creating {}', self.name)
    self.ProduceDiff()


_ResourceSizesDiffResult = collections.namedtuple(
    'ResourceSizesDiffResult', ['section', 'value', 'units'])


class ResourceSizesDiff(BaseDiff):
  _RESOURCE_SIZES_PATH = os.path.join(
      _SRC_ROOT, 'build', 'android', 'resource_sizes.py')

  def __init__(self, archive_dirs, apk_name, slow_options=False):
    self._archive_dirs = archive_dirs
    self._apk_name = apk_name
    self._slow_options = slow_options
    self._diff = None  # Set by |ProduceDiff()|
    super(ResourceSizesDiff, self).__init__('Resource Sizes Diff')

  def DetailedResults(self):
    for section, value, units in self._diff:
      yield '{:>+10,} {} {}'.format(value, units, section)

  def Summary(self):
    for s in self._diff:
      if 'normalized' in s.section:
        return 'Normalized APK size: {:+,} {}'.format(s.value, s.units)
    return ''

  def ProduceDiff(self):
    chartjsons = self._RunResourceSizes()
    diff = []
    with_patch = chartjsons[0]['charts']
    without_patch = chartjsons[1]['charts']
    for section, section_dict in with_patch.iteritems():
      for subsection, v in section_dict.iteritems():
        # Ignore entries when resource_sizes.py chartjson format has changed.
        if (section not in without_patch or
            subsection not in without_patch[section] or
            v['units'] != without_patch[section][subsection]['units']):
          _Print('Found differing dict structures for resource_sizes.py, '
                 'skipping {} {}', section, subsection)
        else:
          diff.append(
              _ResourceSizesDiffResult(
                  '%s %s' % (section, subsection),
                  v['value'] - without_patch[section][subsection]['value'],
                  v['units']))
    self._diff = sorted(diff, key=lambda x: abs(x.value), reverse=True)

  def _RunResourceSizes(self):
    chartjsons = []
    for archive_dir in self._archive_dirs:
      apk_path = os.path.join(archive_dir, self._apk_name)
      chartjson_file = os.path.join(archive_dir, 'results-chart.json')
      cmd = [self._RESOURCE_SIZES_PATH, '--output-dir', archive_dir,
             '--no-output-dir',
             '--chartjson', apk_path]
      if self._slow_options:
        cmd += ['--estimate-patch-size']
      else:
        cmd += ['--no-static-initializer-check']
      _RunCmd(cmd)
      with open(chartjson_file) as f:
        chartjsons.append(json.load(f))
    return chartjsons


class _BuildHelper(object):
  """Helper class for generating and building targets."""

  def __init__(self, args):
    self.enable_chrome_android_internal = args.enable_chrome_android_internal
    self.max_jobs = args.max_jobs
    self.max_load_average = args.max_load_average
    self.output_directory = args.output_directory
    self.target = args.target
    self.target_os = args.target_os
    self.use_goma = args.use_goma
    self._SetDefaults()

  def _SetDefaults(self):
    has_goma_dir = os.path.exists(os.path.join(os.path.expanduser('~'), 'goma'))
    self.use_goma = self.use_goma or has_goma_dir
    self.max_load_average = (self.max_load_average or
                             str(multiprocessing.cpu_count()))
    if not self.max_jobs:
      self.max_jobs = '10000' if self.use_goma else '500'

  def _GenGnCmd(self):
    gn_args = 'is_official_build = true'
    # Excludes some debug info, see crbug/610994.
    gn_args += ' is_chrome_branded = true'
    gn_args += ' use_goma = %s' % str(self.use_goma).lower()
    gn_args += ' target_os = "%s"' % self.target_os
    gn_args += (' enable_chrome_android_internal = %s' %
                str(self.enable_chrome_android_internal).lower())
    return ['gn', 'gen', self.output_directory, '--args=%s' % gn_args]

  def _GenNinjaCmd(self):
    cmd = ['ninja', '-C', self.output_directory]
    cmd += ['-j', self.max_jobs] if self.max_jobs else []
    cmd += ['-l', self.max_load_average] if self.max_load_average else []
    cmd += [self.target]
    return cmd

  def Build(self):
    _Print('Building: {}.', self.target)
    _RunCmd(self._GenGnCmd())
    _RunCmd(self._GenNinjaCmd(), print_stdout=True)


def _GetMainLibPath(target_os, target):
  # TODO(estevenson): Get this from GN instead of hardcoding.
  if target_os == 'linux':
    return 'chrome'
  elif 'monochrome' in target:
    return 'lib.unstripped/libmonochrome.so'
  else:
    return 'lib.unstripped/libchrome.so'


def _ApkNameFromTarget(target):
  # Only works on apk targets that follow: my_great_apk naming convention.
  apk_name = ''.join(s.title() for s in target.split('_')[:-1]) + '.apk'
  return apk_name.replace('Webview', 'WebView')


def _RunCmd(cmd, print_stdout=False):
  """Convenience function for running commands.

  Args:
    cmd: the command to run.
    print_stdout: if this is True, then the stdout of the process will be
        printed, otherwise stdout will be returned.

  Returns:
    Command stdout if |print_stdout| is False otherwise ''.
  """
  cmd_str = ' '.join(c for c in cmd)
  _Print('Running: {}', cmd_str)
  if print_stdout:
    proc_stdout = sys.stdout
  else:
    proc_stdout = subprocess.PIPE

  proc = subprocess.Popen(cmd, stdout=proc_stdout, stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()

  if proc.returncode != 0:
    _Die('command failed: {}\nstderr:\n{}', cmd_str, stderr)

  return stdout.strip() if stdout else ''


def _GitCmd(args):
  return _RunCmd(['git', '-C', _SRC_ROOT] + args)


def _GclientSyncCmd(rev):
  cwd = os.getcwd()
  os.chdir(_SRC_ROOT)
  _RunCmd(['gclient', 'sync', '-r', 'src@' + rev], print_stdout=True)
  os.chdir(cwd)


def _ArchiveBuildResult(archive_dir, build_helper):
  """Save build artifacts necessary for diffing."""
  _Print('Saving build results to: {}', archive_dir)
  if not os.path.exists(archive_dir):
    os.makedirs(archive_dir)

  def ArchiveFile(filename):
    if not os.path.exists(filename):
      _Die('missing expected file: {}', filename)
    shutil.copy(filename, archive_dir)

  lib_path = os.path.join(
      build_helper.output_directory,
      _GetMainLibPath(build_helper.target_os, build_helper.target))
  ArchiveFile(lib_path)

  size_path = os.path.join(
      archive_dir, os.path.splitext(os.path.basename(lib_path))[0] + '.size')
  supersize_path = os.path.join(_SRC_ROOT, 'tools/binary_size/supersize')
  _RunCmd([supersize_path, 'archive', size_path, '--output-directory',
           build_helper.output_directory, '--elf-file', lib_path])

  if build_helper.target_os == 'android':
    apk_path = os.path.join(build_helper.output_directory, 'apks',
                            _ApkNameFromTarget(build_helper.target))
    ArchiveFile(apk_path)


def _SyncAndBuild(revs, archive_dirs, build_helper):
  # Move to a detached state since gclient sync doesn't work with local commits
  # on a branch.
  _GitCmd(['checkout', '--detach'])
  for rev, archive_dir in itertools.izip(revs, archive_dirs):
    _GclientSyncCmd(rev)
    build_helper.Build()
    _ArchiveBuildResult(archive_dir, build_helper)


def _NormalizeRev(rev):
  """Use actual revs instead of HEAD, HEAD^, etc."""
  return _GitCmd(['rev-parse', rev])


def _EnsureDirectoryClean():
  _Print('Checking source directory')
  stdout = _GitCmd(['status', '--porcelain'])
  # Ignore untracked files.
  if stdout and stdout[:2] != '??':
    _Die('please ensure working directory is clean.')


def _SetInitialBranch():
  global _initial_branch
  _initial_branch = _GitCmd(['rev-parse', '--abbrev-ref', 'HEAD'])


def _RestoreInitialBranch():
  if _initial_branch:
    _GitCmd(['checkout', _initial_branch])


def _Die(s, *args, **kwargs):
  _Print('Failure: ' + s, *args, **kwargs)
  _RestoreInitialBranch()
  sys.exit(1)


def _Print(s, *args, **kwargs):
  print s.format(*args, **kwargs)


def _PrintAndWriteToFile(logfile, s):
  """Print |s| to |logfile| and stdout."""
  _Print(s)
  logfile.write('%s\n' % s)


def main():
  parser = argparse.ArgumentParser(
      description='Find the cause of APK size bloat.',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('--archive-dir',
                      default=_DEFAULT_ARCHIVE_DIR,
                      help='Where results are stored.')
  parser.add_argument('--rev-with-patch',
                      default='HEAD',
                      help='Commit with patch.')
  parser.add_argument('--rev-without-patch',
                      help='Older patch to diff against. If not supplied, '
                      'the previous commit to rev_with_patch will be used.')
  parser.add_argument('--include-slow-options',
                      action='store_true',
                      help='Run some extra steps that take longer to complete. '
                      'This includes apk-patch-size estimation and '
                      'static-initializer counting')

  build_group = parser.add_argument_group('ninja', 'Args to use with ninja/gn')
  build_group.add_argument('-j',
                           dest='max_jobs',
                           help='Run N jobs in parallel.')
  build_group.add_argument('-l',
                           dest='max_load_average',
                           help='Do not start new jobs if the load average is '
                           'greater than N.')
  build_group.add_argument('--no-goma',
                           action='store_false',
                           dest='use_goma',
                           default=True,
                           help='Use goma when building with ninja.')
  build_group.add_argument('--target-os',
                           default='android',
                           choices=['android', 'linux'],
                           help='target_os gn arg.')
  build_group.add_argument('--output-directory',
                           default=_DEFAULT_OUT_DIR,
                           help='ninja output directory.')
  build_group.add_argument('--enable_chrome_android_internal',
                           action='store_true',
                           help='Allow downstream targets to be built.')
  build_group.add_argument('--target',
                           default=_DEFAULT_TARGET,
                           help='GN APK target to build.')
  args = parser.parse_args()

  _EnsureDirectoryClean()
  _SetInitialBranch()
  revs = [args.rev_with_patch,
          args.rev_without_patch or args.rev_with_patch + '^']
  revs = [_NormalizeRev(r) for r in revs]
  build_helper = _BuildHelper(args)
  archive_dirs = [os.path.join(args.archive_dir, '%d-%s' % (len(revs) - i, rev))
                  for i, rev in enumerate(revs)]
  _SyncAndBuild(revs, archive_dirs, build_helper)
  _RestoreInitialBranch()

  output_file = os.path.join(args.archive_dir,
                             'diff_result_{}_{}.txt'.format(*revs))
  if os.path.exists(output_file):
    os.remove(output_file)
  diffs = []
  if build_helper.target_os == 'android':
    diffs +=  [
        ResourceSizesDiff(archive_dirs, _ApkNameFromTarget(args.target),
                          slow_options=args.include_slow_options)
    ]
  with open(output_file, 'a') as logfile:
    for d in diffs:
      d.AppendResults(logfile)

if __name__ == '__main__':
  sys.exit(main())

