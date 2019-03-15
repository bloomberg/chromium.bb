# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

def main():
  header = ''
  cc = ''
  genDir = ''
  cddlFile = ''

  # Parse and validate input
  args = parseInput()

  assert validateHeaderInput(args.header), \
         "Error: '%s' is not a valid .h file" % args.header
  assert validateCodeInput(args.cc), \
         "Error: '%s' is not a valid .cc file" % args.cc
  assert validatePathInput(args.gen_dir), \
         "Error: '%s' is not a valid output directory" % args.gen_dir
  assert validateCddlInput(args.file), \
         "Error: '%s' is not a valid CDDL file" % args.file

  # Execute command line commands with these variables.
  print('Creating C++ files from provided CDDL file...')
  echoAndRunCommand(['./cddl', "--header", args.header, "--cc", args.cc,
                     "--gen-dir", args.gen_dir, args.file], False)

  clangFormatLocation = findClangFormat()
  if clangFormatLocation == None:
    print("\tWARNING: clang-format could not be found")
  else:
    echoAndRunCommand([clangFormatLocation + 'clang-format', "-i",
                       os.path.join(args.gen_dir, args.header)], True)
    echoAndRunCommand([clangFormatLocation + 'clang-format', "-i",
                       os.path.join(args.gen_dir, args.cc)], True)

def parseInput():
  parser = argparse.ArgumentParser()
  parser.add_argument("--header", help="Specify the filename of the output \
     header file. This is also the name that will be used for the include \
     guard and as the include path in the source file.")
  parser.add_argument("--cc", help="Specify the filename of the output \
     source file")
  parser.add_argument("--gen-dir", help="Specify the directory prefix that \
     should be added to the output header and source file.")
  parser.add_argument("file", help="the input file which contains the spec")
  return parser.parse_args()

def validateHeaderInput(headerFile):
  return headerFile and headerFile.endswith('.h')

def validateCodeInput(ccFile):
  return ccFile and ccFile.endswith('.cc')

def validatePathInput(dirPath):
  return dirPath and os.path.isdir(dirPath)

def validateCddlInput(cddlFile):
  return cddlFile and os.path.isfile(cddlFile)

def echoAndRunCommand(commandArray, allowFailure):
  print("\tExecuting Command: '%s'" % " ".join(commandArray))
  process = subprocess.Popen(commandArray)
  process.wait()
  returncode = process.returncode
  if returncode != 0:
    if not allowFailure:
      sys.exit("\t\tERROR: Command failed with error code: '%i'!" % returncode)
    else:
      print("\t\tWARNING: Command failed with error code: '%i'!" % returncode)

def findClangFormat():
  executable = "clang-format"

  # Try and run from the environment variable
  for directory in os.environ["PATH"].split(os.pathsep):
    fullPath = os.path.join(directory, executable)
    if os.path.isfile(fullPath):
      return ""

  # Check 2 levels up since this should be correct on the build machine
  path = "../../"
  fullPath = os.path.join(path, executable)
  if os.path.isfile(fullPath):
    return path

  return None

main()
