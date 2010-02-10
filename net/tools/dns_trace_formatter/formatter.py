#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Application to format Chromium's host resolver traces to something readable.

To use it, start by enabling tracing in Chromium:
    chrome://net-internals/hostresolver

Next, copy-paste the trace results (those lines starting with "t=") and save
them to a text file.

Lastly, run:
   python formatter.py <file-containing-trace>
"""

import sys

import parser

# The log lines of the host resolver trace either describe a "request" or a
# "job".  The "request" is the actual DNS resolve request, whereas the "job" is
# a helper that was spawned to service requests on a particular thread.
#
# In the log lines, each request or job is identified using a unique numeric ID.
#
# Since DNS resolving is multi-threaded, the log lines may appear jumbled.
#
# The first thing this code does is to group the log lines by id. This
# collection of log lines is called an "entry", and represented by either
# RequestEntry or JobEntry.
#
# So for example, say we had a log resembling:
#
#   t=1: starting request r1
#   t=1: starting job j1
#   t=2: starting request r2
#   t=3: completed job j1
#   t=4: finished request r2
#   t=5: cancelled request r1
#
# We would start by building three entries:
#
#   (id='r1')
#   t=1: starting request r1
#   t=5: cancelled request r1
#
#   (id='j1')
#   t=1: starting job j1
#   t=3: completed job j1
#
#   (id='r2')
#   t=2: starting request r2
#   t=4: finished request r2

(ENTRY_TYPE_JOB, ENTRY_TYPE_REQUEST) = (0, 1)


class EntryBase(object):
  """Helper structure that describes an individual hostresolver request/job."""

  def __init__(self, entries, entity_id, parsed_lines):
    """Constructor for EntryBase.

    Args:
      entries: The full list of EntryBase that this new instance belongs to.
      entity_id: The ID of this request/job.
      parsed_lines: An ordered list of ParsedLine for the log lines that apply
                    to this request/job.
    """
    self._entries = entries
    self._entity_id = entity_id
    self._parsed_lines = parsed_lines

  def HasParsedLineOfType(self, line_type):
    """Returns true if this request/job contains a log line of type |type|."""
    for line in self._parsed_lines:
      if line.line_type == line_type:
        return True
    return False

  def GetElapsedTime(self):
    """Returns the start to finish duration of this request/job."""
    return self.GetEndTime() - self.GetStartTime()

  def GetStartTime(self):
    """Returns the start time for this request/job."""
    return self._parsed_lines[0].t

  def GetEndTime(self):
    """Returns the end time of this request/job."""
    return self._parsed_lines[-1].t

  def HasUnknownStart(self):
    """Returns true if the exact start of this request/job is unknown."""
    return 'is_fake' in self._parsed_lines[0].details

  def HasUnknownEnd(self):
    """Returns true if the exact end of this request/job is unknown."""
    return 'is_fake' in self._parsed_lines[-1].details

  def WasAliveAt(self, t):
    """Returns true if this request/job was running at time |t|."""
    return t >= self.GetStartTime() and t <= self.GetEndTime()

  def GetEntryType(self):
    """Returns whether this entry represents a request or a job.
    Should be one of the enums ENTRY_TYPE_*"""
    return None

  def GetLiveEntriesAtStart(self, entry_type):
    return [entry for entry in self._entries
            if (entry != self and entry.WasAliveAt(self.GetStartTime()) and
                entry.GetEntryType() == entry_type)]

  def Print(self):
    """Outputs a text description of this request/job."""
    print '------------------------'
    print self.GetCaption()
    print '------------------------'
    print self.GetDetails()

  def GetElapsedTimeString(self):
    """Returns a string that describes how long this request/job took."""
    if self.HasUnknownStart() or self.HasUnknownEnd():
      return '%d+ millis' % self.GetElapsedTime()
    return '%d millis' % self.GetElapsedTime()


class RequestEntry(EntryBase):
  """Specialization of EntryBase that describes hostresolver request."""

  def __init__(self, entries, entity_id, parsed_lines, min_time, max_time):
    """Constructor for RequestEntry.

    Args:
      entries: The full list of EntryBase that this new instance belongs to.
      entity_id: The ID of this request.
      parsed_lines: An ordered list of ParsedLine for the log lines that apply
                    to this request.
      min_time: The start time of the log.
      max_time: The end time of the log.
    """
    EntryBase.__init__(self, entries, entity_id, parsed_lines)

    # Scan the parsed lines for this request to find the details on the request.
    self.request_details = '???'
    for line in parsed_lines:
      if 'request_details' in line.details:
        self.request_details = line.details['request_details']
        break

    # If the log didn't capture when the request was received, manufacture a log
    # entry for the start, at t = min_time - 1
    if not self.HasParsedLineOfType(parser.LINE_TYPE_RECEIVED_REQUEST):
      fake_line = parser.ParsedLine()
      fake_line.t = min_time - 1
      fake_line.line_type = parser.LINE_TYPE_RECEIVED_REQUEST
      fake_line.raw_line = 'Unknown start of request'
      fake_line.details['is_fake'] = True
      self._parsed_lines.insert(0, fake_line)

    # If the log didn't capture when the job ended, manufacture a log entry for
    # the end, at t = max_time + 1
    if not (self.HasParsedLineOfType(parser.LINE_TYPE_FINISHED_REQUEST) or
            self.HasParsedLineOfType(parser.LINE_TYPE_CANCELLED_REQUEST)):
      fake_line = parser.ParsedLine()
      fake_line.t = max_time + 1
      fake_line.line_type = parser.LINE_TYPE_FINISHED_REQUEST
      fake_line.raw_line = 'Unknown end of request'
      fake_line.details['is_fake'] = True
      self._parsed_lines.append(fake_line)

  def GetEntryType(self):
    return ENTRY_TYPE_REQUEST

  def GetCaption(self):
    return 'Request %s (took %s) for %s ' % (self._entity_id,
                                             self.GetElapsedTimeString(),
                                             self.request_details)

  def GetDetails(self):
    reqs = self.GetLiveEntriesAtStart(ENTRY_TYPE_REQUEST)
    out = [('There were %d requests already in progress when this '
            'started:') % len(reqs)]
    out.extend(['    ' + r.GetCaption() for r in reqs])

    out.append('Log lines:')
    out.extend(['    ' + line.raw_line for line in self._parsed_lines])

    return '\n'.join(out)


class JobEntry(EntryBase):
  """Specialization of EntryBase that describes hostresolver request."""

  def __init__(self, entries, entity_id, parsed_lines, min_time, max_time):
    """Constructor for JobEntry.

    Args:
      entries: The full list of EntryBase that this new instance belongs to.
      entity_id: The ID of this job.
      parsed_lines: An ordered list of ParsedLine for the log lines that apply
                    to this job.
      min_time: The start time of the log.
      max_time: The end time of the log.
    """
    EntryBase.__init__(self, entries, entity_id, parsed_lines)

    # Find the hostname/address_family of the job
    self.hostname = '???'
    self.address_family = '???'

    for line in parsed_lines:
      if 'hostname' in line.details and 'address_family' in line.details:
        self.hostname = line.details['hostname']
        self.address_family = line.details['address_family']
        break

    # If the log didn't capture when the job started, manufacture a start time.
    if not (self.HasParsedLineOfType(parser.LINE_TYPE_CREATED_JOB) or
            self.HasParsedLineOfType(parser.LINE_TYPE_OUTSTANDING_JOB) or
            self.HasParsedLineOfType(parser.LINE_TYPE_STARTING_JOB)):
      fake_line = parser.ParsedLine()
      fake_line.t = min_time - 1
      fake_line.line_type = parser.LINE_TYPE_OUTSTANDING_JOB
      fake_line.raw_line = 'Unknown start of job'
      fake_line.details['is_fake'] = True
      self._parsed_lines.insert(0, fake_line)

    # If the log didn't capture when the job ended, manufacture an end time.
    if not self.HasParsedLineOfType(parser.LINE_TYPE_COMPLETING_JOB):
      fake_line = parser.ParsedLine()
      fake_line.t = max_time + 1
      fake_line.line_type = parser.LINE_TYPE_COMPLETING_JOB
      fake_line.raw_line = 'Unknown end of job'
      fake_line.details['is_fake'] = True
      self._parsed_lines.append(fake_line)

  def GetEntryType(self):
    return ENTRY_TYPE_JOB

  def GetCaption(self):
    return 'Job %s (took %s) for "%s" ' % (self._entity_id,
                                           self.GetElapsedTimeString(),
                                           self.hostname)

  def GetDetails(self):
    jobs = self.GetLiveEntriesAtStart(ENTRY_TYPE_JOB)
    out = [('There were %d jobs already in progress when '
            'this started:' % len(jobs))]
    out.extend(['    ' + j.GetCaption() for j in jobs])

    out.append('Log lines:')
    out.extend(['    ' + line.raw_line for line in self._parsed_lines])

    return '\n'.join(out)


def BuildEntries(parsed_lines, min_time, max_time):
  """Returns a list of BaseEntrys built from |parsed_lines|."""

  # In this loop we aggregate all of the parsed lines with common entity_id, and
  # also determine the order that entity_ids are first seen in.
  id_to_line_list = {}
  entity_ids = []
  for line in parsed_lines:
    entity_id = line.entity_id
    if not entity_id in entity_ids:
      entity_ids.append(entity_id)
    lines = id_to_line_list.setdefault(entity_id, [])
    lines.append(line)

  # Create an entry (either JobEntry or RequestEntry) for each unique entity_id
  # in the trace. Ordered by their first appearance in the trace.

  entries = []
  for entity_id in entity_ids:
    if entity_id.startswith('j'):
      entries.append(JobEntry(entries,
                              entity_id, id_to_line_list[entity_id],
                              min_time, max_time))
    if entity_id.startswith('r'):
      entries.append(RequestEntry(entries,
                                  entity_id, id_to_line_list[entity_id],
                                  min_time, max_time))

  return entries


def main():
  if len(sys.argv) != 2:
    print 'Usage: %s <logfile_path>' % sys.argv[0]
    sys.exit(1)

  parsed_lines = parser.ParseFile(sys.argv[1])

  min_time = parsed_lines[0].t
  max_time = parsed_lines[-1].t

  entries = BuildEntries(parsed_lines, min_time, max_time)

  for entry in entries:
    entry.Print()


if __name__ == '__main__':
  main()

