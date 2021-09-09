# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ======================================
"""Experimental support for defining XLA shardings."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as _np  # Avoids becoming a part of public Tensorflow API.

from tensorflow.compiler.tf2xla.python import xla as tf2xla
from tensorflow.compiler.xla import xla_data_pb2
from tensorflow.core.framework import attr_value_pb2


class Sharding(object):
  """A class to support adding sharding attributes to Ops.

  Use the factory constructors and then call apply_to_tensor:
    Sharding.replicate().apply_to_tensor(tensor)
  """

  def __init__(self, proto=None):
    """Do not use this constructor; use the factory functions below."""
    self._proto = proto

  @classmethod
  def replicate(cls):
    """Returns a replicated sharding attribute.

    This causes an op to be computed in its entirety independently on all
    cores in the XLA device.
    """
    return Sharding(
        proto=xla_data_pb2.OpSharding(type=xla_data_pb2.OpSharding.REPLICATED))

  @classmethod
  def assign_device(cls, core):
    """Returns an AssignDevice sharding attribute.

    This causes an op to be computed in its entirety only on one core in
    the XLA device.
    Args:
      core: The core to assign this Op to.
    """
    return Sharding(
        proto=xla_data_pb2.OpSharding(
            type=xla_data_pb2.OpSharding.MAXIMAL,
            tile_assignment_dimensions=[1],
            tile_assignment_devices=[core]))

  @classmethod
  def tile(cls, tile_assignment):
    """Returns a Tiled sharding attribute.

    This causes an op to be partially computed on multiple cores in the
    XLA device.

    Args:
      tile_assignment: An np.ndarray describing the topology of the tiling and
        which device will compute which part of the topology.

    Raises:
      TypeError: tile_assignment was not of np.array type.

    TODO(jmolloy): This concept is nefarious and is not
    something we really want to expose to users (especially as the
    contract for tile_assignment is very strict).
    """
    if not isinstance(tile_assignment, _np.ndarray):
      raise TypeError('Tile assignment must be of type np.ndarray')
    dims = list(tile_assignment.shape)
    flattened_devices = tile_assignment.reshape(-1, order='C')
    return Sharding(
        proto=xla_data_pb2.OpSharding(
            type=xla_data_pb2.OpSharding.OTHER,
            tile_assignment_dimensions=dims,
            tile_assignment_devices=list(flattened_devices)))

  @classmethod
  def split(cls, tensor, split_dimension, num_devices, input_shape=None):
    """Returns a Sharding that splits a tensor across a dimension.

    This creates a Tiled attribute, similar to tile(), but easier to use for the
    common case of tiling a tensor N ways in one dimension.

    Args:
      tensor: A tf.Tensor to split.
      split_dimension: The dimension number to split.
      num_devices: The number of cores to split `tensor` over.
      input_shape: The shape of the original tensor.

    Raises:
      ValueError: The tensor to split was smaller in the split dimension than
        the number of devices to split over.
    """
    if input_shape:
      shape = input_shape
    else:
      shape = tensor.shape.as_list()
    if (shape[split_dimension] is not None and
        shape[split_dimension] < num_devices):
      raise ValueError('Split dimension was smaller than the required number '
                       'of splits: shape=%r, dimension=%r, num_devices=%r' %
                       (shape, split_dimension, num_devices))

    tile_assignment_dims = [1] * len(shape)
    tile_assignment_dims[split_dimension] = num_devices

    return Sharding(
        proto=xla_data_pb2.OpSharding(
            type=xla_data_pb2.OpSharding.OTHER,
            tile_assignment_dimensions=tile_assignment_dims,
            tile_assignment_devices=range(num_devices)))

  def apply_to_tensor(self, tensor, assign_tuple_sharding=False):
    """Applies this Sharding attribute to `tensor`.

    Args:
      tensor: A tf.Tensor to split.
      assign_tuple_sharding: If the sharding type should be a tuple.
    """
    if len(tensor.op.outputs) > 1 or assign_tuple_sharding:
      proto = self._get_or_create_tuple_proto(tensor.op)
      # We can't mutate an element of old_proto.tuple_shardings, so create
      # a new proto.
      tuple_shardings = list(proto.tuple_shardings)
      tuple_shardings[tensor.value_index] = self._proto
      proto = xla_data_pb2.OpSharding(
          type=xla_data_pb2.OpSharding.TUPLE, tuple_shardings=tuple_shardings)
    else:
      proto = self._proto
    attr_value = attr_value_pb2.AttrValue(s=proto.SerializeToString())
    # TODO(jmolloy): This need to be seriously revisited before declaring this
    # API available for public use.
    # pylint: disable=protected-access
    tensor.op._set_attr('_XlaSharding', attr_value)

  @property
  def proto(self):
    """Return the sharding protobuf of type xla_data_pb2.OpSharding."""
    return self._proto

  def _get_or_create_tuple_proto(self, op):
    try:
      attr = op.get_attr('_XlaSharding')
      proto = xla_data_pb2.OpSharding()
      proto.ParseFromString(attr)
      return proto
    except ValueError:
      return self._create_tuple_proto(op)

  def _create_tuple_proto(self, op):
    shardings = [
        xla_data_pb2.OpSharding(type=xla_data_pb2.OpSharding.REPLICATED)
        for _ in op.outputs
    ]
    return xla_data_pb2.OpSharding(
        type=xla_data_pb2.OpSharding.TUPLE, tuple_shardings=shardings)


# Helpers for the above factory functions that allow easy application of
# shardings, for example:
#   tensor = xla_sharding.replicate(tensor)


def replicate(tensor, assign_tuple_sharding=False, use_sharding_op=False):
  if use_sharding_op:
    tensor = tf2xla.sharding(tensor)
  Sharding.replicate().apply_to_tensor(
      tensor,
      assign_tuple_sharding=assign_tuple_sharding)
  return tensor


def assign_device(tensor,
                  device,
                  assign_tuple_sharding=False,
                  use_sharding_op=False):
  """Returns a tensor that has AssignDevice sharding attribute."""
  if use_sharding_op:
    tensor = tf2xla.sharding(tensor)

  Sharding.assign_device(device).apply_to_tensor(
      tensor,
      assign_tuple_sharding=assign_tuple_sharding)
  return tensor


def tile(tensor,
         tile_assignment,
         assign_tuple_sharding=False,
         use_sharding_op=False):
  """Returns a tensor that has tiled sharding.

  Args:
    tensor: A tf.Tensor to shard.
    tile_assignment: An np.ndarray describing the topology of the tiling and
      which device will compute which part of the topology.
    assign_tuple_sharding: If the sharding type should be a tuple.
    use_sharding_op: If true, adds a sharding op to set the sharding.
  """
  if use_sharding_op:
    tensor = tf2xla.sharding(tensor)
  Sharding.tile(tile_assignment).apply_to_tensor(
      tensor,
      assign_tuple_sharding=assign_tuple_sharding
  )
  return tensor


def split(tensor,
          split_dimension,
          num_devices,
          assign_tuple_sharding=False,
          use_sharding_op=False,
          input_shape=None):
  """Returns a tensor that is split along the given dimension.

  Args:
    tensor: A tf.Tensor to split.
    split_dimension: The dimension to split.
    num_devices: The number of devices to partition the dimension.
    assign_tuple_sharding: If the sharding type should be a tuple.
    use_sharding_op: If true, adds a sharding op to set the sharding.
    input_shape: The full shape of the input tensor.
  """
  if use_sharding_op:
    tensor = tf2xla.sharding(tensor)
  Sharding.split(
      tensor, split_dimension, num_devices, input_shape).apply_to_tensor(
          tensor, assign_tuple_sharding=assign_tuple_sharding)
  return tensor


def get_op_sharding(op):
  """Returns sharding attribute of an op.

  Args:
    op: a TensorFlow op.

  Returns:
    The attribute representing XLA sharding on this op.
  """
  return op.get_attr('_XlaSharding')


def auto_to_manual_spmd_partition(tensor, manual_sharding):
  """Switches from automatic SPMD partitioning to manual partitioning.

  Converts a full-shaped tensor (to be automatically partitioned by SPMD
  partitioner) to a shard-shaped tensor to be consumed by manually partitioned
  ops.

  Args:
    tensor: A tf.Tensor in full shape.
    manual_sharding: a serialized string of OpSharding to be used in manual
      partitioning.

  Returns:
    A shard-shaped tensor to be consumed by manually partitioned ops.
  """
  return tf2xla.spmd_full_to_shard_shape(
      tensor, manual_sharding=manual_sharding)


def manual_to_auto_spmd_partition(tensor, manual_sharding, full_shape):
  """Switches from manual partitioning to automatic SPMD partitioning.

  Converts a shard-shaped tensor (manually partitioned in SPMD-style) to a
  full-shaped tensor to be partitioned automatically by the SPMD partitioner.

  Args:
    tensor: A tf.Tensor in shard shape.
    manual_sharding: a serialized string of OpSharding to be used in manual
      partitioning.
    full_shape: the shape of tensor before partitioning.

  Returns:
    A full-shaped tensor to be partitioned automatically by the SPMD
    partitioner.
  """
  return tf2xla.spmd_shard_to_full_shape(
      tensor, manual_sharding=manual_sharding, full_shape=full_shape)
