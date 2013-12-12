# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Provides functions: get_native_path_case(), isabs() and safe_join()."""

import logging
import os
import posixpath
import re
import sys
import unicodedata

## OS-specific imports

if sys.platform == 'win32':
  from ctypes.wintypes import create_unicode_buffer
  from ctypes.wintypes import windll, FormatError  # pylint: disable=E0611
  from ctypes.wintypes import GetLastError  # pylint: disable=E0611
elif sys.platform == 'darwin':
  import Carbon.File  #  pylint: disable=F0401
  import MacOS  # pylint: disable=F0401


if sys.platform == 'win32':
  def QueryDosDevice(drive_letter):
    """Returns the Windows 'native' path for a DOS drive letter."""
    assert re.match(r'^[a-zA-Z]:$', drive_letter), drive_letter
    assert isinstance(drive_letter, unicode)
    # Guesswork. QueryDosDeviceW never returns the required number of bytes.
    chars = 1024
    drive_letter = drive_letter
    p = create_unicode_buffer(chars)
    if 0 == windll.kernel32.QueryDosDeviceW(drive_letter, p, chars):
      err = GetLastError()
      if err:
        # pylint: disable=E0602
        msg = u'QueryDosDevice(%s): %s (%d)' % (
              drive_letter, FormatError(err), err)
        raise WindowsError(err, msg.encode('utf-8'))
    return p.value


  def GetShortPathName(long_path):
    """Returns the Windows short path equivalent for a 'long' path."""
    assert isinstance(long_path, unicode), repr(long_path)
    # Adds '\\\\?\\' when given an absolute path so the MAX_PATH (260) limit is
    # not enforced.
    if os.path.isabs(long_path) and not long_path.startswith('\\\\?\\'):
      long_path = '\\\\?\\' + long_path
    chars = windll.kernel32.GetShortPathNameW(long_path, None, 0)
    if chars:
      p = create_unicode_buffer(chars)
      if windll.kernel32.GetShortPathNameW(long_path, p, chars):
        return p.value

    err = GetLastError()
    if err:
      # pylint: disable=E0602
      msg = u'GetShortPathName(%s): %s (%d)' % (
            long_path, FormatError(err), err)
      raise WindowsError(err, msg.encode('utf-8'))


  def GetLongPathName(short_path):
    """Returns the Windows long path equivalent for a 'short' path."""
    assert isinstance(short_path, unicode)
    # Adds '\\\\?\\' when given an absolute path so the MAX_PATH (260) limit is
    # not enforced.
    if os.path.isabs(short_path) and not short_path.startswith('\\\\?\\'):
      short_path = '\\\\?\\' + short_path
    chars = windll.kernel32.GetLongPathNameW(short_path, None, 0)
    if chars:
      p = create_unicode_buffer(chars)
      if windll.kernel32.GetLongPathNameW(short_path, p, chars):
        return p.value

    err = GetLastError()
    if err:
      # pylint: disable=E0602
      msg = u'GetLongPathName(%s): %s (%d)' % (
            short_path, FormatError(err), err)
      raise WindowsError(err, msg.encode('utf-8'))


  class DosDriveMap(object):
    """Maps \Device\HarddiskVolumeN to N: on Windows."""
    # Keep one global cache.
    _MAPPING = {}

    def __init__(self):
      """Lazy loads the cache."""
      if not self._MAPPING:
        # This is related to UNC resolver on windows. Ignore that.
        self._MAPPING[u'\\Device\\Mup'] = None
        self._MAPPING[u'\\SystemRoot'] = os.environ[u'SystemRoot']

        for letter in (chr(l) for l in xrange(ord('C'), ord('Z')+1)):
          try:
            letter = u'%s:' % letter
            mapped = QueryDosDevice(letter)
            if mapped in self._MAPPING:
              logging.warn(
                  ('Two drives: \'%s\' and \'%s\', are mapped to the same disk'
                   '. Drive letters are a user-mode concept and the kernel '
                   'traces only have NT path, so all accesses will be '
                   'associated with the first drive letter, independent of the '
                   'actual letter used by the code') % (
                     self._MAPPING[mapped], letter))
            else:
              self._MAPPING[mapped] = letter
          except WindowsError:  # pylint: disable=E0602
            pass

    def to_win32(self, path):
      """Converts a native NT path to Win32/DOS compatible path."""
      match = re.match(r'(^\\Device\\[a-zA-Z0-9]+)(\\.*)?$', path)
      if not match:
        raise ValueError(
            'Can\'t convert %s into a Win32 compatible path' % path,
            path)
      if not match.group(1) in self._MAPPING:
        # Unmapped partitions may be accessed by windows for the
        # fun of it while the test is running. Discard these.
        return None
      drive = self._MAPPING[match.group(1)]
      if not drive or not match.group(2):
        return drive
      return drive + match.group(2)


  def isabs(path):
    """Accepts X: as an absolute path, unlike python's os.path.isabs()."""
    return os.path.isabs(path) or len(path) == 2 and path[1] == ':'


  def find_item_native_case(root, item):
    """Gets the native path case of a single item based at root_path."""
    if item == '..':
      return item

    root = get_native_path_case(root)
    return os.path.basename(get_native_path_case(os.path.join(root, item)))


  def get_native_path_case(p):
    """Returns the native path case for an existing file.

    On Windows, removes any leading '\\?\'.
    """
    assert isinstance(p, unicode), repr(p)
    if not isabs(p):
      raise ValueError(
          'get_native_path_case(%r): Require an absolute path' % p, p)

    # Make sure it is normalized to os.path.sep. Do not do it here to keep the
    # function fast
    assert '/' not in p, p
    suffix = ''
    count = p.count(':')
    if count > 1:
      # This means it has an alternate-data stream. There could be 3 ':', since
      # it could be the $DATA datastream of an ADS. Split the whole ADS suffix
      # off and add it back afterward. There is no way to know the native path
      # case of an alternate data stream.
      items = p.split(':')
      p = ':'.join(items[0:2])
      suffix = ''.join(':' + i for i in items[2:])

    # TODO(maruel): Use os.path.normpath?
    if p.endswith('.\\'):
      p = p[:-2]

    # Windows used to have an option to turn on case sensitivity on non Win32
    # subsystem but that's out of scope here and isn't supported anymore.
    # Go figure why GetShortPathName() is needed.
    try:
      out = GetLongPathName(GetShortPathName(p))
    except OSError, e:
      if e.args[0] in (2, 3, 5):
        # The path does not exist. Try to recurse and reconstruct the path.
        base = os.path.dirname(p)
        rest = os.path.basename(p)
        return os.path.join(get_native_path_case(base), rest)
      raise
    if out.startswith('\\\\?\\'):
      out = out[4:]
    # Always upper case the first letter since GetLongPathName() will return the
    # drive letter in the case it was given.
    return out[0].upper() + out[1:] + suffix


