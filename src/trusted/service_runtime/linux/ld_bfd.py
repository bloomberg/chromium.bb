#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for invoking the BFD loader

A simple script to invoke the bfd loader instead of gold.
This script is in a filename "ld" so it can be invoked from gcc
via the -B flag.
"""
# TODO(bradchen): Delete this script when Gold supports linker scripts properly.
import os
import subprocess
import sys

def PathTo(fname):
  if fname[0] == os.pathsep:
    return fname
  for p in os.environ["PATH"].split(os.pathsep):
    fpath = os.path.join(p, fname)
    if os.path.exists(fpath):
      return fpath
  return fname

def GccPrintName(what, switch, defresult):
  cxx = os.getenv("CXX")
  if not cxx:
    cxx = "g++"
  popen = subprocess.Popen(cxx + ' ' + switch,
                           shell=True,
                           stdout=subprocess.PIPE,
                           stdin=subprocess.PIPE)
  result, error = popen.communicate()
  if popen.wait() != 0:
    print "Could not find %s: %s" % (what, error)
    return defresult
  return result.strip()

def FindLDBFD():
  ld = GccPrintName('ld', '-print-prog-name=ld', 'ld')
  ld_bfd = PathTo(ld + ".bfd")
  if os.access(ld_bfd, os.X_OK):
    return ld_bfd
  return ld

def FindLibgcc():
  return GccPrintName('libgcc', '-print-libgcc-file-name', None)

def main():
  args = [FindLDBFD()] + sys.argv[1:]
  libgcc = FindLibgcc()
  if libgcc is not None:
    args.append(libgcc)
  print(sys.argv[0] + ": exec " + ' '.join(args))
  sys.exit(subprocess.call(args))

if __name__ == "__main__":
  main()
