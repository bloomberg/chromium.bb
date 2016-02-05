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
import re
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
    self.executable = sys.executable
    self.platform = sys.platform
    self.sep = os.sep
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
      subp.add_argument('-v', '--verbose', action='store_true',
                        help='verbose logging')

    parser = argparse.ArgumentParser(prog='mb')
    subps = parser.add_subparsers()

    subp = subps.add_parser('analyze',
                            help='analyze whether changes to a set of files '
                                 'will cause a set of binaries to be rebuilt.')
    AddCommonOptions(subp)
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

    subp = subps.add_parser('isolate',
                            help='generate the .isolate files for a given'
                                 'binary')
    AddCommonOptions(subp)
    subp.add_argument('path', nargs=1,
                      help='path build was generated into')
    subp.add_argument('target', nargs=1,
                      help='ninja target to generate the isolate for')
    subp.set_defaults(func=self.CmdIsolate)

    subp = subps.add_parser('lookup',
                            help='look up the command for a given config or '
                                 'builder')
    AddCommonOptions(subp)
    subp.set_defaults(func=self.CmdLookup)

    subp = subps.add_parser('run',
                            help='build and run the isolated version of a '
                                 'binary')
    AddCommonOptions(subp)
    subp.add_argument('-j', '--jobs', dest='jobs', type=int,
                      help='Number of jobs to pass to ninja')
    subp.add_argument('--no-build', dest='build', default=True,
                      action='store_false',
                      help='Do not build, just isolate and run')
    subp.add_argument('path', nargs=1,
                      help='path to generate build into')
    subp.add_argument('target', nargs=1,
                      help='ninja target to build and run')
    subp.set_defaults(func=self.CmdRun)

    subp = subps.add_parser('validate',
                            help='validate the config file')
    subp.add_argument('-f', '--config-file', metavar='PATH',
                      default=self.default_config,
                      help='path to config file '
                          '(default is //tools/mb/mb_config.pyl)')
    subp.set_defaults(func=self.CmdValidate)

    subp = subps.add_parser('help',
                            help='Get help on a subcommand.')
    subp.add_argument(nargs='?', action='store', dest='subcommand',
                      help='The command to get help for.')
    subp.set_defaults(func=self.CmdHelp)

    self.args = parser.parse_args(argv)

  def CmdAnalyze(self):
    vals = self.Lookup()
    if vals['type'] == 'gn':
      return self.RunGNAnalyze(vals)
    else:
      return self.RunGYPAnalyze(vals)

  def CmdGen(self):
    vals = self.Lookup()
    self.ClobberIfNeeded(vals)

    if vals['type'] == 'gn':
      return self.RunGNGen(vals)
    else:
      return self.RunGYPGen(vals)

  def CmdHelp(self):
    if self.args.subcommand:
      self.ParseArgs([self.args.subcommand, '--help'])
    else:
      self.ParseArgs(['--help'])

  def CmdIsolate(self):
    vals = self.GetConfig()
    if not vals:
      return 1

    if vals['type'] == 'gn':
      return self.RunGNIsolate(vals)
    else:
      return self.Build('%s_run' % self.args.target[0])

  def CmdLookup(self):
    vals = self.Lookup()
    if vals['type'] == 'gn':
      cmd = self.GNCmd('gen', '_path_', vals['gn_args'])
      env = None
    else:
      cmd, env = self.GYPCmd('_path_', vals)

    self.PrintCmd(cmd, env)
    return 0

  def CmdRun(self):
    vals = self.GetConfig()
    if not vals:
      return 1

    build_dir = self.args.path[0]
    target = self.args.target[0]

    if vals['type'] == 'gn':
      if self.args.build:
        ret = self.Build(target)
        if ret:
          return ret
      ret = self.RunGNIsolate(vals)
      if ret:
        return ret
    else:
      ret = self.Build('%s_run' % target)
      if ret:
        return ret

    ret, _, _ = self.Run([
        self.executable,
        self.PathJoin('tools', 'swarming_client', 'isolate.py'),
        'run',
        '-s',
        self.ToSrcRelPath('%s/%s.isolated' % (build_dir, target))],
        force_verbose=False, buffer_output=False)

    return ret

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

    self.Print('mb config file %s looks ok.' % self.args.config_file)
    return 0

  def GetConfig(self):
    build_dir = self.args.path[0]

    vals = {}
    if self.args.builder or self.args.master or self.args.config:
      vals = self.Lookup()
      if vals['type'] == 'gn':
        # Re-run gn gen in order to ensure the config is consistent with the
        # build dir.
        self.RunGNGen(vals)
      return vals

    # TODO: We can only get the config for GN build dirs, not GYP build dirs.
    # GN stores the args that were used in args.gn in the build dir,
    # but GYP doesn't store them anywhere. We should consider modifying
    # gyp_chromium to record the arguments it runs with in a similar
    # manner.

    mb_type_path = self.PathJoin(self.ToAbsPath(build_dir), 'mb_type')
    if not self.Exists(mb_type_path):
      gn_args_path = self.PathJoin(self.ToAbsPath(build_dir), 'args.gn')
      if not self.Exists(gn_args_path):
        self.Print('Must either specify a path to an existing GN build dir '
                   'or pass in a -m/-b pair or a -c flag to specify the '
                   'configuration')
        return {}
      else:
        mb_type = 'gn'
    else:
      mb_type = self.ReadFile(mb_type_path).strip()

    if mb_type == 'gn':
      vals = self.GNValsFromDir(build_dir)
    else:
      vals = {}
    vals['type'] = mb_type

    return vals

  def GNValsFromDir(self, build_dir):
    args_contents = self.ReadFile(
        self.PathJoin(self.ToAbsPath(build_dir), 'args.gn'))
    gn_args = []
    for l in args_contents.splitlines():
      fields = l.split(' ')
      name = fields[0]
      val = ' '.join(fields[2:])
      gn_args.append('%s=%s' % (name, val))

    return {
      'gn_args': ' '.join(gn_args),
      'type': 'gn',
    }

  def Lookup(self):
    vals = self.ReadBotConfig()
    if not vals:
      self.ReadConfigFile()
      config = self.ConfigFromArgs()
      if not config in self.configs:
        raise MBErr('Config "%s" not found in %s' %
                    (config, self.args.config_file))

      vals = self.FlattenConfig(config)

    # Do some basic sanity checking on the config so that we
    # don't have to do this in every caller.
    assert 'type' in vals, 'No meta-build type specified in the config'
    assert vals['type'] in ('gn', 'gyp'), (
        'Unknown meta-build type "%s"' % vals['gn_args'])

    return vals

  def ReadBotConfig(self):
    if not self.args.master or not self.args.builder:
      return {}
    path = self.PathJoin(self.chromium_src_dir, 'ios', 'build', 'bots',
                         self.args.master, self.args.builder + '.json')
    if not self.Exists(path):
      return {}

    contents = json.loads(self.ReadFile(path))
    gyp_vals = contents.get('GYP_DEFINES', {})
    if isinstance(gyp_vals, dict):
      gyp_defines = ' '.join('%s=%s' % (k, v) for k, v in gyp_vals.items())
    else:
      gyp_defines = ' '.join(gyp_vals)
    gn_args = ' '.join(contents.get('gn_args', []))

    return {
        'type': contents.get('mb_type', ''),
        'gn_args': gn_args,
        'gyp_defines': gyp_defines,
        'gyp_crosscompile': False,
    }

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
      'gyp_defines': '',
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

  def ClobberIfNeeded(self, vals):
    path = self.args.path[0]
    build_dir = self.ToAbsPath(path)
    mb_type_path = self.PathJoin(build_dir, 'mb_type')
    needs_clobber = False
    new_mb_type = vals['type']
    if self.Exists(build_dir):
      if self.Exists(mb_type_path):
        old_mb_type = self.ReadFile(mb_type_path)
        if old_mb_type != new_mb_type:
          self.Print("Build type mismatch: was %s, will be %s, clobbering %s" %
                     (old_mb_type, new_mb_type, path))
          needs_clobber = True
      else:
        # There is no 'mb_type' file in the build directory, so this probably
        # means that the prior build(s) were not done through mb, and we
        # have no idea if this was a GYP build or a GN build. Clobber it
        # to be safe.
        self.Print("%s/mb_type missing, clobbering to be safe" % path)
        needs_clobber = True

    if self.args.dryrun:
      return

    if needs_clobber:
      self.RemoveDirectory(build_dir)

    self.MaybeMakeDirectory(build_dir)
    self.WriteFile(mb_type_path, new_mb_type)

  def RunGNGen(self, vals):
    build_dir = self.args.path[0]

    cmd = self.GNCmd('gen', build_dir, vals['gn_args'], extra_args=['--check'])

    swarming_targets = []
    if getattr(self.args, 'swarming_targets_file', None):
      # We need GN to generate the list of runtime dependencies for
      # the compile targets listed (one per line) in the file so
      # we can run them via swarming. We use ninja_to_gn.pyl to convert
      # the compile targets to the matching GN labels.
      contents = self.ReadFile(self.args.swarming_targets_file)
      swarming_targets = contents.splitlines()
      gn_isolate_map = ast.literal_eval(self.ReadFile(self.PathJoin(
          self.chromium_src_dir, 'testing', 'buildbot', 'gn_isolate_map.pyl')))
      gn_labels = []
      for target in swarming_targets:
        target_name = self.GNTargetName(target)
        if not target_name in gn_isolate_map:
          raise MBErr('test target "%s"  not found in %s' %
                      (target, '//testing/buildbot/gn_isolate_map.pyl'))
        gn_labels.append(gn_isolate_map[target_name]['label'])

      gn_runtime_deps_path = self.ToAbsPath(build_dir, 'runtime_deps')

      # Since GN hasn't run yet, the build directory may not even exist.
      self.MaybeMakeDirectory(self.ToAbsPath(build_dir))

      self.WriteFile(gn_runtime_deps_path, '\n'.join(gn_labels) + '\n')
      cmd.append('--runtime-deps-list-file=%s' % gn_runtime_deps_path)

    ret, _, _ = self.Run(cmd)
    if ret:
        # If `gn gen` failed, we should exit early rather than trying to
        # generate isolates. Run() will have already logged any error output.
        self.Print('GN gen failed: %d' % ret)
        return ret

    for target in swarming_targets:
      if target.endswith('_apk'):
        # "_apk" targets may be either android_apk or executable. The former
        # will result in runtime_deps associated with the stamp file, while the
        # latter will result in runtime_deps associated with the executable.
        target_name = self.GNTargetName(target)
        label = gn_isolate_map[target_name]['label']
        runtime_deps_targets = [
            target_name,
            'obj/%s.stamp' % label.replace(':', '/')]
      elif gn_isolate_map[target]['type'] == 'gpu_browser_test':
        runtime_deps_targets = ['browser_tests']
      elif (gn_isolate_map[target]['type'] == 'script' or
            gn_isolate_map[target].get('label_type') == 'group'):
        # For script targets, the build target is usually a group,
        # for which gn generates the runtime_deps next to the stamp file
        # for the label, which lives under the obj/ directory.
        label = gn_isolate_map[target]['label']
        runtime_deps_targets = ['obj/%s.stamp' % label.replace(':', '/')]
      else:
        runtime_deps_targets = [target]

      if self.platform == 'win32':
        deps_paths = [
            self.ToAbsPath(build_dir, r + '.exe.runtime_deps')
            for r in runtime_deps_targets]
      else:
        deps_paths = [
            self.ToAbsPath(build_dir, r + '.runtime_deps')
            for r in runtime_deps_targets]

      for d in deps_paths:
        if self.Exists(d):
          deps_path = d
          break
      else:
        raise MBErr('did not generate any of %s' % ', '.join(deps_paths))

      command, extra_files = self.GetIsolateCommand(target, vals,
                                                    gn_isolate_map)

      runtime_deps = self.ReadFile(deps_path).splitlines()

      self.WriteIsolateFiles(build_dir, command, target, runtime_deps,
                             extra_files)

    return 0

  def RunGNIsolate(self, vals):
    gn_isolate_map = ast.literal_eval(self.ReadFile(self.PathJoin(
        self.chromium_src_dir, 'testing', 'buildbot', 'gn_isolate_map.pyl')))

    build_dir = self.args.path[0]
    target = self.args.target[0]
    target_name = self.GNTargetName(target)
    command, extra_files = self.GetIsolateCommand(target, vals, gn_isolate_map)

    label = gn_isolate_map[target_name]['label']
    ret, out, _ = self.Call(['gn', 'desc', build_dir, label, 'runtime_deps'])
    if ret:
      return ret

    runtime_deps = out.splitlines()

    self.WriteIsolateFiles(build_dir, command, target, runtime_deps,
                           extra_files)

    ret, _, _ = self.Run([
        self.executable,
        self.PathJoin('tools', 'swarming_client', 'isolate.py'),
        'check',
        '-i',
        self.ToSrcRelPath('%s/%s.isolate' % (build_dir, target)),
        '-s',
        self.ToSrcRelPath('%s/%s.isolated' % (build_dir, target))],
        buffer_output=False)

    return ret

  def WriteIsolateFiles(self, build_dir, command, target, runtime_deps,
                        extra_files):
    isolate_path = self.ToAbsPath(build_dir, target + '.isolate')
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
          self.ToSrcRelPath('%s/%s.isolated' % (build_dir, target)),
          '--isolate',
          self.ToSrcRelPath('%s/%s.isolate' % (build_dir, target)),
        ],
        'dir': self.chromium_src_dir,
        'version': 1,
      },
      isolate_path + 'd.gen.json',
    )

  def GNCmd(self, subcommand, path, gn_args='', extra_args=None):
    if self.platform == 'linux2':
      subdir = 'linux64'
    elif self.platform == 'darwin':
      subdir = 'mac'
    else:
      subdir = 'win'
    gn_path = self.PathJoin(self.chromium_src_dir, 'buildtools', subdir, 'gn')

    cmd = [gn_path, subcommand, path]
    gn_args = gn_args.replace("$(goma_dir)", self.args.goma_dir)
    if gn_args:
      cmd.append('--args=%s' % gn_args)
    if extra_args:
      cmd.extend(extra_args)
    return cmd

  def RunGYPGen(self, vals):
    path = self.args.path[0]

    output_dir = self.ParseGYPConfigPath(path)
    cmd, env = self.GYPCmd(output_dir, vals)
    ret, _, _ = self.Run(cmd, env=env)
    return ret

  def RunGYPAnalyze(self, vals):
    output_dir = self.ParseGYPConfigPath(self.args.path[0])
    if self.args.verbose:
      inp = self.ReadInputJSON(['files', 'test_targets',
                                'additional_compile_targets'])
      self.Print()
      self.Print('analyze input:')
      self.PrintJSON(inp)
      self.Print()

    cmd, env = self.GYPCmd(output_dir, vals)
    cmd.extend(['-f', 'analyzer',
                '-G', 'config_path=%s' % self.args.input_path[0],
                '-G', 'analyzer_output_path=%s' % self.args.output_path[0]])
    ret, _, _ = self.Run(cmd, env=env)
    if not ret and self.args.verbose:
      outp = json.loads(self.ReadFile(self.args.output_path[0]))
      self.Print()
      self.Print('analyze output:')
      self.PrintJSON(outp)
      self.Print()

    return ret

  def GetIsolateCommand(self, target, vals, gn_isolate_map):
    android = 'target_os="android"' in vals['gn_args']

    # This needs to mirror the settings in //build/config/ui.gni:
    # use_x11 = is_linux && !use_ozone.
    # TODO(dpranke): Figure out how to keep this in sync better.
    use_x11 = (self.platform == 'linux2' and
               not android and
               not 'use_ozone=true' in vals['gn_args'])

    asan = 'is_asan=true' in vals['gn_args']
    msan = 'is_msan=true' in vals['gn_args']
    tsan = 'is_tsan=true' in vals['gn_args']

    target_name = self.GNTargetName(target)
    test_type = gn_isolate_map[target_name]['type']

    executable = gn_isolate_map[target_name].get('executable', target_name)
    executable_suffix = '.exe' if self.platform == 'win32' else ''

    cmdline = []
    extra_files = []

    if android:
      # TODO(jbudorick): This won't work with instrumentation test targets.
      # Revisit this logic when those are added to gn_isolate_map.pyl.
      cmdline = [self.PathJoin('bin', 'run_%s' % target_name)]
    elif use_x11 and test_type == 'windowed_test_launcher':
      extra_files = [
          'xdisplaycheck',
          '../../testing/test_env.py',
          '../../testing/xvfb.py',
      ]
      cmdline = [
        '../../testing/xvfb.py',
        '.',
        './' + str(executable) + executable_suffix,
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
          './' + str(executable) + executable_suffix,
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
          './browser_tests' + executable_suffix,
          '--test-launcher-bot-mode',
          '--enable-gpu',
          '--test-launcher-jobs=1',
          '--gtest_filter=%s' % gtest_filter,
      ]
    elif test_type == 'script':
      extra_files = [
          '../../testing/test_env.py'
      ]
      cmdline = [
          '../../testing/test_env.py',
          '../../' + self.ToSrcRelPath(gn_isolate_map[target]['script'])
      ]
    elif test_type in ('raw'):
      extra_files = []
      cmdline = [
          './' + str(target) + executable_suffix,
      ]

    else:
      self.WriteFailureAndRaise('No command line for %s found (test type %s).'
                                % (target, test_type), output_path=None)

    cmdline += gn_isolate_map[target_name].get('args', [])

    return cmdline, extra_files

  def ToAbsPath(self, build_path, *comps):
    return self.PathJoin(self.chromium_src_dir,
                         self.ToSrcRelPath(build_path),
                         *comps)

  def ToSrcRelPath(self, path):
    """Returns a relative path from the top of the repo."""
    # TODO: Support normal paths in addition to source-absolute paths.
    assert(path.startswith('//'))
    return path[2:].replace('/', self.sep)

  def ParseGYPConfigPath(self, path):
    rpath = self.ToSrcRelPath(path)
    output_dir, _, _ = rpath.rpartition(self.sep)
    return output_dir

  def GYPCmd(self, output_dir, vals):
    gyp_defines = vals['gyp_defines']
    goma_dir = self.args.goma_dir

    # GYP uses shlex.split() to split the gyp defines into separate arguments,
    # so we can support backslashes and and spaces in arguments by quoting
    # them, even on Windows, where this normally wouldn't work.
    if '\\' in goma_dir or ' ' in goma_dir:
      goma_dir = "'%s'" % goma_dir
    gyp_defines = gyp_defines.replace("$(goma_dir)", goma_dir)

    cmd = [
        self.executable,
        self.PathJoin('build', 'gyp_chromium'),
        '-G',
        'output_dir=' + output_dir,
    ]

    # Ensure that we have an environment that only contains
    # the exact values of the GYP variables we need.
    env = os.environ.copy()
    env['GYP_GENERATORS'] = 'ninja'
    if 'GYP_CHROMIUM_NO_ACTION' in env:
      del env['GYP_CHROMIUM_NO_ACTION']
    if 'GYP_CROSSCOMPILE' in env:
      del env['GYP_CROSSCOMPILE']
    env['GYP_DEFINES'] = gyp_defines
    if vals['gyp_crosscompile']:
      env['GYP_CROSSCOMPILE'] = '1'
    return cmd, env

  def RunGNAnalyze(self, vals):
    # analyze runs before 'gn gen' now, so we need to run gn gen
    # in order to ensure that we have a build directory.
    ret = self.RunGNGen(vals)
    if ret:
      return ret

    inp = self.ReadInputJSON(['files', 'test_targets',
                              'additional_compile_targets'])
    if self.args.verbose:
      self.Print()
      self.Print('analyze input:')
      self.PrintJSON(inp)
      self.Print()

    # TODO(crbug.com/555273) - currently GN treats targets and
    # additional_compile_targets identically since we can't tell the
    # difference between a target that is a group in GN and one that isn't.
    # We should eventually fix this and treat the two types differently.
    targets = (set(inp['test_targets']) |
               set(inp['additional_compile_targets']))

    output_path = self.args.output_path[0]

    # Bail out early if a GN file was modified, since 'gn refs' won't know
    # what to do about it. Also, bail out early if 'all' was asked for,
    # since we can't deal with it yet.
    if (any(f.endswith('.gn') or f.endswith('.gni') for f in inp['files']) or
        'all' in targets):
      self.WriteJSON({
            'status': 'Found dependency (all)',
            'compile_targets': sorted(targets),
            'test_targets': sorted(targets & set(inp['test_targets'])),
          }, output_path)
      return 0

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

    ret = 0
    response_file = self.TempFile()
    response_file.write('\n'.join(inp['files']) + '\n')
    response_file.close()

    matching_targets = set()
    try:
      cmd = self.GNCmd('refs', self.args.path[0]) + [
          '@%s' % response_file.name, '--all', '--as=output']
      ret, out, _ = self.Run(cmd, force_verbose=False)
      if ret and not 'The input matches no targets' in out:
        self.WriteFailureAndRaise('gn refs returned %d: %s' % (ret, out),
                                  output_path)
      build_dir = self.ToSrcRelPath(self.args.path[0]) + self.sep
      for output in out.splitlines():
        build_output = output.replace(build_dir, '')
        if build_output in targets:
          matching_targets.add(build_output)

      cmd = self.GNCmd('refs', self.args.path[0]) + [
          '@%s' % response_file.name, '--all']
      ret, out, _ = self.Run(cmd, force_verbose=False)
      if ret and not 'The input matches no targets' in out:
        self.WriteFailureAndRaise('gn refs returned %d: %s' % (ret, out),
                                  output_path)
      for label in out.splitlines():
        build_target = label[2:]
        # We want to accept 'chrome/android:chrome_public_apk' and
        # just 'chrome_public_apk'. This may result in too many targets
        # getting built, but we can adjust that later if need be.
        for input_target in targets:
          if (input_target == build_target or
              build_target.endswith(':' + input_target)):
            matching_targets.add(input_target)
    finally:
      self.RemoveFile(response_file.name)

    if matching_targets:
      self.WriteJSON({
            'status': 'Found dependency',
            'compile_targets': sorted(matching_targets),
            'test_targets': sorted(matching_targets &
                                   set(inp['test_targets'])),
          }, output_path)
    else:
      self.WriteJSON({
          'status': 'No dependency',
          'compile_targets': [],
          'test_targets': [],
      }, output_path)

    if self.args.verbose:
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

    print_env('GYP_CROSSCOMPILE')
    print_env('GYP_DEFINES')

    if cmd[0] == self.executable:
      cmd = ['python'] + cmd[1:]
    self.Print(*[shell_quoter(arg) for arg in cmd])

  def PrintJSON(self, obj):
    self.Print(json.dumps(obj, indent=2, sort_keys=True))

  def Print(self, *args, **kwargs):
    # This function largely exists so it can be overridden for testing.
    print(*args, **kwargs)

  def Build(self, target):
    build_dir = self.ToSrcRelPath(self.args.path[0])
    ninja_cmd = ['ninja', '-C', build_dir]
    if self.args.jobs:
      ninja_cmd.extend(['-j', '%d' % self.args.jobs])
    ninja_cmd.append(target)
    ret, _, _ = self.Run(ninja_cmd, force_verbose=False, buffer_output=False)
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
        self.Print(out, end='')
      if err:
        self.Print(err, end='', file=sys.stderr)
    return ret, out, err

  def Call(self, cmd, env=None, buffer_output=True):
    if buffer_output:
      p = subprocess.Popen(cmd, shell=False, cwd=self.chromium_src_dir,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           env=env)
      out, err = p.communicate()
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

  def GNTargetName(self, target):
    return target[:-len('_apk')] if target.endswith('_apk') else target

  def MaybeMakeDirectory(self, path):
    try:
      os.makedirs(path)
    except OSError, e:
      if e.errno != errno.EEXIST:
        raise

  def PathJoin(self, *comps):
    # This function largely exists so it can be overriden for testing.
    return os.path.join(*comps)

  def ReadFile(self, path):
    # This function largely exists so it can be overriden for testing.
    with open(path) as fp:
      return fp.read()

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

  def TempFile(self, mode='w'):
    # This function largely exists so it can be overriden for testing.
    return tempfile.NamedTemporaryFile(mode=mode, delete=False)

  def WriteFile(self, path, contents, force_verbose=False):
    # This function largely exists so it can be overriden for testing.
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    with open(path, 'w') as fp:
      return fp.write(contents)


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
  # From //tools/gyp/pylib/gyp/msvs_emulation.py:23.
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
  try:
    sys.exit(main(sys.argv[1:]))
  except MBErr as e:
    print(e)
    sys.exit(1)
  except KeyboardInterrupt:
    print("interrupted, exiting", stream=sys.stderr)
    sys.exit(130)