elif sys.platform == 'darwin':


  # On non-windows, keep the stdlib behavior.
  isabs = os.path.isabs


  def _native_case(p):
    """Gets the native path case. Warning: this function resolves symlinks."""
    try:
      rel_ref, _ = Carbon.File.FSPathMakeRef(p.encode('utf-8'))
      # The OSX underlying code uses NFD but python strings are in NFC. This
      # will cause issues with os.listdir() for example. Since the dtrace log
      # *is* in NFC, normalize it here.
      out = unicodedata.normalize(
          'NFC', rel_ref.FSRefMakePath().decode('utf-8'))
      if p.endswith(os.path.sep) and not out.endswith(os.path.sep):
        return out + os.path.sep
      return out
    except MacOS.Error, e:
      if e.args[0] in (-43, -120):
        # The path does not exist. Try to recurse and reconstruct the path.
        # -43 means file not found.
        # -120 means directory not found.
        base = os.path.dirname(p)
        rest = os.path.basename(p)
        return os.path.join(_native_case(base), rest)
      raise OSError(
          e.args[0], 'Failed to get native path for %s' % p, p, e.args[1])


  def _split_at_symlink_native(base_path, rest):
    """Returns the native path for a symlink."""
    base, symlink, rest = split_at_symlink(base_path, rest)
    if symlink:
      if not base_path:
        base_path = base
      else:
        base_path = safe_join(base_path, base)
      symlink = find_item_native_case(base_path, symlink)
    return base, symlink, rest


  def find_item_native_case(root_path, item):
    """Gets the native path case of a single item based at root_path.

    There is no API to get the native path case of symlinks on OSX. So it
    needs to be done the slow way.
    """
    if item == '..':
      return item

    item = item.lower()
    for element in os.listdir(root_path):
      if element.lower() == item:
        return element


  def get_native_path_case(path):
    """Returns the native path case for an existing file.

    Technically, it's only HFS+ on OSX that is case preserving and
    insensitive. It's the default setting on HFS+ but can be changed.
    """
    assert isinstance(path, unicode), repr(path)
    if not isabs(path):
      raise ValueError(
          'get_native_path_case(%r): Require an absolute path' % path, path)
    if path.startswith('/dev'):
      # /dev is not visible from Carbon, causing an exception.
      return path

    # Starts assuming there is no symlink along the path.
    resolved = _native_case(path)
    if path.lower() in (resolved.lower(), resolved.lower() + './'):
      # This code path is incredibly faster.
      logging.debug('get_native_path_case(%s) = %s' % (path, resolved))
      return resolved

    # There was a symlink, process it.
    base, symlink, rest = _split_at_symlink_native(None, path)
    if not symlink:
      # TODO(maruel): This can happen on OSX because we use stale APIs on OSX.
      # Fixing the APIs usage will likely fix this bug. The bug occurs due to
      # hardlinked files, where the API may return one file path or the other
      # depending on how it feels.
      return base
    prev = base
    base = safe_join(_native_case(base), symlink)
    assert len(base) > len(prev)
    while rest:
      prev = base
      relbase, symlink, rest = _split_at_symlink_native(base, rest)
      base = safe_join(base, relbase)
      assert len(base) > len(prev), (prev, base, symlink)
      if symlink:
        base = safe_join(base, symlink)
      assert len(base) > len(prev), (prev, base, symlink)
    # Make sure no symlink was resolved.
    assert base.lower() == path.lower(), (base, path)
    logging.debug('get_native_path_case(%s) = %s' % (path, base))
    return base


