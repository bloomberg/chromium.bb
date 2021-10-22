# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for function_trace_type."""

import timeit

from tensorflow.python import keras
from tensorflow.python.eager import function
from tensorflow.python.eager import function_trace_type
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_spec
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import nn_ops
from tensorflow.python.ops import variables
from tensorflow.python.platform import test


class CacheKeyGenerationBenchmark(test.Benchmark):

  def benchmarkTensor(self):
    shapes = [[1], [2, 19], [5, 11, 24], [4, 5, 9, 23]]
    tensors = []
    for s in shapes:
      tensors.append(array_ops.zeros(s))

    def encode_tensors(tensors):
      function_trace_type.make_input_signature(tensors, False, False)

    iterations = 100000
    t = timeit.timeit(lambda: encode_tensors(tensors), number=iterations)
    self.report_benchmark(
        name='tensor_cache_key_generation', iters=iterations, wall_time=t)

  def benchmarkTensorSpec(self):
    shapes = [[1], [2, 19], [5, 11, 24], [4, 5, 9, 23]]
    tensor_specs = []
    for s in shapes:
      tensor_specs.append(tensor_spec.TensorSpec(s, dtypes.int32))

    def encode_tensor_specs(tensor_specs):
      function_trace_type.make_input_signature(tensor_specs, False, False)

    iterations = 100000
    t = timeit.timeit(
        lambda: encode_tensor_specs(tensor_specs), number=iterations)
    self.report_benchmark(
        name='tensor_spec_cache_key_generation', iters=iterations, wall_time=t)

  def benchmarkVariable(self):
    var_list = [
        variables.Variable(1.0),
        variables.Variable(1),
        variables.Variable([1])
    ]

    def encode_variables(var_list):
      function_trace_type.make_input_signature(var_list, False, False)

    iterations = 100000
    t = timeit.timeit(lambda: encode_variables(var_list), number=iterations)
    self.report_benchmark(
        name='variable_cache_key_generation', iters=iterations, wall_time=t)

  def benchmarkKerasModel(self):
    inputs = keras.Input(shape=(3,))
    x = keras.layers.Dense(4, activation=nn_ops.relu)(inputs)
    outputs = keras.layers.Dense(5, activation=nn_ops.softmax)(x)
    model = keras.Model(inputs=inputs, outputs=outputs)

    def encode_model(model):
      function_trace_type.make_input_signature(model, False, False)

    iterations = 100000
    t = timeit.timeit(lambda: encode_model(model), number=iterations)
    self.report_benchmark(
        name='keras_model_cache_key_generation', iters=iterations, wall_time=t)

  def benchmarkCacheKeyLookup(self):

    @function.defun
    def defined(t):
      return t

    call_arg_list = [
        1,
        array_ops.zeros([5, 13]),
        array_ops.zeros([9, 22, 24]),
        array_ops.zeros([5, 13, 2])
    ]

    for c in call_arg_list:
      defined(c)

    lookup_call_arg = array_ops.zeros([5, 13])

    iterations = 10000
    t = timeit.timeit(stmt=lambda: defined(lookup_call_arg), number=iterations)

    self.report_benchmark(
        name='cache_key_lookup', iters=iterations, wall_time=t)


if __name__ == '__main__':
  ops.enable_eager_execution()
  test.main()
