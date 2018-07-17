#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate the majority of the JSON files in the src/testing/buildbot
directory. Maintaining these files by hand is too unwieldy.
"""

import argparse
import ast
import collections
import copy
import difflib
import itertools
import json
import os
import string
import sys
import traceback

THIS_DIR = os.path.dirname(os.path.abspath(__file__))


class BBGenErr(Exception):
  def __init__(self, message):
    super(BBGenErr, self).__init__(message)


# This class is only present to accommodate certain machines on
# chromium.android.fyi which run certain tests as instrumentation
# tests, but not as gtests. If this discrepancy were fixed then the
# notion could be removed.
class TestSuiteTypes(object):
  GTEST = 'gtest'


class BaseGenerator(object):
  def __init__(self, bb_gen):
    self.bb_gen = bb_gen

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    raise NotImplementedError()

  def sort(self, tests):
    raise NotImplementedError()


def cmp_tests(a, b):
  # Prefer to compare based on the "test" key.
  val = cmp(a['test'], b['test'])
  if val != 0:
    return val
  if 'name' in a and 'name' in b:
    return cmp(a['name'], b['name']) # pragma: no cover
  if 'name' not in a and 'name' not in b:
    return 0 # pragma: no cover
  # Prefer to put variants of the same test after the first one.
  if 'name' in a:
    return 1
  # 'name' is in b.
  return -1 # pragma: no cover


class GPUTelemetryTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(GPUTelemetryTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    isolated_scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_gpu_telemetry_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        isolated_scripts.append(test)
    return isolated_scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['name'])


class GTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(GTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    # The relative ordering of some of the tests is important to
    # minimize differences compared to the handwritten JSON files, since
    # Python's sorts are stable and there are some tests with the same
    # key (see gles2_conform_d3d9_test and similar variants). Avoid
    # losing the order by avoiding coalescing the dictionaries into one.
    gtests = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_gtest(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        # generate_gtest may veto the test generation on this tester.
        gtests.append(test)
    return gtests

  def sort(self, tests):
    return sorted(tests, cmp=cmp_tests)


class IsolatedScriptTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(IsolatedScriptTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    isolated_scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_isolated_script_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        isolated_scripts.append(test)
    return isolated_scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['name'])


class ScriptGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(ScriptGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_script_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['name'])


class JUnitGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(JUnitGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_junit_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts

  def sort(self, tests):
    return sorted(tests, key=lambda x: x['test'])


class CTSGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(CTSGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    # These only contain one entry and it's the contents of the input tests'
    # dictionary, verbatim.
    cts_tests = []
    cts_tests.append(input_tests)
    return cts_tests

  def sort(self, tests):
    return tests


class InstrumentationTestGenerator(BaseGenerator):
  def __init__(self, bb_gen):
    super(InstrumentationTestGenerator, self).__init__(bb_gen)

  def generate(self, waterfall, tester_name, tester_config, input_tests):
    scripts = []
    for test_name, test_config in sorted(input_tests.iteritems()):
      test = self.bb_gen.generate_instrumentation_test(
        waterfall, tester_name, tester_config, test_name, test_config)
      if test:
        scripts.append(test)
    return scripts

  def sort(self, tests):
    return sorted(tests, cmp=cmp_tests)


class BBJSONGenerator(object):
  def __init__(self):
    self.this_dir = THIS_DIR
    self.args = None
    self.waterfalls = None
    self.test_suites = None
    self.exceptions = None

  def generate_abs_file_path(self, relative_path):
    return os.path.join(self.this_dir, relative_path) # pragma: no cover

  def read_file(self, relative_path):
    with open(self.generate_abs_file_path(
        relative_path)) as fp: # pragma: no cover
      return fp.read() # pragma: no cover

  def write_file(self, relative_path, contents):
    with open(self.generate_abs_file_path(
        relative_path), 'wb') as fp: # pragma: no cover
      fp.write(contents) # pragma: no cover

  def pyl_file_path(self, filename):
    if self.args and self.args.pyl_files_dir:
      return os.path.join(self.args.pyl_files_dir, filename)
    return filename

  def load_pyl_file(self, filename):
    try:
      return ast.literal_eval(self.read_file(
          self.pyl_file_path(filename)))
    except (SyntaxError, ValueError) as e: # pragma: no cover
      raise BBGenErr('Failed to parse pyl file "%s": %s' %
                     (filename, e)) # pragma: no cover

  # TOOD(kbr): require that os_type be specified for all bots in waterfalls.pyl.
  # Currently it is only mandatory for bots which run GPU tests. Change these to
  # use [] instead of .get().
  def is_android(self, tester_config):
    return tester_config.get('os_type') == 'android'

  def is_linux(self, tester_config):
    return tester_config.get('os_type') == 'linux'

  def get_exception_for_test(self, test_name, test_config):
    # gtests may have both "test" and "name" fields, and usually, if the "name"
    # field is specified, it means that the same test is being repurposed
    # multiple times with different command line arguments. To handle this case,
    # prefer to lookup per the "name" field of the test itself, as opposed to
    # the "test_name", which is actually the "test" field.
    if 'name' in test_config:
      return self.exceptions.get(test_config['name'])
    else:
      return self.exceptions.get(test_name)

  def should_run_on_tester(self, waterfall, tester_name,test_name, test_config):
    # Currently, the only reason a test should not run on a given tester is that
    # it's in the exceptions. (Once the GPU waterfall generation script is
    # incorporated here, the rules will become more complex.)
    exception = self.get_exception_for_test(test_name, test_config)
    if not exception:
      return True
    remove_from = None
    remove_from = exception.get('remove_from')
    if remove_from:
      if tester_name in remove_from:
        return False
      # TODO(kbr): this code path was added for some tests (including
      # android_webview_unittests) on one machine (Nougat Phone
      # Tester) which exists with the same name on two waterfalls,
      # chromium.android and chromium.fyi; the tests are run on one
      # but not the other. Once the bots are all uniquely named (a
      # different ongoing project) this code should be removed.
      # TODO(kbr): add coverage.
      return (tester_name + ' ' + waterfall['name']
              not in remove_from) # pragma: no cover
    return True

  def get_test_modifications(self, test, test_name, tester_name):
    exception = self.get_exception_for_test(test_name, test)
    if not exception:
      return None
    return exception.get('modifications', {}).get(tester_name)

  def merge_command_line_args(self, arr, prefix, splitter):
    prefix_len = len(prefix)
    idx = 0
    first_idx = -1
    accumulated_args = []
    while idx < len(arr):
      flag = arr[idx]
      delete_current_entry = False
      if flag.startswith(prefix):
        arg = flag[prefix_len:]
        accumulated_args.extend(arg.split(splitter))
        if first_idx < 0:
          first_idx = idx
        else:
          delete_current_entry = True
      if delete_current_entry:
        del arr[idx]
      else:
        idx += 1
    if first_idx >= 0:
      arr[first_idx] = prefix + splitter.join(accumulated_args)
    return arr

  def maybe_fixup_args_array(self, arr):
    # The incoming array of strings may be an array of command line
    # arguments. To make it easier to turn on certain features per-bot or
    # per-test-suite, look specifically for certain flags and merge them
    # appropriately.
    #   --enable-features=Feature1 --enable-features=Feature2
    # are merged to:
    #   --enable-features=Feature1,Feature2
    # and:
    #   --extra-browser-args=arg1 --extra-browser-args=arg2
    # are merged to:
    #   --extra-browser-args=arg1 arg2
    arr = self.merge_command_line_args(arr, '--enable-features=', ',')
    arr = self.merge_command_line_args(arr, '--extra-browser-args=', ' ')
    return arr

  def dictionary_merge(self, a, b, path=None, update=True):
    """http://stackoverflow.com/questions/7204805/
        python-dictionaries-of-dictionaries-merge
    merges b into a
    """
    if path is None:
      path = []
    for key in b:
      if key in a:
        if isinstance(a[key], dict) and isinstance(b[key], dict):
          self.dictionary_merge(a[key], b[key], path + [str(key)])
        elif a[key] == b[key]:
          pass # same leaf value
        elif isinstance(a[key], list) and isinstance(b[key], list):
          # Args arrays are lists of strings. Just concatenate them,
          # and don't sort them, in order to keep some needed
          # arguments adjacent (like --time-out-ms [arg], etc.)
          if all(isinstance(x, str)
                 for x in itertools.chain(a[key], b[key])):
            a[key] = self.maybe_fixup_args_array(a[key] + b[key])
          else:
            # TODO(kbr): this only works properly if the two arrays are
            # the same length, which is currently always the case in the
            # swarming dimension_sets that we have to merge. It will fail
            # to merge / override 'args' arrays which are different
            # length.
            for idx in xrange(len(b[key])):
              try:
                a[key][idx] = self.dictionary_merge(a[key][idx], b[key][idx],
                                                    path + [str(key), str(idx)],
                                                    update=update)
              except (IndexError, TypeError): # pragma: no cover
                raise BBGenErr('Error merging list keys ' + str(key) +
                               ' and indices ' + str(idx) + ' between ' +
                               str(a) + ' and ' + str(b)) # pragma: no cover
        elif update: # pragma: no cover
          a[key] = b[key] # pragma: no cover
        else:
          raise BBGenErr('Conflict at %s' % '.'.join(
            path + [str(key)])) # pragma: no cover
      else:
        a[key] = b[key]
    return a

  def initialize_args_for_test(self, generated_test, tester_config):
    if 'args' in tester_config or 'args' in generated_test:
      generated_test['args'] = self.maybe_fixup_args_array(
        generated_test.get('args', []) + tester_config.get('args', []))

    def add_conditional_args(key, fn):
      if key in generated_test:
        if fn(tester_config):
          if not 'args' in generated_test:
            generated_test['args'] = []
          generated_test['args'] += generated_test[key]
        # Don't put the conditional args in the JSON.
        generated_test.pop(key)

    add_conditional_args('desktop_args', lambda cfg: not self.is_android(cfg))
    add_conditional_args('linux_args', self.is_linux)
    add_conditional_args('android_args', self.is_android)


  def initialize_swarming_dictionary_for_test(self, generated_test,
                                              tester_config):
    if 'swarming' not in generated_test:
      generated_test['swarming'] = {}
    if not 'can_use_on_swarming_builders' in generated_test['swarming']:
      generated_test['swarming'].update({
        'can_use_on_swarming_builders': tester_config.get('use_swarming', True)
      })
    if 'swarming' in tester_config:
      if ('dimension_sets' not in generated_test['swarming'] and
          'dimension_sets' in tester_config['swarming']):
        generated_test['swarming']['dimension_sets'] = copy.deepcopy(
          tester_config['swarming']['dimension_sets'])
      self.dictionary_merge(generated_test['swarming'],
                            tester_config['swarming'])
    # Apply any Android-specific Swarming dimensions after the generic ones.
    if 'android_swarming' in generated_test:
      if self.is_android(tester_config): # pragma: no cover
        self.dictionary_merge(
          generated_test['swarming'],
          generated_test['android_swarming']) # pragma: no cover
      del generated_test['android_swarming'] # pragma: no cover

  def clean_swarming_dictionary(self, swarming_dict):
    # Clean out redundant entries from a test's "swarming" dictionary.
    # This is really only needed to retain 100% parity with the
    # handwritten JSON files, and can be removed once all the files are
    # autogenerated.
    if 'shards' in swarming_dict:
      if swarming_dict['shards'] == 1: # pragma: no cover
        del swarming_dict['shards'] # pragma: no cover
    if 'hard_timeout' in swarming_dict:
      if swarming_dict['hard_timeout'] == 0: # pragma: no cover
        del swarming_dict['hard_timeout'] # pragma: no cover
    if not swarming_dict['can_use_on_swarming_builders']:
      # Remove all other keys.
      for k in swarming_dict.keys(): # pragma: no cover
        if k != 'can_use_on_swarming_builders': # pragma: no cover
          del swarming_dict[k] # pragma: no cover

  def update_and_cleanup_test(self, test, test_name, tester_name):
    # See if there are any exceptions that need to be merged into this
    # test's specification.
    modifications = self.get_test_modifications(test, test_name, tester_name)
    if modifications:
      test = self.dictionary_merge(test, modifications)
    if 'swarming' in test:
      self.clean_swarming_dictionary(test['swarming'])
    return test

  def add_common_test_properties(self, test, tester_config):
    if tester_config.get('use_multi_dimension_trigger_script'):
      test['trigger_script'] = {
        'script': '//testing/trigger_scripts/trigger_multiple_dimensions.py',
        'args': [
          '--multiple-trigger-configs',
          json.dumps(tester_config['swarming']['dimension_sets'] +
                     tester_config.get('alternate_swarming_dimensions', [])),
          '--multiple-dimension-script-verbose',
          'True'
        ],
      }

  def generate_gtest(self, waterfall, tester_name, tester_config, test_name,
                     test_config):
    if not self.should_run_on_tester(
        waterfall, tester_name, test_name, test_config):
      return None
    result = copy.deepcopy(test_config)
    if 'test' in result:
      result['name'] = test_name
    else:
      result['test'] = test_name
    self.initialize_swarming_dictionary_for_test(result, tester_config)
    self.initialize_args_for_test(result, tester_config)
    if self.is_android(tester_config) and tester_config.get('use_swarming',
                                                            True):
      args = result.get('args', [])
      args.append('--gs-results-bucket=chromium-result-details')
      if (result['swarming']['can_use_on_swarming_builders'] and not
          tester_config.get('skip_merge_script', False)):
        result['merge'] = {
          'args': [
            '--bucket',
            'chromium-result-details',
            '--test-name',
            test_name
          ],
          'script': '//build/android/pylib/results/presentation/'
            'test_results_presentation.py',
        } # pragma: no cover
      if not tester_config.get('skip_cipd_packages', False):
        result['swarming']['cipd_packages'] = [
          {
            'cipd_package': 'infra/tools/luci/logdog/butler/${platform}',
            'location': 'bin',
            'revision': 'git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c',
          }
        ]
      if not tester_config.get('skip_output_links', False):
        result['swarming']['output_links'] = [
          {
            'link': [
              'https://luci-logdog.appspot.com/v/?s',
              '=android%2Fswarming%2Flogcats%2F',
              '${TASK_ID}%2F%2B%2Funified_logcats',
            ],
            'name': 'shard #${SHARD_INDEX} logcats',
          },
        ]
      args.append('--recover-devices')
      if args:
        result['args'] = args

    result = self.update_and_cleanup_test(result, test_name, tester_name)
    self.add_common_test_properties(result, tester_config)
    return result

  def generate_isolated_script_test(self, waterfall, tester_name, tester_config,
                                    test_name, test_config):
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = copy.deepcopy(test_config)
    result['isolate_name'] = result.get('isolate_name', test_name)
    result['name'] = test_name
    self.initialize_swarming_dictionary_for_test(result, tester_config)
    self.initialize_args_for_test(result, tester_config)
    result = self.update_and_cleanup_test(result, test_name, tester_name)
    self.add_common_test_properties(result, tester_config)
    return result

  def generate_script_test(self, waterfall, tester_name, tester_config,
                           test_name, test_config):
    del tester_config
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = {
      'name': test_name,
      'script': test_config['script']
    }
    result = self.update_and_cleanup_test(result, test_name, tester_name)
    return result

  def generate_junit_test(self, waterfall, tester_name, tester_config,
                          test_name, test_config):
    del tester_config
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = {
      'test': test_name,
    }
    return result

  def generate_instrumentation_test(self, waterfall, tester_name, tester_config,
                                    test_name, test_config):
    del tester_config
    if not self.should_run_on_tester(waterfall, tester_name, test_name,
                                     test_config):
      return None
    result = copy.deepcopy(test_config)
    if 'test' in result and result['test'] != test_name:
      result['name'] = test_name
    else:
      result['test'] = test_name
    result = self.update_and_cleanup_test(result, test_name, tester_name)
    return result

  def substitute_gpu_args(self, tester_config, args):
    substitutions = {
      # Any machine in waterfalls.pyl which desires to run GPU tests
      # must provide the os_type key.
      'os_type': tester_config['os_type'],
      'gpu_vendor_id': '0',
      'gpu_device_id': '0',
    }
    dimension_set = tester_config['swarming']['dimension_sets'][0]
    if 'gpu' in dimension_set:
      # First remove the driver version, then split into vendor and device.
      gpu = dimension_set['gpu']
      gpu = gpu.split('-')[0].split(':')
      substitutions['gpu_vendor_id'] = gpu[0]
      substitutions['gpu_device_id'] = gpu[1]
    return [string.Template(arg).safe_substitute(substitutions) for arg in args]

  def generate_gpu_telemetry_test(self, waterfall, tester_name, tester_config,
                                  test_name, test_config):
    # These are all just specializations of isolated script tests with
    # a bunch of boilerplate command line arguments added.

    # The step name must end in 'test' or 'tests' in order for the
    # results to automatically show up on the flakiness dashboard.
    # (At least, this was true some time ago.) Continue to use this
    # naming convention for the time being to minimize changes.
    step_name = test_config.get('name', test_name)
    if not (step_name.endswith('test') or step_name.endswith('tests')):
      step_name = '%s_tests' % step_name
    result = self.generate_isolated_script_test(
      waterfall, tester_name, tester_config, step_name, test_config)
    if not result:
      return None
    result['isolate_name'] = 'telemetry_gpu_integration_test'
    args = result.get('args', [])
    test_to_run = result.pop('telemetry_test_name', test_name)
    args = [
      test_to_run,
      '--show-stdout',
      '--browser=%s' % tester_config['browser_config'],
      # --passthrough displays more of the logging in Telemetry when
      # run via typ, in particular some of the warnings about tests
      # being expected to fail, but passing.
      '--passthrough',
      '-v',
      '--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc',
    ] + args
    result['args'] = self.maybe_fixup_args_array(self.substitute_gpu_args(
      tester_config, args))
    return result

  def get_test_generator_map(self):
    return {
      'cts_tests': CTSGenerator(self),
      'gpu_telemetry_tests': GPUTelemetryTestGenerator(self),
      'gtest_tests': GTestGenerator(self),
      'instrumentation_tests': InstrumentationTestGenerator(self),
      'isolated_scripts': IsolatedScriptTestGenerator(self),
      'junit_tests': JUnitGenerator(self),
      'scripts': ScriptGenerator(self),
    }

  def get_test_type_remapper(self):
    return {
      # These are a specialization of isolated_scripts with a bunch of
      # boilerplate command line arguments added to each one.
      'gpu_telemetry_tests': 'isolated_scripts',
    }

  def check_composition_test_suites(self):
    # Pre-pass to catch errors reliably.
    for name, value in self.test_suites.iteritems():
      if isinstance(value, list):
        for entry in value:
          if isinstance(self.test_suites[entry], list):
            raise BBGenErr('Composition test suites may not refer to other '
                           'composition test suites (error found while '
                           'processing %s)' % name)

  def resolve_composition_test_suites(self):
    self.check_composition_test_suites()
    for name, value in self.test_suites.iteritems():
      if isinstance(value, list):
        # Resolve this to a dictionary.
        full_suite = {}
        for entry in value:
          suite = self.test_suites[entry]
          full_suite.update(suite)
        self.test_suites[name] = full_suite

  def link_waterfalls_to_test_suites(self):
    for waterfall in self.waterfalls:
      for tester_name, tester in waterfall['machines'].iteritems():
        for suite, value in tester.get('test_suites', {}).iteritems():
          if not value in self.test_suites:
            # Hard / impossible to cover this in the unit test.
            raise self.unknown_test_suite(
              value, tester_name, waterfall['name']) # pragma: no cover
          tester['test_suites'][suite] = self.test_suites[value]

  def load_configuration_files(self):
    self.waterfalls = self.load_pyl_file('waterfalls.pyl')
    self.test_suites = self.load_pyl_file('test_suites.pyl')
    self.exceptions = self.load_pyl_file('test_suite_exceptions.pyl')

  def resolve_configuration_files(self):
    self.resolve_composition_test_suites()
    self.link_waterfalls_to_test_suites()

  def unknown_bot(self, bot_name, waterfall_name):
    return BBGenErr(
      'Unknown bot name "%s" on waterfall "%s"' % (bot_name, waterfall_name))

  def unknown_test_suite(self, suite_name, bot_name, waterfall_name):
    return BBGenErr(
      'Test suite %s from machine %s on waterfall %s not present in '
      'test_suites.pyl' % (suite_name, bot_name, waterfall_name))

  def unknown_test_suite_type(self, suite_type, bot_name, waterfall_name):
    return BBGenErr(
      'Unknown test suite type ' + suite_type + ' in bot ' + bot_name +
      ' on waterfall ' + waterfall_name)

  def generate_waterfall_json(self, waterfall):
    all_tests = {}
    generator_map = self.get_test_generator_map()
    test_type_remapper = self.get_test_type_remapper()
    for name, config in waterfall['machines'].iteritems():
      tests = {}
      # Copy only well-understood entries in the machine's configuration
      # verbatim into the generated JSON.
      if 'additional_compile_targets' in config:
        tests['additional_compile_targets'] = config[
          'additional_compile_targets']
      for test_type, input_tests in config.get('test_suites', {}).iteritems():
        if test_type not in generator_map:
          raise self.unknown_test_suite_type(
            test_type, name, waterfall['name']) # pragma: no cover
        test_generator = generator_map[test_type]
        # Let multiple kinds of generators generate the same kinds
        # of tests. For example, gpu_telemetry_tests are a
        # specialization of isolated_scripts.
        new_tests = test_generator.generate(
          waterfall, name, config, input_tests)
        remapped_test_type = test_type_remapper.get(test_type, test_type)
        tests[remapped_test_type] = test_generator.sort(
          tests.get(remapped_test_type, []) + new_tests)
      all_tests[name] = tests
    all_tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
    all_tests['AAAAA2 See generate_buildbot_json.py to make changes'] = {}
    return json.dumps(all_tests, indent=2, separators=(',', ': '),
                      sort_keys=True) + '\n'

  def generate_waterfalls(self): # pragma: no cover
    self.load_configuration_files()
    self.resolve_configuration_files()
    filters = self.args.waterfall_filters
    suffix = '.json'
    if self.args.new_files:
      suffix = '.new' + suffix
    for waterfall in self.waterfalls:
      should_gen = not filters or waterfall['name'] in filters
      if should_gen:
        file_path = waterfall['name'] + suffix
        self.write_file(self.pyl_file_path(file_path),
                        self.generate_waterfall_json(waterfall))

  def get_valid_bot_names(self):
    # Extract bot names from infra/config/global/luci-milo.cfg.
    bot_names = set()
    for l in self.read_file(os.path.join(
        '..', '..', 'infra', 'config', 'global', 'luci-milo.cfg')).splitlines():
      if (not 'name: "buildbucket/luci.chromium.' in l and
          not 'name: "buildbot/chromium.' in l):
        continue
      # l looks like `name: "buildbucket/luci.chromium.try/win_chromium_dbg_ng"`
      # Extract win_chromium_dbg_ng part.
      bot_names.add(l[l.rindex('/') + 1:l.rindex('"')])
    return bot_names

  def get_bots_that_do_not_actually_exist(self):
    # Some of the bots on the chromium.gpu.fyi waterfall in particular
    # are defined only to be mirrored into trybots, and don't actually
    # exist on any of the waterfalls or consoles.
    return [
      'Optional Android Release (Nexus 5X)',
      'Optional Linux Release (Intel HD 630)',
      'Optional Linux Release (NVIDIA)',
      'Optional Mac Release (Intel)',
      'Optional Mac Retina Release (AMD)',
      'Optional Mac Retina Release (NVIDIA)',
      'Optional Win10 Release (Intel HD 630)',
      'Optional Win10 Release (NVIDIA)',
      'Win7 ANGLE Tryserver (AMD)',
      # chromium.fyi
      'chromeos-amd64-generic-rel-vm-tests',
      'linux-blink-rel-dummy',
      'mac10.10-blink-rel-dummy',
      'mac10.11-blink-rel-dummy',
      'mac10.12-blink-rel-dummy',
      'mac10.13_retina-blink-rel-dummy',
      'mac10.13-blink-rel-dummy',
      'win7-blink-rel-dummy',
      'win10-blink-rel-dummy',
      'Dummy WebKit Mac10.13',
      'WebKit Linux layout_ng Dummy Builder',
      'WebKit Linux root_layer_scrolls Dummy Builder',
      'WebKit Linux slimming_paint_v2 Dummy Builder',
    ]

  def check_input_file_consistency(self):
    self.load_configuration_files()
    self.check_composition_test_suites()

    # All bots should exist.
    bot_names = self.get_valid_bot_names()
    bots_that_dont_exist = self.get_bots_that_do_not_actually_exist()
    for waterfall in self.waterfalls:
      for bot_name in waterfall['machines']:
        if bot_name in bots_that_dont_exist:
          continue  # pragma: no cover
        if bot_name not in bot_names:
          if waterfall['name'] in ['client.v8.chromium', 'client.v8.fyi']:
            # TODO(thakis): Remove this once these bots move to luci.
            continue  # pragma: no cover
          if waterfall['name'] in ['tryserver.webrtc']:
            # These waterfalls have their bot configs in a different repo.
            # so we don't know about their bot names.
            continue  # pragma: no cover
          raise self.unknown_bot(bot_name, waterfall['name'])

    # All test suites must be referenced.
    suites_seen = set()
    generator_map = self.get_test_generator_map()
    for waterfall in self.waterfalls:
      for bot_name, tester in waterfall['machines'].iteritems():
        for suite_type, suite in tester.get('test_suites', {}).iteritems():
          if suite_type not in generator_map:
            raise self.unknown_test_suite_type(suite_type, bot_name,
                                               waterfall['name'])
          if suite not in self.test_suites:
            raise self.unknown_test_suite(suite, bot_name, waterfall['name'])
          suites_seen.add(suite)
    # Since we didn't resolve the configuration files, this set
    # includes both composition test suites and regular ones.
    resolved_suites = set()
    for suite_name in suites_seen:
      suite = self.test_suites[suite_name]
      if isinstance(suite, list):
        for sub_suite in suite:
          resolved_suites.add(sub_suite)
      resolved_suites.add(suite_name)
    # At this point, every key in test_suites.pyl should be referenced.
    missing_suites = set(self.test_suites.keys()) - resolved_suites
    if missing_suites:
      raise BBGenErr('The following test suites were unreferenced by bots on '
                     'the waterfalls: ' + str(missing_suites))

    # All test suite exceptions must refer to bots on the waterfall.
    all_bots = set()
    missing_bots = set()
    for waterfall in self.waterfalls:
      for bot_name, tester in waterfall['machines'].iteritems():
        all_bots.add(bot_name)
        # In order to disambiguate between bots with the same name on
        # different waterfalls, support has been added to various
        # exceptions for concatenating the waterfall name after the bot
        # name.
        all_bots.add(bot_name + ' ' + waterfall['name'])
    for exception in self.exceptions.itervalues():
      removals = (exception.get('remove_from', []) +
                  exception.get('remove_gtest_from', []) +
                  exception.get('modifications', {}).keys())
      for removal in removals:
        if removal not in all_bots:
          missing_bots.add(removal)
    if missing_bots:
      raise BBGenErr('The following nonexistent machines were referenced in '
                     'the test suite exceptions: ' + str(missing_bots))

  def check_output_file_consistency(self, verbose=False):
    self.load_configuration_files()
    # All waterfalls must have been written by this script already.
    self.resolve_configuration_files()
    ungenerated_waterfalls = set()
    for waterfall in self.waterfalls:
      expected = self.generate_waterfall_json(waterfall)
      file_path = waterfall['name'] + '.json'
      current = self.read_file(self.pyl_file_path(file_path))
      if expected != current:
        ungenerated_waterfalls.add(waterfall['name'])
        if verbose: # pragma: no cover
          print ('Waterfall ' +  waterfall['name'] +
                 ' did not have the following expected '
                 'contents:')
          for line in difflib.unified_diff(
              expected.splitlines(),
              current.splitlines()):
            print line
    if ungenerated_waterfalls:
      raise BBGenErr('The following waterfalls have not been properly '
                     'autogenerated by generate_buildbot_json.py: ' +
                     str(ungenerated_waterfalls))

  def check_consistency(self, verbose=False):
    self.check_input_file_consistency() # pragma: no cover
    self.check_output_file_consistency(verbose) # pragma: no cover

  def parse_args(self, argv): # pragma: no cover
    parser = argparse.ArgumentParser()
    parser.add_argument(
      '-c', '--check', action='store_true', help=
      'Do consistency checks of configuration and generated files and then '
      'exit. Used during presubmit. Causes the tool to not generate any files.')
    parser.add_argument(
      '-n', '--new-files', action='store_true', help=
      'Write output files as .new.json. Useful during development so old and '
      'new files can be looked at side-by-side.')
    parser.add_argument(
      'waterfall_filters', metavar='waterfalls', type=str, nargs='*',
      help='Optional list of waterfalls to generate.')
    parser.add_argument(
      '--pyl-files-dir', type=os.path.realpath,
      help='Path to the directory containing the input .pyl files.')
    self.args = parser.parse_args(argv)

  def main(self, argv): # pragma: no cover
    self.parse_args(argv)
    if self.args.check:
      self.check_consistency()
    else:
      self.generate_waterfalls()
    return 0

if __name__ == "__main__": # pragma: no cover
  generator = BBJSONGenerator()
  sys.exit(generator.main(sys.argv[1:]))
