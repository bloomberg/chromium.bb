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
"""Python module for evaluation loop."""

# pylint: disable=g-direct-tensorflow-import
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors_impl
from tensorflow.python.ops import variables
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.training import checkpoint_utils
from tensorflow.python.training.tracking import util as tracking_util

_PRINT_EVAL_STEP_EVERY_SEC = 60.0
_ITERATIONS_UNINITIALIZED = -1


def list_checkpoint_attributes(ckpt_dir_or_file):
  """Lists all the attributes in a checkpoint.

  Checkpoint keys are paths in a checkpoint graph, and attribute is the first
  element in the path. e.g. with a checkpoint key
  "optimizer/iter/.ATTRIBUTES/VARIABLE_VALUE", optimizer is the attribute. The
  attribute is also used to save/restore a variable in a checkpoint,
  e.g. tf.train.Checkpoint(optimizer=optimizer, model=model).

  Args:
    ckpt_dir_or_file: Directory with checkpoints file or path to checkpoint.

  Returns:
    Set of attributes in a checkpoint.
  """
  reader = checkpoint_utils.load_checkpoint(ckpt_dir_or_file)
  variable_map = reader.get_variable_to_shape_map()
  return {name.split('/')[0] for name in variable_map.keys()}


class SidecarEvaluator(object):
  """A class designed for a dedicated evaluator task.

  `SidecarEvaluator` is expected to be run in a process on a separate machine
  from the training cluster. It is meant for the purpose of a dedicated
  evaluator, evaluating the metric results of a training cluster which has one
  or more workers performing the training, and saving checkpoints.

  The `SidecarEvaluator` API is compatible with both Custom Training Loop (CTL),
  and Keras `Model.fit` to be used in the training cluster. Using the model
  (with compiled metrics) provided at `__init__`, `SidecarEvaluator` repeatedly
  performs evaluation "epochs" when it finds a checkpoint that has not yet been
  used. Depending on the `steps` argument, an eval epoch is evaluation over all
  eval data, or up to certain number of steps (batches). See examples below for
  how the training program should save the checkpoints in order to be recognized
  by `SidecarEvaluator`.

  Since under the hood, `SidecarEvaluator` uses `model.evaluate` for evaluation,
  it also supports arbitrary Keras callbacks. That is, if one or more callbacks
  are provided, their `on_test_batch_begin` and `on_test_batch_end` methods are
  called at the start and end of a batch, and their `on_test_begin` and
  `on_test_end` are called at the start and end of an evaluation epoch. Note
  that `SidecarEvaluator` may skip some checkpoints because it always picks up
  the latest checkpoint available, and during an evaluation epoch, multiple
  checkpoints can be produced from the training side.

  Example:
  ```python
  model = tf.keras.models.Sequential(...)
  model.compile(metrics=tf.keras.metrics.SparseCategoricalAccuracy(
      name="eval_metrics"))
  data = tf.data.Dataset.from_tensor_slices(...)

  SidecarEvaluator(
      model=model,
      data=data,
      checkpoint_dir='/tmp/checkpoint_dir',  # dir for training-saved checkpoint
      steps=None,  # Eval until dataset is exhausted
      max_evaluations=None,  # The evaluation needs to be stopped manually
      callbacks=[tf.keras.callbacks.TensorBoard(log_dir='/tmp/log_dir')]
  ).start()
  ```

  `SidecarEvaluator.start` writes a series of summary
  files which can be visualized by tensorboard (which provides a webpage link):

  ```bash
  $ tensorboard --logdir=/tmp/log_dir/validation
  ...
  TensorBoard 2.4.0a0 at http://host:port (Press CTRL+C to quit)
  ```

  If the training cluster uses a CTL, the `checkpoint_dir` should contain
  checkpoints that track both `model` and `optimizer`, to fulfill
  `SidecarEvaluator`'s expectation. This can be done by a
  `tf.train.Checkpoint` and a `tf.train.CheckpointManager`:

  ```python
  checkpoint_dir = ...  # Same `checkpoint_dir` supplied to `SidecarEvaluator`.
  checkpoint = tf.train.Checkpoint(model=model, optimizer=optimizer)
  checkpoint_manager = tf.train.CheckpointManager(
      checkpoint, checkpoint_dir=..., max_to_keep=...)
  checkpoint_manager.save()
  ```

  If the training cluster uses Keras `Model.fit` API, a
  `tf.keras.callbacks.ModelCheckpoint` should be used, with
  `save_weights_only=True`, and the `filepath` should have 'ckpt-{epoch}'
  appended:

  ```python
  checkpoint_dir = ...  # Same `checkpoint_dir` supplied to `SidecarEvaluator`.
  model_checkpoint = tf.keras.callbacks.ModelCheckpoint(
      filepath=os.path.join(checkpoint_dir, 'ckpt-{epoch}'),
      save_weights_only=True)
  model.fit(dataset, epochs, callbacks=[model_checkpoint])
  ```
  """

  def __init__(self,
               model,
               data,
               checkpoint_dir,
               steps=None,
               max_evaluations=None,
               callbacks=None):
    """Initializes an `SidecarEvaluator` object.

    Args:
      model: Model to use for evaluation. The model object used here should be a
        `tf.keras.Model`, and should be the same as the one that is used in
        training, where `tf.keras.Model`s are checkpointed. The model should
        have one or more metrics compiled before using `SidecarEvaluator`.
      data: The input data for evaluation. `SidecarEvaluator` supports all data
        types that Keras `model.evaluate` supports as the input data `x`, such
        as a `tf.data.Dataset`.
      checkpoint_dir: Directory where checkpoint files are saved.
      steps: Number of steps to perform evaluation for, when evaluating a single
        checkpoint file. If `None`, evaluation continues until the dataset is
        exhausted. For repeated evaluation dataset, user must specify `steps` to
        avoid infinite evaluation loop.
      max_evaluations: Maximum number of the checkpoint file to be evaluated,
        for `SidecarEvaluator` to know when to stop. The evaluator will stop
        after it evaluates a checkpoint filepath ending with
        '<ckpt_name>-<max_evaluations>'. If using
        `tf.train.CheckpointManager.save` for saving checkpoints, the kth saved
        checkpoint has the filepath suffix '<ckpt_name>-<k>' (k=1 for the first
        saved), and if checkpoints are saved every epoch after training, the
        filepath saved at the kth epoch would end with '<ckpt_name>-<k>. Thus,
        if training runs for n epochs, and the evaluator should end after the
        training finishes, use n for this parameter. Note that this is not
        necessarily equal to the number of total evaluations, since some
        checkpoints may be skipped if evaluation is slower than checkpoint
        creation. If `None`, `SidecarEvaluator` will evaluate indefinitely, and
        the user must terminate evaluator program themselves.
      callbacks: List of `keras.callbacks.Callback` instances to apply during
        evaluation. See [callbacks](/api_docs/python/tf/keras/callbacks).
    """
    self.model = model
    self.data = data
    self.checkpoint_dir = checkpoint_dir
    self._iterations = variables.Variable(
        name='iterations',
        initial_value=_ITERATIONS_UNINITIALIZED,
        dtype=dtypes.int64)
    self.max_evaluations = max_evaluations
    self.steps = steps
    self.callbacks = callbacks or []

  def start(self):
    """Starts the evaluation loop."""
    optimizer_checkpoint = tracking_util.Checkpoint(iter=self._iterations)
    checkpoint = tracking_util.Checkpoint(
        model=self.model, optimizer=optimizer_checkpoint)

    for latest_checkpoint in checkpoint_utils.checkpoints_iterator(
        self.checkpoint_dir):
      try:
        # `expect_partial` because the checkpoint can have other `Trackable`s
        # such as `optimizer`.
        checkpoint.restore(latest_checkpoint).expect_partial()
        checkpoint_attributes = list_checkpoint_attributes(latest_checkpoint)
        # The checkpoint should contain model and optimizer for SidecarEvaluator
        # to work. But the model weights saved by ModelCheckpoint callback does
        # not contain model as an attribute. To make SidecarEvaluator compatibly
        # work in this case, use model.load_weights to load the model's weights,
        # while self._iterations is still restored by checkpoint variable.
        if 'model' not in checkpoint_attributes:
          self.model.load_weights(latest_checkpoint)
        # The model checkpoint might not include optimizer in cases, e.g.
        # using a custom training loop. Directly assign the iterations
        # property to be used in callbacks.
        if self.model.optimizer:
          self.model.optimizer.iterations.assign(self._iterations)
      except (errors_impl.OpError,) as e:
        # A couple errors can happen here with the coordinator racing to write
        # checkpoint:
        # 1) OpError: open failed for <file path>: No such file or directory
        # 2) NotFoundError (subclass of OpError): Unsuccessful
        # TensorSliceReader constructor.
        # TODO(rchao): Remove this except block once b/150954027 is resolved.
        logging.info(
            'SidecarEvaluator has an error loading '
            'checkpoint: %s. Retrying. Error: %s: %s', latest_checkpoint,
            e.__class__.__name__, e)
        continue

      if self._iterations.numpy() == _ITERATIONS_UNINITIALIZED:
        raise RuntimeError(
            '`iterations` cannot be loaded from the '
            'checkpoint file. Please ensure `iterations` is '
            'tracked in the `checkpoint` saved by the coordinator.')

      logging.info(
          'Evaluation starts: Model weights loaded from latest '
          'checkpoint file: %s.', latest_checkpoint)

      self.model.evaluate(
          self.data, steps=self.steps, callbacks=self.callbacks, verbose=2)

      return_metrics = {}
      for metric in self.model.metrics:
        result = metric.result()
        if isinstance(result, dict):
          return_metrics.update(result)
        else:
          return_metrics[metric.name] = result

      logging.info(
          'End of evaluation. Metrics: %s', ' '.join([
              '{}={}'.format(name, value.numpy())
              for name, value in return_metrics.items()
          ]))

      # TODO(rchao): Make the max evaluation robust in case users save the
      # checkpoints with epoch format {epoch:03d}.
      if (self.max_evaluations and
          latest_checkpoint.endswith('-{}'.format(self.max_evaluations))):
        # Exit the loop because we have evaluated the final checkpoint file.
        logging.info('Last checkpoint evaluated. SidecarEvaluator stops.')
        return
