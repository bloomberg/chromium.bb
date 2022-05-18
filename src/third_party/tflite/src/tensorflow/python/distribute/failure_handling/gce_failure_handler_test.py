# Copyright 2022 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Tests for GCE specifics of WorkerPreemptionHandler."""
import os
import random
import re
import sys
import time
import urllib

from absl.testing import parameterized

# pylint:disable=g-direct-tensorflow-import
from tensorflow.python.distribute import collective_all_reduce_strategy
from tensorflow.python.distribute import combinations
from tensorflow.python.distribute import multi_process_runner
from tensorflow.python.distribute import multi_worker_test_base
from tensorflow.python.distribute import multi_worker_util
from tensorflow.python.distribute import test_util
from tensorflow.python.distribute.failure_handling import failure_handling
from tensorflow.python.distribute.failure_handling import gce_util
from tensorflow.python.eager import context
from tensorflow.python.eager import def_function
from tensorflow.python.framework import constant_op
from tensorflow.python.module import module
from tensorflow.python.ops import variables as variables_lib
from tensorflow.python.platform import gfile
from tensorflow.python.platform import test
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.training.tracking import util as tracking_util


mock = test.mock


CLUSTER_SIZE = 4
EPOCHS_TO_RUN = 10
STEPS_PER_EPOCH = 15
_PEER_WATCHER_THREAD_PREFIX = 'PeerTerminationWatcher'
_LOCAL_WATCHER_THREAD_PREFIX = 'WorkerTerminationSignalWatcher'


def _is_oss():
  """Returns whether the test is run under OSS."""
  return len(sys.argv) >= 1 and 'bazel' in sys.argv[0]


def _enable_coordination_service(cluster_spec):

  if context.context().coordination_service is None:
    coordination_service = 'standalone'
    coordinated_jobs = ['chief', 'worker']
    context.context().configure_coordination_service(
        service_type=coordination_service,
        service_leader=multi_worker_util.coordination_leader(
            cluster_spec),
        coordinated_jobs=coordinated_jobs)


