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
"""Microbenchmarks for Keras components in eager mode."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import time

from tensorflow.python.eager import context
from tensorflow.python.framework import ops
from tensorflow.python.keras.engine import base_layer
from tensorflow.python.keras.layers import advanced_activations
from tensorflow.python.keras.layers import convolutional
from tensorflow.python.keras.layers import core
from tensorflow.python.keras.layers import embeddings
from tensorflow.python.keras.layers import normalization
from tensorflow.python.ops import array_ops
from tensorflow.python.platform import test
from tensorflow.python.util import tf_inspect


def _run_benchmark(func, num_iters, execution_mode=None):
  ctx = context.context()
  with context.execution_mode(execution_mode):
    # call func to warm up
    func()
    if execution_mode == context.ASYNC:
      ctx.executor.wait()
    start = time.time()
    for _ in range(num_iters):
      func()
    if execution_mode == context.ASYNC:
      ctx.executor.wait()
    end = time.time()

    return end - start


class MicroBenchmarksBase(test.Benchmark):
  """Run and report benchmark results."""

  def run_report(self, run_benchmark, func, num_iters, execution_mode=None):
    """Run and report benchmark results."""
    total_time = run_benchmark(func, num_iters, execution_mode)
    mean_us = total_time * 1e6 / num_iters
    extras = {
        "examples_per_sec": float("{0:.3f}".format(num_iters / total_time)),
        "us_per_example": float("{0:.3f}".format(total_time * 1e6 / num_iters))
    }
    benchmark_name = self._get_benchmark_name()
    self.report_benchmark(
        iters=num_iters, wall_time=mean_us, extras=extras, name=benchmark_name)

  def _get_benchmark_name(self):
    """Mostly copied from benchmark.py _get_name()."""
    stack = tf_inspect.stack()
    name = None
    for frame in stack[::-1]:
      f_locals = frame[0].f_locals
      f_self = f_locals.get("self", None)
      if isinstance(f_self, test.Benchmark):
        name = frame[3]  # Get the method name
        # This is a hack to get around the fact that some methods might have a
        # disable_tfrt decorator around them. In that case a function called
        # 'decorated' wraps the real called function underneath and so we
        # peek one deeper into the stack to get the real name.
        if name == "decorated":
          continue
        else:
          break
    if name is None:
      raise ValueError("Unable to determine calling Benchmark function.")
    if context.is_tfrt_enabled():
      name = name + "_tfrt"
    return name

  def _run(self, func, num_iters, execution_mode=None):
    self.run_report(_run_benchmark, func, num_iters, execution_mode)

  def benchmark_layers_call_overhead(self):

    class OnlyOverheadLayer(base_layer.Layer):

      def call(self, x):
        return x

    layer = OnlyOverheadLayer()
    x = ops.convert_to_tensor([[1.]])

    def fn():
      layer(x)

    self._run(fn, 10000)

  # Naming convention: benchmark_layers_{module_name}_{class}_overhead.
  def benchmark_layers_advanced_activations_leaky_relu_overhead(self):

    layer = advanced_activations.LeakyReLU()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_advanced_activations_prelu_overhead(self):

    layer = advanced_activations.PReLU()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_advanced_activations_elu_overhead(self):

    layer = advanced_activations.ELU()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_advanced_activations_thresholded_relu_overhead(self):

    layer = advanced_activations.ThresholdedReLU()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_advanced_activations_softmax_overhead(self):

    layer = advanced_activations.Softmax()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_advanced_activations_relu_overhead(self):

    layer = advanced_activations.ReLU()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_core_masking_overhead(self):

    layer = core.Masking()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_core_dropout_overhead(self):

    layer = core.Dropout(0.5)
    x = array_ops.ones((1, 1))

    def fn():
      layer(x, training=True)

    self._run(fn, 10000)

  def benchmark_layers_core_flatten_overhead(self):

    layer = core.Flatten()
    x = ops.convert_to_tensor([[[1.]]])

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_core_dense_overhead(self):

    layer = core.Dense(1)
    x = ops.convert_to_tensor([[1.]])

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_convolutional_conv1d_overhead(self):

    layer = convolutional.Conv1D(1, (1,))
    x = array_ops.ones((1, 1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_convolutional_conv2d_overhead(self):

    layer = convolutional.Conv2D(1, (1, 1))
    x = array_ops.ones((1, 1, 1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_convolutional_conv3d_overhead(self):

    layer = convolutional.Conv3D(1, (1, 1, 1))
    x = array_ops.ones((1, 1, 1, 1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_embeddings_embedding_overhead(self):

    layer = embeddings.Embedding(1, 1)
    x = array_ops.zeros((1, 1), dtype="int32")

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_batch_norm_fused_inf(self):

    layer = normalization.BatchNormalization(fused=True)
    x = array_ops.ones((1, 1, 1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_batch_norm_fused_train(self):

    layer = normalization.BatchNormalization(fused=True)
    x = array_ops.ones((1, 1, 1, 1))

    def fn():
      layer(x, training=True)

    self._run(fn, 10000)

  def benchmark_layers_batch_norm_nonfused_inf(self):

    layer = normalization.BatchNormalization(fused=False)
    x = array_ops.ones((1, 1, 1, 1))

    def fn():
      layer(x)

    self._run(fn, 10000)

  def benchmark_layers_batch_norm_nonfused_train(self):

    layer = normalization.BatchNormalization(fused=False)
    x = array_ops.ones((1, 1, 1, 1))

    def fn():
      layer(x, training=True)

    self._run(fn, 10000)

  def benchmark_layers_normalization_layer_normalization_overhead(self):

    layer = normalization.LayerNormalization()
    x = array_ops.ones((1, 1))

    def fn():
      layer(x, training=True)

    self._run(fn, 10000)


if __name__ == "__main__":
  ops.enable_eager_execution()
  test.main()
