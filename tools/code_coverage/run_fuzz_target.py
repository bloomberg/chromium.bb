#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run a single fuzz target built with code coverage instrumentation."""

import argparse
import copy
import json
import multiprocessing
import os
import shutil
import signal
import subprocess
import sys
import time
import zipfile

_CORPUS_BACKUP_URL_FORMAT = (
    'gs://clusterfuzz-libfuzzer-backup/corpus/libfuzzer/{fuzzer}/latest.zip')
_CORPUS_BACKUP_FILENAME = os.path.basename(_CORPUS_BACKUP_URL_FORMAT)
_CORPUS_CURRENT_URL_FORMAT = 'gs://clusterfuzz-corpus/libfuzzer/{fuzzer}'
_CORPUS_DIR_FORMAT = '{fuzzer}_corpus'

_DUMMY_INPUT_CONTENTS = 'dummy input just to have at least one corpus unit'
_DUMMY_INPUT_FILENAME = 'dummy_corpus_input'

_DUMMY_CORPUS_DIRECTORY = 'dummy_corpus_dir_which_should_be_empty'

# Fuzzers are single process, but may use shared libraries, that is why we still
# need to use merge pool specifier to have profraw files for every library used.
_LLVM_PROFILE_FILENAME_FORMAT = '{fuzzer}.%1m.profraw'

_LIBFUZZER_FLAGS = ['-merge=1', '-timeout=60', '-rss_limit_mb=8192']

_SLEEP_DURATION_SECONDS = 8


def _Log(message):
  # TODO: use appropriate logging approach when running on the bots.
  sys.stdout.write(message)
  sys.stdout.write('\n')


def _DownloadAndUnpackBackupCorpus(fuzzer, corpus_dir):
  local_backup_path = _DownloadBackupCorpus(fuzzer, corpus_dir)
  if not local_backup_path:
    return False

  zipfile.ZipFile(local_backup_path).extractall(path=corpus_dir)
  os.remove(local_backup_path)

  return True


def _DownloadBackupCorpus(fuzzer, corpus_dir):
  _Log('Downloading corpus backup for %s.' % fuzzer)
  local_backup_path = os.path.join(corpus_dir, _CORPUS_BACKUP_FILENAME)
  cmd = [
      'gsutil', 'cp',
      _CORPUS_BACKUP_URL_FORMAT.format(fuzzer=fuzzer), local_backup_path
  ]

  try:
    subprocess.check_call(cmd)
  except subprocess.CalledProcessError as e:
    _Log('Corpus backup for %s does not exist.' % fuzzer)
    return None

  _Log('Successfully downloaded corpus backup for %s.' % fuzzer)
  return local_backup_path


def _DownloadCurrentCorpus(fuzzer, corpus_dir):
  _Log('Downloading current corpus for %s.' % fuzzer)
  cmd = [
      'gsutil', '-m', '-q', 'cp', '-r',
      _CORPUS_CURRENT_URL_FORMAT.format(fuzzer=fuzzer), corpus_dir
  ]

  try:
    subprocess.check_call(cmd)
  except subprocess.CalledProcessError as e:
    _Log('Failed to download current corpus for %s.' % fuzzer)
    return False

  _Log('Successfully downloaded current corpus for %s.' % fuzzer)
  return True


def _PrepareCorpus(fuzzer_name, output_dir):
  # Create a directory for the corpus.
  corpus_dir = os.path.join(output_dir,
                            _CORPUS_DIR_FORMAT.format(fuzzer=fuzzer_name))
  _RecreateDir(corpus_dir)

  # Try to download corpus backup first.
  if _DownloadAndUnpackBackupCorpus(fuzzer_name, corpus_dir):
    return corpus_dir

  # Try to download current working corpus from ClusterFuzz.
  if _DownloadCurrentCorpus(fuzzer_name, corpus_dir):
    return corpus_dir

  # Write a dummy input to the corpus to have at least one fuzzer execution.
  _Log('All corpus download attempts failed, create a dummy corpus input.')
  dummy_input_path = os.path.join(corpus_dir, _DUMMY_INPUT_FILENAME)
  with open(dummy_input_path, 'wb') as fh:
    fh.write(_DUMMY_INPUT_CONTENTS)

  return corpus_dir


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()

  arg_parser.add_argument(
      '-f',
      '--fuzzer',
      type=str,
      required=True,
      help='Path to the fuzz target executable.')

  arg_parser.add_argument(
      '-o',
      '--output-dir',
      type=str,
      required=True,
      help='Output directory where corpus and coverage dumps can be stored in.')

  arg_parser.add_argument(
      '-t',
      '--timeout',
      type=int,
      required=True,
      help='Timeout value for running a single fuzz target.')

  # Ignored. Used to comply with isolated script contract, see chromium_tests
  # and swarming recipe modules for more details.
  arg_parser.add_argument(
      '--isolated-script-test-output',
      type=str,
      required=False,
      help=argparse.SUPPRESS)

  # Ditto.
  arg_parser.add_argument(
      '--isolated-script-test-perf-output',
      type=str,
      required=False,
      help=argparse.SUPPRESS)

  if len(sys.argv) == 1:
    arg_parser.print_help()
    sys.exit(1)

  args = arg_parser.parse_args()

  assert os.path.exists(
      args.fuzzer), ("Fuzzer '%s' does not exist." % args.fuzzer)

  assert os.path.exists(
      args.output_dir), ("Output dir '%s' does not exist." % args.output_dir)

  assert args.timeout > 0, 'Invalid timeout value: %d.' % args.timeout

  return args


