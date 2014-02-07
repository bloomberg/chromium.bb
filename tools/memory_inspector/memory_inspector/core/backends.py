# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_backends = {}  # Maps a string (backend name) to a |Backend| instance.


def Register(backend):
  """Called by each backend module to register upon initialization."""
  assert(isinstance(backend, Backend))
  _backends[backend.name] = backend


def ListDevices():
  """Enumerates all the devices from all the registered backends."""
  for backend in _backends.itervalues():
    for device in backend.EnumerateDevices():
      assert(isinstance(device, Device))
      yield device


def GetDevice(backend_name, device_id):
  """Retrieves a specific device given its backend name and device id."""
  for backend in _backends.itervalues():
    if backend.name != backend_name:
      continue
    for device in backend.EnumerateDevices():
      if device.id != device_id:
        continue
      return device
  return None


# The classes below model the contract interfaces exposed to the frontends and
# implemented by each concrete backend.

class Backend(object):
  """Base class for backends.

    This is the extension point for the OS-specific profiler implementations.
  """

  def __init__(self, settings=None):
    # Initialize with empty settings if not required by the overriding backend.
    self.settings = settings or Settings()

  def EnumerateDevices(self):
    """Enumeates the devices discovered and supported by the backend.

    Returns:
        A sequence of |Device| instances.
    """
    raise NotImplementedError()

  @property
  def name(self):
    """A unique name which identifies the backend.

    Typically this will just return the target OS name, e.g., 'Android'."""
    raise NotImplementedError()


class Device(object):
  """Interface contract for devices enumerated by a backend."""

  def __init__(self, backend, settings=None):
    self.backend = backend
    # Initialize with empty settings if not required by the overriding device.
    self.settings = settings or Settings()

  def IsNativeAllocTracingEnabled(self):
    """Check if the device is ready to capture native allocation traces."""
    raise NotImplementedError()

  def EnableNativeAllocTracing(self, enabled):
    """Provision the device and make it ready to trace native allocations."""
    raise NotImplementedError()

  def IsMmapTracingEnabled(self):
    """Check if the device is ready to capture memory map traces."""
    raise NotImplementedError()

  def EnableMmapTracing(self, enabled):
    """Provision the device and make it ready to trace memory maps."""
    raise NotImplementedError()

  def ListProcesses(self):
    """Returns a sequence of |Process|."""
    raise NotImplementedError()

  def GetProcess(self, pid):
    """Returns an instance of |Process| or None (if not found)."""
    raise NotImplementedError()

  @property
  def name(self):
    """Friendly name of the target device (e.g., phone model)."""
    raise NotImplementedError()

  @property
  def id(self):
    """Unique identifier (within the backend) of the device (e.g., S/N)."""
    raise NotImplementedError()


class Process(object):
  """Interface contract for each running process."""

  def __init__(self, device, pid, name):
    assert(isinstance(device, Device))
    assert(isinstance(pid, int))
    self.device = device
    self.pid = pid
    self.name = name

  def DumpMemoryMaps(self):
    """Returns an instance of |memory_map.Map|."""
    raise NotImplementedError()

  def DumpNativeHeap(self):
    """Returns an instance of |native_heap.NativeHeap|."""
    raise NotImplementedError()

  def __str__(self):
    return '[%d] %s' % (self.pid, self.name)


class Settings(object):
  """Models user-definable settings for backends and devices."""

  def __init__(self, expected_keys=None):
    """
    Args:
      expected_keys: A dict. (key-name -> description) of expected settings
    """
    self.expected_keys = expected_keys or {}
    self._settings = dict((k, '') for k in self.expected_keys.iterkeys())

  def __getitem__(self, key):
    assert(key in self.expected_keys)
    return self._settings.get(key)

  def __setitem__(self, key, value):
    assert(key in self.expected_keys)
    self._settings[key] = value
