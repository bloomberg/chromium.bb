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
"""Test configs for topk."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
import tensorflow.compat.v1 as tf
from tensorflow.lite.testing.zip_test_utils import create_tensor_data
from tensorflow.lite.testing.zip_test_utils import make_zip_of_tests
from tensorflow.lite.testing.zip_test_utils import register_make_test_function


@register_make_test_function()
def make_topk_tests(options):
  """Make a set of tests to do topk."""

  test_parameters = [{
      "input_dtype": [tf.float32, tf.int32],
      "input_shape": [[10], [5, 20]],
      "input_k": [None, 1, 3],
  }]

  def build_graph(parameters):
    """Build the topk op testing graph."""
    input_value = tf.compat.v1.placeholder(
        dtype=parameters["input_dtype"],
        name="input",
        shape=parameters["input_shape"])
    if parameters["input_k"] is not None:
      k = tf.compat.v1.placeholder(dtype=tf.int32, name="input_k", shape=[])
      inputs = [input_value, k]
    else:
      k = tf.constant(3, name="k")
      inputs = [input_value]
    out = tf.nn.top_k(input_value, k)
    return inputs, [out[1]]

  def build_inputs(parameters, sess, inputs, outputs):
    input_value = create_tensor_data(parameters["input_dtype"],
                                     parameters["input_shape"])
    if parameters["input_k"] is not None:
      k = np.array(parameters["input_k"], dtype=np.int32)
      return [input_value, k], sess.run(
          outputs, feed_dict=dict(zip(inputs, [input_value, k])))
    else:
      return [input_value], sess.run(
          outputs, feed_dict=dict(zip(inputs, [input_value])))

  make_zip_of_tests(options, test_parameters, build_graph, build_inputs)
