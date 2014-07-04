# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from lib.subcommand import SubCommand


class StacktraceCommand(SubCommand):
  def __init__(self):
    super(StacktraceCommand, self).__init__(
        'Usage: %prog stacktrace <dump>')

  def do(self, sys_argv):
    _, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    (bucket_set, dump) = SubCommand.load_basic_files(dump_path, False)

    StacktraceCommand._output(dump, bucket_set, sys.stdout)
    return 0

  @staticmethod
  def _output(dump, bucket_set, out):
    """Outputs a given stacktrace.

    Args:
        bucket_set: A BucketSet object.
        out: A file object to output.
    """
    for bucket_id, virtual, committed, allocs, frees in dump.iter_stacktrace:
      bucket = bucket_set.get(bucket_id)
      if not bucket:
        continue
      out.write('%d %d %d %d ' % (virtual, committed, allocs, frees))
      for frame in bucket.symbolized_stackfunction:
        out.write(frame + ' ')
      out.write('\n')
