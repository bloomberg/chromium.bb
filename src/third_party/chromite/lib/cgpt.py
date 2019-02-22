# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Read disk information from a CrOS image using cgpt."""

from __future__ import print_function

import collections

from chromite.lib import cros_build_lib


class Error(Exception):
  """Raised when there is an error with Cgpt."""


class MultiplePartitionLabel(Error):
  """Raised when a duplicate partition label is found."""


# Storage of partition properties as read from `cgpt show`.
Partition = collections.namedtuple('Partition', (
    'part_num', 'label', 'start', 'size', 'part_type', 'uuid', 'attr'))


class Disk(object):
  """GPT disk image manager.

  Attributes:
    image_file: cros disk image
    partitions: OrderedDict of partitions, indexed by part_num
  """

  def __init__(self, image_file, partitions=None):
    self.image_file = image_file
    self.partitions = partitions or collections.OrderedDict()

  @classmethod
  def FromImage(cls, image_file):
    """Returns new Disk initialized from given |image_file|.

    Raises:
      RunCommandError: if error running cgpt command
      CgptError: if error parsing out output of cgpt command
    """
    cmd_result = cros_build_lib.RunCommand(['cgpt', 'show', '-n',
                                            image_file],
                                           capture_output=True)

    # Covert output to a file for processing via readline().
    cgpt_result = iter(cmd_result.output.splitlines())

    # Read header.
    if (not cmd_result.output
        or cgpt_result.next().split() != ['start', 'size', 'part', 'contents']):
      raise Error('Unable to find header in cgpt output')

    partitions = collections.OrderedDict()

    for line in cgpt_result:
      if 'Label:' not in line:
        # Skip anything not a partition.
        continue

      # The expected output for a partition is 4 lines in the following format.
      #  <start>  <size>  <part_num> Label: "<label>"
      #  Type: <part_type>
      #  UUID: <uuid>
      #  Attr: <attr>

      start, size, part_num, _, label = line.split()
      start = int(start)
      size = int(size)
      part_num = int(part_num)
      label = label.strip('"')

      part_type = ''
      uuid = ''
      attr = ''
      # Read next 3 lines for Type, UUID, and Attr.
      for _ in range(3):
        line = cgpt_result.next()
        outputs = line.split()
        if len(outputs) != 2:
          raise Error('Unexpected line %r' % outputs)

        if outputs[0] == 'Type:':
          part_type = outputs[1]
        elif outputs[0] == 'UUID:':
          uuid = outputs[1]
        elif outputs[0] == 'Attr:':
          attr = outputs[1]
        else:
          raise Error('Unexpected partition value: %r' % line.strip())

      partitions[part_num] = Partition(
          part_num=part_num, label=label, start=start, size=size,
          part_type=part_type, uuid=uuid, attr=attr)

    return cls(image_file, partitions=partitions)

  def GetPartitionByLabel(self, label):
    """Returns Partition with the given |label|.

    Raises:
      MultiplePartitionLabel: multiple partitions with that label found
      KeyError: label not found
    """
    part = None
    for part_next in self.partitions.values():
      if part_next.label == label:
        if part is None:
          part = part_next
        else:
          raise MultiplePartitionLabel(label)

    if part is None:
      raise KeyError(label)

    return part
