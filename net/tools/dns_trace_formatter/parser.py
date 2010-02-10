#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package for parsing the log lines from a Chromium DNS trace capture."""

import re

# The type of log line.
(LINE_TYPE_OUTSTANDING_JOB,
 LINE_TYPE_CREATED_JOB,
 LINE_TYPE_STARTING_JOB,
 LINE_TYPE_RUNNING_JOB,
 LINE_TYPE_COMPLETED_JOB,
 LINE_TYPE_COMPLETING_JOB,
 LINE_TYPE_RECEIVED_REQUEST,
 LINE_TYPE_ATTACHED_REQUEST,
 LINE_TYPE_FINISHED_REQUEST,
 LINE_TYPE_CANCELLED_REQUEST) = range(10)

class ParsedLine(object):
  """Structure that represents a parsed line from the trace log.

  Attributes:
    t: The time (in milliseconds) when the line was logged.
    line_type: The type of event that this line was logging. One of LINE_TYPE_*.
    raw_line: The full unparsed line.
    details: Dictionary containing additional properties that were parsed from
             this line.
  """

  def __init__(self):
    self.t = None
    self.line_type = None
    self.raw_line = None
    self.details = {}


def ParseLine(line):
  """Parses |line| into a ParsedLine. Returns None on failure."""

  m = re.search(r'^t=(\d+):  "(.*)"\s*$', line)
  if not m:
    return None

  parsed = ParsedLine()
  parsed.t = int(m.group(1))
  parsed.raw_line = line.strip()

  msg = m.group(2)

  m = re.search(r"^Received request (r\d+) for (.*)$", msg)
  if m:
    parsed.line_type = LINE_TYPE_RECEIVED_REQUEST
    parsed.entity_id = m.group(1)
    parsed.details['request_details'] = m.group(2)
    return parsed

  m = re.search(r"^Created job (j\d+) for {hostname='([^']*)', "
                "address_family=(\d+)}$", msg)
  if m:
    parsed.line_type = LINE_TYPE_CREATED_JOB
    parsed.entity_id = m.group(1)
    parsed.details['hostname'] = m.group(2)
    parsed.details['address_family'] = m.group(3)
    return parsed

  m = re.search(r"^Outstanding job (j\d+) for {hostname='([^']*)', address_"
                "family=(\d+)}, which was started at t=(\d+)$", msg)
  if m:
    parsed.line_type = LINE_TYPE_OUTSTANDING_JOB
    parsed.t = int(m.group(4))
    parsed.entity_id = m.group(1)
    parsed.details['hostname'] = m.group(2)
    parsed.details['address_family'] = m.group(3)
    return parsed

  m = re.search(r"^Attached request (r\d+) to job (j\d+)$", msg)
  if m:
    parsed.line_type = LINE_TYPE_ATTACHED_REQUEST
    parsed.entity_id = m.group(1)
    parsed.details['job_id'] = m.group(2)
    return parsed

  m = re.search(r'^Finished request (r\d+) (.*)$', msg)
  if m:
    parsed.line_type = LINE_TYPE_FINISHED_REQUEST
    parsed.entity_id = m.group(1)
    parsed.details['extra'] = m.group(2)
    return parsed

  m = re.search(r'^Cancelled request (r\d+)$', msg)
  if m:
    parsed.line_type = LINE_TYPE_CANCELLED_REQUEST
    parsed.entity_id = m.group(1)
    return parsed

  m = re.search(r'^Starting job (j\d+)$', msg)
  if m:
    parsed.line_type = LINE_TYPE_STARTING_JOB
    parsed.entity_id = m.group(1)
    return parsed

  m = re.search(r'\[resolver thread\] Running job (j\d+)$', msg)
  if m:
    parsed.line_type = LINE_TYPE_RUNNING_JOB
    parsed.entity_id = m.group(1)
    return parsed

  m = re.search(r'\[resolver thread\] Completed job (j\d+)$', msg)
  if m:
    parsed.line_type = LINE_TYPE_COMPLETED_JOB
    parsed.entity_id = m.group(1)
    return parsed

  m = re.search(r'Completing job (j\d+).*$', msg)
  if m:
    parsed.line_type = LINE_TYPE_COMPLETING_JOB
    parsed.entity_id = m.group(1)
    return parsed

  return None


def ParseFile(path):
  """Parses the file at |path| and returns a list of ParsedLines."""
  f = open(path, 'r')

  entries = []

  for line in f:
    parsed = ParseLine(line)
    if parsed:
      entries.append(parsed)

  f.close()
  return entries

