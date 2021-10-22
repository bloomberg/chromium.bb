# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
"""A Python interface for creating dataset servers."""

import collections

# pylint: disable=invalid-import-order,g-bad-import-order, unused-import
from tensorflow.core.protobuf import service_config_pb2
from tensorflow.python import pywrap_tensorflow
from tensorflow.python.data.experimental.service import _pywrap_server_lib
from tensorflow.python.data.experimental.service import _pywrap_utils
from tensorflow.python.util.tf_export import tf_export


def _get_time_or_placeholder(value):
  """Modifies time-based config values to account for special behaviors."""

  # Servers interpret time values of 0 to mean "choose a reasonable
  # default". However, the Python API uses `None` for this, and allows 0 as a
  # normal value. To account for this, if a user explicitly configures the
  # interval/timeout to 0, we interpret it to mean "a very small number", and
  # replace it with 1.
  if value == 0:
    return 1
  # `None` indicates that the user wants to leave the behavior to the runtime.
  if value is None:
    return 0
  return value


@tf_export("data.experimental.service.DispatcherConfig")
class DispatcherConfig(
    collections.namedtuple("DispatcherConfig", [
        "port", "protocol", "work_dir", "fault_tolerant_mode",
        "worker_addresses", "job_gc_check_interval_ms", "job_gc_timeout_ms"
    ])):
  """Configuration class for tf.data service dispatchers.

  Fields:
    port: Specifies the port to bind to. A value of 0 indicates that the server
      may bind to any available port.
    protocol: The protocol to use for communicating with the tf.data service,
      e.g. "grpc".
    work_dir: A directory to store dispatcher state in. This
      argument is required for the dispatcher to be able to recover from
      restarts.
    fault_tolerant_mode: Whether the dispatcher should write its state to a
      journal so that it can recover from restarts. Dispatcher state, including
      registered datasets and created jobs, is synchronously written to the
      journal before responding to RPCs. If `True`, `work_dir` must also be
      specified.
    worker_addresses: If the job uses auto-sharding, it needs to specify a fixed
      list of worker addresses that will register with the dispatcher. The
      worker addresses should be in the format `"host"` or `"host:port"`, where
      `"port"` is an integer, named port, or `%port%` to match any port.
    job_gc_check_interval_ms: How often the dispatcher should scan through to
      delete old and unused jobs, in milliseconds. If not set, the runtime will
      select a reasonable default. A higher value will reduce load on the
      dispatcher, while a lower value will reduce the time it takes for the
      dispatcher to garbage collect expired jobs.
    job_gc_timeout_ms: How long a job needs to be unused before it becomes a
      candidate for garbage collection, in milliseconds. A value of -1 indicates
      that jobs should never be garbage collected. If not set, the runtime will
      select a reasonable default. A higher value will cause jobs to stay around
      longer with no consumers. This is useful if there is a large gap in
      time between when consumers read from the job. A lower value will reduce
      the time it takes to reclaim the resources from expired jobs.
  """

  def __new__(cls,
              port=0,
              protocol=None,
              work_dir=None,
              fault_tolerant_mode=False,
              worker_addresses=None,
              job_gc_check_interval_ms=None,
              job_gc_timeout_ms=None):
    if protocol is None:
      protocol = _pywrap_utils.TF_DATA_DefaultProtocol()
    job_gc_check_interval_ms = _get_time_or_placeholder(
        job_gc_check_interval_ms)
    job_gc_timeout_ms = _get_time_or_placeholder(job_gc_timeout_ms)
    return super(DispatcherConfig,
                 cls).__new__(cls, port, protocol, work_dir,
                              fault_tolerant_mode, worker_addresses,
                              job_gc_check_interval_ms, job_gc_timeout_ms)


