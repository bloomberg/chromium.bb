#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parse NetLog output and reformat it to display in Gnuplot."""

import math
import optparse
import os
import re
import sys


class Entry(object):
  """A single NetLog line/entry."""

  def __init__(self, line):
    self.id = -1
    self.info = ''
    self.time = 0

    time_offset = line.find('t=')
    if time_offset >= 0:
      tmp = line[time_offset + 2:].split(None, 1)
      self.time = tmp[0][:-3] + '.' + tmp[0][-3:]
      if len(tmp) > 1:
        self.info = tmp[1]
    else:
      self.info = line
    self.info = re.sub('\[dt=[0-9 ?]*\]', '', self.info)
    self.info = self.info.strip()


class Object(object):
  """A set of Entries that belong to the same NetLog Object."""

  def __init__(self, line, id_offset):
    self.id = line[id_offset + 4:].rstrip(')')
    self.type = line.split(None, 1)[0]
    self.entries = []

  def SpaceOutEntries(self):
    self.__FindGroups(self.__SpaceRange)

  def SetRotation(self):
    self.__FindGroups(self.__SetRotationRange)

  def __FindGroups(self, proc):
    first = 0
    for i in range(len(self.entries)):
      if self.entries[first].time != self.entries[i].time:
        proc(first, i - 1)
        first = i
    proc(first, len(self.entries) - 1)

  def __SpaceRange(self, start, end):
    if end > start:
      gap = math.floor(100/(end - start + 2))
      for i in range(start + 1, end + 1):
        self.entries[i].time += '%02d'%(gap * (i - start))

  def __SetRotationRange(self, start, end):
    for i in range(start, end + 1):
      self.entries[i].rotation = 85 - (i - start) * 15


def Parse(stream):
  """Parse the NetLog into python objects."""

  result = []
  request_section = 0
  obj = None
  entry = None

  for line in stream:
    line = line.strip()
    if line.startswith('Requests'):
      request_section = 1
    elif line.startswith('Http cache'):
      request_section = 0

    if request_section:
      id_offset = line.find('(id=')
      if id_offset >= 0:
        obj = Object(line, id_offset)
        result.append(obj)
      elif line.startswith('(P) t=') or line.startswith('t='):
        entry = Entry(line)
        obj.entries.append(entry)
      elif line.find('source_dependency') >= 0:
        new_entry = Entry(line)
        new_entry.time = entry.time
        new_entry.id = line.split(':')[1].split(',')[0]
        obj.entries.append(new_entry)
        new_entry = Entry('t=%s '%entry.time.replace('.', ''))
        obj.entries.append(new_entry)
      elif line.startswith('-->'):
        entry.info = entry.info + ' ' + line[4:]

  return result


def GnuplotRenderNetlog(netlog, filename, labels):
  """Render a list of NetLog objects into Gnuplot format."""

  output = open(filename, 'w')

  commands = []
  data = []
  types = []

  commands.append('file="%s"'%filename)
  commands.append('set key bottom')
  commands.append('set xdata time')
  commands.append('set timefmt "%s"')
  commands.append('set datafile separator ","')
  commands.append('set xlabel "Time (s)"')
  commands.append('set ylabel "Netlog ID"')

  plot = []
  index = 1
  for obj in netlog:
    if obj.type not in types:
      types.append(obj.type)
    type_num = types.index(obj.type)
    plot.append('file index %d using 1:2 with linespoints lt %d notitle'%(
        index, type_num))
    obj.SetRotation()
    for entry in obj.entries:
      graph_id = obj.id
      if entry.id > 0:
        graph_id = entry.id
      data.append('%s,%s'%(entry.time, graph_id))
      info = entry.info.replace('"', '')
      if info and labels:
        commands.append('set label "%s" at "%s", %s rotate by %d'%(
            info, entry.time, obj.id, entry.rotation))
    data.append('\n')
    index += 1

  for entry_type in types:
    plot.insert(0, '1/0 lt %d title "%s"'%(types.index(entry_type), entry_type))
  commands.append('plot ' + ','.join(plot))
  commands.append('exit')
  result = '\n'.join(commands) + '\n\n\n' + '\n'.join(data)

  output.write(result)
  output.close()
  os.system('gnuplot %s -'%filename)


def main(_):
  parser = optparse.OptionParser('usage: %prog [options] dump_file')
  parser.add_option_group(
      optparse.OptionGroup(parser, 'Additional Info',
                           'When run, the script will generate a file that can '
                           'be passed to gnuplot, but will also start gnuplot '
                           'for you; left click selects a zoom region, u '
                           'unzooms, middle click adds a marker, and q quits.'))
  parser.add_option('-o', '--output', dest='output', help='Output filename')
  parser.add_option('-l', '--labels', action='store_true', help='Output labels')
  options, args = parser.parse_args()
  if not args:
    parser.error('Must specify input dump_file.')
  if not options.output:
    options.output = args[0] + '.gnuplot'

  netlog = Parse(open(args[0]))
  GnuplotRenderNetlog(netlog, options.output, options.labels)
  return 0


if '__main__' == __name__:
  sys.exit(main(sys.argv))
