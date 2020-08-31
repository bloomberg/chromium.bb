# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Very minimal library for reading LUCI_CONTEXT sections.

See chromium.googlesource.com/infra/luci/luci-py/+/master/client/LUCI_CONTEXT.md
"""

import copy
import os
import sys
import threading

from infra_libs import utils


# LUCI_CONTEXT is immutable, so cache it the global state.
_UNSET = object()
_LUCI_CONTEXT = _UNSET
_LOCK = threading.Lock()


class Error(Exception):
  """Raised if LUCI_CONTEXT cannot be loaded due to unexpected error."""


def read(section_key, environ=None):
  """Returns a section of LUCI_CONTEXT or None if not there.

  Args:
    section_key: (str) the top-level key to read from the LUCI_CONTEXT.
    environ: an environ dict to use instead of os.environ, for tests.

  Returns:
    A copy of the requested section data (as a dict), or None if the section was
    not present.

  Raises:
    Error if the LUCI_CONTEXT cannot be loaded.
  """
  global _LUCI_CONTEXT
  if _LUCI_CONTEXT is _UNSET:
    with _LOCK:
      _LUCI_CONTEXT = _load(environ)
  return copy.deepcopy(_LUCI_CONTEXT.get(section_key, None))

###


def _reset():
  """Resets the cache for tests."""
  global _LUCI_CONTEXT
  _LUCI_CONTEXT = _UNSET


def _load(environ=None):
  """Loads and returns LUCI_CONTEXT dict or {} if missing."""
  if environ is None:  # pragma: no cover
    environ = os.environ
  path = environ.get('LUCI_CONTEXT')
  if not path:
    return {}
  path = path.decode(sys.getfilesystemencoding())
  try:
    loaded = utils.read_json_as_utf8(filename=path)
  except (OSError, IOError, ValueError) as e:
    raise Error('Failed to open, read or decode LUCI_CONTEXT: %s' % e)
  if not isinstance(loaded, dict):
    raise Error('Bad LUCI_CONTEXT, not a dict')
  return loaded
