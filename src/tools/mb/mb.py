#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""MB - the Meta-Build wrapper around GN.

MB is a wrapper script for GN that can be used to generate build files
for sets of canned configurations and analyze them.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
import ast
import collections
import errno
import json
import os
import pipes
import platform
import pprint
import re
import shutil
import sys
import subprocess
import tempfile
import traceback
import urllib2
import zipfile

from collections import OrderedDict

CHROMIUM_SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path = [os.path.join(CHROMIUM_SRC_DIR, 'build')] + sys.path
sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..'))

import gn_helpers
from mb.lib import validation


def DefaultVals():
  """Default mixin values"""
  return {
      'args_file': '',
      # TODO(crbug.com/937821): Get rid of 'cros_passthrough'.
      'cros_passthrough': False,
      'gn_args': '',
  }


def PruneVirtualEnv():
  # Set by VirtualEnv, no need to keep it.
  os.environ.pop('VIRTUAL_ENV', None)

  # Set by VPython, if scripts want it back they have to set it explicitly.
  os.environ.pop('PYTHONNOUSERSITE', None)

  # Look for "activate_this.py" in this path, which is installed by VirtualEnv.
  # This mechanism is used by vpython as well to sanitize VirtualEnvs from
  # $PATH.
  os.environ['PATH'] = os.pathsep.join([
    p for p in os.environ.get('PATH', '').split(os.pathsep)
    if not os.path.isfile(os.path.join(p, 'activate_this.py'))
  ])


def main(args):
  # Prune all evidence of VPython/VirtualEnv out of the environment. This means
  # that we 'unwrap' vpython VirtualEnv path/env manipulation. Invocations of
  # `python` from GN should never inherit the gn.py's own VirtualEnv. This also
  # helps to ensure that generated ninja files do not reference python.exe from
  # the VirtualEnv generated from depot_tools' own .vpython file (or lack
  # thereof), but instead reference the default python from the PATH.
  PruneVirtualEnv()

  mbw = MetaBuildWrapper()
  return mbw.Main(args)

