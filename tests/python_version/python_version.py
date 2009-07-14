#!/usr/bin/python

import sys

print "CURRENT CONFIG"
print sys.version


pver = sys.version.split()[0]
print "CURRENT VERSION"
print pver

if pver < "2.4":
  print "ERROR Python version is too old"
  sys.exit(-1)

# NOTE: we may want to guard against this as well
#if pver > "2.6":
#  print "ERROR Python version is too new"
#  sys.exit(-1)

print "Python version is OK"
sys.exit(0)
