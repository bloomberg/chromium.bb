#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# 'top'-like memory polling for Chrome on Android

import argparse
import curses
import os
import re
import sys
import time

from operator import sub

sys.path.append(os.path.join(os.path.dirname(__file__),
                             os.pardir,
                             os.pardir,
                             'build',
                             'android'))
from pylib import android_commands
from pylib.device import adb_wrapper
from pylib.device import device_errors

class Validator(object):
  """A helper class with validation methods for argparse."""

  @staticmethod
  def ValidatePath(path):
    """An argparse validation method to make sure a file path is writable."""
    if os.path.exists(path):
      return path
    elif os.access(os.path.dirname(path), os.W_OK):
      return path
    raise argparse.ArgumentTypeError("%s is an invalid file path" % path)

  @staticmethod
  def ValidatePdfPath(path):
    """An argparse validation method to make sure a pdf file path is writable.
    Validates a file path to make sure it is writable and also appends '.pdf' if
    necessary."""
    if os.path.splitext(path)[-1].lower() != 'pdf':
      path = path + '.pdf'
    return Validator.ValidatePath(path)

  @staticmethod
  def ValidateNonNegativeNumber(val):
    """An argparse validation method to make sure a number is not negative."""
    ival = int(val)
    if ival < 0:
      raise argparse.ArgumentTypeError("%s is a negative integer" % val)
    return ival

class Timer(object):
  """A helper class to track timestamps based on when this program was
  started"""
  starting_time = time.time()

  @staticmethod
  def GetTimestamp():
    """A helper method to return the time (in seconds) since this program was
    started."""
    return time.time() - Timer.starting_time

class DeviceHelper(object):
  """A helper class with various generic device interaction methods."""

  @staticmethod
  def GetDeviceModel(adb):
    """Returns the model of the device with the |adb| connection."""
    return adb.Shell(' '.join(['getprop', 'ro.product.model'])).strip()

  @staticmethod
  def GetDeviceToTrack(preset=None):
    """Returns a device serial to connect to.  If |preset| is specified it will
    return |preset| if it is connected and |None| otherwise.  If |preset| is not
    specified it will return the first connected device."""
    devices = android_commands.GetAttachedDevices()
    if not devices:
      return None

    if preset:
      return preset if preset in devices else None

    return devices[0]

  @staticmethod
  def GetPidsToTrack(adb, default_pid=None, process_filter=None):
    """Returns a list of pids based on the input arguments.  If |default_pid| is
    specified it will return that pid if it exists.  If |process_filter| is
    specified it will return the pids of processes with that string in the name.
    If both are specified it will intersect the two."""
    pids = []
    try:
      cmd = ['ps']
      if default_pid:
        cmd.extend(['|', 'grep', '-F', str(default_pid)])
      if process_filter:
        cmd.extend(['|', 'grep', '-F', process_filter])
      pid_str = adb.Shell(' '.join(cmd))
      for line in pid_str.splitlines():
        data = re.split('\s+', line.strip())
        pid = data[1]
        name = data[-1]

        # Confirm that the pid and name match.  Using a regular grep isn't
        # reliable when doing it on the whole 'ps' input line.
        pid_matches = not default_pid or pid == str(default_pid)
        name_matches = not process_filter or name.find(process_filter) != -1
        if pid_matches and name_matches:
          pids.append((pid, name))
    except device_errors.AdbShellCommandFailedError:
      pass
    return pids

class MemoryHelper(object):
  """A helper class to query basic memory usage of a process."""

  @staticmethod
  def QueryMemory(adb, pid):
    """Queries the device for memory information about the process with a pid of
    |pid|.  It will query Native, Dalvik, and Pss memory of the process.  It
    returns a list of values: [ Native, Pss, Dalvik ].  If the process is not
    found it will return [ 0, 0, 0 ]."""
    results = [0, 0, 0]

    memstr = adb.Shell(' '.join(['dumpsys', 'meminfo', pid]))
    for line in memstr.splitlines():
      match = re.split('\s+', line.strip())

      # Skip data after the 'App Summary' line.  This is to fix builds where
      # they have more entries that might match the other conditions.
      if len(match) >= 2 and match[0] == 'App' and match[1] == 'Summary':
        break

      result_idx = None
      query_idx = None
      if match[0] == 'Native' and match[1] == 'Heap':
        result_idx = 0
        query_idx = -2
      elif match[0] == 'Dalvik' and match[1] == 'Heap':
        result_idx = 2
        query_idx = -2
      elif match[0] == 'TOTAL':
        result_idx = 1
        query_idx = 1

      # If we already have a result, skip it and don't overwrite the data.
      if result_idx is not None and results[result_idx] != 0:
        continue

      if result_idx is not None and query_idx is not None:
        results[result_idx] = round(float(match[query_idx]) / 1000.0, 2)
    return results

