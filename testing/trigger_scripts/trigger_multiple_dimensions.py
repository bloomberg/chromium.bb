#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom swarming triggering script.

This script does custom swarming triggering logic, to allow one bot to
conceptually span multiple Swarming configurations, while lumping all trigger
calls under one logical step.

The reason this script is needed is to allow seamless upgrades of the GPU, OS
version, or graphics driver. Most Chromium tests, GPU tests in particular, are
triggered with precise values for all of these Swarming dimensions. This ensures
that if a machine is added to the Swarming pool with a slightly different
configuration, tests don't fail for unexpected reasons.

During an upgrade of the fleet, it's not feasible to take half of the machines
offline. Some experience was gained with this during a recent upgrade of the
GPUs in Chromium's main Windows and Linux NVIDIA bots. In the middle of the
upgrade, only 50% of the capacity was available, and CQ jobs started to time
out. Once the hurdle had been passed in the middle of the upgrade, capacity was
sufficient, but it's crucial that this process remain seamless.

This script receives multiple machine configurations on the command line in the
form of quoted strings. These strings are JSON dictionaries that represent
entries in the "dimensions" array of the "swarming" dictionary in the
src/testing/buildbot JSON files. The script queries the Swarming pool for the
number of machines of each configuration, and distributes work (shards) among
them using the following algorithm:

1. If either configuration has machines available (online, not busy at the time
of the query) then distribute shards to them first.

2. Compute the relative fractions of all of the live (online, not quarantined,
not dead) machines of all configurations.

3. Distribute the remaining shards probabilistically among these configurations.

The use of random numbers attempts to avoid the pathology where one
configuration only has a couple of machines, and work is never distributed to it
once all machines are busy.

This script must have roughly the same command line interface as swarming.py
trigger. It modifies it in the following ways:
 * Intercepts the dump-json argument, and creates its own by combining the
   results from each trigger call.
 * Scans through the multiple-trigger-configs dictionaries. For any key found,
   deletes that dimension from the originally triggered task's dimensions. This
   is what allows the Swarming dimensions to be replaced.
 * On a per-shard basis, adds the Swarming dimensions chosen from the
   multiple-trigger-configs list to the dimensions for the shard.

This script is normally called from the swarming recipe module in tools/build.

