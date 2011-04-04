# coding=utf8
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions to handle patches."""

import re


class UnsupportedPatchFormat(Exception):
  def __init__(self, filename, status):
    super(UnsupportedPatchFormat, self).__init__(filename, status)
    self.filename = filename
    self.status = status

  def __str__(self):
    out = 'Can\'t process patch for file %s.' % self.filename
    if self.status:
      out += '\n%s' % self.status
    return out


class FilePatchBase(object):
  """Defines a single file being modified."""
  is_delete = False
  is_binary = False

  def get(self):
    raise NotImplementedError('Nothing to grab')


class FilePatchDelete(FilePatchBase):
  """Deletes a file."""
  is_delete = True

  def __init__(self, filename, is_binary):
    super(FilePatchDelete, self).__init__()
    self.filename = filename
    self.is_binary = is_binary

  def get(self):
    raise NotImplementedError('Nothing to grab')


class FilePatchBinary(FilePatchBase):
  """Content of a new binary file."""
  is_binary = True

  def __init__(self, filename, data, svn_properties):
    super(FilePatchBinary, self).__init__()
    self.filename = filename
    self.data = data
    self.svn_properties = svn_properties or []

  def get(self):
    return self.data


class FilePatchDiff(FilePatchBase):
  """Patch for a single file."""

  def __init__(self, filename, diff, svn_properties):
    super(FilePatchDiff, self).__init__()
    self.filename = filename
    self.diff = diff
    self.svn_properties = svn_properties or []
    self.is_git_diff = self._is_git_diff(diff)
    if self.is_git_diff:
      self.patchlevel = 1
      self._verify_git_patch(filename, diff)
      assert not svn_properties
    else:
      self.patchlevel = 0
      self._verify_svn_patch(filename, diff)

  def get(self):
    return self.diff

  @staticmethod
  def _is_git_diff(diff):
    """Returns True if the diff for a single files was generated with gt.

    Expects the following format:

    Index: <filename>
    diff --git a/<filename> b/<filename>
    <filemode changes>
    <index>
    --- <filename>
    +++ <filename>
    <hunks>

    Index: is a rietveld specific line.
    """
    # Delete: http://codereview.chromium.org/download/issue6368055_22_29.diff
    # Rename partial change:
    # http://codereview.chromium.org/download/issue6250123_3013_6010.diff
    # Rename no change:
    # http://codereview.chromium.org/download/issue6287022_3001_4010.diff
    return any(l.startswith('diff --git') for l in diff.splitlines()[:3])

  @staticmethod
  def _verify_git_patch(filename, diff):
    lines = diff.splitlines()
    # First fine the git diff header:
    while lines:
      line = lines.pop(0)
      match = re.match(r'^diff \-\-git a\/(.*?) b\/(.*)$', line)
      if match:
        a = match.group(1)
        b = match.group(2)
        if a != filename and a != 'dev/null':
          raise UnsupportedPatchFormat(
              filename, 'Unexpected git diff input name.')
        if b != filename and b != 'dev/null':
          raise UnsupportedPatchFormat(
              filename, 'Unexpected git diff output name.')
        if a == 'dev/null' and b == 'dev/null':
          raise UnsupportedPatchFormat(
              filename, 'Unexpected /dev/null git diff.')
        break
    else:
      raise UnsupportedPatchFormat(
          filename, 'Unexpected git diff; couldn\'t find git header.')

    while lines:
      line = lines.pop(0)
      match = re.match(r'^--- a/(.*)$', line)
      if match:
        if a != match.group(1):
          raise UnsupportedPatchFormat(
              filename, 'Unexpected git diff: %s != %s.' % (a, match.group(1)))
        match = re.match(r'^\+\+\+ b/(.*)$', lines.pop(0))
        if not match:
          raise UnsupportedPatchFormat(
              filename, 'Unexpected git diff: --- not following +++.')
        if b != match.group(1):
          raise UnsupportedPatchFormat(
              filename, 'Unexpected git diff: %s != %s.' % (b, match.group(1)))
        break
    # Don't fail if the patch header is not found, the diff could be a
    # file-mode-change-only diff.

  @staticmethod
  def _verify_svn_patch(filename, diff):
    lines = diff.splitlines()
    while lines:
      line = lines.pop(0)
      match = re.match(r'^--- ([^\t]+).*$', line)
      if match:
        if match.group(1) not in (filename, '/dev/null'):
          raise UnsupportedPatchFormat(
              filename,
              'Unexpected diff: %s != %s.' % (filename, match.group(1)))
        # Grab next line.
        line2 = lines.pop(0)
        match = re.match(r'^\+\+\+ ([^\t]+).*$', line2)
        if not match:
          raise UnsupportedPatchFormat(
              filename,
              'Unexpected diff: --- not following +++.:\n%s\n%s' % (
                  line, line2))
        if match.group(1) not in (filename, '/dev/null'):
          raise UnsupportedPatchFormat(
              filename,
              'Unexpected diff: %s != %s.' % (filename, match.group(1)))
        break
    # Don't fail if the patch header is not found, the diff could be a
    # svn-property-change-only diff.


class PatchSet(object):
  """A list of FilePatch* objects."""

  def __init__(self, patches):
    self.patches = patches

  def __iter__(self):
    for patch in self.patches:
      yield patch

  @property
  def filenames(self):
    return [p.filename for p in self.patches]