class GraphicsHelper(object):
  """A helper class to query basic graphics memory usage of a process."""

  # TODO(dtrainor): Find a generic way to query/fall back for other devices.
  # Is showmap consistently reliable?
  __NV_MAP_MODELS = ['Xoom']
  __NV_MAP_FILE_LOCATIONS = ['/d/nvmap/generic-0/clients',
                             '/d/nvmap/iovmm/clients']

  __SHOWMAP_MODELS = ['Nexus S',
                      'Nexus S 4G',
                      'Galaxy Nexus',
                      'Nexus 4',
                      'Nexus 5',
                      'Nexus 7']
  __SHOWMAP_KEY_MATCHES = ['/dev/pvrsrvkm',
                           '/dev/kgsl-3d0']

  @staticmethod
  def __QueryShowmap(adb, pid):
    """Attempts to query graphics memory via the 'showmap' command.  It will
    look for |self.__SHOWMAP_KEY_MATCHES| entries to try to find one that
    represents the graphics memory usage.  Will return this as a single entry
    array of [ Graphics ].  If not found, will return [ 0 ]."""
    try:
      memstr = adb.Shell(' '.join(['showmap', '-t', pid]))
      for line in memstr.splitlines():
        match = re.split('[ ]+', line.strip())
        if match[-1] in GraphicsHelper.__SHOWMAP_KEY_MATCHES:
          return [ round(float(match[2]) / 1000.0, 2) ]
    except device_errors.AdbShellCommandFailedError:
      pass
    return [ 0 ]

  @staticmethod
  def __NvMapPath(adb):
    """Attempts to find a valid NV Map file on the device.  It will look for a
    file in |self.__NV_MAP_FILE_LOCATIONS| and see if one exists.  If so, it
    will return it."""
    for nv_file in GraphicsHelper.__NV_MAP_FILE_LOCATIONS:
      exists = adb.shell(' '.join(['ls', nv_file]))
      if exists == nv_file.split('/')[-1]:
        return nv_file
    return None

  @staticmethod
  def __QueryNvMap(adb, pid):
    """Attempts to query graphics memory via the NV file map method.  It will
    find a possible NV Map file from |self.__NvMapPath| and try to parse the
    graphics memory from it.  Will return this as a single entry array of
    [ Graphics ].  If not found, will return [ 0 ]."""
    nv_file = GraphicsHelper.__NvMapPath(adb)
    if nv_file:
      memstr = adb.Shell(' '.join(['cat', nv_file]))
      for line in memstr.splitlines():
        match = re.split(' +', line.strip())
        if match[2] == pid:
          return [ round(float(match[3]) / 1000000.0, 2) ]
    return [ 0 ]

  @staticmethod
  def QueryVideoMemory(adb, pid):
    """Queries the device for graphics memory information about the process with
    a pid of |pid|.  Not all devices are currently supported.  If possible, this
    will return a single entry array of [ Graphics ].  Otherwise it will return
    [ 0 ].

    Please see |self.__NV_MAP_MODELS| and |self.__SHOWMAP_MODELS|
    to see if the device is supported.  For new devices, see if they can be
    supported by existing methods and add their entry appropriately.  Also,
    please add any new way of querying graphics memory as they become
    available."""
    model = DeviceHelper.GetDeviceModel(adb)
    if model in GraphicsHelper.__NV_MAP_MODELS:
      return GraphicsHelper.__QueryNvMap(adb, pid)
    elif model in GraphicsHelper.__SHOWMAP_MODELS:
      return GraphicsHelper.__QueryShowmap(adb, pid)
    return [ 0 ]

