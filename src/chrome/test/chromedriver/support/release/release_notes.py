#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to generate ChromeDriver release notes.
# Before running this script, download list of fixed issues from
# https://crbug.com/chromedriver?can=1&sort=id&q=label:ChromeDriver-81
# to ~/Download/chromedriver-issues.csv. Output is ./notes.txt.

import csv
import datetime
import os
import sys

if len(sys.argv) != 2:
  print 'usage: python %s version' % sys.argv[0]
  sys.exit(1)

version = sys.argv[1]
major_version = version.split('.')[0]

notes_name = 'notes.txt'

csv_file = os.path.join(os.getenv('HOME'), 'Downloads',
                        'chromedriver-issues.csv')
f = open(csv_file, 'r')
lines = f.read().split('\n')
f.close()

fixed_issues = []
seen_header = False
for issue in csv.reader(lines):
  if not issue:
    continue
  if not seen_header:
    if issue[0] == 'ID':
      for i,title in enumerate(issue):
        if title == 'Pri':
          pri_col = i
        elif title == 'Summary':
          summary_col = i
      if pri_col is None or summary_col is None:
        print 'Missing Pri or Summary in headers'
        sys.exit(1)
      seen_header = True
    continue

  issue_id = issue[0]
  desc = issue[summary_col]
  pri = issue[pri_col]
  fixed_issues.append('Resolved issue %s: %s [Pri-%s]' % (issue_id, desc, pri))

new_notes = '---------ChromeDriver %s (%s)---------\n%s\n%s\n' % (
    version, datetime.date.today().isoformat(),
    'Supports Chrome version %s' % major_version,
    '\n'.join(fixed_issues))
with open(notes_name, 'w') as f:
  f.write(new_notes)
