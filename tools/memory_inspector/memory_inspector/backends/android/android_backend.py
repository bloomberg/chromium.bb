# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Android-specific implementation of the core backend interfaces.

See core/backends.py for more docs.
"""

import datetime
import glob
import hashlib
import json
import logging
import os
import posixpath

from memory_inspector import constants
from memory_inspector.backends import prebuilts_fetcher
from memory_inspector.backends.android import dumpheap_native_parser
from memory_inspector.backends.android import memdump_parser
from memory_inspector.core import backends
from memory_inspector.core import exceptions
from memory_inspector.core import native_heap
from memory_inspector.core import symbol

# The memory_inspector/__init__ module will add the <CHROME_SRC>/build/android
# deps to the PYTHONPATH for pylib.
from pylib import android_commands
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.symbols import elf_symbolizer


_MEMDUMP_PREBUILT_PATH = os.path.join(constants.PROJECT_SRC,
                                      'prebuilts', 'memdump-android-arm')
_MEMDUMP_PATH_ON_DEVICE = '/data/local/tmp/memdump'
_PSEXT_PREBUILT_PATH = os.path.join(constants.PROJECT_SRC,
                                    'prebuilts', 'ps_ext-android-arm')
_PSEXT_PATH_ON_DEVICE = '/data/local/tmp/ps_ext'
_DLMALLOC_DEBUG_SYSPROP = 'libc.debug.malloc'
_DUMPHEAP_OUT_FILE_PATH = '/data/local/tmp/heap-%d-native.dump'


class AndroidBackend(backends.Backend):
  """Android-specific implementation of the core |Backend| interface."""

  _SETTINGS_KEYS = {
      'adb_path': 'Path of directory containing the adb binary',
      'toolchain_path': 'Path of toolchain (for addr2line)'}

  def __init__(self):
    super(AndroidBackend, self).__init__(
        settings=backends.Settings(AndroidBackend._SETTINGS_KEYS))
    self._devices = {}  # 'device id' -> |Device|.

  def EnumerateDevices(self):
    # If a custom adb_path has been setup through settings, prepend that to the
    # PATH. The android_commands module will use that to locate adb.
    if (self.settings['adb_path'] and
        not os.environ['PATH'].startswith(self.settings['adb_path'])):
      os.environ['PATH'] = os.pathsep.join([self.settings['adb_path'],
                                           os.environ['PATH']])
    for device_id in android_commands.GetAttachedDevices():
      device = self._devices.get(device_id)
      if not device:
        device = AndroidDevice(
          self, device_utils.DeviceUtils(device_id))
        self._devices[device_id] = device
      yield device

  def ExtractSymbols(self, native_heaps, sym_paths):
    """Performs symbolization. Returns a |symbol.Symbols| from |NativeHeap|s.

    This method performs the symbolization but does NOT decorate (i.e. add
    symbol/source info) to the stack frames of |native_heaps|. The heaps
    can be decorated as needed using the native_heap.SymbolizeUsingSymbolDB()
    method. Rationale: the most common use case in this application is:
    symbolize-and-store-symbols and load-symbols-and-decorate-heaps (in two
    different stages at two different times).

    Args:
      native_heaps: a collection of native_heap.NativeHeap instances.
      sym_paths: either a list of or a string of semicolon-sep. symbol paths.
    """
    assert(all(isinstance(x, native_heap.NativeHeap) for x in native_heaps))
    symbols = symbol.Symbols()

    # Find addr2line in toolchain_path.
    if isinstance(sym_paths, basestring):
      sym_paths = sym_paths.split(';')
    matches = glob.glob(os.path.join(self.settings['toolchain_path'],
                                     '*addr2line'))
    if not matches:
      raise exceptions.MemoryInspectorException('Cannot find addr2line')
    addr2line_path = matches[0]

    # First group all the stack frames together by lib path.
    frames_by_lib = {}
    for nheap in native_heaps:
      for stack_frame in nheap.stack_frames.itervalues():
        frames = frames_by_lib.setdefault(stack_frame.exec_file_rel_path, set())
        frames.add(stack_frame)

    # The symbolization process is asynchronous (but yet single-threaded). This
    # callback is invoked every time the symbol info for a stack frame is ready.
    def SymbolizeAsyncCallback(sym_info, stack_frame):
      if not sym_info.name:
        return
      sym = symbol.Symbol(name=sym_info.name,
                          source_file_path=sym_info.source_path,
                          line_number=sym_info.source_line)
      symbols.Add(stack_frame.exec_file_rel_path, stack_frame.offset, sym)
      # TODO(primiano): support inline sym info (i.e. |sym_info.inlined_by|).

    # Perform the actual symbolization (ordered by lib).
    for exec_file_rel_path, frames in frames_by_lib.iteritems():
      # Look up the full path of the symbol in the sym paths.
      exec_file_name = posixpath.basename(exec_file_rel_path)
      if exec_file_rel_path.startswith('/'):
        exec_file_rel_path = exec_file_rel_path[1:]
      exec_file_abs_path = ''
      for sym_path in sym_paths:
        # First try to locate the symbol file following the full relative path
        # e.g. /host/syms/ + /system/lib/foo.so => /host/syms/system/lib/foo.so.
        exec_file_abs_path = os.path.join(sym_path, exec_file_rel_path)
        if os.path.exists(exec_file_abs_path):
          break

        # If no luck, try looking just for the file name in the sym path,
        # e.g. /host/syms/ + (/system/lib/)foo.so => /host/syms/foo.so
        exec_file_abs_path = os.path.join(sym_path, exec_file_name)
        if os.path.exists(exec_file_abs_path):
          break

      if not os.path.exists(exec_file_abs_path):
        continue

      symbolizer = elf_symbolizer.ELFSymbolizer(
          elf_file_path=exec_file_abs_path,
          addr2line_path=addr2line_path,
          callback=SymbolizeAsyncCallback,
          inlines=False)

      # Kick off the symbolizer and then wait that all callbacks are issued.
      for stack_frame in sorted(frames, key=lambda x: x.offset):
        symbolizer.SymbolizeAsync(stack_frame.offset, stack_frame)
      symbolizer.Join()

    return symbols

  @property
  def name(self):
    return 'Android'


class AndroidDevice(backends.Device):
  """Android-specific implementation of the core |Device| interface."""

  _SETTINGS_KEYS = {
      'native_symbol_paths': 'Semicolon-sep. list of native libs search path'}

  def __init__(self, backend, underlying_device):
    super(AndroidDevice, self).__init__(
        backend=backend,
        settings=backends.Settings(AndroidDevice._SETTINGS_KEYS))
    self.underlying_device = underlying_device
    self._id = str(underlying_device)
    self._name = underlying_device.GetProp('ro.product.model')
    self._sys_stats = None
    self._last_device_stats = None
    self._sys_stats_last_update = None
    self._processes = {}  # pid (int) -> |Process|
    self._initialized = False

  def Initialize(self):
    """Starts adb root and deploys the prebuilt binaries on initialization."""
    try:
      self.underlying_device.EnableRoot()
    except device_errors.CommandFailedError as e:
      # Try to deploy memdump and ps_ext anyway.
      # TODO(jbudorick) Handle this exception appropriately after interface
      #                 conversions are finished.
      logging.error(str(e))

    # Download (from GCS) and deploy prebuilt helper binaries on the device.
    self._DeployPrebuiltOnDeviceIfNeeded(_MEMDUMP_PREBUILT_PATH,
                                         _MEMDUMP_PATH_ON_DEVICE)
    self._DeployPrebuiltOnDeviceIfNeeded(_PSEXT_PREBUILT_PATH,
                                         _PSEXT_PATH_ON_DEVICE)
    self._initialized = True

  def IsNativeTracingEnabled(self):
    """Checks for the libc.debug.malloc system property."""
    return bool(self.underlying_device.GetProp(
        _DLMALLOC_DEBUG_SYSPROP))

  def EnableNativeTracing(self, enabled):
    """Enables libc.debug.malloc and restarts the shell."""
    assert(self._initialized)
    prop_value = '1' if enabled else ''
    self.underlying_device.SetProp(_DLMALLOC_DEBUG_SYSPROP, prop_value)
    assert(self.IsNativeTracingEnabled())
    # The libc.debug property takes effect only after restarting the Zygote.
    self.underlying_device.old_interface.RestartShell()

  def ListProcesses(self):
    """Returns a sequence of |AndroidProcess|."""
    self._RefreshProcessesList()
    return self._processes.itervalues()

  def GetProcess(self, pid):
    """Returns an instance of |AndroidProcess| (None if not found)."""
    assert(isinstance(pid, int))
    self._RefreshProcessesList()
    return self._processes.get(pid)

  def GetStats(self):
    """Returns an instance of |DeviceStats| with the OS CPU/Memory stats."""
    cur = self.UpdateAndGetSystemStats()
    old = self._last_device_stats or cur  # Handle 1st call case.
    uptime = cur['time']['ticks'] / cur['time']['rate']
    ticks = max(1, cur['time']['ticks'] - old['time']['ticks'])

    cpu_times = []
    for i in xrange(len(cur['cpu'])):
      cpu_time = {
          'usr': 100 * (cur['cpu'][i]['usr'] - old['cpu'][i]['usr']) / ticks,
          'sys': 100 * (cur['cpu'][i]['sys'] - old['cpu'][i]['sys']) / ticks,
          'idle': 100 * (cur['cpu'][i]['idle'] - old['cpu'][i]['idle']) / ticks}
      # The idle tick count on many Linux kernels is frozen when the CPU is
      # offline, and bumps up (compensating all the offline period) when it
      # reactivates. For this reason it needs to be saturated at [0, 100].
      cpu_time['idle'] = max(0, min(cpu_time['idle'],
                                    100 - cpu_time['usr'] - cpu_time['sys']))

      cpu_times.append(cpu_time)

    memory_stats = {'Free': cur['mem']['MemFree:'],
                    'Cache': cur['mem']['Buffers:'] + cur['mem']['Cached:'],
                    'Swap': cur['mem']['SwapCached:'],
                    'Anonymous': cur['mem']['AnonPages:'],
                    'Kernel': cur['mem']['VmallocUsed:']}
    self._last_device_stats = cur

    return backends.DeviceStats(uptime=uptime,
                                cpu_times=cpu_times,
                                memory_stats=memory_stats)

  def UpdateAndGetSystemStats(self):
    """Grabs and caches system stats through ps_ext (max cache TTL = 0.5s).

    Rationale of caching: avoid invoking adb too often, it is slow.
    """
    assert(self._initialized)
    max_ttl = datetime.timedelta(seconds=0.5)
    if (self._sys_stats_last_update and
        datetime.datetime.now() - self._sys_stats_last_update <= max_ttl):
      return self._sys_stats

    dump_out = '\n'.join(
        self.underlying_device.RunShellCommand(_PSEXT_PATH_ON_DEVICE))
    stats = json.loads(dump_out)
    assert(all([x in stats for x in ['cpu', 'processes', 'time', 'mem']])), (
        'ps_ext returned a malformed JSON dictionary.')
    self._sys_stats = stats
    self._sys_stats_last_update = datetime.datetime.now()
    return self._sys_stats

  def _RefreshProcessesList(self):
    sys_stats = self.UpdateAndGetSystemStats()
    processes_to_delete = set(self._processes.keys())
    for pid, proc in sys_stats['processes'].iteritems():
      pid = int(pid)
      process = self._processes.get(pid)
      if not process or process.name != proc['name']:
        process = AndroidProcess(self, int(pid), proc['name'])
        self._processes[pid] = process
      processes_to_delete.discard(pid)
    for pid in processes_to_delete:
      del self._processes[pid]

  def _DeployPrebuiltOnDeviceIfNeeded(self, local_path, path_on_device):
    # TODO(primiano): check that the md5 binary is built-in also on pre-KK.
    # Alternatively add tools/android/md5sum to prebuilts and use that one.
    prebuilts_fetcher.GetIfChanged(local_path)
    with open(local_path, 'rb') as f:
      local_hash = hashlib.md5(f.read()).hexdigest()
    device_md5_out = self.underlying_device.RunShellCommand(
        'md5 "%s"' % path_on_device)
    if local_hash in device_md5_out:
      return
    self.underlying_device.old_interface.Adb().Push(local_path, path_on_device)
    self.underlying_device.RunShellCommand('chmod 755 "%s"' % path_on_device)

  @property
  def name(self):
    """Device name, as defined in the |backends.Device| interface."""
    return self._name

  @property
  def id(self):
    """Device id, as defined in the |backends.Device| interface."""
    return self._id


class AndroidProcess(backends.Process):
  """Android-specific implementation of the core |Process| interface."""

  def __init__(self, device, pid, name):
    super(AndroidProcess, self).__init__(device, pid, name)
    self._last_sys_stats = None

  def DumpMemoryMaps(self):
    """Grabs and parses memory maps through memdump."""
    cmd = '%s %d' % (_MEMDUMP_PATH_ON_DEVICE, self.pid)
    dump_out = self.device.underlying_device.RunShellCommand(cmd)
    return memdump_parser.Parse(dump_out)

  def DumpNativeHeap(self):
    """Grabs and parses malloc traces through am dumpheap -n."""
    # TODO(primiano): grab also mmap bt (depends on pending framework change).
    dump_file_path = _DUMPHEAP_OUT_FILE_PATH % self.pid
    cmd = 'am dumpheap -n %d %s' % (self.pid, dump_file_path)
    self.device.underlying_device.RunShellCommand(cmd)
    # TODO(primiano): Some pre-KK versions of Android might need a sleep here
    # as, IIRC, 'am dumpheap' did not wait for the dump to be completed before
    # returning. Double check this and either add a sleep or remove this TODO.
    dump_out = self.device.underlying_device.ReadFile(dump_file_path)
    self.device.underlying_device.RunShellCommand('rm %s' % dump_file_path)
    return dumpheap_native_parser.Parse(dump_out)

  def GetStats(self):
    """Calculate process CPU/VM stats (CPU stats are relative to last call)."""
    # Process must retain its own copy of _last_sys_stats because CPU times
    # are calculated relatively to the last GetStats() call (for the process).
    cur_sys_stats = self.device.UpdateAndGetSystemStats()
    old_sys_stats = self._last_sys_stats or cur_sys_stats
    cur_proc_stats = cur_sys_stats['processes'].get(str(self.pid))
    old_proc_stats = old_sys_stats['processes'].get(str(self.pid))

    # The process might have gone in the meanwhile.
    if (not cur_proc_stats or not old_proc_stats):
      return None

    run_time = (((cur_sys_stats['time']['ticks'] -
                cur_proc_stats['start_time']) / cur_sys_stats['time']['rate']))
    ticks = max(1, cur_sys_stats['time']['ticks'] -
                old_sys_stats['time']['ticks'])
    cpu_usage = (100 *
                 ((cur_proc_stats['user_time'] + cur_proc_stats['sys_time']) -
                 (old_proc_stats['user_time'] + old_proc_stats['sys_time'])) /
                 ticks) / len(cur_sys_stats['cpu'])
    proc_stats = backends.ProcessStats(
        threads=cur_proc_stats['n_threads'],
        run_time=run_time,
        cpu_usage=cpu_usage,
        vm_rss=cur_proc_stats['vm_rss'],
        page_faults=(
            (cur_proc_stats['maj_faults'] + cur_proc_stats['min_faults']) -
            (old_proc_stats['maj_faults'] + old_proc_stats['min_faults'])))
    self._last_sys_stats = cur_sys_stats
    return proc_stats
