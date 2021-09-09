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
"""Test configs for gather."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import tensorflow.compat.v1 as tf
from tensorflow.lite.testing.zip_test_utils import create_tensor_data
from tensorflow.lite.testing.zip_test_utils import make_zip_of_tests
from tensorflow.lite.testing.zip_test_utils import register_make_test_function


@register_make_test_function()
def make_gather_tests(options):
  """Make a set of tests to do gather."""

  test_parameters = [
      {
          "params_dtype": [tf.float32, tf.int32, tf.int64],
          "params_shape": [[1, 2, 20]],
          "indices_dtype": [tf.int32, tf.int64],
          "indices_shape": [[3], [5]],
          "axis": [-1, 0, 1],
          "constant_params": [False, True],
      },
      {
          "params_dtype": [tf.string],
          "params_shape": [[8]],
          "indices_dtype": [tf.int32],
          "indices_shape": [[3], [3, 2]],
          "axis": [0],
          "constant_params": [False, True],
      }
  ]

  def build_graph(parameters):
    """Build the gather op testing graph."""
    inputs = []

    if parameters["constant_params"]:
      params = create_tensor_data(parameters["params_dtype"],
                                  parameters["params_shape"])
    else:
      params = tf.compat.v1.placeholder(
          dtype=parameters["params_dtype"],
          name="params",
          shape=parameters["params_shape"])
      inputs.append(params)

    indices = tf.compat.v1.placeholder(
        dtype=parameters["indices_dtype"],
        name="indices",
        shape=parameters["indices_shape"])
    inputs.append(indices)
    axis = min(len(parameters["params_shape"]), parameters["axis"])
    out = tf.gather(params, indices, axis=axis)
    return inputs, [out]

  def build_inputs(parameters, sess, inputs, outputs):
    input_values = []
    if not parameters["constant_params"]:
      params = create_tensor_data(parameters["params_dtype"],
                                  parameters["params_shape"])
      input_values.append(params)
    indices = create_tensor_data(parameters["indices_dtype"],
                                 parameters["indices_shape"], 0,
                                 parameters["params_shape"][0] - 1)
    input_values.append(indices)
    return input_values, sess.run(
        outputs, feed_dict=dict(zip(inputs, input_values)))

  make_zip_of_tests(
      options,
      test_parameters,
      build_graph,
      build_inputs,
      expected_tf_failures=0)