@tf_export("data.experimental.service.DispatchServer", v1=[])
class DispatchServer(object):
  """An in-process tf.data service dispatch server.

  A `tf.data.experimental.service.DispatchServer` coordinates a cluster of
  `tf.data.experimental.service.WorkerServer`s. When the workers start, they
  register themselves with the dispatcher.

  >>> dispatcher = tf.data.experimental.service.DispatchServer()
  >>> dispatcher_address = dispatcher.target.split("://")[1]
  >>> worker = tf.data.experimental.service.WorkerServer(
  ...     tf.data.experimental.service.WorkerConfig(
  ...     dispatcher_address=dispatcher_address))
  >>> dataset = tf.data.Dataset.range(10)
  >>> dataset = dataset.apply(tf.data.experimental.service.distribute(
  ...     processing_mode="parallel_epochs", service=dispatcher.target))
  >>> print(list(dataset.as_numpy_iterator()))
  [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

  When starting a dedicated tf.data dispatch process, use join() to block
  indefinitely after starting up the server.

  ```
  dispatcher = tf.data.experimental.service.DispatchServer(
      tf.data.experimental.service.DispatcherConfig(port=5050))
  dispatcher.join()
  ```

  To start a `DispatchServer` in fault-tolerant mode, set `work_dir` and
  `fault_tolerant_mode` like below:

  ```
  dispatcher = tf.data.experimental.service.DispatchServer(
      tf.data.experimental.service.DispatcherConfig(
          port=5050,
          work_dir="gs://my-bucket/dispatcher/work_dir",
          fault_tolerant_mode=True))
  ```
  """

  def __init__(self, config=None, start=True):
    """Creates a new dispatch server.

    Args:
      config: (Optional.) A `tf.data.experimental.service.DispatcherConfig`
        configration. If `None`, the dispatcher will use default
        configuration values.
      start: (Optional.) Boolean, indicating whether to start the server after
        creating it. Defaults to True.
    """
    config = config or DispatcherConfig()
    if config.fault_tolerant_mode and not config.work_dir:
      raise ValueError(
          "Cannot enable fault tolerant mode without configuring a work dir. "
          "Make sure to set `work_dir` in the `config` object passed to "
          "`DispatcherServer`.")
    self._config = config
    if isinstance(config, service_config_pb2.DispatcherConfig):
      config_proto = config
    else:
      config_proto = service_config_pb2.DispatcherConfig(
          port=config.port,
          protocol=config.protocol,
          work_dir=config.work_dir,
          fault_tolerant_mode=config.fault_tolerant_mode,
          worker_addresses=config.worker_addresses,
          job_gc_check_interval_ms=config.job_gc_check_interval_ms,
          job_gc_timeout_ms=config.job_gc_timeout_ms)
    self._server = _pywrap_server_lib.TF_DATA_NewDispatchServer(
        config_proto.SerializeToString())
    if start:
      self._server.start()

  def start(self):
    """Starts this server.

    >>> dispatcher = tf.data.experimental.service.DispatchServer(start=False)
    >>> dispatcher.start()

    Raises:
      tf.errors.OpError: Or one of its subclasses if an error occurs while
        starting the server.
    """
    self._server.start()

  def join(self):
    """Blocks until the server has shut down.

    This is useful when starting a dedicated dispatch process.

    ```
    dispatcher = tf.data.experimental.service.DispatchServer(
        tf.data.experimental.service.DispatcherConfig(port=5050))
    dispatcher.join()
    ```

    Raises:
      tf.errors.OpError: Or one of its subclasses if an error occurs while
        joining the server.
    """
    self._server.join()

  @property
  def target(self):
    """Returns a target that can be used to connect to the server.

    >>> dispatcher = tf.data.experimental.service.DispatchServer()
    >>> dataset = tf.data.Dataset.range(10)
    >>> dataset = dataset.apply(tf.data.experimental.service.distribute(
    ...     processing_mode="parallel_epochs", service=dispatcher.target))

    The returned string will be in the form protocol://address, e.g.
    "grpc://localhost:5050".
    """
    return "{0}://localhost:{1}".format(self._config.protocol,
                                        self._server.bound_port())

  def _stop(self):
    """Stops the server.

    Raises:
      tf.errors.OpError: Or one of its subclasses if an error occurs while
        stopping the server.
    """
    self._server.stop()

  def __del__(self):
    self._stop()

  @property
  def _address(self):
    """Returns the address of the server.

    The returned string will be in the form address:port, e.g. "localhost:1000".
    """
    return "localhost:{0}".format(self._server.bound_port())

  def _num_workers(self):
    """Returns the number of workers registered with the dispatcher."""
    return self._server.num_workers()


