# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
"""Various classes representing distributed inputs."""

import functools
import sys

import six

from tensorflow.python.data.experimental.ops import batching
from tensorflow.python.data.experimental.ops import cardinality as cardinality_lib
from tensorflow.python.data.experimental.ops import distribute
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.data.ops import iterator_ops
from tensorflow.python.data.ops import multi_device_iterator_ops
from tensorflow.python.data.ops import optional_ops
from tensorflow.python.distribute import device_util
from tensorflow.python.distribute import distribute_lib
from tensorflow.python.distribute import distribute_utils
from tensorflow.python.distribute import distribution_strategy_context
from tensorflow.python.distribute import input_ops
from tensorflow.python.distribute import reduce_util
from tensorflow.python.distribute import values
from tensorflow.python.distribute.distribute_lib import InputReplicationMode
from tensorflow.python.eager import context
from tensorflow.python.framework import composite_tensor
from tensorflow.python.framework import device as tf_device
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.framework import tensor_shape
from tensorflow.python.framework import tensor_util
from tensorflow.python.framework import type_spec
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops.ragged import ragged_tensor
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.types import distribute as distribute_types
from tensorflow.python.util import nest
from tensorflow.python.util.compat import collections_abc
from tensorflow.python.util.tf_export import tf_export
from tensorflow.tools.docs import doc_controls


def get_iterator_spec_from_dataset(strategy, dataset):
  """Returns an iterator spec from dataset function.

  This function constructs type spec for iterator obtained from
  iter(dataset).

  Args:
    strategy: a `tf.distribute.Strategy` object, used to run all-reduce to
        handle last partial batch.
    dataset: A tf.data.Dataset instance. If using a function that returns a
      tf.data.Dataset instance, pass dataset_fn.structured_outputs.

  Returns:
    A type_spec for iterator for dataset instance.

  """
  # pylint: disable=protected-access
  output_element_spec = dataset.element_spec
  if isinstance(dataset._type_spec,
                (DistributedDatasetSpec,
                 DistributedDatasetsFromFunctionSpec)):
    iterator_type_spec = DistributedIteratorSpec(
        strategy.extended._input_workers_with_options(),
        output_element_spec,
        strategy.extended._container_strategy(),
        options=None,
        cardinality=dataset.cardinality,
        enable_get_next_as_optional=True)
  else:
    if strategy.extended._num_gpus_per_worker:
      logging.warning(
          f"{strategy.extended._num_gpus_per_worker} GPUs "
          "are allocated per worker. Please use DistributedDataset by "
          "calling strategy.experimental_distribute_dataset or strategy."
          "distribute_datasets_from_function to make best use of GPU "
          "resources"
      )
    iterator_type_spec = iterator_ops.IteratorSpec(output_element_spec)
  return iterator_type_spec
  # pylint: enable=protected-access


@tf_export("distribute.DistributedIterator", v1=[])
class DistributedIteratorInterface(collections_abc.Iterator,
                                   distribute_types.Iterator):
  """An iterator over `tf.distribute.DistributedDataset`.

  `tf.distribute.DistributedIterator` is the primary mechanism for enumerating
  elements of a `tf.distribute.DistributedDataset`. It supports the Python
  Iterator protocol, which means it can be iterated over using a for-loop or by
  fetching individual elements explicitly via `get_next()`.

  You can create a `tf.distribute.DistributedIterator` by calling `iter` on
  a `tf.distribute.DistributedDataset` or creating a python loop over a
  `tf.distribute.DistributedDataset`.

  Visit the [tutorial](https://www.tensorflow.org/tutorials/distribute/input)
  on distributed input for more examples and caveats.
  """

  def get_next(self):
    """Returns the next input from the iterator for all replicas.

    Example use:

    >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
    >>> dataset = tf.data.Dataset.range(100).batch(2)
    >>> dist_dataset = strategy.experimental_distribute_dataset(dataset)
    >>> dist_dataset_iterator = iter(dist_dataset)
    >>> @tf.function
    ... def one_step(input):
    ...   return input
    >>> step_num = 5
    >>> for _ in range(step_num):
    ...   strategy.run(one_step, args=(dist_dataset_iterator.get_next(),))
    >>> strategy.experimental_local_results(dist_dataset_iterator.get_next())
    (<tf.Tensor: shape=(1,), dtype=int64, numpy=array([10])>,
     <tf.Tensor: shape=(1,), dtype=int64, numpy=array([11])>)

    Returns:
      A single `tf.Tensor` or a `tf.distribute.DistributedValues` which contains
      the next input for all replicas.

    Raises:
      `tf.errors.OutOfRangeError`: If the end of the iterator has been reached.
    """
    raise NotImplementedError(
        "DistributedIterator.get_next() must be implemented in descendants.")

  @property
  def element_spec(self):
    # pylint: disable=line-too-long
    """The type specification of an element of `tf.distribute.DistributedIterator`.

    Example usage:

    >>> global_batch_size = 16
    >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
    >>> dataset = tf.data.Dataset.from_tensors(([1.],[2])).repeat(100).batch(global_batch_size)
    >>> distributed_iterator = iter(strategy.experimental_distribute_dataset(dataset))
    >>> distributed_iterator.element_spec
    (PerReplicaSpec(TensorSpec(shape=(None, 1), dtype=tf.float32, name=None),
                    TensorSpec(shape=(None, 1), dtype=tf.float32, name=None)),
     PerReplicaSpec(TensorSpec(shape=(None, 1), dtype=tf.int32, name=None),
                    TensorSpec(shape=(None, 1), dtype=tf.int32, name=None)))

    Returns:
      A nested structure of `tf.TypeSpec` objects matching the structure of an
      element of this `tf.distribute.DistributedIterator`. This returned value
      is typically a `tf.distribute.DistributedValues` object and specifies the
      `tf.TensorSpec` of individual components.
    """
    raise NotImplementedError(
        "DistributedIterator.element_spec() must be implemented in descendants")

  def get_next_as_optional(self):
    # pylint: disable=line-too-long
    """Returns a `tf.experimental.Optional` that contains the next value for all replicas.

    If the `tf.distribute.DistributedIterator` has reached the end of the
    sequence, the returned `tf.experimental.Optional` will have no value.

    Example usage:

    >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
    >>> global_batch_size = 2
    >>> steps_per_loop = 2
    >>> dataset = tf.data.Dataset.range(10).batch(global_batch_size)
    >>> distributed_iterator = iter(
    ...     strategy.experimental_distribute_dataset(dataset))
    >>> def step_fn(x):
    ...   # train the model with inputs
    ...   return x
    >>> @tf.function
    ... def train_fn(distributed_iterator):
    ...   for _ in tf.range(steps_per_loop):
    ...     optional_data = distributed_iterator.get_next_as_optional()
    ...     if not optional_data.has_value():
    ...       break
    ...     per_replica_results = strategy.run(step_fn, args=(optional_data.get_value(),))
    ...     tf.print(strategy.experimental_local_results(per_replica_results))
    >>> train_fn(distributed_iterator)
    ... # ([0 1], [2 3])
    ... # ([4], [])

    Returns:
      An `tf.experimental.Optional` object representing the next value from the
      `tf.distribute.DistributedIterator` (if it has one) or no value.
    """
    # pylint: enable=line-too-long
    raise NotImplementedError(
        "get_next_as_optional() not implemented in descendants")