"""

import argparse
import copy
import json
import os
import random
import subprocess
import sys
import tempfile
import urllib


SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(
  __file__))))

SWARMING_PY = os.path.join(SRC_DIR, 'tools', 'swarming_client', 'swarming.py')


def strip_unicode(obj):
  """Recursively re-encodes strings as utf-8 inside |obj|. Returns the result.
  """
  if isinstance(obj, unicode):
    return obj.encode('utf-8', 'replace')

  if isinstance(obj, list):
    return list(map(strip_unicode, obj))

  if isinstance(obj, dict):
    new_obj = type(obj)(
        (strip_unicode(k), strip_unicode(v)) for k, v in obj.iteritems() )
    return new_obj

  return obj


class MultiDimensionTestTriggerer(object):
  def __init__(self):
    self._bot_configs = None
    self._bot_statuses = []
    self._total_bots = 0

  def modify_args(self, all_args, bot_index, shard_index, total_shards,
                  temp_file):
    """Modifies the given argument list.

    Specifically, it does the following:
      * Adds a --dump_json argument, to read in the results of the
        individual trigger command.
      * Adds the dimensions associated with the bot config at the given index.
      * If the number of shards is greater than one, adds --env
        arguments to set the GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS
        environment variables to _shard_index_ and _total_shards_,
        respectively.

    The arguments are structured like this:
    <args to swarming.py trigger> -- <args to bot running isolate>
    This means we have to add arguments to specific locations in the argument
    list, to either affect the trigger command, or what the bot runs.

    """
    assert '--' in all_args, (
        'Malformed trigger command; -- argument expected but not found')
    dash_ind = all_args.index('--')
    bot_args = ['--dump-json', temp_file]
    if total_shards > 1:
      bot_args.append('--env')
      bot_args.append('GTEST_SHARD_INDEX')
      bot_args.append(str(shard_index))
      bot_args.append('--env')
      bot_args.append('GTEST_TOTAL_SHARDS')
      bot_args.append(str(total_shards))
    for key, val in sorted(self._bot_configs[bot_index].iteritems()):
      bot_args.append('--dimension')
      bot_args.append(key)
      bot_args.append(val)
    return all_args[:dash_ind] + bot_args + all_args[dash_ind:]

  def parse_bot_configs(self, args):
    try:
      self._bot_configs = strip_unicode(json.loads(
        args.multiple_trigger_configs))
    except Exception as e:
      raise ValueError('Error while parsing JSON from bot config string %s: %s'
                       % (args.multiple_trigger_configs, str(e)))
    # Validate the input.
    if not isinstance(self._bot_configs, list):
      raise ValueError('Bot configurations must be a list, were: %s' %
                       args.multiple_trigger_configs)
    if len(self._bot_configs) < 1:
      raise ValueError('Bot configuration list must have at least one entry')
    if not all(isinstance(entry, dict) for entry in self._bot_configs):
      raise ValueError('Bot configurations must all be dictionaries')

  def query_swarming_for_bot_configs(self, verbose):
    # Query Swarming to figure out which bots are available.
    for config in self._bot_configs:
      values = []
      for key, value in sorted(config.iteritems()):
        values.append(('dimensions', '%s:%s' % (key, value)))
      # Ignore dead and quarantined bots.
      values.append(('is_dead', 'FALSE'))
      values.append(('quarantined', 'FALSE'))
      query_arg = urllib.urlencode(values)

      temp_file = self.make_temp_file(prefix='trigger_multiple_dimensions',
                                      suffix='.json')
      try:
        ret = self.run_swarming(['query',
                                 '-S',
                                 'chromium-swarm.appspot.com',
                                 '--limit',
                                 '0',
                                 '--json',
                                 temp_file,
                                 ('bots/count?%s' % query_arg)],
                                verbose)
        if ret:
          raise Exception('Error running swarming.py')
        with open(temp_file) as fp:
          query_result = strip_unicode(json.load(fp))
        # Summarize number of available bots per configuration.
        count = int(query_result['count'])
        # Be robust against errors in computation.
        available = max(0, count - int(query_result['busy']))
        self._bot_statuses.append({'total': count, 'available': available})
        if verbose:
          idx = len(self._bot_statuses) - 1
          print 'Bot config %d: %s' % (idx, str(self._bot_statuses[idx]))
      finally:
        self.delete_temp_file(temp_file)
    # Sum up the total count of all bots.
    self._total_bots = sum(x['total'] for x in self._bot_statuses)
    if verbose:
      print 'Total bots: %d' % (self._total_bots)

  def remove_swarming_dimension(self, args, dimension):
    for i in xrange(len(args)):
      if args[i] == '--dimension' and args[i+1] == dimension:
        return args[:i] + args[i+3:]
    return args

  def choose_random_int(self, max_num):
    return random.randint(1, max_num)

  def pick_bot_configuration(self, verbose):
    # These are the rules used:
    # 1. If any configuration has bots available, pick the configuration with
    #    the most bots available.
    # 2. If no configuration has bots available, pick a random configuration
    #    based on the total number of bots in each configuration.
    #
    # This method updates bot_statuses_ in case (1), and in both cases, returns
    # the index into bot_configs_ that should be used.
    if any(status['available'] > 0 for status in self._bot_statuses):
      # Case 1.
      max_index = 0
      max_val = self._bot_statuses[0]['available']
      for i in xrange(1, len(self._bot_statuses)):
        avail = self._bot_statuses[i]['available']
        if avail > max_val:
          max_index = i
          max_val = avail
      self._bot_statuses[max_index]['available'] -= 1
      assert self._bot_statuses[max_index]['available'] >= 0
      if verbose:
        print 'Chose bot config %d because bots were available' % (max_index)
      return max_index
    # Case 2.
    # We want to choose a bot uniformly at random from all of the bots specified
    # in the bot configs. To do this, we conceptually group the bots into
    # buckets, pick a random number between 1 and the total number of bots, and
    # figure out which bucket of bots it landed in.
    r = self.choose_random_int(self._total_bots)
    for i, status in enumerate(self._bot_statuses):
      if r <= status['total']:
        if verbose:
          print 'Chose bot config %d stochastically' % (i)
        return i
      r -= status['total']
    raise Exception('Should not reach here')

  def make_temp_file(self, prefix=None, suffix=None):
    # This trick of closing the file handle is needed on Windows in order to
    # make the file writeable.
    h, temp_file = tempfile.mkstemp(prefix=prefix, suffix=suffix)
    os.close(h)
    return temp_file

  def delete_temp_file(self, temp_file):
    os.remove(temp_file)

  def read_json_from_temp_file(self, temp_file):
    with open(temp_file) as f:
      return json.load(f)

  def write_json_to_file(self, merged_json, output_file):
    with open(output_file, 'w') as f:
      json.dump(merged_json, f)

  def run_swarming(self, args, verbose):
    if verbose:
      print 'Running Swarming with args:'
      print str(args)
    return subprocess.call([sys.executable, SWARMING_PY] + args)

  def trigger_tasks(self, args, remaining):
    """Triggers tasks for each bot.

    Args:
      args: Parsed arguments which we need to use.
      remaining: The remainder of the arguments, which should be passed to
                 swarming.py calls.

    Returns:
      Exit code for the script.
    """
    verbose = args.multiple_dimension_script_verbose
    self.parse_bot_configs(args)
    self.query_swarming_for_bot_configs(verbose)

    # In the remaining arguments, find the Swarming dimensions that are
    # specified by the bot configs and remove them, because for each shard,
    # we're going to select one of the bot configs and put all of its Swarming
    # dimensions on the command line.
    filtered_remaining_args = copy.deepcopy(remaining)
    for config in self._bot_configs:
      for k in config.iterkeys():
        filtered_remaining_args = self.remove_swarming_dimension(
          filtered_remaining_args, k)

    merged_json = {}

    for i in xrange(args.shards):
      # For each shard that we're going to distribute, do the following:
      # 1. Pick which bot configuration to use.
      # 2. Insert that bot configuration's dimensions as command line
      #    arguments, and invoke "swarming.py trigger".
      bot_index = self.pick_bot_configuration(verbose)
      # Holds the results of the swarming.py trigger call.
      try:
        json_temp = self.make_temp_file(prefix='trigger_multiple_dimensions',
                                        suffix='.json')
        args_to_pass = self.modify_args(filtered_remaining_args, bot_index, i,
                                        args.shards, json_temp)
        ret = self.run_swarming(args_to_pass, verbose)
        if ret:
          sys.stderr.write('Failed to trigger a task, aborting\n')
          return ret
        result_json = self.read_json_from_temp_file(json_temp)
        if i == 0:
          # Copy the entire JSON -- in particular, the "request"
          # dictionary -- from shard 0. "swarming.py collect" uses
          # some keys from this dictionary, in particular related to
          # expiration. It also contains useful debugging information.
          merged_json = copy.deepcopy(result_json)
          # However, reset the "tasks" entry to an empty dictionary,
          # which will be handled specially.
          merged_json['tasks'] = {}
        for k, v in result_json['tasks'].items():
          v['shard_index'] = i
          merged_json['tasks'][k + ':%d:%d' % (i, args.shards)] = v
      finally:
        self.delete_temp_file(json_temp)
    self.write_json_to_file(merged_json, args.dump_json)
    return 0


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--multiple-trigger-configs', type=str, required=True,
                      help='The Swarming configurations to trigger tasks on, '
                      'in the form of a JSON array of dictionaries (these are '
                      'Swarming dimension_sets). At least one entry in this '
                      'dictionary is required.')
  parser.add_argument('--multiple-dimension-script-verbose', type=bool,
                      default=False, help='Turn on verbose logging')
  parser.add_argument('--dump-json', required=True,
                      help='(Swarming Trigger Script API) Where to dump the'
                      ' resulting json which indicates which tasks were'
                      ' triggered for which shards.')
  parser.add_argument('--shards', type=int, default=1,
                      help='How many shards to trigger. Duplicated from the'
                      ' `swarming.py trigger` command.')

  args, remaining = parser.parse_known_args()

  return MultiDimensionTestTriggerer().trigger_tasks(args, remaining)


if __name__ == '__main__':
  sys.exit(main())
