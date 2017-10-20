#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom swarming triggering script.

This script does custom swarming triggering logic, to enable device affinity
for our bots, while lumping all trigger calls under one logical step.

This script must have roughly the same command line interface as swarming.py
trigger. It modifies it in the following ways:
 * Inserts bot id into swarming trigger dimensions, and swarming task arguments.
 * Intercepts the dump-json argument, and creates its own by combining the
   results from each trigger call.

This script is normally called from the swarming recipe module in tools/build.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

from core import path_util


def get_swarming_py_path():
  return os.path.join(
      path_util.GetChromiumSrcDir(), 'tools', 'swarming_client', 'swarming.py')


def modify_args(all_args, bot_id, temp_file):
  """Modifies the given argument list.

  Specifically, it does the following:
    * Adds a --dump_json argument, to read in the results of the
      individual trigger command.
    * Adds a bot id dimension.
    * Adds the bot id as an argument to the test, so it knows which tests to
      run.

  The arguments are structured like this:
  <args to swarming.py trigger> -- <args to bot running isolate>
  This means we have to add arguments to specific locations in the argument
  list, to either affect the trigger command, or what the bot runs.
  """
  assert '--' in all_args, (
      'Malformed trigger command; -- argument expected but not found')
  dash_ind = all_args.index('--')

  return all_args[:dash_ind] + [
      '--dump_json', temp_file, '--dimension', 'id', bot_id] + all_args[
          dash_ind:] + ['--bot', bot_id]


def trigger_tasks(args, remaining):
  """Triggers tasks for each bot.

  Args:
    args: Parsed arguments which we need to use.
    remaining: The remainder of the arguments, which should be passed to
               swarming.py calls.

  Returns:
    Exit code for the script.
  """
  merged_json = {'tasks': {}}

  # TODO: Do these in parallel
  for shard_num, bot_id in enumerate(args.bot_id):
    # Holds the results of the swarming.py trigger call.
    temp_fd, json_temp = tempfile.mkstemp(prefix='perf_device_trigger')
    try:
      args_to_pass = modify_args(remaining[:], bot_id, json_temp)

      ret = subprocess.call(
          sys.executable, [get_swarming_py_path()] + args_to_pass)
      if ret:
        sys.stderr.write('Failed to trigger a task, aborting\n')
        return ret
      with open(json_temp) as f:
        result_json = json.load(f)

      for k, v in result_json['tasks'].items():
        v['shard_index'] = shard_num
        merged_json['tasks'][k + ':%d:%d' % (shard_num, len(args.bot_id))] = v
    finally:
      os.close(temp_fd)
      os.remove(json_temp)
  with open(args.dump_json, 'w') as f:
    json.dump(merged_json, f)
  return 0


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--bot-id', action='append', required=True,
                      help='Which bot IDs to trigger tasks on. Number of bot'
                      ' IDs must match the number of shards. This is because'
                      ' the recipe code uses the --shard argument to determine'
                      ' how many shards it expects in the output json.')
  parser.add_argument('--dump-json', required=True,
                      help='(Swarming Trigger Script API) Where to dump the'
                      ' resulting json which indicates which tasks were'
                      ' triggered for which shards.')
  parser.add_argument('--shards',
                      help='How many shards to trigger. Duplicated from the'
                      ' `swarming.py trigger` command. If passed, will be'
                      ' checked to ensure there are as many shards as bots'
                      ' specified by the --bot-id argument. If omitted,'
                      ' --bot-id will be used to determine the number of shards'
                      ' to create.')
  args, remaining = parser.parse_known_args()

  if args.shards and args.shards != len(args.bot_id):
    raise parser.error(
        'Number of bots to use must equal number of shards (--shards)')
  return trigger_tasks(args, remaining)


if __name__ == '__main__':
  sys.exit(main())