@tf_export("distribute.DistributedDataset", v1=[])
class DistributedDatasetInterface(collections_abc.Iterable,
                                  distribute_types.Iterable):
  # pylint: disable=line-too-long
  """Represents a dataset distributed among devices and machines.

  A `tf.distribute.DistributedDataset` could be thought of as a "distributed"
  dataset. When you use `tf.distribute` API to scale training to multiple
  devices or machines, you also need to distribute the input data, which leads
  to a `tf.distribute.DistributedDataset` instance, instead of a
  `tf.data.Dataset` instance in the non-distributed case. In TF 2.x,
  `tf.distribute.DistributedDataset` objects are Python iterables.

  Note: `tf.distribute.DistributedDataset` instances are *not* of type
  `tf.data.Dataset`. It only supports two usages we will mention below:
  iteration and `element_spec`. We don't support any other APIs to transform or
  inspect the dataset.

  There are two APIs to create a `tf.distribute.DistributedDataset` object:
  `tf.distribute.Strategy.experimental_distribute_dataset(dataset)`and
  `tf.distribute.Strategy.distribute_datasets_from_function(dataset_fn)`.
  *When to use which?* When you have a `tf.data.Dataset` instance, and the
  regular batch splitting (i.e. re-batch the input `tf.data.Dataset` instance
  with a new batch size that is equal to the global batch size divided by the
  number of replicas in sync) and autosharding (i.e. the
  `tf.data.experimental.AutoShardPolicy` options) work for you, use the former
  API. Otherwise, if you are *not* using a canonical `tf.data.Dataset` instance,
  or you would like to customize the batch splitting or sharding, you can wrap
  these logic in a `dataset_fn` and use the latter API. Both API handles
  prefetch to device for the user. For more details and examples, follow the
  links to the APIs.


  There are two main usages of a `DistributedDataset` object:

  1. Iterate over it to generate the input for a single device or multiple
  devices, which is a `tf.distribute.DistributedValues` instance. To do this,
  you can:

    * use a pythonic for-loop construct:

      >>> global_batch_size = 4
      >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
      >>> dataset = tf.data.Dataset.from_tensors(([1.],[1.])).repeat(4).batch(global_batch_size)
      >>> dist_dataset = strategy.experimental_distribute_dataset(dataset)
      >>> @tf.function
      ... def train_step(input):
      ...   features, labels = input
      ...   return labels - 0.3 * features
      >>> for x in dist_dataset:
      ...   # train_step trains the model using the dataset elements
      ...   loss = strategy.run(train_step, args=(x,))
      ...   print("Loss is", loss)
      Loss is PerReplica:{
        0: tf.Tensor(
      [[0.7]
       [0.7]], shape=(2, 1), dtype=float32),
        1: tf.Tensor(
      [[0.7]
       [0.7]], shape=(2, 1), dtype=float32)
      }

      Placing the loop inside a `tf.function` will give a performance boost.
      However `break` and `return` are currently not supported if the loop is
      placed inside a `tf.function`. We also don't support placing the loop
      inside a `tf.function` when using
      `tf.distribute.experimental.MultiWorkerMirroredStrategy` or
      `tf.distribute.experimental.TPUStrategy` with multiple workers.

    * use `__iter__` to create an explicit iterator, which is of type
      `tf.distribute.DistributedIterator`

      >>> global_batch_size = 4
      >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
      >>> train_dataset = tf.data.Dataset.from_tensors(([1.],[1.])).repeat(50).batch(global_batch_size)
      >>> train_dist_dataset = strategy.experimental_distribute_dataset(train_dataset)
      >>> @tf.function
      ... def distributed_train_step(dataset_inputs):
      ...   def train_step(input):
      ...     loss = tf.constant(0.1)
      ...     return loss
      ...   per_replica_losses = strategy.run(train_step, args=(dataset_inputs,))
      ...   return strategy.reduce(tf.distribute.ReduceOp.SUM, per_replica_losses,axis=None)
      >>> EPOCHS = 2
      >>> STEPS = 3
      >>> for epoch in range(EPOCHS):
      ...   total_loss = 0.0
      ...   num_batches = 0
      ...   dist_dataset_iterator = iter(train_dist_dataset)
      ...   for _ in range(STEPS):
      ...     total_loss += distributed_train_step(next(dist_dataset_iterator))
      ...     num_batches += 1
      ...   average_train_loss = total_loss / num_batches
      ...   template = ("Epoch {}, Loss: {:.4f}")
      ...   print (template.format(epoch+1, average_train_loss))
      Epoch 1, Loss: 0.2000
      Epoch 2, Loss: 0.2000


    To achieve a performance improvement, you can also wrap the `strategy.run`
    call with a `tf.range` inside a `tf.function`. This runs multiple steps in a
    `tf.function`. Autograph will convert it to a `tf.while_loop` on the worker.
    However, it is less flexible comparing with running a single step inside
    `tf.function`. For example, you cannot run things eagerly or arbitrary
    python code within the steps.


  2. Inspect the `tf.TypeSpec` of the data generated by `DistributedDataset`.

    `tf.distribute.DistributedDataset` generates
    `tf.distribute.DistributedValues` as input to the devices. If you pass the
    input to a `tf.function` and would like to specify the shape and type of
    each Tensor argument to the function, you can pass a `tf.TypeSpec` object to
    the `input_signature` argument of the `tf.function`. To get the
    `tf.TypeSpec` of the input, you can use the `element_spec` property of the
    `tf.distribute.DistributedDataset` or `tf.distribute.DistributedIterator`
    object.

    For example:

    >>> global_batch_size = 4
    >>> epochs = 1
    >>> steps_per_epoch = 1
    >>> mirrored_strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
    >>> dataset = tf.data.Dataset.from_tensors(([2.])).repeat(100).batch(global_batch_size)
    >>> dist_dataset = mirrored_strategy.experimental_distribute_dataset(dataset)
    >>> @tf.function(input_signature=[dist_dataset.element_spec])
    ... def train_step(per_replica_inputs):
    ...   def step_fn(inputs):
    ...     return tf.square(inputs)
    ...   return mirrored_strategy.run(step_fn, args=(per_replica_inputs,))
    >>> for _ in range(epochs):
    ...   iterator = iter(dist_dataset)
    ...   for _ in range(steps_per_epoch):
    ...     output = train_step(next(iterator))
    ...     print(output)
    PerReplica:{
      0: tf.Tensor(
    [[4.]
     [4.]], shape=(2, 1), dtype=float32),
      1: tf.Tensor(
    [[4.]
     [4.]], shape=(2, 1), dtype=float32)
    }


  Visit the [tutorial](https://www.tensorflow.org/tutorials/distribute/input)
  on distributed input for more examples and caveats.
  """

  def __iter__(self):
    """Creates an iterator for the `tf.distribute.DistributedDataset`.

    The returned iterator implements the Python Iterator protocol.

    Example usage:

    >>> global_batch_size = 4
    >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
    >>> dataset = tf.data.Dataset.from_tensor_slices([1, 2, 3, 4]).repeat().batch(global_batch_size)
    >>> distributed_iterator = iter(strategy.experimental_distribute_dataset(dataset))
    >>> print(next(distributed_iterator))
    PerReplica:{
      0: tf.Tensor([1 2], shape=(2,), dtype=int32),
      1: tf.Tensor([3 4], shape=(2,), dtype=int32)
    }

    Returns:
      An `tf.distribute.DistributedIterator` instance for the given
      `tf.distribute.DistributedDataset` object to enumerate over the
      distributed data.
    """
    raise NotImplementedError("Must be implemented in descendants")

  @property
  def element_spec(self):
    """The type specification of an element of this `tf.distribute.DistributedDataset`.

    Example usage:

    >>> global_batch_size = 16
    >>> strategy = tf.distribute.MirroredStrategy(["GPU:0", "GPU:1"])
    >>> dataset = tf.data.Dataset.from_tensors(([1.],[2])).repeat(100).batch(global_batch_size)
    >>> dist_dataset = strategy.experimental_distribute_dataset(dataset)
    >>> dist_dataset.element_spec
    (PerReplicaSpec(TensorSpec(shape=(None, 1), dtype=tf.float32, name=None),
                    TensorSpec(shape=(None, 1), dtype=tf.float32, name=None)),
     PerReplicaSpec(TensorSpec(shape=(None, 1), dtype=tf.int32, name=None),
                    TensorSpec(shape=(None, 1), dtype=tf.int32, name=None)))

    Returns:
      A nested structure of `tf.TypeSpec` objects matching the structure of an
      element of this `tf.distribute.DistributedDataset`. This returned value is
      typically a `tf.distribute.DistributedValues` object and specifies the
      `tf.TensorSpec` of individual components.
    """
    raise NotImplementedError(
        "DistributedDataset.element_spec must be implemented in descendants.")

  @doc_controls.do_not_generate_docs
  def reduce(self, initial_state, reduce_func):
    raise NotImplementedError(
        "DistributedDataset.reduce must be implemented in descendants.")


class InputWorkers(object):
  """A 1-to-many mapping from input worker devices to compute devices."""

  # TODO(ishark): Remove option canonicalize_devices and make all the callers
  # pass canonicalized or raw device strings as relevant from strategy.
  def __init__(self,
               worker_device_pairs,
               canonicalize_devices=True):
    """Initialize an `InputWorkers` object.

    Args:
      worker_device_pairs: A sequence of pairs: `(input device, a tuple of
        compute devices fed by that input device)`.
      canonicalize_devices: Whether to canonicalize devices for workers fully or
        partially. If False, it will partially canonicalize devices by removing
        job and task.
    """
    self._worker_device_pairs = worker_device_pairs
    self._input_worker_devices = tuple(d for d, _ in self._worker_device_pairs)
    self._canonicalize_devices = canonicalize_devices
    if canonicalize_devices:
      self._fed_devices = tuple(
          tuple(device_util.canonicalize(d)
                for d in f)
          for _, f in self._worker_device_pairs)
    else:
      self._fed_devices = tuple(
          tuple(device_util.canonicalize_without_job_and_task(d)
                for d in f)
          for _, f in self._worker_device_pairs)

  @property
  def num_workers(self):
    return len(self._input_worker_devices)

  @property
  def worker_devices(self):
    return self._input_worker_devices

  def compute_devices_for_worker(self, worker_index):
    return self._fed_devices[worker_index]

  def __repr__(self):
    devices = self.worker_devices
    debug_repr = ",\n".join("  %d %s: %s" %
                            (i, devices[i], self._fed_devices[i])
                            for i in range(len(devices)))
    return "%s:{\n%s}" % (self.__class__.__name__, debug_repr)

  def serialize(self):
    return (self._worker_device_pairs, self._canonicalize_devices)

  def deserialize(self, serialized):
    return InputWorkers(serialized)


def _calculate_replicas_with_values(strategy, input_workers, optional_list):
  """Calcualates the number of replicas that have values.

  Args:
    strategy: the `tf.distribute.Strategy`.
    input_workers: the `InputWorkers`.
    optional_list: a list of lists `tf.experimental.Optional`. The values from
      each compute device grouped by the input device.

  Returns:
    A scalar Tensor.
  """
  worker_has_values = []
  for worker, optionals in zip(input_workers.worker_devices, optional_list):
    with ops.device(worker):
      device_has_values = [
          math_ops.cast(v.has_value(), dtypes.int64) for v in optionals
      ]
      worker_has_values.append(
          math_ops.reduce_sum(device_has_values, keepdims=True))
  client_has_values = math_ops.reduce_sum(worker_has_values, keepdims=True)
  if strategy.extended._in_multi_worker_mode():  # pylint: disable=protected-access
    global_has_values = strategy.reduce(
        reduce_util.ReduceOp.SUM, client_has_values, axis=None)
    return array_ops.reshape(global_has_values, [])
  else:
    return array_ops.reshape(client_has_values, [])