def _RecreateDir(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)
  os.mkdir(dir_path)


def _CreateShardedCorpus(max_shards, corpus_dir, output_dir):
  """Shards the corpus and returns the directories with sharded corpus."""
  testcases = sorted(os.listdir(corpus_dir))
  shards = min(len(testcases), max_shards)

  if shards <= 1:
    return [corpus_dir]

  shard_dirs = []
  testcase_per_shard = len(testcases) / shards + 1
  copy_file = os.symlink or shutil.copy2

  index = 0
  while True:
    start = index * testcase_per_shard
    if start >= len(testcases):
      break

    end = (index + 1) * testcase_per_shard

    shard_dir = os.path.join(output_dir, 'shard_%d' % (index + 1))
    _RecreateDir(shard_dir)
    for testcase in testcases[start : end]:
      copy_file(os.path.join(corpus_dir, testcase),
                os.path.join(shard_dir, testcase))

    shard_dirs.append(shard_dir)
    index += 1

  return shard_dirs


def _RunFuzzTarget(fuzzer, fuzzer_name, output_dir, corpus_dir, timeout):
  # The way we run fuzz targets in code coverage config (-merge=1) requires an
  # empty directory to be provided to fuzz target. We run fuzz targets with
  # -merge=1 because that mode is crash-resistant.
  dummy_corpus_dir = os.path.join(output_dir, _DUMMY_CORPUS_DIRECTORY)
  _RecreateDir(dummy_corpus_dir)
  sharded_corpus_root_dir = os.path.join(output_dir, 'shards')
  _RecreateDir(sharded_corpus_root_dir)

  cpu_count = multiprocessing.cpu_count()
  shards = max(10, cpu_count - 5)  # Use 10+ shards, but leave 5 cpu cores.

  corpus_dirs = _CreateShardedCorpus(
      shards, corpus_dir, sharded_corpus_root_dir)

  cmd = [fuzzer] + _LIBFUZZER_FLAGS + [dummy_corpus_dir]

  try:
    _RunWithTimeout(cmd, timeout, corpus_dirs)
  except Exception as e:
    _Log('Failed to run {fuzzer}: {error}.'.format(
        fuzzer=fuzzer_name, error=str(e)))

  shutil.rmtree(dummy_corpus_dir)
  shutil.rmtree(corpus_dir)
  shutil.rmtree(sharded_corpus_root_dir)


def _RunWithTimeout(cmd, timeout, corpus_dirs):
  _Log('Run fuzz target using the following command in %d shards: %s.' % (
      len(corpus_dirs), str(cmd)))

  # TODO: we may need to use |creationflags=subprocess.CREATE_NEW_PROCESS_GROUP|
  # on Windows or send |signal.CTRL_C_EVENT| signal if the process times out.
  runners = []
  for corpus_dir in corpus_dirs:
    # Update LLVM_PROFILE_FILE for the fuzzer process.
    env = copy.deepcopy(os.environ)
    profile = env['LLVM_PROFILE_FILE']
    profile = os.path.join(
        os.path.dirname(profile), os.path.basename(corpus_dir),
        os.path.basename(profile))
    env['LLVM_PROFILE_FILE'] = profile
    runners.append(subprocess.Popen(cmd + [corpus_dir], env=env))

  def GetRunningProccess():
    running = []
    for runner in runners:
      if runner.poll() is None:
        running.append(runner)
    return running

  timer = 0
  while timer < timeout:
    if not GetRunningProccess():
      break
    time.sleep(_SLEEP_DURATION_SECONDS)
    timer += _SLEEP_DURATION_SECONDS

  timeout_runners = GetRunningProccess()
  _Log('Fuzz target timed out, interrupting %d shards.' % len(timeout_runners))
  for runner in timeout_runners:
    try:
      # libFuzzer may spawn some child processes, that is why we have to call
      # os.killpg, which would send the signal to our Python process as well, so
      # we just catch and ignore it in this try block.
      os.killpg(os.getpgid(runner.pid), signal.SIGINT)
    except KeyboardInterrupt:
      # Python's default signal handler raises KeyboardInterrupt exception for
      # SIGINT, suppress it here to prevent interrupting the script itself.
      pass

    output, error = runner.communicate()

  _Log('Finished running the fuzz target.')


def Main():
  assert 'LLVM_PROFILE_FILE' in os.environ, 'LLVM_PROFILE_FILE not set'

  args = _ParseCommandArguments()
  fuzzer_name = os.path.splitext(os.path.basename(args.fuzzer))[0]
  corpus_dir = _PrepareCorpus(fuzzer_name, args.output_dir)
  start_time = time.time()
  _RunFuzzTarget(args.fuzzer, fuzzer_name, args.output_dir, corpus_dir,
                 args.timeout)
  if args.isolated_script_test_output:
    # TODO(crbug.com/913827): Actually comply with the isolated script contract
    # on src/testing/scripts/common.
    with open(args.isolated_script_test_output, 'w') as f:
      json.dump({
          fuzzer_name: {
              'expected': 'PASS',
              'actual': 'PASS',
          },
          "interrupted": False,
          "path_delimiter": ".",
          "version": 3,
          "seconds_since_epoch": start_time,
          "num_failures_by_type": {
              "FAIL": 0,
              "PASS": 1
          },
      }, f)

  return 0


if __name__ == '__main__':
  sys.exit(Main())
