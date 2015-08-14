#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""MB - the Meta-Build wrapper around GYP and GN

MB is a wrapper script for GYP and GN that can be used to generate build files
for sets of canned configurations and analyze them.
"""

from __future__ import print_function

import argparse
import ast
import errno
import json
import os
import pipes
import pprint
import shlex
import shutil
import sys
import subprocess
import tempfile

def main(args):
  mbw = MetaBuildWrapper()
  mbw.ParseArgs(args)
  return mbw.args.func()


class MetaBuildWrapper(object):
  def __init__(self):
    p = os.path
    d = os.path.dirname
    self.chromium_src_dir = p.normpath(d(d(d(p.abspath(__file__)))))
    self.default_config = p.join(self.chromium_src_dir, 'tools', 'mb',
                                 'mb_config.pyl')
    self.platform = sys.platform
    self.args = argparse.Namespace()
    self.configs = {}
    self.masters = {}
    self.mixins = {}
    self.private_configs = []
    self.common_dev_configs = []
    self.unsupported_configs = []

  def ParseArgs(self, argv):
    def AddCommonOptions(subp):
      subp.add_argument('-b', '--builder',
                        help='builder name to look up config from')
      subp.add_argument('-m', '--master',
                        help='master name to look up config from')
      subp.add_argument('-c', '--config',
                        help='configuration to analyze')
      subp.add_argument('-f', '--config-file', metavar='PATH',
                        default=self.default_config,
                        help='path to config file '
                            '(default is //tools/mb/mb_config.pyl)')
      subp.add_argument('-g', '--goma-dir', default=self.ExpandUser('~/goma'),
                        help='path to goma directory (default is %(default)s).')
      subp.add_argument('-n', '--dryrun', action='store_true',
                        help='Do a dry run (i.e., do nothing, just print '
                             'the commands that will run)')
      subp.add_argument('-q', '--quiet', action='store_true',
                        help='Do not print anything on success, '
                             'just return an exit code.')
      subp.add_argument('-v', '--verbose', action='count',
                        help='verbose logging (may specify multiple times).')

    parser = argparse.ArgumentParser(prog='mb')
    subps = parser.add_subparsers()

    subp = subps.add_parser('analyze',
                            help='analyze whether changes to a set of files '
                                 'will cause a set of binaries to be rebuilt.')
    AddCommonOptions(subp)
    subp.add_argument('--swarming-targets-file',
                      help='save runtime dependencies for targets listed '
                           'in file.')
    subp.add_argument('path', nargs=1,
                      help='path build was generated into.')
    subp.add_argument('input_path', nargs=1,
                      help='path to a file containing the input arguments '
                           'as a JSON object.')
    subp.add_argument('output_path', nargs=1,
                      help='path to a file containing the output arguments '
                           'as a JSON object.')
    subp.set_defaults(func=self.CmdAnalyze)

    subp = subps.add_parser('gen',
                            help='generate a new set of build files')
    AddCommonOptions(subp)
    subp.add_argument('--swarming-targets-file',
                      help='save runtime dependencies for targets listed '
                           'in file.')
    subp.add_argument('path', nargs=1,
                      help='path to generate build into')
    subp.set_defaults(func=self.CmdGen)

    subp = subps.add_parser('lookup',
                            help='look up the command for a given config or '
                                 'builder')
    AddCommonOptions(subp)
    subp.set_defaults(func=self.CmdLookup)

    subp = subps.add_parser('validate',
                            help='validate the config file')
    subp.add_argument('-f', '--config-file', metavar='PATH',
                      default=self.default_config,
                      help='path to config file '
                          '(default is //tools/mb/mb_config.pyl)')
    subp.add_argument('-q', '--quiet', action='store_true',
                      help='Do not print anything on success, '
                           'just return an exit code.')
    subp.set_defaults(func=self.CmdValidate)

    subp = subps.add_parser('help',
                            help='Get help on a subcommand.')
    subp.add_argument(nargs='?', action='store', dest='subcommand',
                      help='The command to get help for.')
    subp.set_defaults(func=self.CmdHelp)

    self.args = parser.parse_args(argv)

  def CmdAnalyze(self):
    vals = self.GetConfig()
    if vals['type'] == 'gn':
      return self.RunGNAnalyze(vals)
    elif vals['type'] == 'gyp':
      return self.RunGYPAnalyze(vals)
    else:
      raise MBErr('Unknown meta-build type "%s"' % vals['type'])

  def CmdGen(self):
    vals = self.GetConfig()
    if vals['type'] == 'gn':
      return self.RunGNGen(vals)
    if vals['type'] == 'gyp':
      return self.RunGYPGen(vals)

    raise MBErr('Unknown meta-build type "%s"' % vals['type'])

  def CmdLookup(self):
    vals = self.GetConfig()
    if vals['type'] == 'gn':
      cmd = self.GNCmd('gen', '<path>', vals['gn_args'])
    elif vals['type'] == 'gyp':
      if vals['gyp_crosscompile']:
        self.Print('GYP_CROSSCOMPILE=1')
      cmd = self.GYPCmd('<path>', vals['gyp_defines'], vals['gyp_config'])
    else:
      raise MBErr('Unknown meta-build type "%s"' % vals['type'])

    self.PrintCmd(cmd)
    return 0

  def CmdHelp(self):
    if self.args.subcommand:
      self.ParseArgs([self.args.subcommand, '--help'])
    else:
      self.ParseArgs(['--help'])

  def CmdValidate(self):
    errs = []

    # Read the file to make sure it parses.
    self.ReadConfigFile()

    # Figure out the whole list of configs and ensure that no config is
    # listed in more than one category.
    all_configs = {}
    for config in self.common_dev_configs:
      all_configs[config] = 'common_dev_configs'
    for config in self.private_configs:
      if config in all_configs:
        errs.append('config "%s" listed in "private_configs" also '
                    'listed in "%s"' % (config, all_configs['config']))
      else:
        all_configs[config] = 'private_configs'
    for config in self.unsupported_configs:
      if config in all_configs:
        errs.append('config "%s" listed in "unsupported_configs" also '
                    'listed in "%s"' % (config, all_configs['config']))
      else:
        all_configs[config] = 'unsupported_configs'

    for master in self.masters:
      for builder in self.masters[master]:
        config = self.masters[master][builder]
        if config in all_configs and all_configs[config] not in self.masters:
          errs.append('Config "%s" used by a bot is also listed in "%s".' %
                      (config, all_configs[config]))
        else:
          all_configs[config] = master

    # Check that every referenced config actually exists.
    for config, loc in all_configs.items():
      if not config in self.configs:
        errs.append('Unknown config "%s" referenced from "%s".' %
                    (config, loc))

    # Check that every actual config is actually referenced.
    for config in self.configs:
      if not config in all_configs:
        errs.append('Unused config "%s".' % config)

    # Figure out the whole list of mixins, and check that every mixin
    # listed by a config or another mixin actually exists.
    referenced_mixins = set()
    for config, mixins in self.configs.items():
      for mixin in mixins:
        if not mixin in self.mixins:
          errs.append('Unknown mixin "%s" referenced by config "%s".' %
                      (mixin, config))
        referenced_mixins.add(mixin)

    for mixin in self.mixins:
      for sub_mixin in self.mixins[mixin].get('mixins', []):
        if not sub_mixin in self.mixins:
          errs.append('Unknown mixin "%s" referenced by mixin "%s".' %
                      (sub_mixin, mixin))
        referenced_mixins.add(sub_mixin)

    # Check that every mixin defined is actually referenced somewhere.
    for mixin in self.mixins:
      if not mixin in referenced_mixins:
        errs.append('Unreferenced mixin "%s".' % mixin)

    if errs:
      raise MBErr(('mb config file %s has problems:' % self.args.config_file) +
                    '\n  ' + '\n  '.join(errs))

    if not self.args.quiet:
      self.Print('mb config file %s looks ok.' % self.args.config_file)
    return 0

  def GetConfig(self):
    self.ReadConfigFile()
    config = self.ConfigFromArgs()
    if not config in self.configs:
      raise MBErr('Config "%s" not found in %s' %
                  (config, self.args.config_file))

    return self.FlattenConfig(config)

  def ReadConfigFile(self):
    if not self.Exists(self.args.config_file):
      raise MBErr('config file not found at %s' % self.args.config_file)

    try:
      contents = ast.literal_eval(self.ReadFile(self.args.config_file))
    except SyntaxError as e:
      raise MBErr('Failed to parse config file "%s": %s' %
                 (self.args.config_file, e))

    self.common_dev_configs = contents['common_dev_configs']
    self.configs = contents['configs']
    self.masters = contents['masters']
    self.mixins = contents['mixins']
    self.private_configs = contents['private_configs']
    self.unsupported_configs = contents['unsupported_configs']

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

    return self.masters[self.args.master][self.args.builder]

  def FlattenConfig(self, config):
    mixins = self.configs[config]
    vals = {
      'type': None,
      'gn_args': [],
      'gyp_config': [],
      'gyp_defines': [],
      'gyp_crosscompile': False,
    }

    visited = []
    self.FlattenMixins(mixins, vals, visited)
    return vals

  def FlattenMixins(self, mixins, vals, visited):
    for m in mixins:
      if m not in self.mixins:
        raise MBErr('Unknown mixin "%s"' % m)

      # TODO: check for cycles in mixins.

      visited.append(m)

      mixin_vals = self.mixins[m]
      if 'type' in mixin_vals:
        vals['type'] = mixin_vals['type']
      if 'gn_args' in mixin_vals:
        if vals['gn_args']:
          vals['gn_args'] += ' ' + mixin_vals['gn_args']
        else:
          vals['gn_args'] = mixin_vals['gn_args']
      if 'gyp_config' in mixin_vals:
        vals['gyp_config'] = mixin_vals['gyp_config']
      if 'gyp_crosscompile' in mixin_vals:
        vals['gyp_crosscompile'] = mixin_vals['gyp_crosscompile']
      if 'gyp_defines' in mixin_vals:
        if vals['gyp_defines']:
          vals['gyp_defines'] += ' ' + mixin_vals['gyp_defines']
        else:
          vals['gyp_defines'] = mixin_vals['gyp_defines']
      if 'mixins' in mixin_vals:
        self.FlattenMixins(mixin_vals['mixins'], vals, visited)
    return vals

  def RunGNGen(self, vals):
    path = self.args.path[0]

    cmd = self.GNCmd('gen', path, vals['gn_args'])

    swarming_targets = []
    if self.args.swarming_targets_file:
      # We need GN to generate the list of runtime dependencies for
      # the compile targets listed (one per line) in the file so
      # we can run them via swarming. We use ninja_to_gn.pyl to convert
      # the compile targets to the matching GN labels.
      contents = self.ReadFile(self.args.swarming_targets_file)
      swarming_targets = contents.splitlines()
      gn_isolate_map = ast.literal_eval(self.ReadFile(os.path.join(
          self.chromium_src_dir, 'testing', 'buildbot', 'gn_isolate_map.pyl')))
      gn_labels = []
      for target in swarming_targets:
        if not target in gn_isolate_map:
          raise MBErr('test target "%s"  not found in %s' %
                      (target, '//testing/buildbot/gn_isolate_map.pyl'))
        gn_labels.append(gn_isolate_map[target]['label'])

      gn_runtime_deps_path = self.ToAbsPath(path, 'runtime_deps')

      # Since GN hasn't run yet, the build directory may not even exist.
      self.MaybeMakeDirectory(self.ToAbsPath(path))

      self.WriteFile(gn_runtime_deps_path, '\n'.join(gn_labels) + '\n')
      cmd.append('--runtime-deps-list-file=%s' % gn_runtime_deps_path)

    ret, _, _ = self.Run(cmd)

    for target in swarming_targets:
      if gn_isolate_map[target]['type'] == 'gpu_browser_test':
        runtime_deps_target = 'browser_tests'
      else:
        runtime_deps_target = target
      if sys.platform == 'win32':
        deps_path = self.ToAbsPath(path,
                                   runtime_deps_target + '.exe.runtime_deps')
      else:
        deps_path = self.ToAbsPath(path,
                                   runtime_deps_target + '.runtime_deps')
      if not self.Exists(deps_path):
          raise MBErr('did not generate %s' % deps_path)

      command, extra_files = self.GetIsolateCommand(target, vals,
                                                    gn_isolate_map)

      runtime_deps = self.ReadFile(deps_path).splitlines()

      isolate_path = self.ToAbsPath(path, target + '.isolate')
      self.WriteFile(isolate_path,
        pprint.pformat({
          'variables': {
            'command': command,
            'files': sorted(runtime_deps + extra_files),
          }
        }) + '\n')

      self.WriteJSON(
        {
          'args': [
            '--isolated',
            self.ToSrcRelPath('%s%s%s.isolated' % (path, os.sep, target)),
            '--isolate',
            self.ToSrcRelPath('%s%s%s.isolate' % (path, os.sep, target)),
          ],
          'dir': self.chromium_src_dir,
          'version': 1,
        },
        isolate_path + 'd.gen.json',
      )


    return ret

  def GNCmd(self, subcommand, path, gn_args=''):
    if self.platform == 'linux2':
      gn_path = os.path.join(self.chromium_src_dir, 'buildtools', 'linux64',
                             'gn')
    elif self.platform == 'darwin':
      gn_path = os.path.join(self.chromium_src_dir, 'buildtools', 'mac',
                             'gn')
    else:
      gn_path = os.path.join(self.chromium_src_dir, 'buildtools', 'win',
                             'gn.exe')

    cmd = [gn_path, subcommand, path]
    gn_args = gn_args.replace("$(goma_dir)", self.args.goma_dir)
    if gn_args:
      cmd.append('--args=%s' % gn_args)
    return cmd

  def RunGYPGen(self, vals):
    path = self.args.path[0]

    output_dir, gyp_config = self.ParseGYPConfigPath(path)
    if gyp_config != vals['gyp_config']:
      raise MBErr('The last component of the path (%s) must match the '
                  'GYP configuration specified in the config (%s), and '
                  'it does not.' % (gyp_config, vals['gyp_config']))
    cmd = self.GYPCmd(output_dir, vals['gyp_defines'], config=gyp_config)
    env = None
    if vals['gyp_crosscompile']:
      if self.args.verbose:
        self.Print('Setting GYP_CROSSCOMPILE=1 in the environment')
      env = os.environ.copy()
      env['GYP_CROSSCOMPILE'] = '1'
    ret, _, _ = self.Run(cmd, env=env)
    return ret

  def RunGYPAnalyze(self, vals):
    output_dir, gyp_config = self.ParseGYPConfigPath(self.args.path[0])
    if gyp_config != vals['gyp_config']:
      raise MBErr('The last component of the path (%s) must match the '
                  'GYP configuration specified in the config (%s), and '
                  'it does not.' % (gyp_config, vals['gyp_config']))
    if self.args.verbose:
      inp = self.ReadInputJSON(['files', 'targets'])
      self.Print()
      self.Print('analyze input:')
      self.PrintJSON(inp)
      self.Print()

    cmd = self.GYPCmd(output_dir, vals['gyp_defines'], config=gyp_config)
    cmd.extend(['-f', 'analyzer',
                '-G', 'config_path=%s' % self.args.input_path[0],
                '-G', 'analyzer_output_path=%s' % self.args.output_path[0]])
    ret, _, _ = self.Run(cmd)
    if not ret and self.args.verbose:
      outp = json.loads(self.ReadFile(self.args.output_path[0]))
      self.Print()
      self.Print('analyze output:')
      self.PrintJSON(outp)
      self.Print()

    return ret

  def RunGNIsolate(self, vals):
    build_path = self.args.path[0]
    inp = self.ReadInputJSON(['targets'])
    if self.args.verbose:
      self.Print()
      self.Print('isolate input:')
      self.PrintJSON(inp)
      self.Print()
    output_path = self.args.output_path[0]

    for target in inp['targets']:
      runtime_deps_path = self.ToAbsPath(build_path, target + '.runtime_deps')

      if not self.Exists(runtime_deps_path):
        self.WriteFailureAndRaise('"%s" does not exist' % runtime_deps_path,
                                  output_path)

      command, extra_files = self.GetIsolateCommand(target, vals, None)

      runtime_deps = self.ReadFile(runtime_deps_path).splitlines()


      isolate_path = self.ToAbsPath(build_path, target + '.isolate')
      self.WriteFile(isolate_path,
        pprint.pformat({
          'variables': {
            'command': command,
            'files': sorted(runtime_deps + extra_files),
          }
        }) + '\n')

      self.WriteJSON(
        {
          'args': [
            '--isolated',
            self.ToSrcRelPath('%s/%s.isolated' % (build_path, target)),
            '--isolate',
            self.ToSrcRelPath('%s/%s.isolate' % (build_path, target)),
          ],
          'dir': self.chromium_src_dir,
          'version': 1,
        },
        isolate_path + 'd.gen.json',
      )

    return 0

  def GetIsolateCommand(self, target, vals, gn_isolate_map):
    # This needs to mirror the settings in //build/config/ui.gni:
    # use_x11 = is_linux && !use_ozone.
    # TODO(dpranke): Figure out how to keep this in sync better.
    use_x11 = (sys.platform == 'linux2' and
               not 'target_os="android"' in vals['gn_args'] and
               not 'use_ozone=true' in vals['gn_args'])

    asan = 'is_asan=true' in vals['gn_args']
    msan = 'is_msan=true' in vals['gn_args']
    tsan = 'is_tsan=true' in vals['gn_args']

    executable_suffix = '.exe' if sys.platform == 'win32' else ''

    test_type = gn_isolate_map[target]['type']
    cmdline = []
    extra_files = []

    if use_x11 and test_type == 'windowed_test_launcher':
      extra_files = [
          'xdisplaycheck',
          '../../testing/test_env.py',
          '../../testing/xvfb.py',
      ]
      cmdline = [
        '../../testing/xvfb.py',
        '.',
        './' + str(target),
        '--brave-new-test-launcher',
        '--test-launcher-bot-mode',
        '--asan=%d' % asan,
        '--msan=%d' % msan,
        '--tsan=%d' % tsan,
      ]
    elif test_type in ('windowed_test_launcher', 'console_test_launcher'):
      extra_files = [
          '../../testing/test_env.py'
      ]
      cmdline = [
          '../../testing/test_env.py',
          './' + str(target) + executable_suffix,
          '--brave-new-test-launcher',
          '--test-launcher-bot-mode',
          '--asan=%d' % asan,
          '--msan=%d' % msan,
          '--tsan=%d' % tsan,
      ]
    elif test_type == 'gpu_browser_test':
      extra_files = [
          '../../testing/test_env.py'
      ]
      gtest_filter = gn_isolate_map[target]['gtest_filter']
      cmdline = [
          '../../testing/test_env.py',
          'browser_tests<(EXECUTABLE_SUFFIX)',
          '--test-launcher-bot-mode',
          '--enable-gpu',
          '--test-launcher-jobs=1',
          '--gtest_filter=%s' % gtest_filter,
      ]
    elif test_type in ('raw'):
      extra_files = []
      cmdline = [
          './' + str(target) + executable_suffix,
      ] + gn_isolate_map[target].get('args')

    else:
      self.WriteFailureAndRaise('No command line for %s found (test type %s).'
                                % (target, test_type), output_path=None)

    return cmdline, extra_files

  def ToAbsPath(self, build_path, *comps):
    return os.path.join(self.chromium_src_dir,
                        self.ToSrcRelPath(build_path),
                        *comps)

  def ToSrcRelPath(self, path):
    """Returns a relative path from the top of the repo."""
    # TODO: Support normal paths in addition to source-absolute paths.
    assert(path.startswith('//'))
    return path[2:].replace('/', os.sep)

  def ParseGYPConfigPath(self, path):
    rpath = self.ToSrcRelPath(path)
    output_dir, _, config = rpath.rpartition('/')
    self.CheckGYPConfigIsSupported(config, path)
    return output_dir, config

  def CheckGYPConfigIsSupported(self, config, path):
    if config not in ('Debug', 'Release'):
      if (sys.platform in ('win32', 'cygwin') and
          config not in ('Debug_x64', 'Release_x64')):
        raise MBErr('Unknown or unsupported config type "%s" in "%s"' %
                    config, path)

  def GYPCmd(self, output_dir, gyp_defines, config):
    gyp_defines = gyp_defines.replace("$(goma_dir)", self.args.goma_dir)
    cmd = [
        sys.executable,
        os.path.join('build', 'gyp_chromium'),
        '-G',
        'output_dir=' + output_dir,
        '-G',
        'config=' + config,
    ]
    for d in shlex.split(gyp_defines):
      cmd += ['-D', d]
    return cmd

  def RunGNAnalyze(self, vals):
    # analyze runs before 'gn gen' now, so we need to run gn gen
    # in order to ensure that we have a build directory.
    ret = self.RunGNGen(vals)
    if ret:
      return ret

    inp = self.ReadInputJSON(['files', 'targets'])
    if self.args.verbose:
      self.Print()
      self.Print('analyze input:')
      self.PrintJSON(inp)
      self.Print()

    output_path = self.args.output_path[0]

    # Bail out early if a GN file was modified, since 'gn refs' won't know
    # what to do about it.
    if any(f.endswith('.gn') or f.endswith('.gni') for f in inp['files']):
      self.WriteJSON({'status': 'Found dependency (all)'}, output_path)
      return 0

    # Bail out early if 'all' was asked for, since 'gn refs' won't recognize it.
    if 'all' in inp['targets']:
      self.WriteJSON({'status': 'Found dependency (all)'}, output_path)
      return 0

    # This shouldn't normally happen, but could due to unusual race conditions,
    # like a try job that gets scheduled before a patch lands but runs after
    # the patch has landed.
    if not inp['files']:
      self.Print('Warning: No files modified in patch, bailing out early.')
      self.WriteJSON({'targets': [],
                      'build_targets': [],
                      'status': 'No dependency'}, output_path)
      return 0

    ret = 0
    response_file = self.TempFile()
    response_file.write('\n'.join(inp['files']) + '\n')
    response_file.close()

    matching_targets = []
    try:
      cmd = self.GNCmd('refs', self.args.path[0]) + [
          '@%s' % response_file.name, '--all', '--as=output']
      ret, out, _ = self.Run(cmd)
      if ret and not 'The input matches no targets' in out:
        self.WriteFailureAndRaise('gn refs returned %d: %s' % (ret, out),
                                  output_path)
      build_dir = self.ToSrcRelPath(self.args.path[0]) + os.sep
      for output in out.splitlines():
        build_output = output.replace(build_dir, '')
        if build_output in inp['targets']:
          matching_targets.append(build_output)

      cmd = self.GNCmd('refs', self.args.path[0]) + [
          '@%s' % response_file.name, '--all']
      ret, out, _ = self.Run(cmd)
      if ret and not 'The input matches no targets' in out:
        self.WriteFailureAndRaise('gn refs returned %d: %s' % (ret, out),
                                  output_path)
      for label in out.splitlines():
        build_target = label[2:]
        # We want to accept 'chrome/android:chrome_shell_apk' and
        # just 'chrome_shell_apk'. This may result in too many targets
        # getting built, but we can adjust that later if need be.
        for input_target in inp['targets']:
          if (input_target == build_target or
              build_target.endswith(':' + input_target)):
            matching_targets.append(input_target)
    finally:
      self.RemoveFile(response_file.name)

    if matching_targets:
      # TODO: it could be that a target X might depend on a target Y
      # and both would be listed in the input, but we would only need
      # to specify target X as a build_target (whereas both X and Y are
      # targets). I'm not sure if that optimization is generally worth it.
      self.WriteJSON({'targets': sorted(matching_targets),
                      'build_targets': sorted(matching_targets),
                      'status': 'Found dependency'}, output_path)
    else:
      self.WriteJSON({'targets': [],
                      'build_targets': [],
                      'status': 'No dependency'}, output_path)

    if not ret and self.args.verbose:
      outp = json.loads(self.ReadFile(output_path))
      self.Print()
      self.Print('analyze output:')
      self.PrintJSON(outp)
      self.Print()

    return 0

  def ReadInputJSON(self, required_keys):
    path = self.args.input_path[0]
    output_path = self.args.output_path[0]
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
      self.WriteJSON({'error': msg}, output_path)
    raise MBErr(msg)

  def WriteJSON(self, obj, path):
    try:
      self.WriteFile(path, json.dumps(obj, indent=2, sort_keys=True) + '\n')
    except Exception as e:
      raise MBErr('Error %s writing to the output path "%s"' %
                 (e, path))

  def PrintCmd(self, cmd):
    if cmd[0] == sys.executable:
      cmd = ['python'] + cmd[1:]
    self.Print(*[pipes.quote(c) for c in cmd])

  def PrintJSON(self, obj):
    self.Print(json.dumps(obj, indent=2, sort_keys=True))

  def Print(self, *args, **kwargs):
    # This function largely exists so it can be overridden for testing.
    print(*args, **kwargs)

  def Run(self, cmd, env=None):
    # This function largely exists so it can be overridden for testing.
    if self.args.dryrun or self.args.verbose:
      self.PrintCmd(cmd)
    if self.args.dryrun:
      return 0, '', ''
    ret, out, err = self.Call(cmd, env=env)
    if self.args.verbose:
      if out:
        self.Print(out, end='')
      if err:
        self.Print(err, end='', file=sys.stderr)
    return ret, out, err

  def Call(self, cmd, env=None):
    p = subprocess.Popen(cmd, shell=False, cwd=self.chromium_src_dir,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         env=env)
    out, err = p.communicate()
    return p.returncode, out, err

  def ExpandUser(self, path):
    # This function largely exists so it can be overridden for testing.
    return os.path.expanduser(path)

  def Exists(self, path):
    # This function largely exists so it can be overridden for testing.
    return os.path.exists(path)

  def MaybeMakeDirectory(self, path):
    try:
      os.makedirs(path)
    except OSError, e:
      if e.errno != errno.EEXIST:
        raise

  def ReadFile(self, path):
    # This function largely exists so it can be overriden for testing.
    with open(path) as fp:
      return fp.read()

  def RemoveFile(self, path):
    # This function largely exists so it can be overriden for testing.
    os.remove(path)

  def TempFile(self, mode='w'):
    # This function largely exists so it can be overriden for testing.
    return tempfile.NamedTemporaryFile(mode=mode, delete=False)

  def WriteFile(self, path, contents):
    # This function largely exists so it can be overriden for testing.
    if self.args.dryrun or self.args.verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    with open(path, 'w') as fp:
      return fp.write(contents)


class MBErr(Exception):
  pass


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except MBErr as e:
    print(e)
    sys.exit(1)
  except KeyboardInterrupt:
    print("interrupted, exiting", stream=sys.stderr)
    sys.exit(130)