def _is_statically_shaped(element_spec):
  """Test if an iterator output is statically shaped.

  For sparse and ragged tensors this only tests the batch dimension.

  Args:
    element_spec: a nest structure of `tf.TypeSpec`. The element spec of the
      dataset of the iterator.

  Returns:
    True if the shape is static, false otherwise.
  """

  for spec in nest.flatten(element_spec):
    if isinstance(
        spec, (sparse_tensor.SparseTensorSpec, ragged_tensor.RaggedTensorSpec)):
      # For sparse or ragged tensor, we should only check the first
      # dimension in order to get_next_as_optional. This is because
      # when these tensors get batched by dataset only the batch dimension
      # is set.
      if spec.shape.rank > 0 and spec.shape.as_list()[0] is None:
        return False
    else:
      for component in nest.flatten(spec._component_specs):  # pylint: disable=protected-access
        if not component.shape.is_fully_defined():
          return False
  return True


class DistributedIteratorBase(DistributedIteratorInterface):
  """Common implementation for all input iterators."""

  # pylint: disable=super-init-not-called
  def __init__(self, input_workers, iterators, strategy, cardinality,
               enable_get_next_as_optional):
    assert isinstance(input_workers, InputWorkers)
    if not input_workers.worker_devices:
      raise ValueError("Should have at least one worker for input iterator.")

    self._iterators = iterators
    self._input_workers = input_workers
    self._strategy = strategy
    self._cardinality = cardinality
    self._enable_get_next_as_optional = enable_get_next_as_optional

  def next(self):
    return self.__next__()

  def __next__(self):
    try:
      return self.get_next()
    except errors.OutOfRangeError:
      raise StopIteration

  def __iter__(self):
    return self

  def get_next_as_optional(self):
    # Ideally get_next_as_optional() should be consistent with get_next(), but
    # we used to always do partial batch handling in get_next_as_optional(). We
    # are keeping this behavior for now until we understantd the impact.

    # Skip partial batch handling when the dataset is infinite or empty, as
    # there won't be any partial batches in those cases. This gives the user
    # more static shapes as it avoids the tf.cond. Note that for empty datasets,
    # we can only skip in single client mode, as the dataset can be non-empty on
    # other workers.
    if self._cardinality == cardinality_lib.INFINITE:
      return optional_ops.Optional.from_value(
          self._get_next_no_partial_batch_handling())
    if (self._cardinality == 0 and
        not self._strategy.extended._in_multi_worker_mode()):  # pylint: disable=protected-access
      return optional_ops.Optional.empty(self._element_spec)

    optional_list = []
    for i, worker in enumerate(self._input_workers.worker_devices):
      with ops.device(worker):
        optional_list.append(self._iterators[i].get_next_as_optional_list())

    def _create_optional_with_dummy():
      value_list = _get_value_or_dummy(
          self._input_workers, optional_list, produce_dummy=True)
      per_replica = _create_per_replica(value_list, self._strategy)
      return optional_ops.Optional.from_value(per_replica)

    def _create_empty_optional():
      return optional_ops.Optional.empty(self._element_spec)

    num_replicas_with_values = _calculate_replicas_with_values(
        self._strategy, self._input_workers, optional_list)

    return control_flow_ops.cond(
        num_replicas_with_values > 0,
        _create_optional_with_dummy,
        _create_empty_optional,
        strict=True)

  def get_next(self, name=None):
    """Returns the next input from the iterator for all replicas."""
    with distribution_strategy_context.enter_or_assert_strategy(
        self._strategy):
      if distribution_strategy_context.get_replica_context() is not None:
        raise ValueError("next(iterator) should be called from outside of "
                         "replica_fn. e.g. strategy.run(replica_fn, "
                         "args=(next(iterator),))")

    if not self._enable_get_next_as_optional:
      return self._get_next_no_partial_batch_handling(name)

    optional_list = []
    for i, worker in enumerate(self._input_workers.worker_devices):
      with ops.device(worker):
        optional_list.append(self._iterators[i].get_next_as_optional_list())
    num_replicas_with_values = _calculate_replicas_with_values(
        self._strategy, self._input_workers, optional_list)

    def _value_or_dummy():
      value_list = _get_value_or_dummy(
          self._input_workers, optional_list, produce_dummy=True)
      return _create_per_replica(value_list, self._strategy)

    def _eof():
      # Optional.get_value raises InvalidArgumentError when there's no value,
      # so we need to call GetNext to raise EOFError.
      return self._get_next_no_partial_batch_handling()

    return control_flow_ops.cond(
        num_replicas_with_values > 0, _value_or_dummy, _eof, strict=True)

  def _get_next_no_partial_batch_handling(self, name=None):
    replicas = []
    for i, worker in enumerate(self._input_workers.worker_devices):
      if name is not None:
        d = tf_device.DeviceSpec.from_string(worker)
        new_name = "%s_%s_%d" % (name, d.job, d.task)
      else:
        new_name = None
      with ops.device(worker):
        # Make `replicas` a flat list of values across all replicas.
        replicas.extend(self._iterators[i].get_next_as_list(new_name))
    return _create_per_replica(replicas, self._strategy)


class DistributedDatasetAndIteratorSpec(type_spec.TypeSpec):
  """Common Type specification for `DistributedDataset and DistributedDatasetsFromFunction."""

  __slots__ = [
      "_input_workers", "_element_spec", "_strategy", "_cardinality",
      "_enable_get_next_as_optional", "_options", "_canonicalize_devices"
  ]

  def __init__(self,
               input_workers,
               element_spec,
               strategy,
               options,
               cardinality=cardinality_lib.UNKNOWN,
               enable_get_next_as_optional=None):
    # We don't want to allow deserialization of this class because we don't
    # serialize the strategy object. Currently the only places where
    # _deserialize is called is when we save/restore using SavedModels.
    if isinstance(input_workers, tuple):
      raise NotImplementedError("DistributedIteratorSpec does not have support "
                                "for deserialization.")
    else:
      self._input_workers = input_workers
      self._element_spec = element_spec
      self._strategy = strategy
      self._cardinality = cardinality
      self._enable_get_next_as_optional = enable_get_next_as_optional
      self._options = options
      if self._strategy:
        self._canonicalize_devices = getattr(self._strategy,
                                             "_canonicalize_devices", True)
      else:
        self._canonicalize_devices = True

  def _serialize(self):
    # We cannot serialize the strategy object so we convert it to an id that we
    # can use for comparison.
    return (self._input_workers.serialize(), self._element_spec,
            id(self._strategy), id(self._options))

  def _deserialize(self):
    raise ValueError(
        f"Deserialization is currently unsupported for {type(self)}.")

  def sanity_check_type(self, other):
    """Returns the most specific TypeSpec compatible with `self` and `other`.

    Args:
      other: A `TypeSpec`.

    Raises:
      ValueError: If there is no TypeSpec that is compatible with both `self`
        and `other`.
    """
    # pylint: disable=protected-access
    if type(self) is not type(other):
      raise ValueError("No TypeSpec is compatible with both %s and %s" %
                       (self, other))
    if self._input_workers.serialize() != other._input_workers.serialize():
      raise ValueError("_input_workers is not compatible with both %s "
                       "and %s" % (self, other))
    if self._strategy is not other._strategy:
      raise ValueError("tf.distribute strategy is not compatible with both %s "
                       "and %s" % (self, other))


class DistributedIteratorSpec(DistributedDatasetAndIteratorSpec):
  """Type specification for `DistributedIterator`."""

  @property
  def value_type(self):
    return DistributedIterator

  # Overriding this method so that we can merge and reconstruct the spec object
  def most_specific_compatible_type(self, other):
    """Returns the most specific TypeSpec compatible with `self` and `other`.

    Args:
      other: A `TypeSpec`.

    Raises:
      ValueError: If there is no TypeSpec that is compatible with both `self`
        and `other`.
    """
    # pylint: disable=protected-access
    self.sanity_check_type(other)
    element_spec = nest.map_structure(
        lambda a, b: a.most_specific_compatible_type(b), self._element_spec,
        other._element_spec)
    return DistributedIteratorSpec(
        self._input_workers,
        element_spec,
        self._strategy,
        self._options,
        cardinality=self._cardinality,
        enable_get_next_as_optional=self._enable_get_next_as_optional)

  @property
  def _component_specs(self):
    specs = []
    worker_device_pairs = self._input_workers._worker_device_pairs  # pylint: disable=protected-access

    for i, (input_device, compute_devices) in enumerate(worker_device_pairs):
      element_spec = nest.map_structure(
          functools.partial(_replace_per_replica_spec, i=i), self._element_spec)
      specs.append(
          _SingleWorkerDatasetIteratorSpec(input_device, compute_devices,
                                           element_spec, self._options,
                                           self._canonicalize_devices))
    return specs

  def _to_components(self, value):
    return value._iterators  # pylint: disable=protected-access

  def _from_components(self, components):
    return DistributedIterator(
        input_workers=self._input_workers,
        iterators=None,
        components=components,
        element_spec=self._element_spec,
        strategy=self._strategy,
        cardinality=self._cardinality,
        enable_get_next_as_optional=self._enable_get_next_as_optional,
        options=self._options)

  @staticmethod
  def from_value(value):
    # pylint: disable=protected-access
    return DistributedIteratorSpec(
        value._input_workers,
        value._element_spec,
        value._strategy,
        value._options,
        cardinality=value._cardinality,
        enable_get_next_as_optional=value._enable_get_next_as_optional)

  def _with_tensor_ranks_only(self):
    element_spec = nest.map_structure(
        lambda s: s._with_tensor_ranks_only(),  # pylint: disable=protected-access
        self._element_spec)
    return DistributedIteratorSpec(
        self._input_workers,
        element_spec,
        self._strategy,
        self._options,
        cardinality=self._cardinality,
        enable_get_next_as_optional=self._enable_get_next_as_optional)


