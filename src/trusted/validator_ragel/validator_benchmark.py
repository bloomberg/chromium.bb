#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import subprocess
import timeit


def main():
  parser = optparse.OptionParser(
      '%prog [options] <validator_test> <nexe> [<nexe2> ...]')
  parser.add_option(
      '-r',
      '--repeat',
      dest='repeat',
      type=int,
      default=1000,
      help='number of times to run parser'
  )
  options, args = parser.parse_args()

  if len(args) == 0:
    parser.error('specify validator_test')
  elif len(args) == 1:
    parser.error('specify at least one nexe file')

  validator_test = args[0]
  nexes = args[1:]

  print '-' * 30

  start = timeit.default_timer()

  for nexe in nexes:
    print 'validating %s %s times...' % (nexe, options.repeat)
    subprocess.check_call([
         validator_test,
         '--repeat',
         str(options.repeat),
         nexe
    ])

  t = timeit.default_timer() - start

  print 'It took %.3fs' % t
  print '-' * 30


if __name__ == '__main__':
  main()
