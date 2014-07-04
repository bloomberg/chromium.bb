# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

from lib.policy import PolicySet
from lib.subcommand import SubCommand


LOGGER = logging.getLogger('dmprof')


class ExpandCommand(SubCommand):
  def __init__(self):
    super(ExpandCommand, self).__init__(
        'Usage: %prog expand <dump> <policy> <component> <depth>')
    self._parser.add_option('--alternative-dirs', dest='alternative_dirs',
                            metavar='/path/on/target@/path/on/host[:...]',
                            help='Read files in /path/on/host/ instead of '
                                 'files in /path/on/target/.')

  def do(self, sys_argv):
    options, args = self._parse_args(sys_argv, 4)
    dump_path = args[1]
    target_policy = args[2]
    component_name = args[3]
    depth = args[4]
    alternative_dirs_dict = {}

    policy_set = PolicySet.load(SubCommand._parse_policy_list(target_policy))
    if not policy_set[target_policy].find_rule(component_name):
      sys.stderr.write("ERROR: Component %s not found in policy %s\n"
          % (component_name, target_policy))
      return 1

    if options.alternative_dirs:
      for alternative_dir_pair in options.alternative_dirs.split(':'):
        target_path, host_path = alternative_dir_pair.split('@', 1)
        alternative_dirs_dict[target_path] = host_path
    (bucket_set, dump) = SubCommand.load_basic_files(
        dump_path, False, alternative_dirs=alternative_dirs_dict)

    ExpandCommand._output(dump, policy_set[target_policy], bucket_set,
                          component_name, int(depth), sys.stdout)
    return 0

  @staticmethod
  def _output(dump, policy, bucket_set, component_name, depth, out):
    """Prints all stacktraces in a given component of given depth.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        component_name: A name of component for filtering.
        depth: An integer representing depth to be printed.
        out: An IO object to output.
    """
    sizes = {}

    ExpandCommand._accumulate(
        dump, policy, bucket_set, component_name, depth, sizes)

    sorted_sizes_list = sorted(
        sizes.iteritems(), key=(lambda x: x[1]), reverse=True)
    total = 0
    # TODO(dmikurube): Better formatting.
    for size_pair in sorted_sizes_list:
      out.write('%10d %s\n' % (size_pair[1], size_pair[0]))
      total += size_pair[1]
    LOGGER.info('total: %d\n' % total)

  @staticmethod
  def _add_size(precedence, bucket, depth, committed, sizes):
    stacktrace_sequence = precedence
    for function, sourcefile in zip(
        bucket.symbolized_stackfunction[
            0 : min(len(bucket.symbolized_stackfunction), 1 + depth)],
        bucket.symbolized_stacksourcefile[
            0 : min(len(bucket.symbolized_stacksourcefile), 1 + depth)]):
      stacktrace_sequence += '%s(@%s) ' % (function, sourcefile)
    if not stacktrace_sequence in sizes:
      sizes[stacktrace_sequence] = 0
    sizes[stacktrace_sequence] += committed

  @staticmethod
  def _accumulate(dump, policy, bucket_set, component_name, depth, sizes):
    rule = policy.find_rule(component_name)
    if not rule:
      pass
    elif rule.allocator_type == 'malloc':
      for bucket_id, _, committed, allocs, frees in dump.iter_stacktrace:
        bucket = bucket_set.get(bucket_id)
        if not bucket or bucket.allocator_type == 'malloc':
          component_match = policy.find_malloc(bucket)
        elif bucket.allocator_type == 'mmap':
          continue
        else:
          assert False
        if component_match == component_name:
          precedence = ''
          precedence += '(alloc=%d) ' % allocs
          precedence += '(free=%d) ' % frees
          if bucket.typeinfo:
            precedence += '(type=%s) ' % bucket.symbolized_typeinfo
            precedence += '(type.name=%s) ' % bucket.typeinfo_name
          ExpandCommand._add_size(precedence, bucket, depth, committed, sizes)
    elif rule.allocator_type == 'mmap':
      for _, region in dump.iter_map:
        if region[0] != 'hooked':
          continue
        component_match, bucket = policy.find_mmap(region, bucket_set)
        if component_match == component_name:
          ExpandCommand._add_size('', bucket, depth,
                                  region[1]['committed'], sizes)