class DistributedIterator(DistributedIteratorBase,
                          composite_tensor.CompositeTensor):
  """Input Iterator for a distributed dataset."""

  def __init__(self,
               input_workers=None,
               iterators=None,
               strategy=None,
               components=None,
               element_spec=None,
               cardinality=cardinality_lib.UNKNOWN,
               enable_get_next_as_optional=False,
               options=None):
    if input_workers is None:
      raise ValueError("`input_workers` should be "
                       "provided.")

    error_message = ("Either `input_workers` or "
                     "both `components` and `element_spec` need to be "
                     "provided.")
    self._options = options

    if iterators is None:
      if (components is None or element_spec is None):
        raise ValueError(error_message)
      self._element_spec = element_spec
      self._input_workers = input_workers
      self._iterators = components
      self._strategy = strategy
      self._cardinality = cardinality
      self._enable_get_next_as_optional = enable_get_next_as_optional
    else:
      if (components is not None and element_spec is not None):
        raise ValueError(error_message)

      super(DistributedIterator,
            self).__init__(input_workers, iterators, strategy, cardinality,
                           enable_get_next_as_optional)

  @property
  def element_spec(self):
    # When partial batch handling is enabled, always set the batch dimension to
    # None, otherwise we just follow element_spec of the underlying dataset
    # (whose batch dimension may also be None). This is because with partial
    # batching handling we could always produce empty batches.
    if (self._enable_get_next_as_optional and
        self._strategy.extended._in_multi_worker_mode()):  # pylint: disable=protected-access
      return nest.map_structure(
          _rebatch_as_dynamic, self._element_spec, expand_composites=False)
    return self._element_spec

  @property
  def _type_spec(self):
    # Note that we use actual element_spec instead of the rebatched-as-dynamic
    # one to create DistributedIteratorSpec, to be consistent with the
    # underlying iterators' specs.
    return DistributedIteratorSpec(self._input_workers, self._element_spec,
                                   self._strategy,
                                   self._options,
                                   self._cardinality,
                                   self._enable_get_next_as_optional)


class _IterableInput(DistributedDatasetInterface):
  """Base class for iterable inputs for distribution strategies."""

  # pylint: disable=super-init-not-called
  def __init__(self, input_workers):
    assert isinstance(input_workers, InputWorkers)
    self._input_workers = input_workers

  def __iter__(self):
    raise NotImplementedError("must be implemented in descendants")

  def reduce(self, initial_state, reduce_fn):
    """Execute a `reduce_fn` over all the elements of the input."""
    iterator = iter(self)
    optional_data = iterator.get_next_as_optional()

    def cond(optional_data, state):
      del state  # Unused.
      return optional_data.has_value()

    def loop_body(optional_data, state):
      """Executes `reduce_fn` in a loop till the dataset is empty."""
      state = reduce_fn(state, optional_data.get_value())
      optional_data = iterator.get_next_as_optional()
      return optional_data, state

    optional_data, final_state = control_flow_ops.while_loop(
        cond,
        loop_body, [optional_data, initial_state],
        parallel_iterations=1,
        return_same_structure=True)
    return final_state


class DistributedDatasetSpec(DistributedDatasetAndIteratorSpec):
  """Type specification for `DistributedDataset."""

  @property
  def value_type(self):
    return DistributedDataset

  # Overriding this method so that we can merge and reconstruct the spec object
  def most_specific_compatible_type(self, other):
    """Returns the most specific TypeSpec compatible with `self` and `other`.

    Args:
      other: A `TypeSpec`.

    Raises:
      ValueError: If there is no TypeSpec that is compatible with both `self`
        and `other`.
    """
    # pylint: disable=protected-access
    self.sanity_check_type(other)
    element_spec = nest.map_structure(
        lambda a, b: a.most_specific_compatible_type(b), self._element_spec,
        other._element_spec)
    return DistributedDatasetSpec(
        self._input_workers,
        element_spec,
        self._strategy,
        self._options,
        enable_get_next_as_optional=self._enable_get_next_as_optional)

  @property
  def _component_specs(self):
    specs = []
    worker_device_pairs = self._input_workers._worker_device_pairs  # pylint: disable=protected-access

    for i, _ in enumerate(worker_device_pairs):
      element_spec = nest.map_structure(
          functools.partial(_replace_per_replica_spec, i=i), self._element_spec)
      specs.append(dataset_ops.DatasetSpec(element_spec))
    return specs

  def _to_components(self, value):
    return value._cloned_datasets  # pylint: disable=protected-access

  def _from_components(self, components):
    return DistributedDataset(
        input_workers=self._input_workers,
        strategy=self._strategy,
        components=components,
        element_spec=self._element_spec,
        enable_get_next_as_optional=self._enable_get_next_as_optional,
        options=self._options)

  @staticmethod
  def from_value(value):
    # pylint: disable=protected-access
    return DistributedDatasetSpec(
        value._input_workers,
        value._element_spec,
        value._strategy,
        value._options,
        enable_get_next_as_optional=value._enable_get_next_as_optional)
    # pylint: enable=protected-access