class MemorySnapshot(object):
  """A class holding a snapshot of memory for various pids that are being
  tracked.

  Attributes:
    pids:      A list of tuples (pid, process name) that should be tracked.
    memory:    A map of entries of pid => memory consumption array.  Right now
               the indices are [ Native, Pss, Dalvik, Graphics ].
    timestamp: The amount of time (in seconds) between when this program started
               and this snapshot was taken.
  """

  def __init__(self, adb, pids):
    """Creates an instances of a MemorySnapshot with an |adb| device connection
    and a list of (pid, process name) tuples."""
    super(MemorySnapshot, self).__init__()

    self.pids = pids
    self.memory = {}
    self.timestamp = Timer.GetTimestamp()

    for (pid, name) in pids:
      self.memory[pid] = self.__QueryMemoryForPid(adb, pid)

  @staticmethod
  def __QueryMemoryForPid(adb, pid):
    """Queries the |adb| device for memory information about |pid|.  This will
    return a list of memory values that map to [ Native, Pss, Dalvik,
    Graphics ]."""
    results = MemoryHelper.QueryMemory(adb, pid)
    results.extend(GraphicsHelper.QueryVideoMemory(adb, pid))
    return results

  def __GetProcessNames(self):
    """Returns a list of all of the process names tracked by this snapshot."""
    return [tuple[1] for tuple in self.pids]

  def HasResults(self):
    """Whether or not this snapshot was tracking any processes."""
    return self.pids

  def GetPidAndNames(self):
    """Returns a list of (pid, process name) tuples that are being tracked in
    this snapshot."""
    return self.pids

  def GetNameForPid(self, search_pid):
    """Returns the process name of a tracked |search_pid|.  This only works if
    |search_pid| is tracked by this snapshot."""
    for (pid, name) in self.pids:
      if pid == search_pid:
        return name
    return None

  def GetResults(self, pid):
    """Returns a list of entries about the memory usage of the process specified
    by |pid|.  This will be of the format [ Native, Pss, Dalvik, Graphics ]."""
    if pid in self.memory:
      return self.memory[pid]
    return None

  def GetLongestNameLength(self):
    """Returns the length of the longest process name tracked by this
    snapshot."""
    return len(max(self.__GetProcessNames(), key=len))

  def GetTimestamp(self):
    """Returns the time since program start that this snapshot was taken."""
    return self.timestamp