class GceFailureHandlingTest(test.TestCase, parameterized.TestCase):
  """Integration test for WorkerPreemptionHandler."""

  def _mwms_write_checkpoint_dir(self, checkpoint_dir, cluster_spec, task_type,
                                 task_id):
    dirpath = os.path.dirname(checkpoint_dir)
    base = os.path.basename(checkpoint_dir)
    if not multi_worker_util.is_chief(
        cluster_spec=cluster_spec, task_type=task_type, task_id=task_id):
      base_dirpath = 'workertemp_' + str(task_id)
      dirpath = os.path.join(dirpath, base_dirpath)
      gfile.MakeDirs(dirpath)
    return os.path.join(dirpath, base)

  def worker_fn(self,
                checkpoint_dir,
                cluster_spec,
                maintenance_event=None,
                training_finished=None,
                frequent_send=False,
                termination_config=failure_handling.TerminationConfig(
                    time_till_termination=0)):

    _enable_coordination_service(cluster_spec)
    strategy = collective_all_reduce_strategy.CollectiveAllReduceStrategy()

    def mock_termination_watcher_function_gce(*args, **kwargs):
      del args, kwargs
      if not frequent_send:
        time.sleep(1)
        if (not maintenance_event.is_set()) and (random.randrange(0, 20) > 18):
          maintenance_event.set()
          logging.info('Termination notice available.')
          return True

      elif frequent_send and not maintenance_event.is_set():
        logging.info('Termination notice available.')
        return True

      return False

    with mock.patch.object(
        gce_util, 'termination_watcher_function_gce',
        mock_termination_watcher_function_gce), mock.patch.object(
            gce_util, 'detect_platform',
            lambda: gce_util.PlatformDevice.GCE_GPU):

      class Model(module.Module):

        def __init__(self):
          self.v = variables_lib.Variable(
              0.,
              synchronization=variables_lib.VariableSynchronization.ON_WRITE,
              aggregation=variables_lib.VariableAggregation.SUM)

        @def_function.function(input_signature=[])
        def __call__(self):
          return self.v.read_value()

      with strategy.scope():
        model = Model()
        fh_ckpt = tracking_util.Checkpoint(model=model)

        worker_preemption_watcher = failure_handling.WorkerPreemptionHandler(
            strategy.cluster_resolver, fh_ckpt, checkpoint_dir,
            termination_config)

      def distributed_train_step(current_epoch, current_step):

        @def_function.function
        def train_step():
          model.v.assign_add(constant_op.constant(1.))

        strategy.run(train_step)

        if current_step == STEPS_PER_EPOCH - 1:
          logging.info('epoch %d finished', current_epoch)

      logging.info('Start training at %d', worker_preemption_watcher.total_runs)
      for epoch in range(
          worker_preemption_watcher.total_runs // STEPS_PER_EPOCH,
          EPOCHS_TO_RUN):

        for step in range(
            worker_preemption_watcher.total_runs % STEPS_PER_EPOCH,
            STEPS_PER_EPOCH):
          worker_preemption_watcher.run(distributed_train_step, epoch, step)

      logging.info('Training finished.')
      training_finished.set()

      self.assertEqual(
          model.v.numpy(),
          strategy.num_replicas_in_sync * EPOCHS_TO_RUN * STEPS_PER_EPOCH)

      running_threads = test_util.get_running_threads()
      if test_util.has_thread(_PEER_WATCHER_THREAD_PREFIX,
                              running_threads) and test_util.has_thread(
                                  _LOCAL_WATCHER_THREAD_PREFIX,
                                  running_threads):
        try:
          # Explicitly call __del__ since making it None and gc.collect does
          # not invoke __del__ here.
          worker_preemption_watcher.__del__()

          time.sleep(2)

          running_threads = test_util.get_running_threads()
          self.assertFalse(
              test_util.has_thread(_LOCAL_WATCHER_THREAD_PREFIX,
                                   running_threads))
          self.assertFalse(
              test_util.has_thread(_PEER_WATCHER_THREAD_PREFIX,
                                   running_threads))

        except urllib.error.URLError as e:
          if 'Temporary failure in name resolution' in e.message:
            # This is caused by a weird flakiness that mock.patch does not
            # correctly patch gce_util.request_compute_metadata, a real request
            # is attempted, and an error is hit in
            # gce_util.request_compute_metadata
            logging.warning('Hit a mock issue.')
            return

  def test_basic_run(self):
    has_chief = False
    cluster_spec = multi_worker_test_base.create_cluster_spec(
        has_chief=has_chief,
        num_workers=CLUSTER_SIZE)
    maintenance_event = multi_process_runner.manager().Event()
    training_finished = multi_process_runner.manager().Event()

    checkpoint_dir = os.path.join(self.get_temp_dir(), 'fh_ckpt')

    if _is_oss():
      rpc_layer = 'grpc'
    else:
      rpc_layer = 'grpc+loas'

    mpr = multi_process_runner.MultiProcessRunner(
        self.worker_fn,
        cluster_spec,
        args=(checkpoint_dir, cluster_spec, maintenance_event,
              training_finished),
        rpc_layer=rpc_layer,
        return_output=True,
        dependence_on_chief=has_chief)

    logging.info('Cluster starting.')
    mpr.start()

    while (not maintenance_event.is_set()) and (not training_finished.is_set()):
      time.sleep(1)

    time.sleep(5)
    if not training_finished.is_set():
      logging.info('restarting workers')
      for worker_id in range(CLUSTER_SIZE):
        mpr.start_single_process('worker', worker_id, cluster_spec)
      logging.info('workers restarted')

    stdout = mpr.join(timeout=250).stdout
    if maintenance_event.is_set():
      all_start_point = []
      for msg in stdout:
        matched_group = re.search(r'.*Start training at (\d+)', msg)

        if matched_group:
          all_start_point.append(int(matched_group.group(1)))

      # remove duplicate logs created due to presence of multiple workers
      start_points = all_start_point[::CLUSTER_SIZE]

      if len(start_points) > 1:
        # assert that after restarting, we don't repeat previous training steps
        self.assertNotEqual(start_points[-1], 0)

  @combinations.generate(
      combinations.combine(
          grace_period=[0, 7],
          ))
  def test_multiple_workers_preempted_consecutively(self, grace_period):
    has_chief = False
    cluster_spec = multi_worker_test_base.create_cluster_spec(
        has_chief=has_chief,
        num_workers=CLUSTER_SIZE)
    maintenance_event = multi_process_runner.manager().Event()
    training_finished = multi_process_runner.manager().Event()

    checkpoint_dir = os.path.join(self.get_temp_dir(), 'fh_ckpt')

    if _is_oss():
      rpc_layer = 'grpc'
    else:
      rpc_layer = 'grpc+loas'

    termination_config = failure_handling.TerminationConfig(
        time_till_termination=grace_period)
    mpr = multi_process_runner.MultiProcessRunner(
        self.worker_fn,
        cluster_spec,
        args=(checkpoint_dir, cluster_spec,
              maintenance_event,
              training_finished, True, termination_config),
        rpc_layer=rpc_layer,
        return_output=True,
        dependence_on_chief=has_chief)

    logging.info('Cluster starting.')
    mpr.start()

    # wait for all cluster to exit with a time out
    waiting_time = 0
    exit_process_count = 0
    # this addition to mitigate the fact that our step time is too short in test
    while exit_process_count != CLUSTER_SIZE and waiting_time < max(
        grace_period + 15, 40):
      exit_process_count = 0
      for worker_id in range(CLUSTER_SIZE):
        if not mpr.process_exists('worker', worker_id):
          exit_process_count += 1
      waiting_time += 1
      time.sleep(1)

    if waiting_time == max(grace_period + 5, 40):
      raise RuntimeError('Waited long but at least one worker still exist. '
                         'Considering size of our model, this should not'
                         ' happen.')

    maintenance_event.set()
    logging.info('restarting workers')
    for worker_id in range(CLUSTER_SIZE):
      mpr.start_single_process('worker', worker_id, cluster_spec)
    logging.info('workers restarted')

    stdout = mpr.join(timeout=250).stdout
    found_message = 0
    checkpoint_count = []
    for msg in stdout:
      matched_group = re.search(r'.*has received termination notice*', msg)
      checkpoint_group = re.search(r'.*RUN_TO_CHECKPOINT set to (\d+)', msg)
      if matched_group:
        found_message += 1
      if checkpoint_group:
        checkpoint_count.append(int(checkpoint_group.group(1)))

    self.assertGreaterEqual(found_message, 1)
    if grace_period > 0:
      self.assertLen(set(checkpoint_count), 2)

  def test_grace_period_continue_training(self):
    grace_period = 7
    has_chief = False
    cluster_spec = multi_worker_test_base.create_cluster_spec(
        has_chief=has_chief,
        num_workers=CLUSTER_SIZE)

    checkpoint_dir = os.path.join(self.get_temp_dir(), 'fh_ckpt')
    maintenance_event = multi_process_runner.manager().Event()
    training_finished = multi_process_runner.manager().Event()

    if _is_oss():
      rpc_layer = 'grpc'
    else:
      rpc_layer = 'grpc+loas'

    termination_config = failure_handling.TerminationConfig(
        time_till_termination=grace_period)
    mpr = multi_process_runner.MultiProcessRunner(
        self.worker_fn,
        cluster_spec,
        args=(checkpoint_dir, cluster_spec, maintenance_event,
              training_finished, False, termination_config),
        rpc_layer=rpc_layer,
        return_output=True,
        dependence_on_chief=has_chief)

    logging.info('Cluster starting.')
    mpr.start()

    while (not maintenance_event.is_set()) and (not training_finished.is_set()):
      time.sleep(1)

    # this addition to mitigate the fact that our step time is too short in test
    time.sleep(grace_period + 10)
    if not training_finished.is_set():
      logging.info('restarting workers')
      for worker_id in range(CLUSTER_SIZE):
        mpr.start_single_process('worker', worker_id, cluster_spec)
      logging.info('workers restarted')

    if maintenance_event.is_set():
      stdout = mpr.join(timeout=250).stdout
      all_start_point = []
      checkpoint_count = []
      for msg in stdout:
        matched_group = re.search(r'.*Start training at (\d+)', msg)
        checkpoint_group = re.search(r'.*RUN_TO_CHECKPOINT set to (\d+)', msg)

        if matched_group:
          all_start_point.append(int(matched_group.group(1)))

        if checkpoint_group:
          checkpoint_count.append(int(checkpoint_group.group(1)))

      # remove duplicate logs created due to presence of multiple workers
      start_points = all_start_point[::CLUSTER_SIZE]

      # if maintenance_event is set at the very end of training and training
      # completes, there won't be a restart.
      if len(start_points) > 1:
        # assert that after restarting, we don't repeat previous training steps
        self.assertNotEqual(start_points[-1], 0)

        # One for timing, another for final call.
        self.assertLen(set(checkpoint_count), 2)


if __name__ == '__main__':
  test_util.main()