class DistributedDataset(_IterableInput, composite_tensor.CompositeTensor):
  """Distributed dataset that supports prefetching to multiple devices."""

  def __init__(self,
               input_workers,
               strategy,
               dataset=None,
               num_replicas_in_sync=None,
               input_context=None,
               components=None,
               element_spec=None,
               enable_get_next_as_optional=None,
               build=True,
               options=None):
    """Distribute the dataset on all workers.

    If `num_replicas_in_sync` is not None, we split each batch of the dataset
    into `num_replicas_in_sync` smaller batches, to be distributed among that
    worker's replicas, so that the batch size for a global step (across all
    workers and replicas) is as expected.

    Args:
      input_workers: an `InputWorkers` object.
      strategy: a `tf.distribute.Strategy` object, used to run all-reduce to
        handle last partial batch.
      dataset: `tf.data.Dataset` that will be used as the input source. Either
        dataset or components field should be passed when constructing
        DistributedDataset. Use this when contructing DistributedDataset from a
        new `tf.data.Dataset`. Use components when constructing using
        DistributedDatasetSpec.
      num_replicas_in_sync: Optional integer. If this is not None, the value
        is used to decide how to rebatch datasets into smaller batches so that
        the total batch size for each step (across all workers and replicas)
        adds up to `dataset`'s batch size.
      input_context: `InputContext` for sharding. Only pass this in for between
        graph multi-worker cases where there is only one `input_worker`. In
        these cases, we will shard based on the `input_pipeline_id` and
        `num_input_pipelines` in the `InputContext`.
      components: datasets when DistributedDataset is constructed from
        DistributedDatasetSpec. Either field dataset or components should be
        passed.
      element_spec: element spec for DistributedDataset when constructing from
        DistributedDatasetSpec. This will be used to set the element_spec for
        DistributedDataset and verified against element_spec from components.
      enable_get_next_as_optional: this is required when components is passed
        instead of dataset.
      build: whether to build underlying datasets when this object is created.
        This is only useful for `ParameterServerStrategy` now.
      options: `tf.distribute.InputOptions` used to control options on how this
        dataset is distributed.
    """
    super(DistributedDataset, self).__init__(input_workers=input_workers)
    if input_workers is None or strategy is None:
      raise ValueError("input_workers and strategy are required arguments")
    if dataset is not None and components is not None:
      raise ValueError("Only one of dataset or components should be present")
    if dataset is None and components is None:
      raise ValueError("At least one of dataset or components should be passed")

    self._input_workers = input_workers
    self._strategy = strategy
    self._options = options
    self._input_context = input_context
    self._num_replicas_in_sync = num_replicas_in_sync

    if dataset is not None:
      self._original_dataset = dataset
      self._built = False
      if build:
        self.build()
    else:
      if not build:
        raise ValueError(
            "When constructing DistributedDataset with components, build "
            "should not be False. This is an internal error. Please file a "
            "bug.")
      if enable_get_next_as_optional is None:
        raise ValueError(
            "When constructing DistributedDataset with components, " +
            "enable_get_next_as_optional should also be passed")
      self._cloned_datasets = components
      self._cardinality = _cardinality(self._cloned_datasets[0])
      self._enable_get_next_as_optional = enable_get_next_as_optional

      assert element_spec is not None
      if element_spec != _create_distributed_tensor_spec(
          self._strategy, self._cloned_datasets[0].element_spec):
        raise ValueError("Mismatched element_spec from the passed components")
      self._element_spec = element_spec

      self._built = True

  def build(self, dataset_to_replace=None):
    assert not self._built
    dataset = dataset_to_replace or self._original_dataset
    self._cardinality = _cardinality(dataset)
    self._enable_get_next_as_optional = _enable_get_next_as_optional(
        self._strategy, dataset, self._cardinality)
    self._create_cloned_datasets_from_dataset(dataset, self._input_context,
                                              self._input_workers,
                                              self._strategy,
                                              self._num_replicas_in_sync)
    self._element_spec = _create_distributed_tensor_spec(
        self._strategy, self._cloned_datasets[0].element_spec)
    self._built = True

  @property
  def cardinality(self):
    if not self._built:
      raise ValueError(
          "Cannot get the cardinality of a dataset that is not built")
    return self._cardinality

  def _create_cloned_datasets_from_dataset(self, dataset, input_context,
                                           input_workers, strategy,
                                           num_replicas_in_sync):
    # We clone and shard the dataset on each worker. The current setup tries to
    # shard the dataset by files if possible so that each worker sees a
    # different subset of files. If that is not possible, will attempt to shard
    # the final input such that each worker will run the entire preprocessing
    # pipeline and only receive its own shard of the dataset.

    # Additionally, we rebatch the dataset on each worker into
    # `num_replicas_in_sync` smaller batches to be distributed among that
    # worker's replicas, so that the batch size for a global step (across all
    # workers and replicas) adds up to the original dataset's batch size.
    if num_replicas_in_sync is not None:
      num_workers = input_context.num_input_pipelines if input_context else len(
          input_workers.worker_devices)
      rebatch_fn = self._make_rebatch_fn(dataset, num_workers,
                                         num_replicas_in_sync)
    else:
      rebatch_fn = None
    self._cloned_datasets = []
    if input_context:
      # Between-graph where we rely on the input_context for sharding
      assert input_workers.num_workers == 1
      if rebatch_fn is not None:
        dataset = rebatch_fn(dataset, input_context.input_pipeline_id)
      dataset = input_ops.auto_shard_dataset(dataset,
                                             input_context.num_input_pipelines,
                                             input_context.input_pipeline_id,
                                             num_replicas_in_sync)
      self._cloned_datasets.append(dataset)
    else:
      replicated_ds = distribute.replicate(dataset,
                                           input_workers.worker_devices)
      for i, worker in enumerate(input_workers.worker_devices):
        with ops.device(worker):
          cloned_dataset = replicated_ds[worker]
          if rebatch_fn is not None:
            cloned_dataset = rebatch_fn(cloned_dataset, i)
          cloned_dataset = input_ops.auto_shard_dataset(
              cloned_dataset, len(input_workers.worker_devices), i,
              num_replicas_in_sync)
          self._cloned_datasets.append(cloned_dataset)

  def _make_rebatch_fn(self, dataset, num_workers, num_replicas_in_sync):
    """Returns a callable that rebatches the input dataset.

    Args:
      dataset: A `tf.data.Dataset` representing the dataset to be distributed.
      num_workers: An integer representing the number of workers to distribute
        `dataset` among.
      num_replicas_in_sync: An integer representing the number of replicas in
        sync across all workers.
    """
    if num_replicas_in_sync % num_workers:
      raise ValueError(
          "tf.distribute expects every worker to have the same number of "
          "replicas. However, encountered `num_replicas_in_sync` ({}) that "
          "cannot be divided by `num_workers` ({})".format(
              num_replicas_in_sync, num_workers))

    num_replicas_per_worker = num_replicas_in_sync // num_workers
    with ops.colocate_with(dataset._variant_tensor):  # pylint: disable=protected-access
      batch_size = distribute.compute_batch_size(dataset)

    def rebatch_fn(dataset, worker_index):
      try:
        # pylint: disable=protected-access
        def apply_rebatch():
          batch_sizes = distribute.batch_sizes_for_worker(
              batch_size, num_workers, num_replicas_per_worker, worker_index)
          return distribute._RebatchDataset(
              dataset, batch_sizes).prefetch(num_replicas_per_worker)

        def apply_legacy_rebatch():
          return distribute._LegacyRebatchDataset(
              dataset, num_replicas_in_sync).prefetch(num_replicas_per_worker)

        with ops.colocate_with(dataset._variant_tensor):
          return control_flow_ops.cond(
              math_ops.not_equal(batch_size, -1),
              true_fn=apply_rebatch,
              false_fn=apply_legacy_rebatch)
      except errors.InvalidArgumentError as e:
        if "without encountering a batch" in str(e):
          six.reraise(
              ValueError,
              ValueError(
                  "Call the `batch` method on the input Dataset in order to be "
                  "able to split your input across {} replicas.\n Please see "
                  "the tf.distribute.Strategy guide. {}".format(
                      num_replicas_in_sync, e)),
              sys.exc_info()[2])
        else:
          raise

    return rebatch_fn

  def __iter__(self):
    if not (context.executing_eagerly() or
            ops.get_default_graph().building_function):
      raise RuntimeError("__iter__() is only supported inside of tf.function "
                         "or when eager execution is enabled.")
    if not self._built:
      raise ValueError("To use this dataset, you need to pass this dataset to "
                       "ClusterCoordinator.create_per_worker_dataset.")

    canonicalize_devices = getattr(self._strategy, "_canonicalize_devices",
                                   True)

    worker_iterators = _create_iterators_per_worker(
        self._cloned_datasets,
        self._input_workers,
        options=self._options,
        canonicalize_devices=canonicalize_devices)
    iterator = DistributedIterator(
        self._input_workers,
        worker_iterators,
        self._strategy,
        cardinality=self._cardinality,
        enable_get_next_as_optional=self._enable_get_next_as_optional,
        options=self._options)
    iterator._element_spec = self._element_spec  # pylint: disable=protected-access

    # When async eager is enabled, sometimes the iterator may not finish
    # initialization before passing to a multi device function, add a sync point
    # here to make sure all underlying iterators are initialized.
    if context.executing_eagerly():
      context.async_wait()

    return iterator

  @property
  def element_spec(self):
    """The type specification of an element of this dataset."""
    # When partial batch handling is enabled, always set the batch dimension to
    # None, otherwise we just follow element_spec of the underlying dataset
    # (whose batch dimension may also be None). This is because with partial
    # batching handling we could always produce empty batches.
    if (self._enable_get_next_as_optional and
        self._strategy.extended._in_multi_worker_mode()):  # pylint: disable=protected-access
      return nest.map_structure(
          _rebatch_as_dynamic, self._element_spec, expand_composites=False)
    return self._element_spec

  @property
  def _type_spec(self):
    return DistributedDatasetSpec(
        self._input_workers,
        self._element_spec,
        self._strategy,
        self._options,
        enable_get_next_as_optional=self._enable_get_next_as_optional)


class DistributedDatasetsFromFunctionSpec(DistributedDatasetAndIteratorSpec):
  """Type specification for `DistributedDatasetsFromFunction."""

  @property
  def value_type(self):
    return DistributedDatasetsFromFunction

  @property
  def _component_specs(self):
    specs = []
    worker_device_pairs = self._input_workers._worker_device_pairs  # pylint: disable=protected-access

    for i, _ in enumerate(worker_device_pairs):
      element_spec = nest.map_structure(
          functools.partial(_replace_per_replica_spec, i=i), self._element_spec)
      specs.append(dataset_ops.DatasetSpec(element_spec))
    return specs

  # Overriding this method so that we can merge and reconstruct the spec object
  def most_specific_compatible_type(self, other):
    """Returns the most specific TypeSpec compatible with `self` and `other`.

    Args:
      other: A `TypeSpec`.

    Raises:
      ValueError: If there is no TypeSpec that is compatible with both `self`
        and `other`.
    """
    # pylint: disable=protected-access
    self.sanity_check_type(other)
    element_spec = nest.map_structure(
        lambda a, b: a.most_specific_compatible_type(b), self._element_spec,
        other._element_spec)  # pylint: disable=protected-access
    return DistributedDatasetsFromFunctionSpec(self._input_workers,
                                               element_spec, self._strategy,
                                               self._options)

  def _to_components(self, value):
    return value._datasets  # pylint: disable=protected-access

  def _from_components(self, components):
    return DistributedDatasetsFromFunction(
        input_workers=self._input_workers,
        strategy=self._strategy,
        components=components,
        element_spec=self._element_spec,
        options=self._options)

  @staticmethod
  def from_value(value):
    # pylint: disable=protected-access
    return DistributedDatasetsFromFunctionSpec(
        input_workers=value._input_workers,
        element_spec=value._element_spec,
        strategy=value._strategy,
        options=value._options)