class MetaBuildWrapper(object):
  def __init__(self):
    self.chromium_src_dir = CHROMIUM_SRC_DIR
    self.default_config = os.path.join(self.chromium_src_dir, 'tools', 'mb',
                                       'mb_config.pyl')
    self.default_isolate_map = os.path.join(self.chromium_src_dir, 'testing',
                                            'buildbot', 'gn_isolate_map.pyl')
    self.executable = sys.executable
    self.platform = sys.platform
    self.sep = os.sep
    self.args = argparse.Namespace()
    self.configs = {}
    self.public_artifact_builders = None
    self.masters = {}
    self.mixins = {}
    self.isolate_exe = 'isolate.exe' if self.platform.startswith(
        'win') else 'isolate'
    self.use_luci_auth = False

  def Main(self, args):
    self.ParseArgs(args)
    try:
      ret = self.args.func()
      if ret:
        self.DumpInputFiles()
      return ret
    except KeyboardInterrupt:
      self.Print('interrupted, exiting')
      return 130
    except Exception:
      self.DumpInputFiles()
      s = traceback.format_exc()
      for l in s.splitlines():
        self.Print(l)
      return 1

  def ParseArgs(self, argv):
    def AddCommonOptions(subp):
      group = subp.add_mutually_exclusive_group()
      group.add_argument(
          '-m', '--master', help='master name to look up config from')
      subp.add_argument('-b', '--builder',
                        help='builder name to look up config from')
      subp.add_argument('-c', '--config',
                        help='configuration to analyze')
      subp.add_argument('--phase',
                        help='optional phase name (used when builders '
                             'do multiple compiles with different '
                             'arguments in a single build)')
      subp.add_argument('-f',
                        '--config-file',
                        metavar='PATH',
                        help=('path to config file '
                              '(default is mb_config.pyl'))
      subp.add_argument('-i', '--isolate-map-file', metavar='PATH',
                        help='path to isolate map file '
                             '(default is %(default)s)',
                        default=[],
                        action='append',
                        dest='isolate_map_files')
      subp.add_argument('-g', '--goma-dir',
                        help='path to goma directory')
      subp.add_argument('--android-version-code',
                        help='Sets GN arg android_default_version_code')
      subp.add_argument('--android-version-name',
                        help='Sets GN arg android_default_version_name')
      subp.add_argument('-n', '--dryrun', action='store_true',
                        help='Do a dry run (i.e., do nothing, just print '
                             'the commands that will run)')
      subp.add_argument('-v', '--verbose', action='store_true',
                        help='verbose logging')

      # TODO(crbug.com/1060857): Remove this once swarming task templates
      # support command prefixes.
      luci_auth_group = subp.add_mutually_exclusive_group()
      luci_auth_group.add_argument(
          '--luci-auth',
          action='store_true',
          help='Run isolated commands under `luci-auth context`.')
      luci_auth_group.add_argument(
          '--no-luci-auth',
          action='store_false',
          dest='luci_auth',
          help='Do not run isolated commands under `luci-auth context`.')

    parser = argparse.ArgumentParser(
      prog='mb', description='mb (meta-build) is a python wrapper around GN. '
                             'See the user guide in '
                             '//tools/mb/docs/user_guide.md for detailed usage '
                             'instructions.')

    subps = parser.add_subparsers()

    subp = subps.add_parser('analyze',
                            description='Analyze whether changes to a set of '
                                        'files will cause a set of binaries to '
                                        'be rebuilt.')
    AddCommonOptions(subp)
    subp.add_argument('path',
                      help='path build was generated into.')
    subp.add_argument('input_path',
                      help='path to a file containing the input arguments '
                           'as a JSON object.')
    subp.add_argument('output_path',
                      help='path to a file containing the output arguments '
                           'as a JSON object.')
    subp.add_argument('--json-output',
                      help='Write errors to json.output')
    subp.set_defaults(func=self.CmdAnalyze)

    subp = subps.add_parser('export',
                            description='Print out the expanded configuration '
                            'for each builder as a JSON object.')
    subp.add_argument('-f',
                      '--config-file',
                      metavar='PATH',
                      help=('path to config file '
                            '(default is mb_config.pyl'))
    subp.add_argument('-g', '--goma-dir',
                      help='path to goma directory')
    subp.set_defaults(func=self.CmdExport)

    subp = subps.add_parser('gen',
                            description='Generate a new set of build files.')
    AddCommonOptions(subp)
    subp.add_argument('--swarming-targets-file',
                      help='generates runtime dependencies for targets listed '
                           'in file as .isolate and .isolated.gen.json files. '
                           'Targets should be listed by name, separated by '
                           'newline.')
    subp.add_argument('--json-output',
                      help='Write errors to json.output')
    subp.add_argument('path',
                      help='path to generate build into')
    subp.set_defaults(func=self.CmdGen)

    subp = subps.add_parser('isolate-everything',
                            description='Generates a .isolate for all targets. '
                                        'Requires that mb.py gen has already '
                                        'been run.')
    AddCommonOptions(subp)
    subp.set_defaults(func=self.CmdIsolateEverything)
    subp.add_argument('path',
                      help='path build was generated into')

    subp = subps.add_parser('isolate',
                            description='Generate the .isolate files for a '
                                        'given binary.')
    AddCommonOptions(subp)
    subp.add_argument('--no-build', dest='build', default=True,
                      action='store_false',
                      help='Do not build, just isolate')
    subp.add_argument('-j', '--jobs', type=int,
                      help='Number of jobs to pass to ninja')
    subp.add_argument('path',
                      help='path build was generated into')
    subp.add_argument('target',
                      help='ninja target to generate the isolate for')
    subp.set_defaults(func=self.CmdIsolate)

    subp = subps.add_parser('lookup',
                            description='Look up the command for a given '
                                        'config or builder.')
    AddCommonOptions(subp)
    subp.add_argument('--quiet', default=False, action='store_true',
                      help='Print out just the arguments, '
                           'do not emulate the output of the gen subcommand.')
    subp.add_argument('--recursive', default=False, action='store_true',
                      help='Lookup arguments from imported files, '
                           'implies --quiet')
    subp.set_defaults(func=self.CmdLookup)

    subp = subps.add_parser('try',
                            description='Try your change on a remote builder')
    AddCommonOptions(subp)
    subp.add_argument('target',
                      help='ninja target to build and run')
    subp.add_argument('--force', default=False, action='store_true',
                      help='Force the job to run. Ignores local checkout state;'
                      ' by default, the tool doesn\'t trigger jobs if there are'
                      ' local changes which are not present on Gerrit.')
    subp.set_defaults(func=self.CmdTry)

    subp = subps.add_parser(
      'run', formatter_class=argparse.RawDescriptionHelpFormatter)
    subp.description = (
        'Build, isolate, and run the given binary with the command line\n'
        'listed in the isolate. You may pass extra arguments after the\n'
        'target; use "--" if the extra arguments need to include switches.\n'
        '\n'
        'Examples:\n'
        '\n'
        '  % tools/mb/mb.py run -m chromium.linux -b "Linux Builder" \\\n'
        '    //out/Default content_browsertests\n'
        '\n'
        '  % tools/mb/mb.py run out/Default content_browsertests\n'
        '\n'
        '  % tools/mb/mb.py run out/Default content_browsertests -- \\\n'
        '    --test-launcher-retry-limit=0'
        '\n'
    )
    AddCommonOptions(subp)
    subp.add_argument('-j', '--jobs', type=int,
                      help='Number of jobs to pass to ninja')
    subp.add_argument('--no-build', dest='build', default=True,
                      action='store_false',
                      help='Do not build, just isolate and run')
    subp.add_argument('path',
                      help=('path to generate build into (or use).'
                            ' This can be either a regular path or a '
                            'GN-style source-relative path like '
                            '//out/Default.'))
    subp.add_argument('-s', '--swarmed', action='store_true',
                      help='Run under swarming with the default dimensions')
    subp.add_argument('-d', '--dimension', default=[], action='append', nargs=2,
                      dest='dimensions', metavar='FOO bar',
                      help='dimension to filter on')
    subp.add_argument('--no-default-dimensions', action='store_false',
                      dest='default_dimensions', default=True,
                      help='Do not automatically add dimensions to the task')
    subp.add_argument('target',
                      help='ninja target to build and run')
    subp.add_argument('extra_args', nargs='*',
                      help=('extra args to pass to the isolate to run. Use '
                            '"--" as the first arg if you need to pass '
                            'switches'))
    subp.set_defaults(func=self.CmdRun)

    subp = subps.add_parser('validate',
                            description='Validate the config file.')
    subp.add_argument('-f', '--config-file', metavar='PATH',
                      help='path to config file (default is %(default)s)')
    subp.set_defaults(func=self.CmdValidate)

    subp = subps.add_parser('zip',
                            description='Generate a .zip containing the files '
                                        'needed for a given binary.')
    AddCommonOptions(subp)
    subp.add_argument('--no-build', dest='build', default=True,
                      action='store_false',
                      help='Do not build, just isolate')
    subp.add_argument('-j', '--jobs', type=int,
                      help='Number of jobs to pass to ninja')
    subp.add_argument('path',
                      help='path build was generated into')
    subp.add_argument('target',
                      help='ninja target to generate the isolate for')
    subp.add_argument('zip_path',
                      help='path to zip file to create')
    subp.set_defaults(func=self.CmdZip)

    subp = subps.add_parser('help',
                            help='Get help on a subcommand.')
    subp.add_argument(nargs='?', action='store', dest='subcommand',
                      help='The command to get help for.')
    subp.set_defaults(func=self.CmdHelp)

    self.args = parser.parse_args(argv)

    self.use_luci_auth = getattr(self.args, 'luci_auth', False)

    if (self.args.func != self.CmdValidate
        and getattr(self.args, 'config_file', None) is None):
      self.args.config_file = self.default_config


  def DumpInputFiles(self):

    def DumpContentsOfFilePassedTo(arg_name, path):
      if path and self.Exists(path):
        self.Print("\n# To recreate the file passed to %s:" % arg_name)
        self.Print("%% cat > %s <<EOF" % path)
        contents = self.ReadFile(path)
        self.Print(contents)
        self.Print("EOF\n%\n")

    if getattr(self.args, 'input_path', None):
      DumpContentsOfFilePassedTo(
          'argv[0] (input_path)', self.args.input_path)
    if getattr(self.args, 'swarming_targets_file', None):
      DumpContentsOfFilePassedTo(
          '--swarming-targets-file', self.args.swarming_targets_file)

  def CmdAnalyze(self):
    vals = self.Lookup()
    return self.RunGNAnalyze(vals)

  def CmdExport(self):
    self.ReadConfigFile(self.args.config_file)
    obj = {}
    for master, builders in self.masters.items():
      obj[master] = {}
      for builder in builders:
        config = self.masters[master][builder]
        if not config:
          continue

        if isinstance(config, dict):
          args = {
              k: FlattenConfig(self.configs, self.mixins, v)['gn_args']
              for k, v in config.items()
          }
        elif config.startswith('//'):
          args = config
        else:
          args = FlattenConfig(self.configs, self.mixins, config)['gn_args']
          if 'error' in args:
            continue

        obj[master][builder] = args

    # Dump object and trim trailing whitespace.
    s = '\n'.join(l.rstrip() for l in
                  json.dumps(obj, sort_keys=True, indent=2).splitlines())
    self.Print(s)
    return 0

  def CmdGen(self):
    vals = self.Lookup()
    return self.RunGNGen(vals)

  def CmdIsolateEverything(self):
    vals = self.Lookup()
    return self.RunGNGenAllIsolates(vals)

  def CmdHelp(self):
    if self.args.subcommand:
      self.ParseArgs([self.args.subcommand, '--help'])
    else:
      self.ParseArgs(['--help'])

  def CmdIsolate(self):
    vals = self.GetConfig()
    if not vals:
      return 1
    if self.args.build:
      ret = self.Build(self.args.target)
      if ret:
        return ret
    return self.RunGNIsolate(vals)

  def CmdLookup(self):
    vals = self.Lookup()
    gn_args = self.GNArgs(vals, expand_imports=self.args.recursive)
    if self.args.quiet or self.args.recursive:
      self.Print(gn_args, end='')
    else:
      cmd = self.GNCmd('gen', '_path_')
      self.Print('\nWriting """\\\n%s""" to _path_/args.gn.\n' % gn_args)
      env = None

      self.PrintCmd(cmd, env)
    return 0

  def CmdTry(self):
    ninja_target = self.args.target
    if ninja_target.startswith('//'):
      self.Print("Expected a ninja target like base_unittests, got %s" % (
        ninja_target))
      return 1

    _, out, _ = self.Run(['git', 'cl', 'diff', '--stat'], force_verbose=False)
    if out:
      self.Print("Your checkout appears to local changes which are not uploaded"
                 " to Gerrit. Changes must be committed and uploaded to Gerrit"
                 " to be tested using this tool.")
      if not self.args.force:
        return 1

    json_path = self.PathJoin(self.chromium_src_dir, 'out.json')
    try:
      ret, out, err = self.Run(
        ['git', 'cl', 'issue', '--json=out.json'], force_verbose=False)
      if ret != 0:
        self.Print(
          "Unable to fetch current issue. Output and error:\n%s\n%s" % (
            out, err
        ))
        return ret
      with open(json_path) as f:
        issue_data = json.load(f)
    finally:
      if self.Exists(json_path):
        os.unlink(json_path)

    if not issue_data['issue']:
      self.Print("Missing issue data. Upload your CL to Gerrit and try again.")
      return 1

    class LedException(Exception):
      pass

    def run_cmd(previous_res, cmd):
      if self.args.verbose:
        self.Print(('| ' if previous_res else '') + ' '.join(cmd))

      res, out, err = self.Call(cmd, stdin=previous_res)
      if res != 0:
        self.Print("Err while running '%s'. Output:\n%s\nstderr:\n%s" % (
          ' '.join(cmd), out, err))
        raise LedException()
      return out

    try:
      result = LedResult(None, run_cmd).then(
        # TODO(martiniss): maybe don't always assume the bucket?
        'led', 'get-builder', 'luci.chromium.try:%s' % self.args.builder).then(
        'led', 'edit', '-r', 'chromium_trybot_experimental',
          '-p', 'tests=["%s"]' % ninja_target).then(
        'led', 'edit-system', '--tag=purpose:user-debug-mb-try').then(
        'led', 'edit-cr-cl', issue_data['issue_url']).then(
        'led', 'launch').result
    except LedException:
      self.Print("If this is an unexpected error message, please file a bug"
                 " with https://goto.google.com/mb-try-bug")
      raise

    swarming_data = json.loads(result)['swarming']
    self.Print("Launched task at https://%s/task?id=%s" % (
      swarming_data['host_name'], swarming_data['task_id']))

  def CmdRun(self):
    vals = self.GetConfig()
    if not vals:
      return 1
    if self.args.build:
      self.Print('')
      ret = self.Build(self.args.target)
      if ret:
        return ret

    self.Print('')
    ret = self.RunGNIsolate(vals)
    if ret:
      return ret

    self.Print('')
    if self.args.swarmed:
      return self._RunUnderSwarming(self.args.path, self.args.target)
    else:
      return self._RunLocallyIsolated(self.args.path, self.args.target)

  def CmdZip(self):
    ret = self.CmdIsolate()
    if ret:
      return ret

    zip_dir = None
    try:
      zip_dir = self.TempDir()
      remap_cmd = [
          self.PathJoin(self.chromium_src_dir, 'tools', 'luci-go',
                        self.isolate_exe), 'remap', '-i',
          self.PathJoin(self.args.path, self.args.target + '.isolate'),
          '-outdir', zip_dir
      ]
      self.Run(remap_cmd)

      zip_path = self.args.zip_path
      with zipfile.ZipFile(
          zip_path, 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as fp:
        for root, _, files in os.walk(zip_dir):
          for filename in files:
            path = self.PathJoin(root, filename)
            fp.write(path, self.RelPath(path, zip_dir))
    finally:
      if zip_dir:
        self.RemoveDirectory(zip_dir)

  def _AddBaseSoftware(self, cmd):
    # HACK(iannucci): These packages SHOULD NOT BE HERE.
    # Remove method once Swarming Pool Task Templates are implemented.
    # crbug.com/812428

    # Read vpython version from pinned depot_tools manifest. Better than
    # duplicating the pin here. The pin file format is simple enough to parse
    # it inline here.
    manifest_path = self.PathJoin(self.chromium_src_dir, 'third_party',
                                  'depot_tools', 'cipd_manifest.txt')
    vpython_pkg = vpython_version = None
    for line in self.ReadFile(manifest_path).splitlines():
      # lines look like:
      # name/of/package version
      if 'vpython' in line and 'git_revision' in line:
        vpython_pkg, vpython_version = line.split()
        break
    if vpython_pkg is None:
      raise ValueError('unable to read vpython pin from %s' % (manifest_path, ))

    # Add in required base software. This should be kept in sync with the
    # `chromium_swarming` recipe module in build.git. All references to
    # `swarming_module` below are purely due to this.
    cipd_packages = [
      ('infra/python/cpython/${platform}',
       'version:2.7.15.chromium14'),
      ('infra/tools/luci/logdog/butler/${platform}',
       'git_revision:e1abc57be62d198b5c2f487bfb2fa2d2eb0e867c'),
    ]
    cipd_packages.append((vpython_pkg, vpython_version))
    cipd_packages.append((vpython_pkg.replace('vpython', 'vpython-native'),
                          vpython_version))

    for pkg, vers in cipd_packages:
      cmd.append('--cipd-package=.swarming_module:%s:%s' % (pkg, vers))

    # Add packages to $PATH
    cmd.extend([
      '--env-prefix=PATH', '.swarming_module',
      '--env-prefix=PATH', '.swarming_module/bin',
    ])

    # Add cache directives for vpython.
    vpython_cache_path = '.swarming_module_cache/vpython'
    cmd.extend([
      '--named-cache=swarming_module_cache_vpython', vpython_cache_path,
      '--env-prefix=VPYTHON_VIRTUALENV_ROOT', vpython_cache_path,
    ])

  def _RunUnderSwarming(self, build_dir, target):
    isolate_server = 'isolateserver.appspot.com'
    namespace = 'default-gzip'
    swarming_server = 'chromium-swarm.appspot.com'
    # TODO(dpranke): Look up the information for the target in
    # the //testing/buildbot.json file, if possible, so that we
    # can determine the isolate target, command line, and additional
    # swarming parameters, if possible.
    #
    # TODO(dpranke): Also, add support for sharding and merging results.
    dimensions = []
    swarming_pool = ''
    for k, v in self._DefaultDimensions() + self.args.dimensions:
      dimensions += ['-d', k, v]
      if k == 'pool':
        swarming_pool = v

    archive_json_path = self.ToSrcRelPath(
        '%s/%s.archive.json' % (build_dir, target))
    cmd = [
        self.PathJoin(self.chromium_src_dir, 'tools', 'luci-go',
                      self.isolate_exe),
        'archive',
        '-i',
        self.ToSrcRelPath('%s/%s.isolate' % (build_dir, target)),
        '-s',
        self.ToSrcRelPath('%s/%s.isolated' % (build_dir, target)),
        '-I',
        isolate_server,
        '-namespace',
        namespace,
        '-dump-json',
        archive_json_path,
    ]

    # Talking to the isolateserver may fail because we're not logged in.
    # We trap the command explicitly and rewrite the error output so that
    # the error message is actually correct for a Chromium check out.
    self.PrintCmd(cmd, env=None)
    ret, out, err = self.Run(cmd, force_verbose=False)
    if ret:
      self.Print('  -> returned %d' % ret)
      if out:
        self.Print(out, end='')
      if err:
        # The swarming client will return an exit code of 2 (via
        # argparse.ArgumentParser.error()) and print a message to indicate
        # that auth failed, so we have to parse the message to check.
        if (ret == 2 and 'Please login to' in err):
          err = err.replace(' auth.py', ' tools/swarming_client/auth.py')
          self.Print(err, end='', file=sys.stderr)

      return ret

    try:
      archive_hashes = json.loads(self.ReadFile(archive_json_path))
    except Exception:
      self.Print(
          'Failed to read JSON file "%s"' % archive_json_path, file=sys.stderr)
      return 1
    try:
      isolated_hash = archive_hashes[target]
    except Exception:
      self.Print(
          'Cannot find hash for "%s" in "%s", file content: %s' %
          (target, archive_json_path, archive_hashes),
          file=sys.stderr)
      return 1

    cmd = [
        self.executable,
        self.PathJoin('tools', 'swarming_client', 'swarming.py'),
          'run',
          '-s', isolated_hash,
          '-I', isolate_server,
          '--namespace', namespace,
          '-S', swarming_server,
          '--tags=purpose:user-debug-mb',
      ] + dimensions
    # TODO(crbug.com/812428): Remove this once all pools have migrated to task
    # templates.
    if not swarming_pool.endswith('.template'):
      self._AddBaseSoftware(cmd)
    if self.args.extra_args:
      cmd += ['--'] + self.args.extra_args
    self.Print('')
    ret, _, _ = self.Run(cmd, force_verbose=True, buffer_output=False)
    return ret

  def _RunLocallyIsolated(self, build_dir, target):
    cmd = [
        self.PathJoin(self.chromium_src_dir, 'tools', 'luci-go',
                      self.isolate_exe),
        'run',
        '-i',
        self.ToSrcRelPath('%s/%s.isolate' % (build_dir, target)),
    ]
    if self.args.extra_args:
      cmd += ['--'] + self.args.extra_args
    ret, _, _ = self.Run(cmd, force_verbose=True, buffer_output=False)
    return ret

  def _DefaultDimensions(self):
    if not self.args.default_dimensions:
      return []

    # This code is naive and just picks reasonable defaults per platform.
    if self.platform == 'darwin':
      os_dim = ('os', 'Mac-10.13')
    elif self.platform.startswith('linux'):
      os_dim = ('os', 'Ubuntu-16.04')
    elif self.platform == 'win32':
      os_dim = ('os', 'Windows-10')
    else:
      raise MBErr('unrecognized platform string "%s"' % self.platform)

    return [('pool', 'chromium.tests'),
            ('cpu', 'x86-64'),
            os_dim]

  def CmdValidate(self, print_ok=True):
    errs = []

    # Validate master config if a specific one isn't specified
    if getattr(self.args, 'config_file', None) is None:
      # Read the file to make sure it parses.
      self.ReadConfigFile(self.default_config)
    else:
      self.ReadConfigFile(self.args.config_file)

    # Build a list of all of the configs referenced by builders.
    all_configs = validation.GetAllConfigs(self.masters)

    # Check that every referenced args file or config actually exists.
    for config, loc in all_configs.items():
      if config.startswith('//'):
        if not self.Exists(self.ToAbsPath(config)):
          errs.append('Unknown args file "%s" referenced from "%s".' %
                      (config, loc))
      elif not config in self.configs:
        errs.append('Unknown config "%s" referenced from "%s".' %
                    (config, loc))

    # Check that every config and mixin is referenced.
    validation.CheckAllConfigsAndMixinsReferenced(errs, all_configs,
                                                  self.configs, self.mixins)

    validation.EnsureNoProprietaryMixins(errs, self.default_config,
                                         self.args.config_file, self.masters,
                                         self.configs, self.mixins)

    validation.CheckDuplicateConfigs(errs, self.configs, self.mixins,
                                     self.masters, FlattenConfig)

    if errs:
      raise MBErr(('mb config file %s has problems:' %
                   (self.args.config_file if self.args.config_file else self.
                    default_config)) + '\n  ' + '\n  '.join(errs))

    if print_ok:
      self.Print('mb config file %s looks ok.' %
                 (self.args.config_file
                  if self.args.config_file else self.default_config))
    return 0

  def GetConfig(self):
    build_dir = self.args.path

    vals = DefaultVals()
    if self.args.builder or self.args.master or self.args.config:
      vals = self.Lookup()
      # Re-run gn gen in order to ensure the config is consistent with the
      # build dir.
      self.RunGNGen(vals)
      return vals

    toolchain_path = self.PathJoin(self.ToAbsPath(build_dir),
                                   'toolchain.ninja')
    if not self.Exists(toolchain_path):
      self.Print('Must either specify a path to an existing GN build dir '
                 'or pass in a -m/-b pair or a -c flag to specify the '
                 'configuration')
      return {}

    vals['gn_args'] = self.GNArgsFromDir(build_dir)
    return vals

  def GNArgsFromDir(self, build_dir):
    args_contents = ""
    gn_args_path = self.PathJoin(self.ToAbsPath(build_dir), 'args.gn')
    if self.Exists(gn_args_path):
      args_contents = self.ReadFile(gn_args_path)
    gn_args = []
    for l in args_contents.splitlines():
      l = l.split('#', 2)[0].strip()
      if not l:
        continue
      (name, value) = l.split('=', 1)
      gn_args.append('%s=%s' % (name.strip(), value.strip()))

    return ' '.join(gn_args)

  def Lookup(self):
    self.ReadConfigFile(self.args.config_file)
    try:
      config = self.ConfigFromArgs()
    except MBErr as e:
      # TODO(crbug.com/912681) While iOS bots are migrated to use the
      # Chromium recipe, we want to ensure that we're checking MB's
      # configurations first before going to iOS.
      # This is to be removed once the migration is complete.
      vals = self.ReadIOSBotConfig()
      if not vals:
        raise e
      return vals

    # TODO(crbug.com/912681) Some iOS bots have a definition, with ios_error
    # as an indicator that it's incorrect. We utilize this to check the
    # iOS JSON instead, and error out if there exists no definition at all.
    # This is to be removed once the migration is complete.
    if config == 'ios_error':
      vals = self.ReadIOSBotConfig()
      if not vals:
        raise MBErr('No iOS definition was found. Please ensure there is a '
                    'definition for the given iOS bot under '
                    'mb_config.pyl or a JSON file definition under '
                    '//ios/build/bots.')
      return vals

    if config.startswith('//'):
      if not self.Exists(self.ToAbsPath(config)):
        raise MBErr('args file "%s" not found' % config)
      vals = DefaultVals()
      vals['args_file'] = config
    else:
      if not config in self.configs:
        raise MBErr(
            'Config "%s" not found in %s' % (config, self.args.config_file))
      vals = FlattenConfig(self.configs, self.mixins, config)
    return vals

  def ReadIOSBotConfig(self):
    if not self.args.master or not self.args.builder:
      return {}
    path = self.PathJoin(self.chromium_src_dir, 'ios', 'build', 'bots',
                         self.args.master, self.args.builder + '.json')
    if not self.Exists(path):
      return {}

    contents = json.loads(self.ReadFile(path))
    gn_args = ' '.join(contents.get('gn_args', []))

    vals = DefaultVals()
    vals['gn_args'] = gn_args
    return vals

  def ReadConfigFile(self, config_file):
    if not self.Exists(config_file):
      raise MBErr('config file not found at %s' % config_file)

    try:
      contents = ast.literal_eval(self.ReadFile(config_file))
    except SyntaxError as e:
      raise MBErr('Failed to parse config file "%s": %s' % (config_file, e))

    self.configs = contents['configs']
    self.mixins = contents['mixins']
    self.masters = contents.get('masters')
    self.public_artifact_builders = contents.get('public_artifact_builders')

  def ReadIsolateMap(self):
    if not self.args.isolate_map_files:
      self.args.isolate_map_files = [self.default_isolate_map]

    for f in self.args.isolate_map_files:
      if not self.Exists(f):
        raise MBErr('isolate map file not found at %s' % f)
    isolate_maps = {}
    for isolate_map in self.args.isolate_map_files:
      try:
        isolate_map = ast.literal_eval(self.ReadFile(isolate_map))
        duplicates = set(isolate_map).intersection(isolate_maps)
        if duplicates:
          raise MBErr(
              'Duplicate targets in isolate map files: %s.' %
              ', '.join(duplicates))
        isolate_maps.update(isolate_map)
      except SyntaxError as e:
        raise MBErr(
            'Failed to parse isolate map file "%s": %s' % (isolate_map, e))
    return isolate_maps

  def ConfigFromArgs(self):
    if self.args.config:
      if self.args.master or self.args.builder:
        raise MBErr('Can not specific both -c/--config and -m/--master or '
                    '-b/--builder')

      return self.args.config

    if not self.args.master or not self.args.builder:
      raise MBErr('Must specify either -c/--config or '
                  '(-m/--master and -b/--builder)')

    if not self.args.master in self.masters:
      raise MBErr('Master name "%s" not found in "%s"' %
                  (self.args.master, self.args.config_file))

    if not self.args.builder in self.masters[self.args.master]:
      raise MBErr('Builder name "%s"  not found under masters[%s] in "%s"' %
                  (self.args.builder, self.args.master, self.args.config_file))

    config = self.masters[self.args.master][self.args.builder]
    if isinstance(config, dict):
      if self.args.phase is None:
        raise MBErr('Must specify a build --phase for %s on %s' %
                    (self.args.builder, self.args.master))
      phase = str(self.args.phase)
      if phase not in config:
        raise MBErr('Phase %s doesn\'t exist for %s on %s' %
                    (phase, self.args.builder, self.args.master))
      return config[phase]

    if self.args.phase is not None:
      raise MBErr('Must not specify a build --phase for %s on %s' %
                  (self.args.builder, self.args.master))
    return config

  def RunGNGen(self, vals, compute_inputs_for_analyze=False, check=True):
    build_dir = self.args.path

    if check:
      cmd = self.GNCmd('gen', build_dir, '--check')
    else:
      cmd = self.GNCmd('gen', build_dir)
    gn_args = self.GNArgs(vals)
    if compute_inputs_for_analyze:
      gn_args += ' compute_inputs_for_analyze=true'

    # Since GN hasn't run yet, the build directory may not even exist.
    self.MaybeMakeDirectory(self.ToAbsPath(build_dir))

    gn_args_path = self.ToAbsPath(build_dir, 'args.gn')
    self.WriteFile(gn_args_path, gn_args, force_verbose=True)

    if getattr(self.args, 'swarming_targets_file', None):
      # We need GN to generate the list of runtime dependencies for
      # the compile targets listed (one per line) in the file so
      # we can run them via swarming. We use gn_isolate_map.pyl to convert
      # the compile targets to the matching GN labels.
      path = self.args.swarming_targets_file
      if not self.Exists(path):
        self.WriteFailureAndRaise('"%s" does not exist' % path,
                                  output_path=None)
      contents = self.ReadFile(path)
      isolate_targets = set(contents.splitlines())

      isolate_map = self.ReadIsolateMap()
      self.RemovePossiblyStaleRuntimeDepsFiles(vals, isolate_targets,
                                               isolate_map, build_dir)

      err, labels = self.MapTargetsToLabels(isolate_map, isolate_targets)
      if err:
        raise MBErr(err)

      gn_runtime_deps_path = self.ToAbsPath(build_dir, 'runtime_deps')
      self.WriteFile(gn_runtime_deps_path, '\n'.join(labels) + '\n')
      cmd.append('--runtime-deps-list-file=%s' % gn_runtime_deps_path)

    # Detect if we are running in a vpython interpreter, and if so force GN to
    # use the real python interpreter. crbug.com/1049421
    # This ensures that ninja will only use the real python interpreter and not
    # vpython, so that any python scripts in the build will only use python
    # modules vendored into //third_party.
    # This can be deleted when python 3 becomes the only supported interpreter,
    # because in python 3 vpython will no longer have its current 'viral'
    # qualities and will require explicit usage to opt in to.
    prefix = getattr(sys, "real_prefix", sys.prefix)
    python_exe = 'python.exe' if self.platform.startswith('win') else 'python'
    # The value of prefix varies. Sometimes it extends to include the bin/
    # directory of the python install such that prefix/python is the
    # interpreter, and other times prefix/bin/python is the interpreter.
    # Therefore we need to check both. Also, it is safer to check prefix/bin
    # first because there have been previous installs where prefix/bin/python
    # was the real binary and prefix/python was actually vpython-native.
    possible_python_locations = [
        os.path.join(prefix, 'bin', python_exe),
        os.path.join(prefix, python_exe),
    ]
    for p in possible_python_locations:
      if os.path.isfile(p):
        cmd.append('--script-executable=%s' % p)
        break
    else:
      self.Print('python interpreter not under %s' % prefix)

    ret, output, _ = self.Run(cmd)
    if ret:
      if self.args.json_output:
        # write errors to json.output
        self.WriteJSON({'output': output}, self.args.json_output)
      # If `gn gen` failed, we should exit early rather than trying to
      # generate isolates. Run() will have already logged any error output.
      self.Print('GN gen failed: %d' % ret)
      return ret

    if getattr(self.args, 'swarming_targets_file', None):
      ret = self.GenerateIsolates(vals, isolate_targets, isolate_map, build_dir)

    return ret

  def RunGNGenAllIsolates(self, vals):
    """
    This command generates all .isolate files.

    This command assumes that "mb.py gen" has already been run, as it relies on
    "gn ls" to fetch all gn targets. If uses that output, combined with the
    isolate_map, to determine all isolates that can be generated for the current
    gn configuration.
    """
    build_dir = self.args.path
    ret, output, _ = self.Run(self.GNCmd('ls', build_dir),
                              force_verbose=False)
    if ret:
      # If `gn ls` failed, we should exit early rather than trying to
      # generate isolates.
      self.Print('GN ls failed: %d' % ret)
      return ret

    # Create a reverse map from isolate label to isolate dict.
    isolate_map = self.ReadIsolateMap()
    isolate_dict_map = {}
    for key, isolate_dict in isolate_map.iteritems():
      isolate_dict_map[isolate_dict['label']] = isolate_dict
      isolate_dict_map[isolate_dict['label']]['isolate_key'] = key

    runtime_deps = []

    isolate_targets = []
    # For every GN target, look up the isolate dict.
    for line in output.splitlines():
      target = line.strip()
      if target in isolate_dict_map:
        if isolate_dict_map[target]['type'] == 'additional_compile_target':
          # By definition, additional_compile_targets are not tests, so we
          # shouldn't generate isolates for them.
          continue

        isolate_targets.append(isolate_dict_map[target]['isolate_key'])
        runtime_deps.append(target)

    self.RemovePossiblyStaleRuntimeDepsFiles(vals, isolate_targets,
                                             isolate_map, build_dir)

    gn_runtime_deps_path = self.ToAbsPath(build_dir, 'runtime_deps')
    self.WriteFile(gn_runtime_deps_path, '\n'.join(runtime_deps) + '\n')
    cmd = self.GNCmd('gen', build_dir)
    cmd.append('--runtime-deps-list-file=%s' % gn_runtime_deps_path)
    self.Run(cmd)

    return self.GenerateIsolates(vals, isolate_targets, isolate_map, build_dir)

  def RemovePossiblyStaleRuntimeDepsFiles(self, vals, targets, isolate_map,
                                          build_dir):
    # TODO(crbug.com/932700): Because `gn gen --runtime-deps-list-file`
    # puts the runtime_deps file in different locations based on the actual
    # type of a target, we may end up with multiple possible runtime_deps
    # files in a given build directory, where some of the entries might be
    # stale (since we might be reusing an existing build directory).
    #
    # We need to be able to get the right one reliably; you might think
    # we can just pick the newest file, but because GN won't update timestamps
    # if the contents of the files change, an older runtime_deps
    # file might actually be the one we should use over a newer one (see
    # crbug.com/932387 for a more complete explanation and example).
    #
    # In order to avoid this, we need to delete any possible runtime_deps
    # files *prior* to running GN. As long as the files aren't actually
    # needed during the build, this hopefully will not cause unnecessary
    # build work, and so it should be safe.
    #
    # Ultimately, we should just make sure we get the runtime_deps files
    # in predictable locations so we don't have this issue at all, and
    # that's what crbug.com/932700 is for.
    possible_rpaths = self.PossibleRuntimeDepsPaths(vals, targets, isolate_map)
    for rpaths in possible_rpaths.values():
      for rpath in rpaths:
        path = self.ToAbsPath(build_dir, rpath)
        if self.Exists(path):
          self.RemoveFile(path)

  def GenerateIsolates(self, vals, ninja_targets, isolate_map, build_dir):
    """
    Generates isolates for a list of ninja targets.

    Ninja targets are transformed to GN targets via isolate_map.

    This function assumes that a previous invocation of "mb.py gen" has
    generated runtime deps for all targets.
    """
    possible_rpaths = self.PossibleRuntimeDepsPaths(vals, ninja_targets,
                                                    isolate_map)

    for target, rpaths in possible_rpaths.items():
      # TODO(crbug.com/932700): We don't know where each .runtime_deps
      # file might be, but assuming we called
      # RemovePossiblyStaleRuntimeDepsFiles prior to calling `gn gen`,
      # there should only be one file.
      found_one = False
      path_to_use = None
      for r in rpaths:
        path = self.ToAbsPath(build_dir, r)
        if self.Exists(path):
          if found_one:
            raise MBErr('Found more than one of %s' % ', '.join(rpaths))
          path_to_use = path
          found_one = True

      if not found_one:
        raise MBErr('Did not find any of %s' % ', '.join(rpaths))

      command, extra_files = self.GetIsolateCommand(target, vals)
      runtime_deps = self.ReadFile(path_to_use).splitlines()

      canonical_target = target.replace(':','_').replace('/','_')
      ret = self.WriteIsolateFiles(build_dir, command, canonical_target,
                                   runtime_deps, vals, extra_files)
      if ret:
        return ret
    return 0

  def PossibleRuntimeDepsPaths(self, vals, ninja_targets, isolate_map):
    """Returns a map of targets to possible .runtime_deps paths.

    Each ninja target maps on to a GN label, but depending on the type
    of the GN target, `gn gen --runtime-deps-list-file` will write
    the .runtime_deps files into different locations. Unfortunately, in
    some cases we don't actually know which of multiple locations will
    actually be used, so we return all plausible candidates.

    The paths that are returned are relative to the build directory.
    """

    android = 'target_os="android"' in vals['gn_args']
    ios = 'target_os="ios"' in vals['gn_args']
    fuchsia = 'target_os="fuchsia"' in vals['gn_args']
    win = self.platform == 'win32' or 'target_os="win"' in vals['gn_args']
    possible_runtime_deps_rpaths = {}
    for target in ninja_targets:
      target_type = isolate_map[target]['type']
      label = isolate_map[target]['label']
      stamp_runtime_deps = 'obj/%s.stamp.runtime_deps' % label.replace(':', '/')
      # TODO(https://crbug.com/876065): 'official_tests' use
      # type='additional_compile_target' to isolate tests. This is not the
      # intended use for 'additional_compile_target'.
      if (target_type == 'additional_compile_target' and
          target != 'official_tests'):
        # By definition, additional_compile_targets are not tests, so we
        # shouldn't generate isolates for them.
        raise MBErr('Cannot generate isolate for %s since it is an '
                    'additional_compile_target.' % target)
      elif fuchsia or ios or target_type == 'generated_script':
        # iOS and Fuchsia targets end up as groups.
        # generated_script targets are always actions.
        rpaths = [stamp_runtime_deps]
      elif android:
        # Android targets may be either android_apk or executable. The former
        # will result in runtime_deps associated with the stamp file, while the
        # latter will result in runtime_deps associated with the executable.
        label = isolate_map[target]['label']
        rpaths = [
            target + '.runtime_deps',
            stamp_runtime_deps]
      elif (target_type == 'script' or target_type == 'windowed_script'
            or isolate_map[target].get('label_type') == 'group'):
        # For script targets, the build target is usually a group,
        # for which gn generates the runtime_deps next to the stamp file
        # for the label, which lives under the obj/ directory, but it may
        # also be an executable.
        label = isolate_map[target]['label']
        rpaths = [stamp_runtime_deps]
        if win:
          rpaths += [ target + '.exe.runtime_deps' ]
        else:
          rpaths += [ target + '.runtime_deps' ]
      elif win:
        rpaths = [target + '.exe.runtime_deps']
      else:
        rpaths = [target + '.runtime_deps']

      possible_runtime_deps_rpaths[target] = rpaths

    return possible_runtime_deps_rpaths

  def RunGNIsolate(self, vals):
    target = self.args.target
    isolate_map = self.ReadIsolateMap()
    err, labels = self.MapTargetsToLabels(isolate_map, [target])
    if err:
      raise MBErr(err)

    label = labels[0]

    build_dir = self.args.path
    command, extra_files = self.GetIsolateCommand(target, vals)

    # Any warning for an unused arg will get interleaved into the cmd's stdout.
    # When that happens, the isolate step below will fail with an obscure error
    # when it tries processing the lines of the warning. Fail quickly in that
    # case to avoid confusion.
    cmd = self.GNCmd('desc', build_dir, label, 'runtime_deps',
                     '--fail-on-unused-args')
    ret, out, _ = self.Call(cmd)
    if ret:
      if out:
        self.Print(out)
      return ret

    runtime_deps = out.splitlines()

    ret = self.WriteIsolateFiles(build_dir, command, target, runtime_deps, vals,
                                 extra_files)
    if ret:
      return ret

    ret, _, _ = self.Run([
        self.PathJoin(self.chromium_src_dir, 'tools', 'luci-go',
                      self.isolate_exe),
        'check',
        '-i',
        self.ToSrcRelPath('%s/%s.isolate' % (build_dir, target)),
    ],
                         buffer_output=False)

    return ret

  def WriteIsolateFiles(self, build_dir, command, target, runtime_deps, vals,
                        extra_files):
    isolate_path = self.ToAbsPath(build_dir, target + '.isolate')
    files = sorted(set(runtime_deps + extra_files))

    # Complain if any file is a directory that's inside the build directory,
    # since that makes incremental builds incorrect. See
    # https://crbug.com/912946
    is_android = 'target_os="android"' in vals['gn_args']
    is_cros = ('target_os="chromeos"' in vals['gn_args']
               or 'is_chromeos_device=true' in vals['gn_args']
               or vals.get('cros_passthrough', False))
    is_mac = self.platform == 'darwin'
    is_msan = 'is_msan=true' in vals['gn_args']
    is_ios = 'target_os="ios"' in vals['gn_args']

    err = ''
    for f in files:
      # Skip a few configs that need extra cleanup for now.
      # TODO(https://crbug.com/912946): Fix everything on all platforms and
      # enable check everywhere.
      if is_android:
        break

      # iOS has generated directories in gn data items.
      # Skipping for iOS instead of listing all apps.
      if is_ios:
        break

      # Skip a few existing violations that need to be cleaned up. Each of
      # these will lead to incorrect incremental builds if their directory
      # contents change. Do not add to this list, except for mac bundles until
      # crbug.com/1000667 is fixed.
      # TODO(https://crbug.com/912946): Remove this if statement.
      if ((is_msan and f == 'instrumented_libraries_prebuilt/')
          or f == 'mr_extension/' or  # https://crbug.com/997947
          f.startswith('nacl_test_data/') or
          f.startswith('ppapi_nacl_tests_libs/') or
          (is_cros and f in (  # https://crbug.com/1002509
              'chromevox_test_data/',
              'gen/ui/file_manager/file_manager/',
              'resources/chromeos/',
              'resources/chromeos/accessibility/autoclick/',
              'resources/chromeos/accessibility/chromevox/',
              'resources/chromeos/accessibility/select_to_speak/',
              'test_data/chrome/browser/resources/chromeos/accessibility/'
              'autoclick/',
              'test_data/chrome/browser/resources/chromeos/accessibility/'
              'chromevox/',
              'test_data/chrome/browser/resources/chromeos/accessibility/'
              'select_to_speak/',
          )) or (is_mac and f in (  # https://crbug.com/1000667
              'AlertNotificationService.xpc/',
              'Chromium Framework.framework/',
              'Chromium Helper.app/',
              'Chromium.app/',
              'ChromiumUpdater.app/',
              'Content Shell.app/',
              'Google Chrome Framework.framework/',
              'Google Chrome Helper (GPU).app/',
              'Google Chrome Helper (Plugin).app/',
              'Google Chrome Helper (Renderer).app/',
              'Google Chrome Helper.app/',
              'Google Chrome.app/',
              'GoogleUpdater.app/',
              'UpdaterTestApp Framework.framework/',
              'UpdaterTestApp.app/',
              'blink_deprecated_test_plugin.plugin/',
              'blink_test_plugin.plugin/',
              'corb_test_plugin.plugin/',
              'obj/tools/grit/brotli_mac_asan_workaround/',
              'power_saver_test_plugin.plugin/',
              'ppapi_tests.plugin/',
              'ui_unittests Framework.framework/',
          ))):
        continue

      # This runs before the build, so we can't use isdir(f). But
      # isolate.py luckily requires data directories to end with '/', so we
      # can check for that.
      if not f.startswith('../../') and f.endswith('/'):
        # Don't use self.PathJoin() -- all involved paths consistently use
        # forward slashes, so don't add one single backslash on Windows.
        err += '\n' + build_dir + '/' +  f

    if err:
      self.Print('error: gn `data` items may not list generated directories; '
                 'list files in directory instead for:' + err)
      return 1

    self.WriteFile(isolate_path,
      pprint.pformat({
        'variables': {
          'command': command,
          'files': files,
        }
      }) + '\n')

    self.WriteJSON(
      {
        'args': [
          '--isolated',
          self.ToSrcRelPath('%s/%s.isolated' % (build_dir, target)),
          '--isolate',
          self.ToSrcRelPath('%s/%s.isolate' % (build_dir, target)),
        ],
        'dir': self.chromium_src_dir,
        'version': 1,
      },
      isolate_path + 'd.gen.json',
    )

  def MapTargetsToLabels(self, isolate_map, targets):
    labels = []
    err = ''

    for target in targets:
      if target == 'all':
        labels.append(target)
      elif target.startswith('//'):
        labels.append(target)
      else:
        if target in isolate_map:
          if isolate_map[target]['type'] == 'unknown':
            err += ('test target "%s" type is unknown\n' % target)
          else:
            labels.append(isolate_map[target]['label'])
        else:
          err += ('target "%s" not found in '
                  '//testing/buildbot/gn_isolate_map.pyl\n' % target)

    return err, labels

  def GNCmd(self, subcommand, path, *args):
    if self.platform == 'linux2':
      subdir, exe = 'linux64', 'gn'
    elif self.platform == 'darwin':
      subdir, exe = 'mac', 'gn'
    elif self.platform == 'aix6':
      subdir, exe = 'aix', 'gn'
    else:
      subdir, exe = 'win', 'gn.exe'

    gn_path = self.PathJoin(self.chromium_src_dir, 'buildtools', subdir, exe)
    return [gn_path, subcommand, path] + list(args)


  def GNArgs(self, vals, expand_imports=False):
    if vals['cros_passthrough']:
      if not 'GN_ARGS' in os.environ:
        raise MBErr('MB is expecting GN_ARGS to be in the environment')
      gn_args = os.environ['GN_ARGS']
      if not re.search('target_os.*=.*"chromeos"', gn_args):
        raise MBErr('GN_ARGS is missing target_os = "chromeos": (GN_ARGS=%s)' %
                    gn_args)
      if vals['gn_args']:
        gn_args += ' ' + vals['gn_args']
    else:
      gn_args = vals['gn_args']

    if self.args.goma_dir:
      gn_args += ' goma_dir="%s"' % self.args.goma_dir

    android_version_code = self.args.android_version_code
    if android_version_code:
      gn_args += ' android_default_version_code="%s"' % android_version_code

    android_version_name = self.args.android_version_name
    if android_version_name:
      gn_args += ' android_default_version_name="%s"' % android_version_name

    args_gn_lines = []
    parsed_gn_args = {}

    # If we're using the Simple Chrome SDK, add a comment at the top that
    # points to the doc. This must happen after the gn_helpers.ToGNString()
    # call above since gn_helpers strips comments.
    if vals['cros_passthrough']:
      args_gn_lines.extend([
          '# These args are generated via the Simple Chrome SDK. See the link',
          '# below for more details:',
          '# https://chromium.googlesource.com/chromiumos/docs/+/master/simple_chrome_workflow.md',  # pylint: disable=line-too-long
      ])

    args_file = vals.get('args_file', None)
    if args_file:
      if expand_imports:
        content = self.ReadFile(self.ToAbsPath(args_file))
        parsed_gn_args = gn_helpers.FromGNArgs(content)
      else:
        args_gn_lines.append('import("%s")' % args_file)

    # Canonicalize the arg string into a sorted, newline-separated list
    # of key-value pairs, and de-dup the keys if need be so that only
    # the last instance of each arg is listed.
    parsed_gn_args.update(gn_helpers.FromGNArgs(gn_args))
    args_gn_lines.append(gn_helpers.ToGNString(parsed_gn_args))

    return '\n'.join(args_gn_lines)

  def GetIsolateCommand(self, target, vals):
    isolate_map = self.ReadIsolateMap()

    is_android = 'target_os="android"' in vals['gn_args']
    is_fuchsia = 'target_os="fuchsia"' in vals['gn_args']
    is_cros = 'target_os="chromeos"' in vals['gn_args']
    is_ios = 'target_os="ios"' in vals['gn_args']
    is_cros_device = ('is_chromeos_device=true' in vals['gn_args']
                      or vals.get('cros_passthrough', False))
    is_mac = self.platform == 'darwin'
    is_win = self.platform == 'win32' or 'target_os="win"' in vals['gn_args']

    # This should be true if tests with type='windowed_test_launcher' or
    # type='windowed_script' are expected to run using xvfb. For example,
    # Linux Desktop, X11 CrOS and Ozone CrOS builds on Linux (xvfb is not used
    # on CrOS HW or VMs). Note that one Ozone build can be used to run
    # different backends. Currently, tests are executed for the headless and
    # X11 backends and both can run under Xvfb on Linux.
    # TODO(tonikitoo,msisov,fwang): Find a way to run tests for the Wayland
    # backend.
    use_xvfb = (self.platform == 'linux2' and not is_android and not is_fuchsia
                and not is_cros_device)

    asan = 'is_asan=true' in vals['gn_args']
    msan = 'is_msan=true' in vals['gn_args']
    tsan = 'is_tsan=true' in vals['gn_args']
    cfi_diag = 'use_cfi_diag=true' in vals['gn_args']
    clang_coverage = 'use_clang_coverage=true' in vals['gn_args']
    java_coverage = 'use_jacoco_coverage=true' in vals['gn_args']

    test_type = isolate_map[target]['type']
    use_python3 = isolate_map[target].get('use_python3', False)

    executable = isolate_map[target].get('executable', target)
    executable_suffix = isolate_map[target].get(
        'executable_suffix', '.exe' if is_win else '')

    # TODO(crbug.com/1060857): Remove this once swarming task templates
    # support command prefixes.
    if self.use_luci_auth:
      cmdline = ['luci-auth.exe' if is_win else 'luci-auth', 'context', '--']
    else:
      cmdline = []

    if use_python3:
      cmdline += ['vpython3']
      extra_files = ['../../.vpython3']
    else:
      cmdline += ['vpython']
      extra_files = ['../../.vpython']
    extra_files += [
      '../../testing/test_env.py',
    ]

    if test_type == 'nontest':
      self.WriteFailureAndRaise('We should not be isolating %s.' % target,
                                output_path=None)

    if test_type == 'generated_script':
      script = isolate_map[target]['script']
      if self.platform == 'win32':
        script += '.bat'
      cmdline += [
          '../../testing/test_env.py',
          script,
      ]
    elif is_ios and test_type != "raw":
      # iOS commands are all wrapped with generate_wrapper. Some targets
      # shared with iOS aren't defined with generated_script (ie/ basic_
      # unittests) so we force those to follow iOS' execution process by
      # mimicking what generated_script would do
      script = 'bin/run_{}'.format(target)
      cmdline += ['../../testing/test_env.py', script]
    elif is_android and test_type not in ('script', 'windowed_script'):
      if asan:
        cmdline += [os.path.join('bin', 'run_with_asan'), '--']
      cmdline += [
          '../../testing/test_env.py',
          '../../build/android/test_wrapper/logdog_wrapper.py',
          '--target', target,
          '--logdog-bin-cmd', '../../.task_template_packages/logdog_butler',
          '--store-tombstones']
      if clang_coverage or java_coverage:
        cmdline += ['--coverage-dir', '${ISOLATED_OUTDIR}']
    elif is_fuchsia and test_type not in ('script', 'windowed_script'):
      cmdline += [
          '../../testing/test_env.py',
          os.path.join('bin', 'run_%s' % target),
          '--test-launcher-bot-mode',
          '--system-log-file', '${ISOLATED_OUTDIR}/system_log'
      ]
    elif is_cros_device and test_type not in ('script', 'windowed_script'):
      cmdline += [
          '../../testing/test_env.py',
          os.path.join('bin', 'run_%s' % target),
      ]
    elif use_xvfb and test_type == 'windowed_test_launcher':
      extra_files.append('../../testing/xvfb.py')
      cmdline += [
          '../../testing/xvfb.py',
          './' + str(executable) + executable_suffix,
          '--test-launcher-bot-mode',
          '--asan=%d' % asan,
          # Enable lsan when asan is enabled except on Windows where LSAN isn't
          # supported.
          # TODO(https://crbug.com/948939): Enable on Mac once things pass.
          # TODO(https://crbug.com/974478): Enable on ChromeOS once things pass.
          '--lsan=%d' % (asan and not is_mac and not is_win and not is_cros),
          '--msan=%d' % msan,
          '--tsan=%d' % tsan,
          '--cfi-diag=%d' % cfi_diag,
      ]
    elif test_type in ('windowed_test_launcher', 'console_test_launcher'):
      cmdline += [
          '../../testing/test_env.py',
          './' + str(executable) + executable_suffix,
          '--test-launcher-bot-mode',
          '--asan=%d' % asan,
          # Enable lsan when asan is enabled except on Windows where LSAN isn't
          # supported.
          # TODO(https://crbug.com/948939): Enable on Mac once things pass.
          # TODO(https://crbug.com/974478): Enable on ChromeOS once things pass.
          '--lsan=%d' % (asan and not is_mac and not is_win and not is_cros),
          '--msan=%d' % msan,
          '--tsan=%d' % tsan,
          '--cfi-diag=%d' % cfi_diag,
      ]
    elif use_xvfb and test_type == 'windowed_script':
      extra_files.append('../../testing/xvfb.py')
      cmdline += [
          '../../testing/xvfb.py',
          '../../' + self.ToSrcRelPath(isolate_map[target]['script'])
      ]
    elif test_type in ('script', 'windowed_script'):
      # If we're testing a CrOS simplechrome build, assume we need to prepare a
      # DUT for testing. So prepend the command to run with the test wrapper.
      if is_cros_device:
        cmdline = [
            os.path.join('bin', 'cros_test_wrapper'),
            '--logs-dir=${ISOLATED_OUTDIR}',
        ]
      cmdline += [
          '../../testing/test_env.py',
          '../../' + self.ToSrcRelPath(isolate_map[target]['script'])
      ]
    elif test_type in ('raw', 'additional_compile_target'):
      cmdline = [
          './' + str(target) + executable_suffix,
      ]
    else:
      self.WriteFailureAndRaise('No command line for %s found (test type %s).'
                                % (target, test_type), output_path=None)

    cmdline += isolate_map[target].get('args', [])

    return cmdline, extra_files

  def ToAbsPath(self, build_path, *comps):
    return self.PathJoin(self.chromium_src_dir,
                         self.ToSrcRelPath(build_path),
                         *comps)

  def ToSrcRelPath(self, path):
    """Returns a relative path from the top of the repo."""
    if path.startswith('//'):
      return path[2:].replace('/', self.sep)
    return self.RelPath(path, self.chromium_src_dir)

  def RunGNAnalyze(self, vals):
    # Analyze runs before 'gn gen' now, so we need to run gn gen
    # in order to ensure that we have a build directory.
    ret = self.RunGNGen(vals, compute_inputs_for_analyze=True, check=False)
    if ret:
      return ret

    build_path = self.args.path
    input_path = self.args.input_path
    gn_input_path = input_path + '.gn'
    output_path = self.args.output_path
    gn_output_path = output_path + '.gn'

    inp = self.ReadInputJSON(['files', 'test_targets',
                              'additional_compile_targets'])
    if self.args.verbose:
      self.Print()
      self.Print('analyze input:')
      self.PrintJSON(inp)
      self.Print()


    # This shouldn't normally happen, but could due to unusual race conditions,
    # like a try job that gets scheduled before a patch lands but runs after
    # the patch has landed.
    if not inp['files']:
      self.Print('Warning: No files modified in patch, bailing out early.')
      self.WriteJSON({
            'status': 'No dependency',
            'compile_targets': [],
            'test_targets': [],
          }, output_path)
      return 0

    gn_inp = {}
    gn_inp['files'] = ['//' + f for f in inp['files'] if not f.startswith('//')]

    isolate_map = self.ReadIsolateMap()
    err, gn_inp['additional_compile_targets'] = self.MapTargetsToLabels(
        isolate_map, inp['additional_compile_targets'])
    if err:
      raise MBErr(err)

    err, gn_inp['test_targets'] = self.MapTargetsToLabels(
        isolate_map, inp['test_targets'])
    if err:
      raise MBErr(err)
    labels_to_targets = {}
    for i, label in enumerate(gn_inp['test_targets']):
      labels_to_targets[label] = inp['test_targets'][i]

    try:
      self.WriteJSON(gn_inp, gn_input_path)
      cmd = self.GNCmd('analyze', build_path, gn_input_path, gn_output_path)
      ret, output, _ = self.Run(cmd, force_verbose=True)
      if ret:
        if self.args.json_output:
          # write errors to json.output
          self.WriteJSON({'output': output}, self.args.json_output)
        return ret

      gn_outp_str = self.ReadFile(gn_output_path)
      try:
        gn_outp = json.loads(gn_outp_str)
      except Exception as e:
        self.Print("Failed to parse the JSON string GN returned: %s\n%s"
                   % (repr(gn_outp_str), str(e)))
        raise

      outp = {}
      if 'status' in gn_outp:
        outp['status'] = gn_outp['status']
      if 'error' in gn_outp:
        outp['error'] = gn_outp['error']
      if 'invalid_targets' in gn_outp:
        outp['invalid_targets'] = gn_outp['invalid_targets']
      if 'compile_targets' in gn_outp:
        all_input_compile_targets = sorted(
            set(inp['test_targets'] + inp['additional_compile_targets']))

        # If we're building 'all', we can throw away the rest of the targets
        # since they're redundant.
        if 'all' in gn_outp['compile_targets']:
          outp['compile_targets'] = ['all']
        else:
          outp['compile_targets'] = gn_outp['compile_targets']

        # crbug.com/736215: When GN returns targets back, for targets in
        # the default toolchain, GN will have generated a phony ninja
        # target matching the label, and so we can safely (and easily)
        # transform any GN label into the matching ninja target. For
        # targets in other toolchains, though, GN doesn't generate the
        # phony targets, and we don't know how to turn the labels into
        # compile targets. In this case, we also conservatively give up
        # and build everything. Probably the right thing to do here is
        # to have GN return the compile targets directly.
        if any("(" in target for target in outp['compile_targets']):
          self.Print('WARNING: targets with non-default toolchains were '
                     'found, building everything instead.')
          outp['compile_targets'] = all_input_compile_targets
        else:
          outp['compile_targets'] = [
              label.replace('//', '') for label in outp['compile_targets']]

        # Windows has a maximum command line length of 8k; even Linux
        # maxes out at 128k; if analyze returns a *really long* list of
        # targets, we just give up and conservatively build everything instead.
        # Probably the right thing here is for ninja to support response
        # files as input on the command line
        # (see https://github.com/ninja-build/ninja/issues/1355).
        # Android targets use a lot of templates and often exceed 7kb.
        # https://crbug.com/946266
        max_cmd_length_kb = 64 if platform.system() == 'Linux' else 7

        if len(' '.join(outp['compile_targets'])) > max_cmd_length_kb * 1024:
          self.Print('WARNING: Too many compile targets were affected.')
          self.Print('WARNING: Building everything instead to avoid '
                     'command-line length issues.')
          outp['compile_targets'] = all_input_compile_targets


      if 'test_targets' in gn_outp:
        outp['test_targets'] = [
          labels_to_targets[label] for label in gn_outp['test_targets']]

      if self.args.verbose:
        self.Print()
        self.Print('analyze output:')
        self.PrintJSON(outp)
        self.Print()

      self.WriteJSON(outp, output_path)

    finally:
      if self.Exists(gn_input_path):
        self.RemoveFile(gn_input_path)
      if self.Exists(gn_output_path):
        self.RemoveFile(gn_output_path)

    return 0

  def ReadInputJSON(self, required_keys):
    path = self.args.input_path
    output_path = self.args.output_path
    if not self.Exists(path):
      self.WriteFailureAndRaise('"%s" does not exist' % path, output_path)

    try:
      inp = json.loads(self.ReadFile(path))
    except Exception as e:
      self.WriteFailureAndRaise('Failed to read JSON input from "%s": %s' %
                                (path, e), output_path)

    for k in required_keys:
      if not k in inp:
        self.WriteFailureAndRaise('input file is missing a "%s" key' % k,
                                  output_path)

    return inp

  def WriteFailureAndRaise(self, msg, output_path):
    if output_path:
      self.WriteJSON({'error': msg}, output_path, force_verbose=True)
    raise MBErr(msg)

  def WriteJSON(self, obj, path, force_verbose=False):
    try:
      self.WriteFile(path, json.dumps(obj, indent=2, sort_keys=True) + '\n',
                     force_verbose=force_verbose)
    except Exception as e:
      raise MBErr('Error %s writing to the output path "%s"' %
                 (e, path))


  def PrintCmd(self, cmd, env):
    if self.platform == 'win32':
      env_prefix = 'set '
      env_quoter = QuoteForSet
      shell_quoter = QuoteForCmd
    else:
      env_prefix = ''
      env_quoter = pipes.quote
      shell_quoter = pipes.quote

    def print_env(var):
      if env and var in env:
        self.Print('%s%s=%s' % (env_prefix, var, env_quoter(env[var])))

    print_env('LLVM_FORCE_HEAD_REVISION')

    if cmd[0] == self.executable:
      cmd = ['python'] + cmd[1:]
    self.Print(*[shell_quoter(arg) for arg in cmd])

  def PrintJSON(self, obj):
    self.Print(json.dumps(obj, indent=2, sort_keys=True))

  def Build(self, target):
    build_dir = self.ToSrcRelPath(self.args.path)
    if self.platform == 'win32':
      # On Windows use the batch script since there is no exe
      ninja_cmd = ['autoninja.bat', '-C', build_dir]
    else:
      ninja_cmd = ['autoninja', '-C', build_dir]
    if self.args.jobs:
      ninja_cmd.extend(['-j', '%d' % self.args.jobs])
    ninja_cmd.append(target)
    ret, _, _ = self.Run(ninja_cmd, buffer_output=False)
    return ret

  def Run(self, cmd, env=None, force_verbose=True, buffer_output=True):
    # This function largely exists so it can be overridden for testing.
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.PrintCmd(cmd, env)
    if self.args.dryrun:
      return 0, '', ''

    ret, out, err = self.Call(cmd, env=env, buffer_output=buffer_output)
    if self.args.verbose or force_verbose:
      if ret:
        self.Print('  -> returned %d' % ret)
      if out:
        # This is the error seen on the logs
        self.Print(out, end='')
      if err:
        self.Print(err, end='', file=sys.stderr)
    return ret, out, err

  def Call(self, cmd, env=None, buffer_output=True, stdin=None):
    if buffer_output:
      p = subprocess.Popen(cmd, shell=False, cwd=self.chromium_src_dir,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           env=env, stdin=subprocess.PIPE)
      out, err = p.communicate(input=stdin)
    else:
      p = subprocess.Popen(cmd, shell=False, cwd=self.chromium_src_dir,
                           env=env)
      p.wait()
      out = err = ''
    return p.returncode, out, err

  def ExpandUser(self, path):
    # This function largely exists so it can be overridden for testing.
    return os.path.expanduser(path)

  def Exists(self, path):
    # This function largely exists so it can be overridden for testing.
    return os.path.exists(path)

  def Fetch(self, url):
    # This function largely exists so it can be overridden for testing.
    f = urllib2.urlopen(url)
    contents = f.read()
    f.close()
    return contents

  def MaybeMakeDirectory(self, path):
    try:
      os.makedirs(path)
    except OSError, e:
      if e.errno != errno.EEXIST:
        raise

  def PathJoin(self, *comps):
    # This function largely exists so it can be overriden for testing.
    return os.path.join(*comps)

  def Print(self, *args, **kwargs):
    # This function largely exists so it can be overridden for testing.
    print(*args, **kwargs)
    if kwargs.get('stream', sys.stdout) == sys.stdout:
      sys.stdout.flush()

  def ReadFile(self, path):
    # This function largely exists so it can be overriden for testing.
    with open(path) as fp:
      return fp.read()

  def RelPath(self, path, start='.'):
    # This function largely exists so it can be overriden for testing.
    return os.path.relpath(path, start)

  def RemoveFile(self, path):
    # This function largely exists so it can be overriden for testing.
    os.remove(path)

  def RemoveDirectory(self, abs_path):
    if self.platform == 'win32':
      # In other places in chromium, we often have to retry this command
      # because we're worried about other processes still holding on to
      # file handles, but when MB is invoked, it will be early enough in the
      # build that their should be no other processes to interfere. We
      # can change this if need be.
      self.Run(['cmd.exe', '/c', 'rmdir', '/q', '/s', abs_path])
    else:
      shutil.rmtree(abs_path, ignore_errors=True)

  def TempDir(self):
    # This function largely exists so it can be overriden for testing.
    return tempfile.mkdtemp(prefix='mb_')

  def TempFile(self, mode='w'):
    # This function largely exists so it can be overriden for testing.
    return tempfile.NamedTemporaryFile(mode=mode, delete=False)

  def WriteFile(self, path, contents, force_verbose=False):
    # This function largely exists so it can be overriden for testing.
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    with open(path, 'w') as fp:
      return fp.write(contents)


class LedResult(object):
  """Holds the result of a led operation. Can be chained using |then|."""

  def __init__(self, result, run_cmd):
    self._result = result
    self._run_cmd = run_cmd

  @property
  def result(self):
    """The mutable result data of the previous led call as decoded JSON."""
    return self._result

  def then(self, *cmd):
    """Invoke led, passing it the current `result` data as input.

    Returns another LedResult object with the output of the command.
    """
    return self.__class__(
        self._run_cmd(self._result, cmd), self._run_cmd)


def FlattenConfig(config_pool, mixin_pool, config):
  mixins = config_pool[config]
  vals = DefaultVals()

  visited = []
  FlattenMixins(mixin_pool, mixins, vals, visited)
  return vals


def FlattenMixins(mixin_pool, mixins_to_flatten, vals, visited):
  for m in mixins_to_flatten:
    if m not in mixin_pool:
      raise MBErr('Unknown mixin "%s"' % m)

    visited.append(m)

    mixin_vals = mixin_pool[m]

    if 'cros_passthrough' in mixin_vals:
      vals['cros_passthrough'] = mixin_vals['cros_passthrough']
    if 'args_file' in mixin_vals:
      if vals['args_file']:
        raise MBErr('args_file specified multiple times in mixins '
                    'for mixin %s' % m)
      vals['args_file'] = mixin_vals['args_file']
    if 'gn_args' in mixin_vals:
      if vals['gn_args']:
        vals['gn_args'] += ' ' + mixin_vals['gn_args']
      else:
        vals['gn_args'] = mixin_vals['gn_args']

    if 'mixins' in mixin_vals:
      FlattenMixins(mixin_pool, mixin_vals['mixins'], vals, visited)
  return vals



class MBErr(Exception):
  pass


# See http://goo.gl/l5NPDW and http://goo.gl/4Diozm for the painful
# details of this next section, which handles escaping command lines
# so that they can be copied and pasted into a cmd window.
UNSAFE_FOR_SET = set('^<>&|')
UNSAFE_FOR_CMD = UNSAFE_FOR_SET.union(set('()%'))
ALL_META_CHARS = UNSAFE_FOR_CMD.union(set('"'))


def QuoteForSet(arg):
  if any(a in UNSAFE_FOR_SET for a in arg):
    arg = ''.join('^' + a if a in UNSAFE_FOR_SET else a for a in arg)
  return arg


def QuoteForCmd(arg):
  # First, escape the arg so that CommandLineToArgvW will parse it properly.
  if arg == '' or ' ' in arg or '"' in arg:
    quote_re = re.compile(r'(\\*)"')
    arg = '"%s"' % (quote_re.sub(lambda mo: 2 * mo.group(1) + '\\"', arg))

  # Then check to see if the arg contains any metacharacters other than
  # double quotes; if it does, quote everything (including the double
  # quotes) for safety.
  if any(a in UNSAFE_FOR_CMD for a in arg):
    arg = ''.join('^' + a if a in ALL_META_CHARS else a for a in arg)
  return arg


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