class OutputBeautifier(object):
  """A helper class to beautify the memory output to various destinations.

  Attributes:
    can_color: Whether or not the output should include ASCII color codes to
               make it look nicer.  Default is |True|.  This is disabled when
               writing to a file or a graph.
    overwrite: Whether or not the output should overwrite the previous output.
               Default is |True|.  This is disabled when writing to a file or a
               graph.
  """

  __MEMORY_COLUMN_TITLES = ['Native',
                            'Pss',
                            'Dalvik',
                            'Graphics']

  __TERMINAL_COLORS = {'ENDC': 0,
                       'BOLD': 1,
                       'GREY30': 90,
                       'RED': 91,
                       'DARK_YELLOW': 33,
                       'GREEN': 92}

  def __init__(self, can_color=True, overwrite=True):
    """Creates an instance of an OutputBeautifier."""
    super(OutputBeautifier, self).__init__()
    self.can_color = can_color
    self.overwrite = overwrite

    self.lines_printed = 0
    self.printed_header = False

  @staticmethod
  def __FindPidsForSnapshotList(snapshots):
    """Find the set of unique pids across all every snapshot in |snapshots|."""
    pids = set()
    for snapshot in snapshots:
      for (pid, name) in snapshot.GetPidAndNames():
        pids.add((pid, name))
    return pids

  @staticmethod
  def __TermCode(num):
    """Escapes a terminal code.  See |self.__TERMINAL_COLORS| for a list of some
    terminal codes that are used by this program."""
    return '\033[%sm' % num

  @staticmethod
  def __PadString(string, length, left_align):
    """Pads |string| to at least |length| with spaces.  Depending on
    |left_align| the padding will appear at either the left or the right of the
    original string."""
    return (('%' if left_align else '%-') + str(length) + 's') % string

  @staticmethod
  def __GetDiffColor(delta):
    """Returns a color based on |delta|.  Used to color the deltas between
    different snapshots."""
    if not delta or delta == 0.0:
      return 'GREY30'
    elif delta < 0:
      return 'GREEN'
    elif delta > 0:
      return 'RED'

  def __ColorString(self, string, color):
    """Colors |string| based on |color|.  |color| must be in
    |self.__TERMINAL_COLORS|.  Returns the colored string or the original
    string if |self.can_color| is |False| or the |color| is invalid."""
    if not self.can_color or not color or not self.__TERMINAL_COLORS[color]:
      return string

    return '%s%s%s' % (
        self.__TermCode(self.__TERMINAL_COLORS[color]),
        string,
        self.__TermCode(self.__TERMINAL_COLORS['ENDC']))

  def __PadAndColor(self, string, length, left_align, color):
    """A helper method to both pad and color the string.  See
    |self.__ColorString| and |self.__PadString|."""
    return self.__ColorString(
      self.__PadString(string, length, left_align), color)

  def __OutputLine(self, line):
    """Writes a line to the screen.  This also tracks how many times this method
    was called so that the screen can be cleared properly if |self.overwrite| is
    |True|."""
    sys.stdout.write(line + '\n')
    if self.overwrite:
      self.lines_printed += 1

  def __ClearScreen(self):
    """Clears the screen based on the number of times |self.__OutputLine| was
    called."""
    if self.lines_printed == 0 or not self.overwrite:
      return

    key_term_up = curses.tparm(curses.tigetstr('cuu1'))
    key_term_clear_eol = curses.tparm(curses.tigetstr('el'))
    key_term_go_to_bol = curses.tparm(curses.tigetstr('cr'))

    sys.stdout.write(key_term_go_to_bol)
    sys.stdout.write(key_term_clear_eol)

    for i in range(self.lines_printed):
      sys.stdout.write(key_term_up)
      sys.stdout.write(key_term_clear_eol)
    self.lines_printed = 0

  def __PrintBasicStatsHeader(self):
    """Returns a common header for the memory usage stats."""
    titles = ''
    for title in self.__MEMORY_COLUMN_TITLES:
       titles += self.__PadString(title, 8, True) + ' '
       titles += self.__PadString('', 8, True)
    return self.__ColorString(titles, 'BOLD')

  def __PrintLabeledStatsHeader(self, snapshot):
    """Returns a header for the memory usage stats that includes sections for
    the pid and the process name.  The available room given to the process name
    is based on the length of the longest process name tracked by |snapshot|.
    This header also puts the timestamp of the snapshot on the right."""
    if not snapshot or not snapshot.HasResults():
      return

    name_length = max(8, snapshot.GetLongestNameLength())

    titles = self.__PadString('Pid', 8, True) + ' '
    titles += self.__PadString('Name', name_length, False) + ' '
    titles += self.__PrintBasicStatsHeader()
    titles += '(' + str(round(snapshot.GetTimestamp(), 2)) + 's)'
    titles = self.__ColorString(titles, 'BOLD')
    return titles

  def __PrintTimestampedBasicStatsHeader(self):
    """Returns a header for the memory usage stats that includes a the
    timestamp of the snapshot."""
    titles = self.__PadString('Timestamp', 8, False) + ' '
    titles = self.__ColorString(titles, 'BOLD')
    titles += self.__PrintBasicStatsHeader()
    return titles

  def __PrintBasicSnapshotStats(self, pid, snapshot, prev_snapshot):
    """Returns a string that contains the basic snapshot memory statistics.
    This string should line up with the header returned by
    |self.__PrintBasicStatsHeader|."""
    if not snapshot or not snapshot.HasResults():
      return

    results = snapshot.GetResults(pid)
    if not results:
      return

    old_results = prev_snapshot.GetResults(pid) if prev_snapshot else None

    # Build Delta List
    deltas = [ 0, 0, 0, 0 ]
    if old_results:
      deltas = map(sub, results, old_results)
      assert len(deltas) == len(results)

    output = ''
    for idx, mem in enumerate(results):
      output += self.__PadString(mem, 8, True) + ' '
      output += self.__PadAndColor('(' + str(round(deltas[idx], 2)) + ')',
          8, False, self.__GetDiffColor(deltas[idx]))

    return output

  def __PrintLabeledSnapshotStats(self, pid, snapshot, prev_snapshot):
    """Returns a string that contains memory usage stats along with the pid and
    process name.  This string should line up with the header returned by
    |self.__PrintLabeledStatsHeader|."""
    if not snapshot or not snapshot.HasResults():
      return

    name_length = max(8, snapshot.GetLongestNameLength())
    name = snapshot.GetNameForPid(pid)

    output = self.__PadAndColor(pid, 8, True, 'DARK_YELLOW') + ' '
    output += self.__PadAndColor(name, name_length, False, None) + ' '
    output += self.__PrintBasicSnapshotStats(pid, snapshot, prev_snapshot)
    return output

  def __PrintTimestampedBasicSnapshotStats(self, pid, snapshot, prev_snapshot):
    """Returns a string that contains memory usage stats along with the
    timestamp of the snapshot.  This string should line up with the header
    returned by |self.__PrintTimestampedBasicStatsHeader|."""
    if not snapshot or not snapshot.HasResults():
      return

    timestamp_length = max(8, len("Timestamp"))
    timestamp = round(snapshot.GetTimestamp(), 2)

    output = self.__PadString(str(timestamp), timestamp_length, True) + ' '
    output += self.__PrintBasicSnapshotStats(pid, snapshot, prev_snapshot)
    return output

  def PrettyPrint(self, snapshot, prev_snapshot):
    """Prints |snapshot| to the console.  This will show memory deltas between
    |snapshot| and |prev_snapshot|.  This will also either color or overwrite
    the previous entries based on |self.can_color| and |self.overwrite|."""
    self.__ClearScreen()

    if not snapshot or not snapshot.HasResults():
      self.__OutputLine("No results...")
      return

    self.__OutputLine(self.__PrintLabeledStatsHeader(snapshot))

    for (pid, name) in snapshot.GetPidAndNames():
      self.__OutputLine(self.__PrintLabeledSnapshotStats(pid,
                                                         snapshot,
                                                         prev_snapshot))

  def PrettyFile(self, file_path, snapshots, diff_against_start):
    """Writes |snapshots| (a list of MemorySnapshots) to |file_path|.
    |diff_against_start| determines whether or not the snapshot deltas are
    between the first entry and all entries or each previous entry.  This output
    will not follow |self.can_color| or |self.overwrite|."""
    if not file_path or not snapshots:
      return

    pids = self.__FindPidsForSnapshotList(snapshots)

    # Disable special output formatting for file writing.
    can_color = self.can_color
    self.can_color = False

    with open(file_path, 'w') as out:
      for (pid, name) in pids:
        out.write(name + ' (' + str(pid) + '):\n')
        out.write(self.__PrintTimestampedBasicStatsHeader())
        out.write('\n')

        prev_snapshot = None
        for snapshot in snapshots:
          if not snapshot.GetResults(pid):
            continue
          out.write(self.__PrintTimestampedBasicSnapshotStats(pid,
                                                              snapshot,
                                                              prev_snapshot))
          out.write('\n')
          if not prev_snapshot or not diff_against_start:
            prev_snapshot = snapshot
        out.write('\n\n')

    # Restore special output formatting.
    self.can_color = can_color

  def PrettyGraph(self, file_path, snapshots):
    """Creates a pdf graph of |snapshots| (a list of MemorySnapshots) at
    |file_path|."""
    # Import these here so the rest of the functionality doesn't rely on
    # matplotlib
    from matplotlib import pyplot
    from matplotlib.backends.backend_pdf import PdfPages

    if not file_path or not snapshots:
      return

    pids = self.__FindPidsForSnapshotList(snapshots)

    pp = PdfPages(file_path)
    for (pid, name) in pids:
      figure = pyplot.figure()
      ax = figure.add_subplot(1, 1, 1)
      ax.set_xlabel('Time (s)')
      ax.set_ylabel('MB')
      ax.set_title(name + ' (' + pid + ')')

      mem_list = [[] for x in range(len(self.__MEMORY_COLUMN_TITLES))]
      timestamps = []

      for snapshot in snapshots:
        results = snapshot.GetResults(pid)
        if not results:
          continue

        timestamps.append(round(snapshot.GetTimestamp(), 2))

        assert len(results) == len(self.__MEMORY_COLUMN_TITLES)
        for idx, result in enumerate(results):
          mem_list[idx].append(result)

      colors = []
      for data in mem_list:
        colors.append(ax.plot(timestamps, data)[0])
        for i in xrange(len(timestamps)):
          ax.annotate(data[i], xy=(timestamps[i], data[i]))
      figure.legend(colors, self.__MEMORY_COLUMN_TITLES)
      pp.savefig()
    pp.close()

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--process',
                      dest='procname',
                      help="A (sub)string to match against process names.")
  parser.add_argument('-p',
                      '--pid',
                      dest='pid',
                      type=Validator.ValidateNonNegativeNumber,
                      help='Which pid to scan for.')
  parser.add_argument('-d',
                      '--device',
                      dest='device',
                      help='Device serial to scan.')
  parser.add_argument('-t',
                      '--timelimit',
                      dest='timelimit',
                      type=Validator.ValidateNonNegativeNumber,
                      help='How long to track memory in seconds.')
  parser.add_argument('-f',
                      '--frequency',
                      dest='frequency',
                      default=0,
                      type=Validator.ValidateNonNegativeNumber,
                      help='How often to poll in seconds.')
  parser.add_argument('-s',
                      '--diff-against-start',
                      dest='diff_against_start',
                      action='store_true',
                      help='Whether or not to always compare against the'
                           ' original memory values for deltas.')
  parser.add_argument('-b',
                      '--boring-output',
                      dest='dull_output',
                      action='store_true',
                      help='Whether or not to dull down the output.')
  parser.add_argument('-n',
                      '--no-overwrite',
                      dest='no_overwrite',
                      action='store_true',
                      help='Keeps printing the results in a list instead of'
                           ' overwriting the previous values.')
  parser.add_argument('-g',
                      '--graph-file',
                      dest='graph_file',
                      type=Validator.ValidatePdfPath,
                      help='PDF file to save graph of memory stats to.')
  parser.add_argument('-o',
                      '--text-file',
                      dest='text_file',
                      type=Validator.ValidatePath,
                      help='File to save memory tracking stats to.')

  args = parser.parse_args()

  # Add a basic filter to make sure we search for something.
  if not args.procname and not args.pid:
    args.procname = 'chrome'

  curses.setupterm()

  printer = OutputBeautifier(not args.dull_output, not args.no_overwrite)

  sys.stdout.write("Running... Hold CTRL-C to stop (or specify timeout).\n")
  try:
    last_time = time.time()

    adb = None
    old_snapshot = None
    snapshots = []
    while not args.timelimit or Timer.GetTimestamp() < float(args.timelimit):
      # Check if we need to track another device
      device = DeviceHelper.GetDeviceToTrack(args.device)
      if not device:
        adb = None
      elif not adb or device != str(adb):
        adb = adb_wrapper.AdbWrapper(device)
        old_snapshot = None
        snapshots = []
        try:
          adb.Root()
        except device_errors.AdbCommandFailedError:
          sys.stderr.write('Unable to run adb as root.\n')
          sys.exit(1)

      # Grab a snapshot if we have a device
      snapshot = None
      if adb:
        pids = DeviceHelper.GetPidsToTrack(adb, args.pid, args.procname)
        snapshot = MemorySnapshot(adb, pids) if pids else None

      if snapshot and snapshot.HasResults():
        snapshots.append(snapshot)

      printer.PrettyPrint(snapshot, old_snapshot)

      # Transfer state for the next iteration and sleep
      delay = max(1, args.frequency)
      if snapshot:
        delay = max(0, args.frequency - (time.time() - last_time))
      time.sleep(delay)

      last_time = time.time()
      if not old_snapshot or not args.diff_against_start:
        old_snapshot = snapshot
  except KeyboardInterrupt:
    pass

  if args.graph_file:
    printer.PrettyGraph(args.graph_file, snapshots)

  if args.text_file:
    printer.PrettyFile(args.text_file, snapshots, args.diff_against_start)

if __name__ == '__main__':
  sys.exit(main(sys.argv))