# TODO(priyag): Add other replication modes.
class DistributedDatasetsFromFunction(_IterableInput,
                                      composite_tensor.CompositeTensor):
  """Inputs created from dataset function."""

  def __init__(self,
               input_workers,
               strategy,
               input_contexts=None,
               dataset_fn=None,
               options=None,
               components=None,
               element_spec=None,
               build=True):
    """Makes an iterable from datasets created by the given function.

    Args:
      input_workers: an `InputWorkers` object.
      strategy: a `tf.distribute.Strategy` object, used to run all-reduce to
        handle last partial batch.
      input_contexts: A list of `InputContext` instances to be passed to call(s)
        to `dataset_fn`. Length and order should match worker order in
        `worker_device_pairs`.
      dataset_fn: A function that returns a `Dataset` given an `InputContext`.
        Either dataset_fn or components should be passed to construct
        DistributedDatasetsFromFunction. Use this when constructing
        DistributedDataset using a function. Use components when constructing
        using DistributedDatasetsFromFunctionSpec.
      options: `tf.distribute.InputOptions` used to control options on how this
        dataset is distributed.
      components: datasets when DistributedDatasetsFromFunction is constructed
        from DistributedDatasetsFromFunctionSpec. Only one of dataset or
        components should be passed.
      element_spec: element spec for DistributedDataset when constructing from
        DistributedDatasetSpec. This will be used to set the element_spec for
        DistributedDatasetsFromFunctionSpec and verified against element_spec
        from components.
      build: whether to build underlying datasets when this object is created.
        This is only useful for `ParameterServerStrategy` now.
    """
    super(DistributedDatasetsFromFunction, self).__init__(
        input_workers=input_workers)
    self._input_workers = input_workers
    self._strategy = strategy
    self._options = options
    if dataset_fn is not None and components is not None:
      raise ValueError("Only one of dataset_fn or components should be set")
    if dataset_fn is None and components is None:
      raise ValueError("At least one of dataset_fn or components should be set")

    if dataset_fn is not None:
      if input_workers.num_workers != len(input_contexts):
        raise ValueError(
            "Number of input workers (%d) is not same as number of "
            "input_contexts (%d)" %
            (input_workers.num_workers, len(input_contexts)))
      self._input_contexts = input_contexts
      self._dataset_fn = dataset_fn
      self._built = False
      if build:
        self.build()
    else:
      if element_spec is None:
        raise ValueError(
            "element_spec should also be passed when passing components")
      if not build:
        raise ValueError(
            "When constructing DistributedDatasetFromFunction with components, "
            "build should not be False. This is an internal error. Please file "
            "a bug.")
      self._element_spec = element_spec
      self._datasets = components
      self._built = True
      self._cardinality = _cardinality(self._datasets[0])
      self._enable_get_next_as_optional = _enable_get_next_as_optional(
          self._strategy, self._datasets[0], self._cardinality)

  def build(self):
    assert not self._built
    self._datasets, element_spec = (
        _create_datasets_from_function_with_input_context(
            self._input_contexts, self._input_workers, self._dataset_fn))
    self._element_spec = _create_distributed_tensor_spec(
        self._strategy, element_spec)
    self._cardinality = _cardinality(self._datasets[0])
    self._enable_get_next_as_optional = _enable_get_next_as_optional(
        self._strategy, self._datasets[0], self._cardinality)
    self._built = True

  @property
  def cardinality(self):
    if not self._built:
      raise ValueError(
          "Cannot get the cardinality of a dataset that is not built")
    return self._cardinality

  def __iter__(self):
    if not (ops.executing_eagerly_outside_functions() or
            ops.get_default_graph().building_function):
      raise RuntimeError("__iter__() is only supported inside of tf.function "
                         "or when eager execution is enabled.")

    if not self._built:
      raise ValueError("You need to use this dataset in "
                       "ClusterCoordinator.create_per_worker_dataset.")

    canonicalize_devices = getattr(self._strategy, "_canonicalize_devices",
                                   True)

    iterators = _create_iterators_per_worker(
        self._datasets,
        self._input_workers,
        options=self._options,
        canonicalize_devices=canonicalize_devices)
    iterator = DistributedIterator(
        input_workers=self._input_workers,
        iterators=iterators,
        strategy=self._strategy,
        cardinality=self._cardinality,
        enable_get_next_as_optional=self._enable_get_next_as_optional,
        options=self._options)
    iterator._element_spec = self._element_spec  # pylint: disable=protected-access

    # When async eager is enabled, sometimes the iterator may not finish
    # initialization before passing to a multi device function, add a sync
    # point here to make sure all underlying iterators are initialized.
    if context.executing_eagerly():
      context.async_wait()

    return iterator

  @property
  def element_spec(self):
    """The type specification of an element of this dataset."""
    # When partial batch handling is enabled, always set the batch dimension to
    # None, otherwise we just follow element_spec of the underlying dataset
    # (whose batch dimension may also be None). This is because with partial
    # batching handling we could always produce empty batches.
    if (self._enable_get_next_as_optional and
        self._strategy.extended._in_multi_worker_mode()):  # pylint: disable=protected-access
      return nest.map_structure(
          _rebatch_as_dynamic, self._element_spec, expand_composites=False)
    return self._element_spec

  @property
  def _type_spec(self):
    return DistributedDatasetsFromFunctionSpec(self._input_workers,
                                               self._element_spec,
                                               self._strategy, self._options)


def _dummy_tensor_fn(value_structure):
  """A function to create dummy tensors from `value_structure`."""

  def create_dummy_tensor(spec):
    """Create a dummy tensor with possible batch dimensions set to 0."""
    if hasattr(spec, "_create_empty_value"):
      # Type spec may overwrite default dummy values behavior by declaring the
      # `_create_empty_value(self)` method. This method must return a value
      # compatible with the type spec with batch dimensions set to 0 or fail if
      # such a value does not exist. This allows a composite tensor to customize
      # dummy values creation as, in general, its dummy value is not composed
      # from dummy components (e.g. `row_splits` tensor of a RaggedTensor is
      # never allowed to be empty). See b/183969859 for more discussions.
      # TODO(b/186079336): reconsider CompositeTensor support.
      return spec._create_empty_value()  # pylint: disable=protected-access

    if isinstance(spec, ragged_tensor.RaggedTensorSpec):
      # Splice out the ragged dimensions.
      # pylint: disable=protected-access
      feature_shape = spec._shape[:1].concatenate(
          spec._shape[(1 + spec._ragged_rank):])
      feature_type = spec._dtype
      # pylint: enable=protected-access
    else:
      feature_shape = spec.shape
      feature_type = spec.dtype
    # Ideally we should set the batch dimension to 0, however as in
    # DistributionStrategy we don't know the batch dimension, we try to
    # guess it as much as possible. If the feature has unknown dimensions, we
    # will set them to 0. If the feature shape is already static, we guess the
    # first dimension as batch dimension and set it to 0.
    dims = ([dim if dim is not None else 0 for dim in feature_shape.as_list()]
            if feature_shape else [])
    if dims and (isinstance(spec, ragged_tensor.RaggedTensorSpec) or
                 feature_shape.is_fully_defined()):
      dims[0] = tensor_shape.Dimension(0)

    if isinstance(spec, sparse_tensor.SparseTensorSpec):
      return sparse_tensor.SparseTensor(
          values=array_ops.zeros(0, feature_type),
          indices=array_ops.zeros((0, len(dims)), dtypes.int64),
          dense_shape=dims)

    # Create the dummy tensor.
    dummy_tensor = array_ops.zeros(tensor_shape.TensorShape(dims), feature_type)
    if isinstance(spec, ragged_tensor.RaggedTensorSpec):
      # Reinsert the ragged dimensions with size 0.
      # pylint: disable=protected-access
      row_splits = array_ops.zeros(1, spec._row_splits_dtype)
      dummy_tensor = ragged_tensor.RaggedTensor.from_nested_row_splits(
          dummy_tensor, (row_splits,) * spec._ragged_rank, validate=False)
      # pylint: enable=protected-access
    return dummy_tensor

  return nest.map_structure(create_dummy_tensor, value_structure)


def _get_value_or_dummy(input_workers, optional_list, produce_dummy):
  """Returns the value of the optionals or dummy values.

  Args:
    input_workers: the `InputWorkers`.
    optional_list: a list of lists `tf.experimental.Optional`. The values from
      each compute device grouped by the input device.
    produce_dummy: a bool. Whether to produce dummy tensors when the optional
      doesn't have a value.

  Returns:
    A flatten list of Tensors.

  """
  value_list = []
  for i, worker in enumerate(input_workers.worker_devices):
    with ops.device(worker):
      devices = input_workers.compute_devices_for_worker(i)
      for j, device in enumerate(devices):
        with ops.device(device):
          if produce_dummy:
            # pylint: disable=cell-var-from-loop
            value_list.append(
                control_flow_ops.cond(
                    optional_list[i][j].has_value(),
                    lambda: optional_list[i][j].get_value(),  # pylint: disable=unnecessary-lambda
                    lambda: _dummy_tensor_fn(optional_list[i][j].element_spec),
                    strict=True,
                ))
            # pylint: enable=cell-var-from-loop
          else:
            value_list.append(optional_list[i][j].get_value())
  return value_list


class _SingleWorkerDatasetIteratorBase(object):
  """Iterator for a single `tf.data.Dataset`."""

  def __init__(self, dataset, worker, devices, options=None):
    """Create iterator for the `dataset` to fetch data to worker's `devices` .

    A `MultiDeviceIterator`  or `OwnedMultiDeviceIterator` is used to prefetch
    input to the devices on the given worker.

    Args:
      dataset: A `tf.data.Dataset` instance.
      worker: Worker on which ops should be created.
      devices: Distribute data from `dataset` to these devices.
      options: options.
    """
    self._dataset = dataset
    self._worker = worker
    self._devices = devices
    self._element_spec = dataset.element_spec
    self._options = options
    self._make_iterator()

  def _make_iterator(self):
    raise NotImplementedError("must be implemented in descendants")

  def _format_data_list_with_options(self, data_list):
    """Change the data in to a list type if required.

    The OwnedMultiDeviceIterator returns the list data type,
    while the PER_REPLICA iterator (when used with prefetch disabled)
    returns without the enclosed list. This is to fix the inconsistency.
    Args:
      data_list: data_list
    Returns:
      list
    """
    if (self._options and self._options.experimental_replication_mode ==
        InputReplicationMode.PER_REPLICA and
        not self._options.experimental_fetch_to_device):
      return [data_list]
    else:
      return data_list

  def get_next(self, device, name=None):
    """Get next element for the given device."""
    del name
    with ops.device(self._worker):
      if _should_use_multi_device_iterator(self._options):
        return self._iterator.get_next(device)
      else:
        return self._iterator.get_next()

  def get_next_as_list(self, name=None):
    """Get next element from the underlying iterator.

    Runs the iterator get_next() within a device scope. Since this doesn't use
    get_next_as_optional(), it is considerably faster than get_next_as_list(),
    but it raises EOFError if any of the device doesn't get any data.

    Args:
      name: not used.

    Returns:
      A list consisting of the next data from each device.
    """
    del name
    with ops.device(self._worker):
      return self._format_data_list_with_options(self._iterator.get_next())

  def get_next_as_optional_list(self):
    with ops.device(self._worker):
      return self._format_data_list_with_options(
          self._iterator.get_next_as_optional())