else:  # OSes other than Windows and OSX.


  # On non-windows, keep the stdlib behavior.
  isabs = os.path.isabs


  def find_item_native_case(root, item):
    """Gets the native path case of a single item based at root_path."""
    if item == '..':
      return item

    root = get_native_path_case(root)
    return os.path.basename(get_native_path_case(os.path.join(root, item)))


  def get_native_path_case(path):
    """Returns the native path case for an existing file.

    On OSes other than OSX and Windows, assume the file system is
    case-sensitive.

    TODO(maruel): This is not strictly true. Implement if necessary.
    """
    assert isinstance(path, unicode), repr(path)
    if not isabs(path):
      raise ValueError(
          'get_native_path_case(%r): Require an absolute path' % path, path)
    # Give up on cygwin, as GetLongPathName() can't be called.
    # Linux traces tends to not be normalized so use this occasion to normalize
    # it. This function implementation already normalizes the path on the other
    # OS so this needs to be done here to be coherent between OSes.
    out = os.path.normpath(path)
    if path.endswith(os.path.sep) and not out.endswith(os.path.sep):
      return out + os.path.sep
    return out


if sys.platform != 'win32':  # All non-Windows OSes.


  def safe_join(*args):
    """Joins path elements like os.path.join() but doesn't abort on absolute
    path.

    os.path.join('foo', '/bar') == '/bar'
    but safe_join('foo', '/bar') == 'foo/bar'.
    """
    out = ''
    for element in args:
      if element.startswith(os.path.sep):
        if out.endswith(os.path.sep):
          out += element[1:]
        else:
          out += element
      else:
        if out.endswith(os.path.sep):
          out += element
        else:
          out += os.path.sep + element
    return out


  def split_at_symlink(base_dir, relfile):
    """Scans each component of relfile and cut the string at the symlink if
    there is any.

    Returns a tuple (base_path, symlink, rest), with symlink == rest == None if
    not symlink was found.
    """
    if base_dir:
      assert relfile
      assert os.path.isabs(base_dir)
      index = 0
    else:
      assert os.path.isabs(relfile)
      index = 1

    def at_root(rest):
      if base_dir:
        return safe_join(base_dir, rest)
      return rest

    while True:
      try:
        index = relfile.index(os.path.sep, index)
      except ValueError:
        index = len(relfile)
      full = at_root(relfile[:index])
      if os.path.islink(full):
        # A symlink!
        base = os.path.dirname(relfile[:index])
        symlink = os.path.basename(relfile[:index])
        rest = relfile[index:]
        logging.debug(
            'split_at_symlink(%s, %s) -> (%s, %s, %s)' %
            (base_dir, relfile, base, symlink, rest))
        return base, symlink, rest
      if index == len(relfile):
        break
      index += 1
    return relfile, None, None


