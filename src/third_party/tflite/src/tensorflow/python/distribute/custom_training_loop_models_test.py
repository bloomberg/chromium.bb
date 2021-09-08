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
"""Tests for custom training loops."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

from absl.testing import parameterized
import numpy as np

from tensorflow.python import keras
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.distribute import combinations
from tensorflow.python.distribute import reduce_util
from tensorflow.python.distribute import strategy_combinations
from tensorflow.python.eager import backprop
from tensorflow.python.eager import def_function
from tensorflow.python.eager import test
from tensorflow.python.module import module
from tensorflow.python.ops import math_ops
from tensorflow.python.util import nest


class CustomModel(module.Module):

  def __init__(self, name=None):
    super(CustomModel, self).__init__(name=name)
    with self.name_scope:
      self._layers = [
          keras.layers.Dense(4, name="dense"),
      ]

  @module.Module.with_name_scope
  def __call__(self, x):
    for layer in self._layers:
      x = layer(x)
    return x


class KerasModelsTest(test.TestCase, parameterized.TestCase):

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_single_keras_layer_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = keras.layers.Dense(4, name="dense")

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        return grads

      outputs = distribution.run(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_model_creation_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = self._get_model()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        return grads

      outputs = distribution.run(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_model_optimizer_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = self._get_model()
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      outputs = distribution.run(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_subclass_model_optimizer_experimental_run(self, distribution):
    def get_subclass_model():

      class KerasSubclassModel(keras.Model):

        def __init__(self):
          super(KerasSubclassModel, self).__init__()
          self.l = keras.layers.Dense(4, name="dense")

        def call(self, x):
          return self.l(x)

      return KerasSubclassModel()
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = get_subclass_model()
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      outputs = distribution.run(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_keras_model_optimizer_experimental_run_loop(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = self._get_model()
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      for _ in range(5):
        distribution.run(step_fn, args=(next(iterator),))

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_batch_norm_with_dynamic_batch(self, distribution):
    inputs = np.zeros((10, 3, 3, 3), dtype=np.float32)
    targets = np.zeros((10, 4), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets))
    dataset = dataset.repeat()
    dataset = dataset.batch(10, drop_remainder=False)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      x = keras.layers.Input(shape=(3, 3, 3), name="input")
      y = keras.layers.BatchNormalization(fused=True, name="bn")(x)
      y = keras.layers.Flatten()(y)
      y = keras.layers.Dense(4, name="dense")(y)
      model = keras.Model(x, y)
      optimizer = keras.optimizer_v2.rmsprop.RMSprop()

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images, training=True)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      distribution.run(step_fn, args=(next(iterator),))

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_lstm(self, distribution):

    batch_size = 32

    def create_lstm_model():
      model = keras.models.Sequential()
      # We only have LSTM variables so we can detect no gradient issues more
      # easily.
      model.add(
          keras.layers.LSTM(1, return_sequences=False, input_shape=(10, 1)))
      return model

    def create_lstm_data():
      seq_length = 10

      x_train = np.random.rand(batch_size, seq_length, 1).astype("float32")
      y_train = np.random.rand(batch_size, 1).astype("float32")
      return x_train, y_train

    x, y = create_lstm_data()
    dataset = dataset_ops.Dataset.from_tensor_slices((x, y))
    dataset = dataset.batch(batch_size, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = create_lstm_model()
      optimizer = keras.optimizer_v2.gradient_descent.SGD()

    @def_function.function
    def train_step(input_iterator):

      def step_fn(inputs):
        inps, targ = inputs
        with backprop.GradientTape() as tape:
          output = model(inps)
          loss = math_ops.reduce_mean(
              keras.losses.binary_crossentropy(
                  y_true=targ, y_pred=output, from_logits=False))
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))
        return loss

      outputs = distribution.run(
          step_fn, args=(next(input_iterator),))
      return distribution.experimental_local_results(outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies, mode=["eager"]))
  def test_nested_tf_functions(self, distribution):
    # The test builds two computations with keras layers, one with nested
    # tf.function, and the other without nested tf.function. We run these
    # computations independently on the model with same weights, and make sure
    # the variables are still the same after one training step.

    inputs = np.random.random((10, 3)).astype(np.float32)
    targets = np.ones((10, 4), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets)).repeat()
    dataset = dataset.batch(10, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    def get_model():
      x = keras.layers.Input(shape=(3,), name="input")
      y = keras.layers.Dense(4, name="dense")(x)
      model = keras.Model(x, y)
      return model

    with distribution.scope():
      model = get_model()
      optimizer = keras.optimizer_v2.gradient_descent.SGD(0.1, momentum=0.01)
      weights_file = os.path.join(self.get_temp_dir(), ".h5")
      model.save_weights(weights_file)
      model2 = get_model()
      model2.load_weights(weights_file)

    # Make sure model and model2 variables are in sync when initialized.
    for model_v, model2_v in zip(model.variables, model2.variables):
      self.assertAllClose(model_v.numpy(), model2_v.numpy())

    def compute_loss(images, targets):
      outputs = model(images)
      return math_ops.reduce_sum(outputs - targets)

    @def_function.function
    def train_step_without_nested_tf_function(inputs):

      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          loss = compute_loss(images, targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))

      distribution.run(step_fn, args=(inputs,))

    @def_function.function
    def compute_loss2(images, targets):
      outputs = model2(images)
      return math_ops.reduce_sum(outputs - targets)

    @def_function.function
    def train_step_with_nested_tf_function(inputs):

      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          loss = compute_loss2(images, targets)
        grads = tape.gradient(loss, model2.variables)
        optimizer.apply_gradients(zip(grads, model2.variables))

      distribution.run(step_fn, args=(inputs,))

    inputs = next(input_iterator)

    train_step_without_nested_tf_function(inputs)
    train_step_with_nested_tf_function(inputs)

    # Make sure model and model2 variables are still in sync.
    for model_v, model2_v in zip(model.variables, model2.variables):
      self.assertAllClose(model_v.numpy(), model2_v.numpy())

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies, mode=["eager"]))
  def test_nested_tf_functions_with_control_flow(self, distribution):
    inputs = np.random.random((10, 3)).astype(np.float32)
    targets = np.ones((10, 4), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets)).repeat()
    dataset = dataset.batch(10, drop_remainder=True)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    def get_model():
      x = keras.layers.Input(shape=(3,), name="input")
      y = keras.layers.Dense(4, name="dense")(x)
      model = keras.Model(x, y)
      return model

    with distribution.scope():
      model = get_model()
      optimizer = keras.optimizer_v2.gradient_descent.SGD(0.1, momentum=0.01)

    @def_function.function
    def train_step(iterator):

      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        optimizer.apply_gradients(zip(grads, model.variables))

      distribution.run(step_fn, args=(next(iterator),))

    @def_function.function
    def train_steps(iterator):
      for _ in math_ops.range(10):
        train_step(iterator)

    train_steps(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies,
          mode=["eager"]
      ))
  def test_customized_tf_module_experimental_run(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      model = CustomModel()

    @def_function.function
    def train_step(iterator):

      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        return grads

      outputs = distribution.run(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.all_strategies, mode=["eager"]))
  def test_reduce_loss(self, distribution):
    inputs = np.zeros((10, 4), dtype=np.float32)
    targets = np.zeros((10, 1), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets))
    dataset = dataset.batch(10, drop_remainder=False)
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    with distribution.scope():
      x = keras.layers.Input(shape=(4), name="input")
      y = keras.layers.Dense(3, name="dense")(x)
      model = keras.Model(x, y)

    @def_function.function
    def train_step(iterator):

      def step_fn(inputs):
        images, targets = inputs
        outputs = model(images)
        loss = keras.losses.sparse_categorical_crossentropy(targets, outputs)
        return loss

      return distribution.run(step_fn, args=(next(iterator),))

    loss = train_step(input_iterator)
    loss = distribution.reduce(reduce_util.ReduceOp.MEAN, loss, axis=0)

  @combinations.generate(
      combinations.combine(
          distribution=strategy_combinations.tpu_strategies, mode=["eager"]))
  def test_tf_function_experimental_compile(self, distribution):
    dataset = self._get_dataset()
    input_iterator = iter(distribution.experimental_distribute_dataset(dataset))

    class CustomDense(keras.layers.Layer):

      def __init__(self, num_outputs):
        super(CustomDense, self).__init__()
        self.num_outputs = num_outputs

      def build(self, input_shape):
        self.kernel = self.add_variable(
            "kernel", shape=[int(input_shape[-1]), self.num_outputs])

      @def_function.function(experimental_compile=True)
      def call(self, inputs):
        return math_ops.matmul(inputs, self.kernel)

    with distribution.scope():
      x = keras.layers.Input(shape=(3,))
      y = CustomDense(4)(x)
      model = keras.Model(x, y)

    @def_function.function
    def train_step(iterator):
      def step_fn(inputs):
        images, targets = inputs
        with backprop.GradientTape() as tape:
          outputs = model(images)
          loss = math_ops.reduce_sum(outputs - targets)
        grads = tape.gradient(loss, model.variables)
        return grads

      outputs = distribution.run(
          step_fn, args=(next(iterator),))
      return nest.map_structure(distribution.experimental_local_results,
                                outputs)

    train_step(input_iterator)

  def _get_dataset(self):
    inputs = np.zeros((10, 3), dtype=np.float32)
    targets = np.zeros((10, 4), dtype=np.float32)
    dataset = dataset_ops.Dataset.from_tensor_slices((inputs, targets))
    dataset = dataset.repeat(100)
    dataset = dataset.batch(10, drop_remainder=True)
    return dataset

  def _get_model(self):
    x = keras.layers.Input(shape=(3,), name="input")
    y = keras.layers.Dense(4, name="dense")(x)
    model = keras.Model(x, y)
    return model


if __name__ == "__main__":
  test.main()