class _SingleWorkerDatasetIteratorSpec(type_spec.TypeSpec):
  """Type specification for `_SingleWorkerOwnedDatasetIterator`."""

  __slots__ = [
      "_worker", "_devices", "_element_spec", "_options",
      "_canonicalize_devices"
  ]

  def __init__(self, worker, devices, element_spec, options,
               canonicalize_devices=True):
    self._worker = worker
    if canonicalize_devices:
      self._devices = tuple(device_util.canonicalize(d) for d in devices)
    else:
      self._devices = tuple(
          device_util.canonicalize_without_job_and_task(d) for d in devices)
    self._element_spec = element_spec
    # `self._options` intentionally made not `None` for proper serialization.
    self._options = (options if options is not None else
                     distribute_lib.InputOptions())
    self._canonicalize_devices = canonicalize_devices

  @property
  def value_type(self):
    return _SingleWorkerOwnedDatasetIterator

  def _serialize(self):
    return (self._worker, self._devices, self._element_spec, self._options,
            self._canonicalize_devices)

  def _get_multi_device_iterator_spec(self, specs):
    device_scope = device_util.canonicalize(self._worker, device_util.current())
    host_device = device_util.get_host_for_device(device_scope)
    # source_device while creating iterator governs the worker device in
    # iterator spec.
    worker = host_device
    specs.append(
        multi_device_iterator_ops.MultiDeviceIteratorSpec(
            self._devices, worker, element_spec=self._element_spec))

  @property
  def _component_specs(self):
    specs = []
    if _should_use_multi_device_iterator(self._options):
      self._get_multi_device_iterator_spec(specs)
    else:
      specs.append(iterator_ops.IteratorSpec(element_spec=self._element_spec))
    return specs

  def _to_components(self, value):
    return [value._iterator]  # pylint: disable=protected-access

  def _from_components(self, components):
    return _SingleWorkerOwnedDatasetIterator(
        dataset=None,
        worker=self._worker,
        devices=self._devices,
        components=components,
        element_spec=self._element_spec,
        options=self._options,
        canonicalize_devices=self._canonicalize_devices)

  @staticmethod
  def from_value(value):
    # pylint: disable=protected-access
    return _SingleWorkerDatasetIteratorSpec(value._worker, value._devices,
                                            value._element_spec, value._options,
                                            value._canonicalize_devices)


class _SingleWorkerOwnedDatasetIterator(_SingleWorkerDatasetIteratorBase,
                                        composite_tensor.CompositeTensor):
  """Iterator for a DistributedDataset instance."""

  def __init__(self,
               dataset=None,
               worker=None,
               devices=None,
               components=None,
               element_spec=None,
               options=None,
               canonicalize_devices=None):
    """Create iterator for the `dataset` to fetch data to worker's `devices` .

    `OwnedMultiDeviceIterator` is used to prefetch input to the devices on the
    given worker. The lifetime of this iterator is tied to the encompassing
    python object. Once we go out of scope of the python object or return from
    a tf.function the underlying iterator resource is deleted.

    Args:
      dataset: A `tf.data.Dataset` instance.
      worker: Worker on which ops should be created.
      devices: Distribute data from `dataset` to these devices.
      components: Tensor components to construct the
        _SingleWorkerOwnedDatasetIterator from.
      element_spec: A nested structure of `TypeSpec` objects that represents the
      type specification of elements of the iterator.
      options: `tf.distribute.InputOptions` used to control options on how this
      dataset is distributed.
      canonicalize_devices: Whether to canonicalize devices for workers fully or
      partially. If False, it will partially canonicalize devices by removing
      job and task.
    """
    if worker is None or devices is None:
      raise ValueError("Both `worker` and `devices` should be provided")

    error_message = ("Either `dataset` or both `components` and `element_spec` "
                     "need to be provided.")

    self._options = options
    self._canonicalize_devices = canonicalize_devices
    if dataset is None:
      if (components is None or element_spec is None):
        raise ValueError(error_message)
      self._element_spec = element_spec
      self._worker = worker
      self._devices = devices
      self._iterator = components[0]
    else:
      if (components is not None or element_spec is not None):
        raise ValueError(error_message)
      super(_SingleWorkerOwnedDatasetIterator,
            self).__init__(dataset, worker, devices, self._options)

  def _create_owned_multi_device_iterator(self):
    # If the worker devices are already canonicalized, canonicalizing again
    # would have no impact.
    # For strategies running on remote workers such as PS Strategy, the device
    # scope will be derived from current worker, if used under init_scope().
    device_scope = device_util.canonicalize(self._worker,
                                            device_util.current())
    host_device = device_util.get_host_for_device(device_scope)
    with ops.device(device_scope):
      if self._options is not None:
        self._iterator = multi_device_iterator_ops.OwnedMultiDeviceIterator(
            self._dataset,
            self._devices,
            source_device=host_device,
            max_buffer_size=self._options
            .experimental_per_replica_buffer_size,
            prefetch_buffer_size=self._options
            .experimental_per_replica_buffer_size)
      else:
        self._iterator = multi_device_iterator_ops.OwnedMultiDeviceIterator(
            self._dataset, self._devices, source_device=host_device)

  def _make_iterator(self):
    """Make appropriate iterator on the dataset."""
    if not self._worker:
      raise ValueError("Worker device must be specified when creating an "
                       "owned iterator.")
    if _should_use_multi_device_iterator(self._options):
      self._create_owned_multi_device_iterator()
    else:
      with ops.device(self._worker):
        self._iterator = iter(self._dataset)

  @property
  def element_spec(self):
    return self._element_spec

  @property
  def _type_spec(self):
    return _SingleWorkerDatasetIteratorSpec(self._worker, self._devices,
                                            self._element_spec, self._options,
                                            self._canonicalize_devices)

  @property
  def output_classes(self):
    """Returns the class of each component of an element of this iterator.

    The expected values are `tf.Tensor` and `tf.SparseTensor`.

    Returns:
      A nested structure of Python `type` objects corresponding to each
      component of an element of this dataset.
    """
    return nest.map_structure(
        lambda component_spec: component_spec._to_legacy_output_classes(),  # pylint: disable=protected-access
        self._element_spec)

  @property
  def output_shapes(self):
    """Returns the shape of each component of an element of this iterator.

    Returns:
      A nested structure of `tf.TensorShape` objects corresponding to each
      component of an element of this dataset.
    """
    return nest.map_structure(
        lambda component_spec: component_spec._to_legacy_output_shapes(),  # pylint: disable=protected-access
        self._element_spec)

  @property
  def output_types(self):
    """Returns the type of each component of an element of this iterator.

    Returns:
      A nested structure of `tf.DType` objects corresponding to each component
      of an element of this dataset.
    """
    return nest.map_structure(
        lambda component_spec: component_spec._to_legacy_output_types(),  # pylint: disable=protected-access
        self._element_spec)


def _create_iterators_per_worker(worker_datasets,
                                 input_workers,
                                 options=None,
                                 canonicalize_devices=False):
  """Create a multidevice iterator on each of the workers."""
  assert isinstance(input_workers, InputWorkers)
  assert len(worker_datasets) == len(input_workers.worker_devices)
  iterators = []
  for i, worker in enumerate(input_workers.worker_devices):
    with ops.device(worker):
      worker_devices = input_workers.compute_devices_for_worker(i)
      iterator = _SingleWorkerOwnedDatasetIterator(
          dataset=worker_datasets[i],
          worker=worker,
          devices=worker_devices,
          options=options,
          canonicalize_devices=canonicalize_devices)
      iterators.append(iterator)
  return iterators


def _create_datasets_from_function_with_input_context(input_contexts,
                                                      input_workers,
                                                      dataset_fn):
  """Create device datasets per worker given a dataset function."""
  datasets = []
  for i, ctx in enumerate(input_contexts):
    worker = input_workers.worker_devices[i]
    with ops.device(worker):
      dataset = dataset_fn(ctx)
      datasets.append(dataset)
  return datasets, dataset.element_spec


# TODO(sourabhbajaj): Remove this in lieu of distributed datasets
def _get_batched_dataset(d):
  """Get the batched dataset from `d`."""
  # pylint: disable=protected-access
  if isinstance(d, dataset_ops.DatasetV1Adapter):
    d = d._dataset

  if isinstance(d, (dataset_ops.BatchDataset, batching._MapAndBatchDataset)):
    return d
  elif isinstance(d, (dataset_ops.PrefetchDataset,
                      dataset_ops._OptionsDataset)):
    return _get_batched_dataset(d._input_dataset)

  raise ValueError(
      "Unable to get batched dataset from the input dataset. `batch` "
      "`map_and_batch` need to be the last operations on the dataset. "
      "The batch operations can be followed by a prefetch.")


def _get_batched_dataset_attributes(d):
  """Get `batch_size`, `drop_remainder` of dataset."""
  # pylint: disable=protected-access
  assert isinstance(d,
                    (dataset_ops.BatchDataset, batching._MapAndBatchDataset))
  if isinstance(d, dataset_ops.BatchDataset):
    batch_size = d._batch_size
    drop_remainder = d._drop_remainder
  elif isinstance(d, batching._MapAndBatchDataset):
    batch_size = d._batch_size_t
    drop_remainder = d._drop_remainder_t
  # pylint: enable=protected-access

  if tensor_util.is_tf_type(batch_size):
    batch_size = tensor_util.constant_value(batch_size)

  if tensor_util.is_tf_type(drop_remainder):
    drop_remainder = tensor_util.constant_value(drop_remainder)

  return batch_size, drop_remainder


