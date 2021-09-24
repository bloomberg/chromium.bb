# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
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
r"""Benchmarks on Keras components with different Keras model types."""

import time

import numpy as np

import tensorflow as tf

from tensorflow.python.eager import context
from tensorflow.python.eager.context import get_executor
from tensorflow.python.platform import test
from tensorflow.python.profiler import profiler_v2 as profiler


class SubclassedKerasModel(tf.keras.Model):

  def __init__(self, initializer="ones"):
    super(SubclassedKerasModel, self).__init__()
    self.layer_a = tf.keras.layers.Dense(
        64, kernel_initializer=initializer, bias_initializer="zeros")
    self.layer_b = tf.keras.layers.Dense(
        128, kernel_initializer=initializer, bias_initializer="zeros")
    self.layer_c = tf.keras.layers.Dense(
        256, kernel_initializer=initializer, bias_initializer="zeros")
    self.layer_d = tf.keras.layers.Dense(
        256, kernel_initializer=initializer, bias_initializer="zeros")
    self.layer_e = tf.keras.layers.Dense(
        10, kernel_initializer=initializer, bias_initializer="zeros")

  def call(self, x):
    x = self.layer_a(x)
    x = self.layer_b(x)
    x = self.layer_c(x)
    x = self.layer_d(x)
    return self.layer_e(x)


def make_keras_model(initializer="ones"):
  model_input = tf.keras.Input(shape=(10,))
  x = tf.keras.layers.Dense(
      64, kernel_initializer=initializer, bias_initializer="zeros")(model_input)
  x = tf.keras.layers.Dense(
      128, kernel_initializer=initializer, bias_initializer="zeros")(x)
  x = tf.keras.layers.Dense(
      256, kernel_initializer=initializer, bias_initializer="zeros")(x)
  x = tf.keras.layers.Dense(
      256, kernel_initializer=initializer, bias_initializer="zeros")(x)
  x = tf.keras.layers.Dense(
      10, kernel_initializer=initializer, bias_initializer="zeros")(x)
  return tf.keras.Model(inputs=model_input, outputs=x)


def make_sequential_keras_model(initializer="ones"):
  model = tf.keras.models.Sequential()
  model.add(tf.keras.layers.Dense(
      64, kernel_initializer=initializer, bias_initializer="zeros",
      input_shape=(10,)))
  model.add(tf.keras.layers.Dense(
      128, kernel_initializer=initializer, bias_initializer="zeros"))
  model.add(tf.keras.layers.Dense(
      256, kernel_initializer=initializer, bias_initializer="zeros"))
  model.add(tf.keras.layers.Dense(
      256, kernel_initializer=initializer, bias_initializer="zeros"))
  model.add(tf.keras.layers.Dense(
      10, kernel_initializer=initializer, bias_initializer="zeros"))
  return model


def run_benchmark(func, num_iters, execution_mode=None):
  with context.execution_mode(execution_mode):
    # call func to warm up
    func()
    if execution_mode == context.ASYNC:
      get_executor().wait()
    start = time.time()
    for _ in range(num_iters):
      func()
    if execution_mode == context.ASYNC:
      get_executor().wait()
    end = time.time()

    return end - start


