# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys

import path_utils

import suppressions


def ReadReportsFromFile(filename):
  input_file = file(filename, 'r')
  ret = []
  in_suppression = False
  cur_supp = []
  for line in input_file:
    line = line.strip()
    line = line.replace("</span><span class=\"stdout\">", "")
    line = line.replace("</span><span class=\"stderr\">", "")
    line = line.replace("&lt;", "<")
    line = line.replace("&gt;", ">")
    if in_suppression:
      if line == "}":
        cur_supp += ["}"]
        ret += ["\n".join(cur_supp)]
        in_suppression = False
        cur_supp = []
      else:
        cur_supp += [" "*3 + line]
    elif line == "{":
      in_suppression = True
      cur_supp = ["{"]
  return ret

filenames = [
  "memcheck/suppressions.txt",
]
# TODO(timurrrr): Support platform-specific suppressions

all_suppressions = []
suppressions_root = path_utils.ScriptDir()

for f in filenames:
  supp_filename = os.path.join(suppressions_root, f)
  all_suppressions += suppressions.ReadSuppressionsFromFile(supp_filename)

reports = []
for f in sys.argv[1:]:
  reports += ReadReportsFromFile(f)
reports = set(reports)
# TODO(timurrrr): For each reports, keep a list of files containing it.

reports_count = 0
for r in reports:
  match = False
  for s in all_suppressions:
    if s.Match(r.split("\n")):
      match = True
      break
  if not match:
    reports_count += 1
    print "==================================="
    print "This report didn't match any suppressions:"
    print r
    print "==================================="

if reports_count > 0:
  print "%d unique reports don't match any of the suppressions" % reports_count
else:
  print "Congratulations! All reports are suppressed!"
  # TODO(timurrrr): also make sure none of the old suppressions
  # were narrowed too much.
