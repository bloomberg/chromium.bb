# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Field handler classes.

The field handlers are meant to parse information from or do some other generic
action for a specific field type for the build_api script.
"""

from __future__ import print_function

import contextlib
import functools
import os
import shutil

from google.protobuf import message as protobuf_message

from chromite.api.controller import controller_util
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


class Error(Exception):
  """Base error class for the module."""


class InvalidResultPathError(Error):
  """Result path is invalid."""


class ChrootHandler(object):
  """Translate a Chroot message to chroot enter arguments and env."""

  def __init__(self, clear_field):
    self.clear_field = clear_field

  def handle(self, message):
    """Parse a message for a chroot field."""
    # Find the Chroot field. Search for the field by type to prevent it being
    # tied to a naming convention.
    for descriptor in message.DESCRIPTOR.fields:
      field = getattr(message, descriptor.name)
      if isinstance(field, common_pb2.Chroot):
        chroot = field
        if self.clear_field:
          message.ClearField(descriptor.name)
        return self.parse_chroot(chroot)

    return None

  def parse_chroot(self, chroot_message):
    """Parse a Chroot message instance."""
    return controller_util.ParseChroot(chroot_message)


def handle_chroot(message, clear_field=True):
  """Find and parse the chroot field, returning the Chroot instance.

  Returns:
    chroot_lib.Chroot
  """
  handler = ChrootHandler(clear_field)
  chroot = handler.handle(message)
  if chroot:
    return chroot

  logging.warning('No chroot message found, falling back to defaults.')
  return handler.parse_chroot(common_pb2.Chroot())


class PathHandler(object):
  """Handles copying a file or directory into or out of the chroot."""

  INSIDE = common_pb2.Path.INSIDE
  OUTSIDE = common_pb2.Path.OUTSIDE

  def __init__(self, field, destination, delete, prefix=None, reset=True):
    """Path handler initialization.

    Args:
      field (common_pb2.Path): The Path message.
      destination (str): The destination base path.
      delete (bool): Whether the copied file(s) should be deleted on cleanup.
      prefix (str|None): A path prefix to remove from the destination path
        when moving files inside the chroot, or to add to the source paths when
        moving files out of the chroot.
      reset (bool): Whether to reset the state on cleanup.
    """
    assert isinstance(field, common_pb2.Path)
    assert field.path
    assert field.location

    self.field = field
    self.destination = destination
    self.prefix = prefix or ''
    self.delete = delete
    self.tempdir = None
    self.reset = reset

    # For resetting the state.
    self._transferred = False
    self._original_message = common_pb2.Path()
    self._original_message.CopyFrom(self.field)

  def transfer(self, direction):
    """Copy the file or directory to its destination.

    Args:
      direction (int): The direction files are being copied (into or out of
        the chroot). Specifying the direction allows avoiding performing
        unnecessary copies.
    """
    if self._transferred:
      return

    assert direction in [self.INSIDE, self.OUTSIDE]

    if self.field.location == direction:
      # Already in the correct location, nothing to do.
      return

    # Create a tempdir for the copied file if we're cleaning it up afterwords.
    if self.delete:
      self.tempdir = osutils.TempDir(base_dir=self.destination)
      destination = self.tempdir.tempdir
    else:
      destination = self.destination

    source = self.field.path
    if direction == self.OUTSIDE and self.prefix:
      # When we're extracting files, we need /tmp/result to be
      # /path/to/chroot/tmp/result.
      source = os.path.join(self.prefix, source.lstrip(os.sep))

    if os.path.isfile(source):
      # File - use the old file name, just copy it into the destination.
      dest_path = os.path.join(destination, os.path.basename(source))
      copy_fn = shutil.copy
    else:
      # Directory - just copy everything into the new location.
      dest_path = destination
      copy_fn = functools.partial(osutils.CopyDirContents, allow_nonempty=True)

    logging.debug('Copying %s to %s', source, dest_path)
    copy_fn(source, dest_path)

    # Clean up the destination path for returning, if applicable.
    return_path = dest_path
    if direction == self.INSIDE and return_path.startswith(self.prefix):
      return_path = return_path[len(self.prefix):]

    self.field.path = return_path
    self.field.location = direction
    self._transferred = True

  def cleanup(self):
    if self.tempdir:
      self.tempdir.Cleanup()
      self.tempdir = None

    if self.reset:
      self.field.CopyFrom(self._original_message)


@contextlib.contextmanager
def copy_paths_in(message, destination, delete=True, prefix=None):
  """Context manager function to transfer and cleanup all Path messages.

  Args:
    message (Message): A message whose Path messages should be transferred.
    destination (str): A base destination path.
    delete (bool): Whether the file(s) should be deleted.
    prefix (str|None): A prefix path to remove from the final destination path
      in the Path message (i.e. remove the chroot path).

  Returns:
    list[PathHandler]: The path handlers.
  """
  assert destination

  handlers = _extract_handlers(message, destination, delete, prefix, reset=True)

  for handler in handlers:
    handler.transfer(PathHandler.INSIDE)

  try:
    yield handlers
  finally:
    for handler in handlers:
      handler.cleanup()


def extract_results(request_message, response_message, chroot):
  """Transfer all response Path messages to the request's ResultPath.

  Args:
    request_message (Message): The request message containing a ResultPath
      message.
    response_message (Message): The response message whose Path message(s)
      are to be transferred.
    chroot (chroot_lib.Chroot): The chroot the files are being copied out of.
  """
  # Find the ResultPath.
  for descriptor in request_message.DESCRIPTOR.fields:
    field = getattr(request_message, descriptor.name)
    if isinstance(field, common_pb2.ResultPath):
      result_path_message = field
      break
  else:
    # No ResultPath to handle.
    return

  destination = result_path_message.path.path
  handlers = _extract_handlers(response_message, destination, delete=False,
                               prefix=chroot.path, reset=False)

  for handler in handlers:
    handler.transfer(PathHandler.OUTSIDE)
    handler.cleanup()


def _extract_handlers(message, destination, delete, prefix, reset,
                      field_name=None):
  """Recursive helper for handle_paths to extract Path messages."""
  is_message = isinstance(message, protobuf_message.Message)
  is_result_path = isinstance(message, common_pb2.ResultPath)
  if not is_message or is_result_path:
    # Base case: Nothing to handle.
    # There's nothing we can do with scalar values.
    # Skip ResultPath instances to avoid unnecessary file copying.
    return []
  elif isinstance(message, common_pb2.Path):
    # Base case: Create handler for this message.
    if not message.path or not message.location:
      logging.debug('Skipping %s; incomplete.', field_name or 'message')
      return []

    handler = PathHandler(message, destination, delete=delete, prefix=prefix,
                          reset=reset)
    return [handler]

  # Iterate through each field and recurse.
  handlers = []
  for descriptor in message.DESCRIPTOR.fields:
    field = getattr(message, descriptor.name)
    if field_name:
      new_field_name = '%s.%s' % (field_name, descriptor.name)
    else:
      new_field_name = descriptor.name

    if isinstance(field, protobuf_message.Message):
      # Recurse for nested Paths.
      handlers.extend(
          _extract_handlers(field, destination, delete, prefix, reset,
                            field_name=new_field_name))
    else:
      # If it's iterable it may be a repeated field, try each element.
      try:
        iterator = iter(field)
      except TypeError:
        # Definitely not a repeated field, just move on.
        continue

      for element in iterator:
        handlers.extend(
            _extract_handlers(element, destination, delete, prefix, reset,
                              field_name=new_field_name))

  return handlers