class KerasComponentsBenchmarks(test.Benchmark):

  def _run(self, func, num_iters, execution_mode=None):
    total_time = run_benchmark(func, num_iters, execution_mode)
    mean_us = total_time * 1e6 / num_iters
    self.report_benchmark(
        iters=num_iters,
        wall_time=mean_us,
        metrics=[
            {
                "name": "exp_per_sec",
                "value": float("{0:.3f}".format(num_iters / total_time))
            },
            {
                "name": "us_per_exp",
                "value": float("{0:.3f}".format(total_time * 1e6 / num_iters))
            },
        ])

  def benchmark_keras_model_subclassed(self):
    model = SubclassedKerasModel()
    data = tf.random.uniform((10, 10))

    func = lambda: model(data)  # pylint: disable=not-callable
    # First call is more expensive (creates variables etc.), discount that.
    func()

    # The whole point of this test is to contrast subclassing with
    # the functional style of keras model building, so validate that
    # the models are equivalent.
    assert np.equal(func(), make_keras_model()(data)).all()

    self._run(func, 30000)

  def benchmark_keras_model_functional(self):
    model = make_keras_model()
    data = tf.random.uniform((10, 10))
    func = lambda: model(data)  # pylint: disable=not-callable
    # Symmetry with benchmark_keras_model_subclassed
    func()
    assert np.equal(func(), SubclassedKerasModel()(data)).all()  # pylint: disable=not-callable
    self._run(func, 30000)

  def benchmark_keras_model_sequential(self):
    model = make_sequential_keras_model()
    data = tf.random.uniform((10, 10))
    func = lambda: model(data)
    # Symmetry with benchmark_keras_model_functional
    func()
    assert np.equal(func(), make_keras_model()(data)).all()
    self._run(func, 30000)

  def _benchmark_keras_model_fit(self, model, run_eagerly=False):
    data = tf.random.uniform((10, 10), minval=-1, maxval=1)
    labels = tf.random.uniform((10, 10), minval=-1, maxval=1)
    dataset = tf.data.Dataset.from_tensors((data, labels)).repeat()
    model.compile(
        "sgd",
        loss="mse", run_eagerly=run_eagerly)
    func = lambda: model.fit(dataset, epochs=1, steps_per_epoch=1000, verbose=0)
    # First call is more expensive (creates variables etc.), discount that.
    model.fit(dataset, epochs=1, steps_per_epoch=1, verbose=0)

    self._run(func, 1)

  def _benchmark_keras_model_evaluate(self, model, run_eagerly=False):
    data = tf.random.uniform((10, 10), minval=-1, maxval=1)
    labels = tf.random.uniform((10, 10), minval=-1, maxval=1)
    dataset = tf.data.Dataset.from_tensors((data, labels)).repeat()
    model.compile(
        "sgd",
        loss="mse", run_eagerly=run_eagerly)
    func = lambda: model.evaluate(dataset, steps=1000, verbose=0)
    # First call is more expensive (creates variables etc.), discount that.
    model.evaluate(dataset, steps=1, verbose=0)

    self._run(func, 1)

  def _benchmark_keras_model_predict(self, model, run_eagerly=False):
    data = tf.random.uniform((10, 10), minval=-1, maxval=1)
    dataset = tf.data.Dataset.from_tensors(data).repeat()
    model.compile(
        "sgd",
        loss="mse", run_eagerly=run_eagerly)
    func = lambda: model.predict(dataset, steps=1000, verbose=0)
    # First call is more expensive (creates variables etc.), discount that.
    model.predict(dataset, steps=1, verbose=0)

    self._run(func, 1)

  def benchmark_keras_model_subclassed_fit(self):
    model = SubclassedKerasModel(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model)

  def benchmark_keras_model_subclassed_fit_graph_mode(self):
    with context.graph_mode():
      model = SubclassedKerasModel(initializer="glorot_uniform")
      self._benchmark_keras_model_fit(model)

  def benchmark_keras_model_subclassed_fit_run_model_eagerly(self):
    model = SubclassedKerasModel(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model, run_eagerly=True)

  def benchmark_keras_model_functional_fit(self):
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model)

  def benchmark_keras_model_functional_fit_graph_mode(self):
    with context.graph_mode():
      model = make_keras_model(initializer="glorot_uniform")
      self._benchmark_keras_model_fit(model)

  def benchmark_keras_model_functional_fit_graph_mode_with_profiler(self):
    profiler.start("")
    with context.graph_mode():
      model = make_keras_model(initializer="glorot_uniform")
      self._benchmark_keras_model_fit(model)
    profiler.stop(save=False)

  def benchmark_keras_model_functional_fit_run_model_eagerly(self):
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model, run_eagerly=True)

  def benchmark_keras_model_functional_fit_run_model_eagerly_with_profiler(
      self):
    profiler.start("")
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model, run_eagerly=True)
    profiler.stop(save=False)

  def benchmark_keras_model_sequential_fit(self):
    model = make_sequential_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model)

  def benchmark_keras_model_sequential_fit_graph_mode(self):
    with context.graph_mode():
      model = make_sequential_keras_model(initializer="glorot_uniform")
      self._benchmark_keras_model_fit(model)

  def benchmark_keras_model_sequential_fit_run_model_eagerly(self):
    model = make_sequential_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_fit(model, run_eagerly=True)

  def benchmark_keras_model_subclassed_evaluate(self):
    model = SubclassedKerasModel(initializer="glorot_uniform")
    self._benchmark_keras_model_evaluate(model)

  def benchmark_keras_model_subclassed_evaluate_run_model_eagerly(self):
    model = SubclassedKerasModel(initializer="glorot_uniform")
    self._benchmark_keras_model_evaluate(model, run_eagerly=True)

  def benchmark_keras_model_functional_evaluate(self):
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_evaluate(model)

  def benchmark_keras_model_functional_evaluate_run_model_eagerly(self):
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_evaluate(model, run_eagerly=True)

  def benchmark_keras_model_sequential_evaluate(self):
    model = make_sequential_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_evaluate(model)

  def benchmark_keras_model_sequential_evaluate_run_model_eagerly(self):
    model = make_sequential_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_evaluate(model, run_eagerly=True)

  def benchmark_keras_model_subclassed_predict(self):
    model = SubclassedKerasModel(initializer="glorot_uniform")
    self._benchmark_keras_model_predict(model)

  def benchmark_keras_model_subclassed_predict_run_model_eagerly(self):
    model = SubclassedKerasModel(initializer="glorot_uniform")
    self._benchmark_keras_model_predict(model, run_eagerly=True)

  def benchmark_keras_model_functional_predict(self):
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_predict(model)

  def benchmark_keras_model_functional_predict_run_model_eagerly(self):
    model = make_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_predict(model, run_eagerly=True)

  def benchmark_keras_model_sequential_predict(self):
    model = make_sequential_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_predict(model)

  def benchmark_keras_model_sequential_predict_run_model_eagerly(self):
    model = make_sequential_keras_model(initializer="glorot_uniform")
    self._benchmark_keras_model_predict(model, run_eagerly=True)


if __name__ == "__main__":
  test.main()
