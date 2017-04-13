#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool for finding the cause of APK bloat.

Run diagnose_apk_bloat.py -h for detailed usage help.
"""

import argparse
import collections
import distutils.spawn
import itertools
import json
import multiprocessing
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

_BUILDER_URL = \
    'https://build.chromium.org/p/chromium.perf/builders/Android%20Builder'
_CLOUD_OUT_DIR = os.path.join('out', 'Release')
_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
_DEFAULT_ARCHIVE_DIR = os.path.join(_SRC_ROOT, 'binary-size-bloat')
_DEFAULT_OUT_DIR = os.path.join(_SRC_ROOT, 'out', 'diagnose-apk-bloat')
_DEFAULT_TARGET = 'monochrome_public_apk'

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
      cmd = [self._RESOURCE_SIZES_PATH, apk_path,'--output-dir', archive_dir,
             '--no-output-dir', '--chartjson']
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
    self.extra_gn_args_str = ''
    self.max_jobs = args.max_jobs
    self.max_load_average = args.max_load_average
    self.output_directory = args.output_directory
    self.target = args.target
    self.target_os = args.target_os
    self.use_goma = args.use_goma
    self._SetDefaults()

  @property
  def abs_apk_path(self):
    return os.path.join(self.output_directory, self.apk_path)

  @property
  def apk_name(self):
    # Only works on apk targets that follow: my_great_apk naming convention.
    apk_name = ''.join(s.title() for s in self.target.split('_')[:-1]) + '.apk'
    return apk_name.replace('Webview', 'WebView')

  @property
  def apk_path(self):
    return os.path.join('apks', self.apk_name)

  @property
  def main_lib_name(self):
    # TODO(estevenson): Get this from GN instead of hardcoding.
    if self.IsLinux():
      return 'chrome'
    elif 'monochrome' in self.target:
      return 'lib.unstripped/libmonochrome.so'
    else:
      return 'lib.unstripped/libchrome.so'

  @property
  def main_lib_path(self):
    return os.path.join(self.output_directory, self.main_lib_name)

  @property
  def map_file_name(self):
    return self.main_lib_name + '.map.gz'

  def _SetDefaults(self):
    has_goma_dir = os.path.exists(os.path.join(os.path.expanduser('~'), 'goma'))
    self.use_goma = self.use_goma or has_goma_dir
    self.max_load_average = (self.max_load_average or
                             str(multiprocessing.cpu_count()))
    if not self.max_jobs:
      self.max_jobs = '10000' if self.use_goma else '500'

    if os.path.exists(os.path.join(os.path.dirname(_SRC_ROOT), 'src-internal')):
      self.extra_gn_args_str = ' is_chrome_branded=true'
    else:
      self.extra_gn_args_str = (' exclude_unwind_tables=true '
          'ffmpeg_branding="Chrome" proprietary_codecs=true')

  def _GenGnCmd(self):
    gn_args = 'is_official_build=true symbol_level=1'
    gn_args += ' use_goma=%s' % str(self.use_goma).lower()
    gn_args += ' target_os="%s"' % self.target_os
    gn_args += (' enable_chrome_android_internal=%s' %
                str(self.enable_chrome_android_internal).lower())
    gn_args += self.extra_gn_args_str
    return ['gn', 'gen', self.output_directory, '--args=%s' % gn_args]

  def _GenNinjaCmd(self):
    cmd = ['ninja', '-C', self.output_directory]
    cmd += ['-j', self.max_jobs] if self.max_jobs else []
    cmd += ['-l', self.max_load_average] if self.max_load_average else []
    cmd += [self.target]
    return cmd

  def Run(self):
    _Print('Building: {}.', self.target)
    _RunCmd(self._GenGnCmd(), print_stdout=True)
    _RunCmd(self._GenNinjaCmd(), print_stdout=True)

  def IsAndroid(self):
    return self.target_os == 'android'

  def IsLinux(self):
    return self.target_os == 'linux'


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

  if proc.returncode:
    _Die('command failed: {}\nstderr:\n{}', cmd_str, stderr)

  return stdout.strip() if stdout else ''


def _GitCmd(args):
  return _RunCmd(['git', '-C', _SRC_ROOT] + args)


def _GclientSyncCmd(rev):
  cwd = os.getcwd()
  os.chdir(_SRC_ROOT)
  _RunCmd(['gclient', 'sync', '-r', 'src@' + rev], print_stdout=True)
  os.chdir(cwd)


def _ArchiveBuildResult(archive_dir, build):
  """Save build artifacts necessary for diffing.

  Expects |build.output_directory| to be correct.
  """
  _Print('Saving build results to: {}', archive_dir)
  if not os.path.exists(archive_dir):
    os.makedirs(archive_dir)

  def ArchiveFile(filename):
    if not os.path.exists(filename):
      _Die('missing expected file: {}', filename)
    shutil.copy(filename, archive_dir)

  ArchiveFile(build.main_lib_path)
  lib_name_noext = os.path.splitext(os.path.basename(build.main_lib_path))[0]
  size_path = os.path.join(archive_dir, lib_name_noext + '.size')
  supersize_path = os.path.join(_SRC_ROOT, 'tools/binary_size/supersize')
  tool_prefix = _FindToolPrefix(build.output_directory)
  supersize_cmd = [supersize_path, 'archive', size_path, '--elf-file',
                   build.main_lib_path, '--tool-prefix', tool_prefix,
                   '--output-directory', build.output_directory,
                   '--no-source-paths']
  if build.IsAndroid():
    supersize_cmd += ['--apk-file', build.abs_apk_path]
    ArchiveFile(build.abs_apk_path)

  _RunCmd(supersize_cmd)


def _FindToolPrefix(output_directory):
  build_vars_path = os.path.join(output_directory, 'build_vars.txt')
  if os.path.exists(build_vars_path):
    with open(build_vars_path) as f:
      build_vars = dict(l.rstrip().split('=', 1) for l in f if '=' in l)
    # Tool prefix is relative to output dir, rebase to source root.
    tool_prefix = build_vars['android_tool_prefix']
    while os.path.sep in tool_prefix:
      rebased_tool_prefix = os.path.join(_SRC_ROOT, tool_prefix)
      if os.path.exists(rebased_tool_prefix + 'readelf'):
        return rebased_tool_prefix
      tool_prefix = tool_prefix[tool_prefix.find(os.path.sep) + 1:]
  return ''


def _SyncAndBuild(revs, archive_dirs, build):
  # Move to a detached state since gclient sync doesn't work with local commits
  # on a branch.
  _GitCmd(['checkout', '--detach'])
  for rev, archive_dir in itertools.izip(revs, archive_dirs):
    _GclientSyncCmd(rev)
    build.Run()
    _ArchiveBuildResult(archive_dir, build)


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


def _DownloadBuildArtifacts(revs, archive_dirs, build, depot_tools_path=None):
  """Download artifacts from arm32 chromium perf builder."""
  if depot_tools_path:
    gsutil_path = os.path.join(depot_tools_path, 'gsutil.py')
  else:
    gsutil_path = distutils.spawn.find_executable('gsutil.py')

  if not gsutil_path:
    _Die('gsutil.py not found, please provide path to depot_tools via '
         '--depot-tools-path or add it to your PATH')

  download_dir = tempfile.mkdtemp(dir=_SRC_ROOT)
  try:
    for rev, archive_dir in itertools.izip(revs, archive_dirs):
      _DownloadAndArchive(gsutil_path, rev, archive_dir, download_dir, build)
  finally:
    shutil.rmtree(download_dir)


def _DownloadAndArchive(gsutil_path, rev, archive_dir, dl_dir, build):
  dl_file = 'full-build-linux_%s.zip' % rev
  dl_url = 'gs://chrome-perf/Android Builder/%s' % dl_file
  dl_dst = os.path.join(dl_dir, dl_file)
  _Print('Downloading build artifacts for {}', rev)
  # gsutil writes stdout and stderr to stderr, so pipe stdout and stderr to
  # sys.stdout.
  retcode = subprocess.call([gsutil_path, 'cp', dl_url, dl_dir],
                             stdout=sys.stdout, stderr=subprocess.STDOUT)
  if retcode:
      _Die('unexpected error while downloading {}. It may no longer exist on '
           'the server or it may not have been uploaded yet (check {}). '
           'Otherwise, you may not have the correct access permissions.',
           dl_url, _BUILDER_URL)

  # Files needed for supersize and resource_sizes. Paths relative to out dir.
  to_extract = [build.main_lib_name, build.map_file_name, 'args.gn',
                'build_vars.txt', build.apk_path]
  extract_dir = os.path.join(os.path.splitext(dl_dst)[0], 'unzipped')
  # Storage bucket stores entire output directory including out/Release prefix.
  _Print('Extracting build artifacts')
  with zipfile.ZipFile(dl_dst, 'r') as z:
    _ExtractFiles(to_extract, _CLOUD_OUT_DIR, extract_dir, z)
    dl_out = os.path.join(extract_dir, _CLOUD_OUT_DIR)
    build.output_directory, output_directory = dl_out, build.output_directory
    _ArchiveBuildResult(archive_dir, build)
    build.output_directory = output_directory


def _ExtractFiles(to_extract, prefix, dst, z):
  zip_infos = z.infolist()
  assert all(info.filename.startswith(prefix) for info in zip_infos), (
      'Storage bucket folder structure doesn\'t start with %s' % prefix)
  to_extract = [os.path.join(prefix, f) for f in to_extract]
  for f in to_extract:
    z.extract(f, path=dst)


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
  parser.add_argument('--cloud',
                      action='store_true',
                      help='Download build artifacts from perf builders '
                      '(Android only, Googlers only).')
  parser.add_argument('--depot-tools-path',
                      help='Custom path to depot tools. Needed for --cloud if '
                      'depot tools isn\'t in your PATH')

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
  build = _BuildHelper(args)
  if args.cloud and build.IsLinux():
    parser.error('--cloud only works for android')

  _EnsureDirectoryClean()
  _SetInitialBranch()
  revs = [args.rev_with_patch,
          args.rev_without_patch or args.rev_with_patch + '^']
  revs = [_NormalizeRev(r) for r in revs]
  archive_dirs = [os.path.join(args.archive_dir, '%d-%s' % (len(revs) - i, rev))
                  for i, rev in enumerate(revs)]
  if args.cloud:
    _DownloadBuildArtifacts(revs, archive_dirs, build,
                            depot_tools_path=args.depot_tools_path)
  else:
    _SetInitialBranch()
    _SyncAndBuild(revs, archive_dirs, build)
    _RestoreInitialBranch()

  output_file = os.path.join(args.archive_dir,
                             'diff_result_{}_{}.txt'.format(*revs))
  if os.path.exists(output_file):
    os.remove(output_file)
  diffs = []
  if build.IsAndroid():
    diffs +=  [
        ResourceSizesDiff(archive_dirs, build.apk_name,
                          slow_options=args.include_slow_options)
    ]
  with open(output_file, 'a') as logfile:
    for d in diffs:
      d.AppendResults(logfile)

if __name__ == '__main__':
  sys.exit(main())