def relpath(path, root):
  """os.path.relpath() that keeps trailing os.path.sep."""
  out = os.path.relpath(path, root)
  if path.endswith(os.path.sep):
    out += os.path.sep
  return out


def safe_relpath(filepath, basepath):
  """Do not throw on Windows when filepath and basepath are on different drives.

  Different than relpath() above since this one doesn't keep the trailing
  os.path.sep and it swallows exceptions on Windows and return the original
  absolute path in the case of different drives.
  """
  try:
    return os.path.relpath(filepath, basepath)
  except ValueError:
    assert sys.platform == 'win32'
    return filepath


def normpath(path):
  """os.path.normpath() that keeps trailing os.path.sep."""
  out = os.path.normpath(path)
  if path.endswith(os.path.sep):
    out += os.path.sep
  return out


def posix_relpath(path, root):
  """posix.relpath() that keeps trailing slash.

  It is different from relpath() since it can be used on Windows.
  """
  out = posixpath.relpath(path, root)
  if path.endswith('/'):
    out += '/'
  return out


def cleanup_path(x):
  """Cleans up a relative path. Converts any os.path.sep to '/' on Windows."""
  if x:
    x = x.rstrip(os.path.sep).replace(os.path.sep, '/')
  if x == '.':
    x = ''
  if x:
    x += '/'
  return x


def is_url(path):
  """Returns True if it looks like an HTTP url instead of a file path."""
  return bool(re.match(r'^https?://.+$', path))


def path_starts_with(prefix, path):
  """Returns true if the components of the path |prefix| are the same as the
  initial components of |path| (or all of the components of |path|). The paths
  must be absolute.
  """
  assert os.path.isabs(prefix) and os.path.isabs(path)
  prefix = os.path.normpath(prefix)
  path = os.path.normpath(path)
  assert prefix == get_native_path_case(prefix), prefix
  assert path == get_native_path_case(path), path
  prefix = prefix.rstrip(os.path.sep) + os.path.sep
  path = path.rstrip(os.path.sep) + os.path.sep
  return path.startswith(prefix)


def fix_native_path_case(root, path):
  """Ensures that each component of |path| has the proper native case.

  It does so by iterating slowly over the directory elements of |path|. The file
  must exist.
  """
  native_case_path = root
  for raw_part in path.split(os.sep):
    if not raw_part or raw_part == '.':
      break

    part = find_item_native_case(native_case_path, raw_part)
    if not part:
      raise OSError(
          'File %s doesn\'t exist' %
          os.path.join(native_case_path, raw_part))
    native_case_path = os.path.join(native_case_path, part)

  return os.path.normpath(native_case_path)
