# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Android-specific implementation of the core backend interfaces.

See core/backends.py for more docs.
"""

import datetime
import hashlib
import json
import os

from memory_inspector import constants
from memory_inspector.backends import prebuilts_fetcher
from memory_inspector.backends.android import dumpheap_native_parser
from memory_inspector.backends.android import memdump_parser
from memory_inspector.core import backends

# The embedder of this module (unittest runner, web server, ...) is expected
# to add the <CHROME_SRC>/build/android to the PYTHONPATH for pylib.
from pylib import android_commands  # pylint: disable=F0401


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

  def __init__(self, settings=None):
    super(AndroidBackend, self).__init__(
        settings=backends.Settings(AndroidBackend._SETTINGS_KEYS))

  def EnumerateDevices(self):
    # If a custom adb_path has been setup through settings, prepend that to the
    # PATH. The android_commands module will use that to locate adb.
    if (self.settings['adb_path'] and
        not os.environ['PATH'].startswith(self.settings['adb_path'])):
      os.environ['PATH'] = os.pathsep.join([self.settings['adb_path'],
                                           os.environ['PATH']])
    for device in android_commands.GetAttachedDevices():
      yield AndroidDevice(self, android_commands.AndroidCommands(device))

  @property
  def name(self):
    return 'Android'


class AndroidDevice(backends.Device):
  """Android-specific implementation of the core |Device| interface."""

  _SETTINGS_KEYS = {
      'native_symbol_paths': 'Comma-separated list of native libs search path'}

  def __init__(self, backend, adb):
    super(AndroidDevice, self).__init__(
        backend=backend,
        settings=backends.Settings(AndroidDevice._SETTINGS_KEYS))
    self.adb = adb
    self._id = adb.GetDevice()
    self._name = adb.GetProductModel()
    self._sys_stats = None
    self._sys_stats_last_update = None

  def Initialize(self):
    """Starts adb root and deploys the prebuilt binaries on initialization."""
    self.adb.EnableAdbRoot()

    # Download (from GCS) and deploy prebuilt helper binaries on the device.
    self._DeployPrebuiltOnDeviceIfNeeded(_MEMDUMP_PREBUILT_PATH,
                                         _MEMDUMP_PATH_ON_DEVICE)
    self._DeployPrebuiltOnDeviceIfNeeded(_PSEXT_PREBUILT_PATH,
                                         _PSEXT_PATH_ON_DEVICE)

  def EnableMmapTracing(self, enabled):
    """Nothing to do here. memdump is already deployed in Initialize()."""
    pass

  def IsMmapTracingEnabled(self):
    return True

  def IsNativeAllocTracingEnabled(self):
    """Checks for the libc.debug.malloc system property."""
    return self.adb.system_properties[_DLMALLOC_DEBUG_SYSPROP]

  def EnableNativeAllocTracing(self, enabled):
    """Enables libc.debug.malloc and restarts the shell."""
    prop_value = '1' if enabled else ''
    self.adb.system_properties[_DLMALLOC_DEBUG_SYSPROP] = prop_value
    assert(self.IsNativeAllocTracingEnabled())
    # The libc.debug property takes effect only after restarting the Zygote.
    self.adb.RestartShell()

  def ListProcesses(self):
    """Returns a sequence of |AndroidProcess|."""
    sys_stats = self.UpdateAndGetSystemStats()
    for pid, proc in sys_stats['processes'].iteritems():
      yield AndroidProcess(self, int(pid), proc['name'])

  def GetProcess(self, pid):
    """Returns an instance of |AndroidProcess| (None if not found)."""
    assert(isinstance(pid, int))
    sys_stats = self.UpdateAndGetSystemStats()
    proc = sys_stats['processes'].get(str(pid))
    if not proc:
      return None
    return AndroidProcess(self, pid, proc['name'])

  def GetStats(self):
    """Returns an instance of |DeviceStats| with the OS CPU/Memory stats."""
    old = self._sys_stats
    cur = self.UpdateAndGetSystemStats()
    old = old or cur  # Handle 1st call case.
    uptime = cur['time']['ticks'] / cur['time']['rate']
    ticks = max(1, cur['time']['ticks'] - old['time']['ticks'])

    cpu_times = []
    for i in xrange(len(cur['cpu'])):
      cpu_time = {
          'usr': 100 * (cur['cpu'][i]['usr'] - old['cpu'][i]['usr']) / ticks,
          'sys': 100 * (cur['cpu'][i]['sys'] - old['cpu'][i]['sys']) / ticks,
          'idle': 100 * (cur['cpu'][i]['idle'] - old['cpu'][i]['idle']) / ticks}
      # The idle tick count on many Linux kernels stays frozen when the CPU is
      # offline, and bumps up (compensating all the offline period) when it
      # reactivates. For this reason it needs to be saturated at 100.
      cpu_time['idle'] = min(cpu_time['idle'],
                             100 - cpu_time['usr'] - cpu_time['sys'])
      cpu_times.append(cpu_time)
    return backends.DeviceStats(uptime=uptime,
                               cpu_times=cpu_times,
                               memory_stats=cur['mem'])

  def UpdateAndGetSystemStats(self):
    """Grabs and caches system stats through ps_ext (max cache TTL = 1s).

    Rationale of caching: avoid invoking adb too often, it is slow.
    """
    max_ttl = datetime.timedelta(seconds=1)
    if (self._sys_stats_last_update and
        datetime.datetime.now() - self._sys_stats_last_update <= max_ttl):
      return self._sys_stats

    dump_out = '\n'.join(self.adb.RunShellCommand(_PSEXT_PATH_ON_DEVICE))
    stats = json.loads(dump_out)
    assert(all([x in stats for x in ['cpu', 'processes', 'time', 'mem']])), (
        'ps_ext returned a malformed JSON dictionary.')
    self._sys_stats = stats
    self._sys_stats_last_update = datetime.datetime.now()
    return self._sys_stats

  def _DeployPrebuiltOnDeviceIfNeeded(self, local_path, path_on_device):
    # TODO(primiano): check that the md5 binary is built-in also on pre-KK.
    # Alternatively add tools/android/md5sum to prebuilts and use that one.
    prebuilts_fetcher.GetIfChanged(local_path)
    with open(local_path, 'rb') as f:
      local_hash = hashlib.md5(f.read()).hexdigest()
    device_md5_out = self.adb.RunShellCommand('md5 "%s"' % path_on_device)
    if local_hash in device_md5_out:
      return
    self.adb.Adb().Push(local_path, path_on_device)
    self.adb.RunShellCommand('chmod 755 "%s"' % path_on_device)

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
    dump_out = self.device.adb.RunShellCommand(cmd)
    return memdump_parser.Parse(dump_out)

  def DumpNativeHeap(self):
    """Grabs and parses malloc traces through am dumpheap -n."""
    # TODO(primiano): grab also mmap bt (depends on pending framework change).
    dump_file_path = _DUMPHEAP_OUT_FILE_PATH % self.pid
    cmd = 'am dumpheap -n %d %s' % (self.pid, dump_file_path)
    self.device.adb.RunShellCommand(cmd)
    # TODO(primiano): Some pre-KK versions of Android might need a sleep here
    # as, IIRC, 'am dumpheap' did not wait for the dump to be completed before
    # returning. Double check this and either add a sleep or remove this TODO.
    dump_out = self.device.adb.GetFileContents(dump_file_path)
    self.device.adb.RunShellCommand('rm %s' % dump_file_path)
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
        page_faults=cur_proc_stats['maj_faults'] + cur_proc_stats['min_faults'])
    self._last_sys_stats = cur_sys_stats
    return proc_stats
