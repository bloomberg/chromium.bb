#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import os
import re
import sys

import suppressions


def ReadReportsFromFile(filename):
  """ Returns a list of (report_hash, report) and the URL of the report on the
  waterfall.
  """
  input_file = file(filename, 'r')
  # reports is a list of (error hash, report) pairs.
  reports = []
  in_suppression = False
  cur_supp = []
  # This stores the last error hash found while reading the file.
  last_hash = ""
  for line in input_file:
    line = line.strip()
    line = line.replace("</span><span class=\"stdout\">", "")
    line = line.replace("</span><span class=\"stderr\">", "")
    line = line.replace("&lt;", "<")
    line = line.replace("&gt;", ">")
    if in_suppression:
      if line == "}":
        cur_supp += ["}"]
        reports += [[last_hash, "\n".join(cur_supp)]]
        in_suppression = False
        cur_supp = []
        last_hash = ""
      else:
        cur_supp += [" "*3 + line]
    elif line == "{":
      in_suppression = True
      cur_supp = ["{"]
    elif line.find("Suppression (error hash=#") == 0:
      last_hash = line[25:41]
  # The line at the end of the file is assumed to store the URL of the report.
  return reports,line


def main(argv):
  supp = suppressions.GetSuppressions()

  # all_reports is a map {report: list of urls containing this report}
  all_reports = defaultdict(list)
  report_hashes = {}

  for f in argv:
    f_reports, url = ReadReportsFromFile(f)
    for (hash, report) in f_reports:
      all_reports[report] += [url]
      report_hashes[report] = hash

  reports_count = 0
  for r in all_reports:
    cur_supp = supp['common_suppressions']
    if all([re.search("%20Mac%20|mac_valgrind", url)
            for url in all_reports[r]]):
      # Include mac suppressions if the report is only present on Mac
      cur_supp += supp['mac_suppressions']
    elif all([re.search("Windows%20", url) for url in all_reports[r]]):
      # Include win32 suppressions if the report is only present on Windows
      cur_supp += supp['win_suppressions']
    elif all([re.search("Linux%20", url) for url in all_reports[r]]):
      cur_supp += supp['linux_suppressions']
    elif all([re.search("%20Heapcheck", url)
              for url in all_reports[r]]):
      cur_supp += supp['heapcheck_suppressions']
    if all(["DrMemory" in url for url in all_reports[r]]):
      cur_supp += supp['drmem_suppressions']
    if all(["DrMemory%20full" in url for url in all_reports[r]]):
      cur_supp += supp['drmem_full_suppressions']

    match = False
    for s in cur_supp:
      if s.Match(r.split("\n")):
        match = True
        break
    if not match:
      reports_count += 1
      print "==================================="
      print "This report observed at"
      for url in all_reports[r]:
        print "  %s" % url
      print "didn't match any suppressions:"
      print "Suppression (error hash=#%s#):" % (report_hashes[r])
      print r
      print "==================================="

  if reports_count > 0:
    print ("%d unique reports don't match any of the suppressions" %
           reports_count)
  else:
    print "Congratulations! All reports are suppressed!"
    # TODO(timurrrr): also make sure none of the old suppressions
    # were narrowed too much.


if __name__ == "__main__":
  main(sys.argv[1:])
