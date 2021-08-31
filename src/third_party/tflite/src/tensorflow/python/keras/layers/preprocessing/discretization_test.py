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
"""Tests for Keras discretization preprocessing layer."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.python import keras

from tensorflow.python.framework import dtypes
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.keras import keras_parameterized
from tensorflow.python.keras.layers.preprocessing import discretization
from tensorflow.python.keras.layers.preprocessing import preprocessing_test_utils
from tensorflow.python.ops.ragged import ragged_factory_ops
from tensorflow.python.platform import test


@keras_parameterized.run_all_keras_modes
class DiscretizationTest(keras_parameterized.TestCase,
                         preprocessing_test_utils.PreprocessingLayerTest):

  def test_bucketize_with_explicit_buckets_integer(self):
    input_array = np.array([[-1.5, 1.0, 3.4, .5], [0.0, 3.0, 1.3, 0.0]])

    expected_output = [[0, 2, 3, 1], [1, 3, 2, 1]]
    expected_output_shape = [None, None]

    input_data = keras.Input(shape=(None,))
    layer = discretization.Discretization(bins=[0., 1., 2.])
    bucket_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, bucket_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=bucket_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bucketize_with_explicit_buckets_int_input(self):
    input_array = np.array([[-1, 1, 3, 0], [0, 3, 1, 0]], dtype=np.int64)

    expected_output = [[0, 2, 3, 1], [1, 3, 2, 1]]
    expected_output_shape = [None, None]

    input_data = keras.Input(shape=(None,), dtype=dtypes.int64)
    layer = discretization.Discretization(bins=[-.5, 0.5, 1.5])
    bucket_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, bucket_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=bucket_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bucketize_with_explicit_buckets_sparse_float_input(self):
    indices = [[0, 1], [0, 2], [1, 1]]
    input_array = sparse_tensor.SparseTensor(
        indices=indices, values=[-1.5, 1.0, 3.4], dense_shape=[2, 3])
    expected_output = [0, 2, 3]
    input_data = keras.Input(shape=(3,), dtype=dtypes.float32, sparse=True)
    layer = discretization.Discretization(bins=[-.5, 0.5, 1.5])
    bucket_data = layer(input_data)

    model = keras.Model(inputs=input_data, outputs=bucket_data)
    output_dataset = model.predict(input_array, steps=1)
    self.assertAllEqual(indices, output_dataset.indices)
    self.assertAllEqual(expected_output, output_dataset.values)

  def test_bucketize_with_explicit_buckets_ragged_float_input(self):
    input_array = ragged_factory_ops.constant([[-1.5, 1.0, 3.4, .5],
                                               [0.0, 3.0, 1.3]])

    expected_output = [[0, 2, 3, 1], [1, 3, 2]]
    expected_output_shape = [None, None]

    input_data = keras.Input(shape=(None,), ragged=True)
    layer = discretization.Discretization(bins=[0., 1., 2.])
    bucket_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, bucket_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=bucket_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bucketize_with_explicit_buckets_ragged_int_input(self):
    input_array = ragged_factory_ops.constant([[-1, 1, 3, 0], [0, 3, 1]],
                                              dtype=dtypes.int64)

    expected_output = [[0, 2, 3, 1], [1, 3, 2]]
    expected_output_shape = [None, None]

    input_data = keras.Input(shape=(None,), ragged=True, dtype=dtypes.int64)
    layer = discretization.Discretization(bins=[-.5, 0.5, 1.5])
    bucket_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, bucket_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=bucket_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bucketize_with_explicit_buckets_sparse_int_input(self):
    indices = [[0, 1], [0, 2], [1, 1]]
    input_array = sparse_tensor.SparseTensor(
        indices=indices, values=[-1, 1, 3], dense_shape=[2, 3])
    expected_output = [0, 2, 3]
    input_data = keras.Input(shape=(3,), dtype=dtypes.int32, sparse=True)
    layer = discretization.Discretization(bins=[-.5, 0.5, 1.5])
    bucket_data = layer(input_data)

    model = keras.Model(inputs=input_data, outputs=bucket_data)
    output_dataset = model.predict(input_array, steps=1)
    self.assertAllEqual(indices, output_dataset.indices)
    self.assertAllEqual(expected_output, output_dataset.values)


if __name__ == "__main__":
  test.main()
