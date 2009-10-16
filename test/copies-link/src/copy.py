#!/usr/bin/python

import shutil
import sys


def main(argv):
  if len(argv) != 3:
    print 'USAGE: copy.py <src> <dst>'
    return 1

  shutil.copy(argv[1], argv[2])
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
