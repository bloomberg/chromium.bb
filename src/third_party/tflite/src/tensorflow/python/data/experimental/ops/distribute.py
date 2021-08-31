# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Distribution Strategy-related dataset transformations."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.data.experimental.ops.distribute_options import ExternalStatePolicy
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.data.util import nest
from tensorflow.python.framework import ops
from tensorflow.python.ops import gen_experimental_dataset_ops as ged_ops


class _AutoShardDataset(dataset_ops.UnaryDataset):
  """A `Dataset` that shards the `Dataset` automatically.

  This dataset takes in an existing dataset and tries to automatically figure
  out how to shard the dataset in a multi-worker scenario. Currently, it uses
  Grappler to walk up the dataset graph until it finds a reader dataset (e.g.
  CSVDataset, TFRecordDataset), then inserts a ShardDataset op before that node
  so that each worker only sees some files.

  Args:
    num_workers: Total number of workers to shard this dataset across.
    index: The current worker index (out of the total number of workers) this
      dataset is for.

  Raises:
    NotFoundError: If we cannot find a suitable reader dataset to begin
      automatically sharding the dataset.
  """

  def __init__(self, input_dataset, num_workers, index):
    self._input_dataset = input_dataset

    self._element_spec = input_dataset.element_spec
    variant_tensor = ged_ops.auto_shard_dataset(
        self._input_dataset._variant_tensor,  # pylint: disable=protected-access
        num_workers=num_workers,
        index=index,
        auto_shard_policy=int(
            input_dataset.options().experimental_distribute.auto_shard_policy),
        **self._flat_structure)
    super(_AutoShardDataset, self).__init__(input_dataset, variant_tensor)

  @property
  def element_spec(self):
    return self._element_spec


def _AutoShardDatasetV1(input_dataset, num_workers, index):  # pylint: disable=invalid-name
  return dataset_ops.DatasetV1Adapter(
      _AutoShardDataset(input_dataset, num_workers, index))


class _RebatchDataset(dataset_ops.UnaryDataset):
  """A `Dataset` that divides the batch size by `num_replicas`.

  For each batch in the input dataset, the resulting dataset will produce
  `num_replicas` minibatches whose sizes add up to the original batch size.
  """

  def __init__(self, input_dataset, num_replicas, use_fallback=True):

    def recalculate_batch_size(output_shape):
      """Recalculates the output_shape after dividing it by num_replicas."""
      # If the output shape is unknown, we set the batch dimension to unknown.
      if output_shape.rank is None:
        return None

      if len(output_shape) < 1:
        raise ValueError("Expected a dataset whose elements have rank >= 1 "
                         "but found a dataset whose elements are scalars. "
                         "You can fix the issue by adding the `batch` "
                         "transformation to the dataset.")
      output_dims = [d.value for d in output_shape.dims]

      if output_dims[0] is not None and output_dims[0] % num_replicas == 0:
        return output_dims[0] // num_replicas

      # Set the batch dimension to unknown. If the global batch size does not
      # divide num_replicas evenly, the minibatches may have different sizes.
      return None

    def rebatch(type_spec):
      # pylint: disable=protected-access
      batch_size = recalculate_batch_size(type_spec._to_legacy_output_shapes())
      return type_spec._unbatch()._batch(batch_size)
      # pylint: enable=protected-access

    self._element_spec = nest.map_structure(
        rebatch, dataset_ops.get_structure(input_dataset))
    input_dataset = dataset_ops.normalize_to_dense(input_dataset)
    variant_tensor = ged_ops.rebatch_dataset(
        input_dataset._variant_tensor,  # pylint: disable=protected-access
        num_replicas=num_replicas,
        **self._flat_structure)
    super(_RebatchDataset, self).__init__(input_dataset, variant_tensor)

  @property
  def element_spec(self):
    return self._element_spec


class _RemoteDataset(dataset_ops.DatasetSource):
  """Creates a dataset on a given `device` given a graph def."""

  def __init__(self, graph_def, device, element_spec):
    self._elem_spec = element_spec
    with ops.device(device):
      variant_tensor = ged_ops.dataset_from_graph(graph_def)
    super(_RemoteDataset, self).__init__(variant_tensor)

  @property
  def element_spec(self):
    return self._elem_spec


def replicate(dataset, devices):
  """A transformation that replicates `dataset` onto a list of devices.

  Args:
    dataset: A `tf.data.Dataset` object.
    devices: A list of devices to replicate the dataset on.

  Returns:
    A dictionary mapping device name to a dataset on that device.
  """
  if not isinstance(dataset, dataset_ops.DatasetV2):
    raise TypeError("`dataset` must be a `tf.data.Dataset` object.")

  # pylint: disable=protected-access
  dataset_device = dataset._variant_tensor.device

  datasets = {}
  if len(devices) == 1 and devices[0] == dataset_device:
    datasets[devices[0]] = dataset
    return datasets

  with ops.colocate_with(dataset._variant_tensor):
    dataset = dataset._apply_options()
    policy = dataset.options().experimental_external_state_policy
    if policy is None:
      policy = ExternalStatePolicy.WARN
    graph_def = dataset._as_serialized_graph(
        strip_device_assignment=True,
        external_state_policy=policy)
  for device in devices:
    ds = _RemoteDataset(graph_def, device, dataset.element_spec)
    datasets[device] = ds
  return datasets


_AutoShardDatasetV1.__doc__ = _AutoShardDataset.__doc__