@tf_export("data.experimental.service.WorkerConfig")
class WorkerConfig(
    collections.namedtuple("WorkerConfig", [
        "dispatcher_address", "worker_address", "port", "protocol",
        "heartbeat_interval_ms", "dispatcher_timeout_ms"
    ])):
  """Configuration class for tf.data service dispatchers.

  Fields:
    dispatcher_address: Specifies the address of the dispatcher.
    worker_address: Specifies the address of the worker server. This address is
      passed to the dispatcher so that the dispatcher can tell clients how to
      connect to this worker.
    port: Specifies the port to bind to. A value of 0 indicates that the worker
      can bind to any available port.
    protocol: (Optional.) Specifies the protocol to be used by the server, e.g.
      "grpc".
    heartbeat_interval_ms: How often the worker should heartbeat to the
      dispatcher, in milliseconds. If not set, the runtime will select a
      reasonable default. A higher value will reduce the load on the dispatcher,
      while a lower value will reduce the time it takes to reclaim resources
      from finished jobs.
    dispatcher_timeout_ms: How long, in milliseconds, to retry requests to the
      dispatcher before giving up and reporting an error. Defaults to 1 hour.
  """

  def __new__(cls,
              dispatcher_address,
              worker_address=None,
              port=0,
              protocol=None,
              heartbeat_interval_ms=None,
              dispatcher_timeout_ms=None):
    if worker_address is None:
      worker_address = "localhost:%port%"
    if protocol is None:
      protocol = _pywrap_utils.TF_DATA_DefaultProtocol()
    heartbeat_interval_ms = _get_time_or_placeholder(heartbeat_interval_ms)
    dispatcher_timeout_ms = _get_time_or_placeholder(dispatcher_timeout_ms)

    return super(WorkerConfig,
                 cls).__new__(cls, dispatcher_address, worker_address, port,
                              protocol, heartbeat_interval_ms,
                              dispatcher_timeout_ms)


@tf_export("data.experimental.service.WorkerServer", v1=[])
class WorkerServer(object):
  """An in-process tf.data service worker server.

  A `tf.data.experimental.service.WorkerServer` performs `tf.data.Dataset`
  processing for user-defined datasets, and provides the resulting elements over
  RPC. A worker is associated with a single
  `tf.data.experimental.service.DispatchServer`.

  >>> dispatcher = tf.data.experimental.service.DispatchServer()
  >>> dispatcher_address = dispatcher.target.split("://")[1]
  >>> worker = tf.data.experimental.service.WorkerServer(
  ...     tf.data.experimental.service.WorkerConfig(
  ...         dispatcher_address=dispatcher_address))
  >>> dataset = tf.data.Dataset.range(10)
  >>> dataset = dataset.apply(tf.data.experimental.service.distribute(
  ...     processing_mode="parallel_epochs", service=dispatcher.target))
  >>> print(list(dataset.as_numpy_iterator()))
  [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

  When starting a dedicated tf.data worker process, use join() to block
  indefinitely after starting up the server.

  ```
  worker = tf.data.experimental.service.WorkerServer(
      port=5051, dispatcher_address="localhost:5050")
  worker.join()
  ```
  """

  def __init__(self, config, start=True):
    """Creates a new worker server.

    Args:
      config: A `tf.data.experimental.service.WorkerConfig` configration.
      start: (Optional.) Boolean, indicating whether to start the server after
        creating it. Defaults to True.
    """
    if config.dispatcher_address is None:
      raise ValueError(
          "Must specify a `dispatcher_address` in the `config` passed "
          "to `WorkerServer`.")
    if isinstance(config, service_config_pb2.WorkerConfig):
      config_proto = config
    else:
      config_proto = service_config_pb2.WorkerConfig(
          dispatcher_address=config.dispatcher_address,
          worker_address=config.worker_address,
          port=config.port,
          protocol=config.protocol,
          heartbeat_interval_ms=config.heartbeat_interval_ms,
          dispatcher_timeout_ms=config.dispatcher_timeout_ms,
          data_transfer_protocol=None)
    self._server = _pywrap_server_lib.TF_DATA_NewWorkerServer(
        config_proto.SerializeToString())
    if start:
      self._server.start()

  def start(self):
    """Starts this server.

    Raises:
      tf.errors.OpError: Or one of its subclasses if an error occurs while
        starting the server.
    """
    self._server.start()

  def join(self):
    """Blocks until the server has shut down.

    This is useful when starting a dedicated worker process.

    ```
    worker_server = tf.data.experimental.service.WorkerServer(
        port=5051, dispatcher_address="localhost:5050")
    worker_server.join()
    ```

    This method currently blocks forever.

    Raises:
      tf.errors.OpError: Or one of its subclasses if an error occurs while
        joining the server.
    """
    self._server.join()

  def _stop(self):
    """Stops the server.

    Raises:
      tf.errors.OpError: Or one of its subclasses if an error occurs while
        stopping the server.
    """
    self._server.stop()

  def __del__(self):
    self._stop()

  @property
  def _address(self):
    """Returns the address of the server.

    The returned string will be in the form address:port, e.g. "localhost:1000".
    """
    return "localhost:{0}".format(self._server.bound_port())

  def _num_tasks(self):
    """Returns the number of tasks currently being executed on the worker."""
    return self._server.num_tasks()