# TODO(sourabhbajaj): Remove this in lieu of distributed datasets
def _get_dataset_attributes(dataset):
  """Get the underlying attributes from the dataset object."""
  # pylint: disable=protected-access

  # First, get batch_size and drop_remainder from the dataset. We need
  # to walk back the dataset creation process and find the batched version in
  # order to get the attributes.
  batched_dataset = _get_batched_dataset(dataset)
  batch_size, drop_remainder = _get_batched_dataset_attributes(batched_dataset)

  # Second, prefetch buffer should be get from the original dataset.
  prefetch_buffer = None
  if isinstance(dataset, dataset_ops.PrefetchDataset):
    prefetch_buffer = dataset._buffer_size
  elif (isinstance(dataset, dataset_ops.DatasetV1Adapter)
        and isinstance(dataset._dataset, dataset_ops.PrefetchDataset)):
    prefetch_buffer = dataset._dataset._buffer_size

  return batch_size, drop_remainder, prefetch_buffer


def _should_use_multi_device_iterator(options):
  """Determine whether to use multi_device_iterator_ops."""
  if (options is None or
      options.experimental_replication_mode == InputReplicationMode.PER_WORKER
      or
      (options.experimental_replication_mode == InputReplicationMode.PER_REPLICA
       and options.experimental_fetch_to_device)):
    return True
  return False


class MultiStepContext(object):
  """A context object that can be used to capture things when running steps.

  This context object is useful when running multiple steps at a time using the
  `experimental_run_steps_on_iterator` API. For e.g. it allows the user's step
  function to specify which outputs to emit at what frequency. Currently it
  supports capturing output from the last step, as well as capturing non tensor
  outputs.  In the future it will be augmented to support other use cases such
  as output each N steps.
  """

  def __init__(self):
    """Initialize an output context.

    Returns:
      A context object.
    """
    self._last_step_outputs = {}
    self._last_step_outputs_reduce_ops = {}
    self._non_tensor_outputs = {}

  @property
  def last_step_outputs(self):
    """A dictionary consisting of outputs to be captured on last step.

    Keys in the dictionary are names of tensors to be captured, as specified
    when `set_last_step_output` is called.
    Values in the dictionary are the tensors themselves. If
    `set_last_step_output` was called with a `reduce_op` for this output,
    then the value is the reduced value.

    Returns:
      A dictionary with last step outputs.
    """
    return self._last_step_outputs

  def _set_last_step_outputs(self, outputs):
    """Replace the entire dictionary of last step outputs."""
    if not isinstance(outputs, dict):
      raise ValueError("Need a dictionary to set last_step_outputs.")
    self._last_step_outputs = outputs

  def set_last_step_output(self, name, output, reduce_op=None):
    """Set `output` with `name` to be outputted from the last step.

    Args:
      name: String, name to identify the output. Doesn't need to match tensor
        name.
      output: The tensors that should be outputted with `name`. See below for
        actual types supported.
      reduce_op: Reduction method to use to reduce outputs from multiple
        replicas. Required if `set_last_step_output` is called in a replica
        context. Optional in cross_replica_context.
        When present, the outputs from all the replicas are reduced using the
        current distribution strategy's `reduce` method. Hence, the type of
        `output` must be what's supported by the corresponding `reduce` method.
        For e.g. if using MirroredStrategy and reduction is set, output
        must be a `PerReplica` value.
        The reduce method is also recorded in a dictionary
        `_last_step_outputs_reduce_ops` for later interpreting of the
        outputs as already reduced or not.
    """
    if distribution_strategy_context.in_cross_replica_context():
      self._last_step_outputs_reduce_ops[name] = reduce_op
      if reduce_op is None:
        self._last_step_outputs[name] = output
      else:
        distribution = distribution_strategy_context.get_strategy()
        self._last_step_outputs[name] = distribution.reduce(reduce_op, output,
                                                            axis=None)
    else:
      assert reduce_op is not None
      def merge_fn(distribution, value):
        self._last_step_outputs[name] = distribution.reduce(reduce_op, value,
                                                            axis=None)
        # Setting this inside the `merge_fn` because all replicas share the same
        # context object, so it's more robust to set it only once (even if all
        # the replicas are trying to set the same value).
        self._last_step_outputs_reduce_ops[name] = reduce_op

      distribution_strategy_context.get_replica_context().merge_call(
          merge_fn, args=(output,))

  @property
  def non_tensor_outputs(self):
    """A dictionary consisting of any non tensor outputs to be captured."""
    return self._non_tensor_outputs

  def set_non_tensor_output(self, name, output):
    """Set `output` with `name` to be captured as a non tensor output."""
    if distribution_strategy_context.in_cross_replica_context():
      self._non_tensor_outputs[name] = output
    else:
      def merge_fn(distribution, value):
        # NOTE(priyag): For non tensor outputs, we simply return all the values
        # in a list as reduction doesn't make sense on non tensors.
        self._non_tensor_outputs[name] = (
            distribution.experimental_local_results(value))
      distribution_strategy_context.get_replica_context().merge_call(
          merge_fn, args=(output,))


def _create_distributed_tensor_spec(strategy, tensor_spec):
  """Create a `tf.TypeSpec` for a given strategy and input `tensor_spec`.

  Args:
    strategy: The given `tf.distribute` strategy.
    tensor_spec: `tf.TensorSpec` of a given value. The batch dimension of the
      shape should be None if you have partial batches.

  Returns:
    A `tf.TypeSpec` that matches the values produced by a given strategy. This
    can be a `tf.TensorSpec` or a `PerRelicaSpec`.
  """
  num_replicas = len(strategy.extended.worker_devices)

  # For one device strategy that is not MultiWorkerMirroredStrategy,  return the
  # tensor_spec as is, since we don't wrap the output with PerReplica in this
  # case.
  # TODO(b/166464552): remove after we always wrap for all strategies.
  if not _always_wrap(strategy):
    return tensor_spec

  # For other cases we assume the input to tf.function is a per replica type.
  def _get_value_per_replica(tensor_spec_per_input):
    value_specs = [tensor_spec_per_input for _ in range(num_replicas)]
    return values.PerReplicaSpec(*value_specs)

  return nest.map_structure(_get_value_per_replica, tensor_spec)


def _replace_per_replica_spec(spec, i):
  """If `spec` is a `PerReplicaSpec`, then return its `i`th value_spec."""
  if isinstance(spec, values.PerReplicaSpec):
    return spec._value_specs[i]  # pylint: disable=protected-access
  else:
    return spec


def _cardinality(dataset):
  """Returns the cardinality of the dataset."""
  if context.executing_eagerly():
    with ops.device(dataset._variant_tensor.device):  # pylint: disable=protected-access
      return dataset.cardinality().numpy()
  return cardinality_lib.UNKNOWN


def _enable_get_next_as_optional(strategy, dataset, cardinality):
  """Returns whether to enable using partial batch handling."""
  # TODO(b/133073708): we currently need a flag to control the usage because
  # there is a performance difference between get_next() and
  # get_next_as_optional(). And we only enable get_next_as_optional when the
  # output shapes are not static.
  #
  # TODO(rxsang): We want to always enable the get_next_as_optional behavior
  # when user passed input_fn instead of dataset.
  if not getattr(
      strategy.extended, "enable_partial_batch_handling",
      getattr(strategy.extended, "experimental_enable_get_next_as_optional",
              False)):
    return False

  # If the dataset is infinite, we don't need to enable last partial batch
  # support. Note that we can only evaluate the cardinality of the dataset in
  # eager.
  if cardinality == cardinality_lib.INFINITE:
    return False

  return not _is_statically_shaped(
      dataset.element_spec) or strategy.extended._in_multi_worker_mode()  # pylint: disable=protected-access


def _create_per_replica(value_list, strategy):
  """Creates a PerReplica.

  For strategies other than OneDeviceStrategy, it creates a PerReplica whose
  type spec is set to the element spec of the dataset. This helps avoid
  retracing for partial batches. Retracing is problematic for multi client when
  different client retraces different time, since retracing changes the
  collective keys in the tf.function, and causes mismatches among clients.

  For single client strategies, this simply calls distribute_utils.regroup().

  Args:
    value_list: a list of values, one for each replica.
    strategy: the `tf.distribute.Strategy`.

  Returns:
    a structure of PerReplica.

  """
  # TODO(b/166464552): always wrap for all one device strategies as well.
  always_wrap = _always_wrap(strategy)
  per_replicas = distribute_utils.regroup(value_list, always_wrap=always_wrap)
  return per_replicas


def _always_wrap(strategy):
  """Returns whether to always wrap the values in a DistributedValues."""
  return strategy.extended._in_multi_worker_mode() or len(  # pylint: disable=protected-access
      strategy.extended.worker_devices) > 1


def _rebatch_as_dynamic(per_replica_spec):
  """Rebatch the spec to have a dynamic batch dimension."""
  assert isinstance(per_replica_spec, values.PerReplicaSpec), per_replica_spec

  # pylint: disable=protected-access
  def _rebatch(spec):
    # Rebatch if possible.
    try:
      return spec._unbatch()._batch(None)
    except ValueError:
      pass
    return spec

  return values.PerReplicaSpec(
      *nest.map_structure(_rebatch, per_replica_spec._value_specs))
  # pylint: enable=protected-access
