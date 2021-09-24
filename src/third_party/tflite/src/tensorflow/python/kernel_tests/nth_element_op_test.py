# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.python.eager import backprop
from tensorflow.python.eager import context
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import gradients_impl
from tensorflow.python.ops import nn_ops
import tensorflow.python.ops.nn_grad  # pylint: disable=unused-import
from tensorflow.python.platform import test


class NthElementTest(test.TestCase):

  def _validateNthElement(self, inputs, dtype, n, reverse, expected_values):
    np_expected_values = np.array(expected_values)
    with self.cached_session(use_gpu=False) as sess:
      inputs_op = ops.convert_to_tensor(inputs, dtype=dtype)
      values_op = nn_ops.nth_element(inputs_op, n, reverse=reverse)
      values = self.evaluate(values_op)

      self.assertShapeEqual(np_expected_values, values_op)
      self.assertAllClose(np_expected_values, values)

  def testExample1(self):
    inputs = [2.2, 4.4, 1.1, 5.5, 3.3]
    self._validateNthElement(inputs, dtypes.float32, 1, False, 2.2)
    self._validateNthElement(inputs, dtypes.float32, 1, True, 4.4)

  def testExample2(self):
    inputs = [[2.2, 4.4, 1.1], [5.5, 3.3, 6.6]]
    self._validateNthElement(inputs, dtypes.float64, 2, False, [4.4, 6.6])
    self._validateNthElement(inputs, dtypes.float64, 2, True, [1.1, 3.3])

  def testExample3(self):
    inputs = [[[2, 4, 1], [5, -3, 6]],
              [[7, 9, -8], [9, 0, 4]]]
    self._validateNthElement(inputs, dtypes.int32, 0, False,
                             [[1, -3], [-8, 0]])
    self._validateNthElement(inputs, dtypes.int64, 0, True,
                             [[4, 6], [9, 9]])

  def _testFloatLargeInput(self, input_shape):
    inputs = np.random.random_sample(input_shape)
    n = np.random.randint(input_shape[-1])
    sort_inputs = np.sort(inputs)
    expected_values = sort_inputs[..., n]
    self._validateNthElement(
        inputs, dtypes.float32, n, False, expected_values)
    expected_values = sort_inputs[..., ::-1][..., n]
    self._validateNthElement(
        inputs, dtypes.float64, n, True, expected_values)

  def _testIntLargeInput(self, input_shape):
    inputs = np.random.randint(-1e3, 1e3, input_shape)
    n = np.random.randint(input_shape[-1])
    sort_inputs = np.sort(inputs)
    expected_values = sort_inputs[..., n]
    self._validateNthElement(
        inputs, dtypes.int32, n, False, expected_values)
    expected_values = sort_inputs[..., ::-1][..., n]
    self._validateNthElement(
        inputs, dtypes.int64, n, True, expected_values)

  def _testLargeInput(self, input_shape):
    self._testFloatLargeInput(input_shape)
    self._testIntLargeInput(input_shape)

  def testLargeInput(self):
    self._testLargeInput([1])
    self._testLargeInput([10])
    self._testLargeInput([5, 10])
    self._testLargeInput([50, 100])
    self._testLargeInput([50, 10000])
    self._testLargeInput([50, 10, 100])
    self._testLargeInput([50, 10, 10, 100])

  def _testEnumerateN(self, input_shape):
    inputs = np.random.random_sample(input_shape)
    sort_inputs = np.sort(inputs)
    for n in range(input_shape[-1]):
      expected_values = sort_inputs[..., n]
      self._validateNthElement(
          inputs, dtypes.float32, n, False, expected_values)
      expected_values = sort_inputs[..., ::-1][..., n]
      self._validateNthElement(
          inputs, dtypes.float64, n, True, expected_values)

  def testEnumerateN(self):
    self._testEnumerateN([1])
    self._testEnumerateN([10])
    self._testEnumerateN([10, 10])
    self._testEnumerateN([10, 10, 10])
    self._testEnumerateN([10, 10, 10, 10])

  def testInvalidInput(self):
    with self.assertRaisesRegex((ValueError, errors.InvalidArgumentError),
                                "at least rank 1 but is rank 0"):
      nn_ops.nth_element(5, 0)

    # Test with placeholders
    with ops.Graph().as_default():
      with self.session(use_gpu=False):
        v = array_ops.placeholder(dtype=dtypes.int32)
        with self.assertRaisesOpError("at least rank 1 but is rank 0"):
          nn_ops.nth_element(v, 0).eval(feed_dict={v: 5})

  def testInvalidN(self):
    with self.assertRaisesRegex((ValueError, errors.InvalidArgumentError),
                                "non-negative but is -1"):
      nn_ops.nth_element([5], -1)
    with self.assertRaisesRegex((ValueError, errors.InvalidArgumentError),
                                "scalar but has rank 1"):
      nn_ops.nth_element([5, 6, 3], [1])

    # Test with placeholders
    with ops.Graph().as_default():
      with self.session(use_gpu=False):
        n = array_ops.placeholder(dtypes.int32)
        values = nn_ops.nth_element([5], n)
        with self.assertRaisesOpError("non-negative but is -1"):
          values.eval(feed_dict={n: -1})

  def testNTooLarge(self):
    inputs = [[0.1, 0.2], [0.3, 0.4]]
    with self.assertRaisesRegex((ValueError, errors.InvalidArgumentError),
                                "must have last dimension > n = 2"):
      nn_ops.nth_element(inputs, 2)

    # Test with placeholders
    with ops.Graph().as_default():
      with self.session(use_gpu=False):
        n = array_ops.placeholder(dtypes.int32)
        values = nn_ops.nth_element(inputs, n)
        with self.assertRaisesOpError("must have last dimension > n = 2"):
          values.eval(feed_dict={n: 2})

  def testGradients(self):
    x = [
        [2., -1., 1000., 3., 1000.],
        [1., 5., 2., 4., 3.],
        [2., 2., 2., 2., 2.],
    ]
    grad_ys = [[-1., 2., 5.]]
    result = [
        [0, 0, -0.5, 0, -0.5],
        [0, 0, 0, 2, 0],
        [1, 1, 1, 1, 1],
    ]
    if context.executing_eagerly():
      inputs = ops.convert_to_tensor(x)
      with backprop.GradientTape() as tape:
        tape.watch(inputs)
        values = nn_ops.nth_element(inputs, 3)
      grad = tape.gradient(values, inputs, ops.convert_to_tensor(grad_ys))
      self.assertAllClose(grad[0], result)

    # Test with tf.gradients
    with ops.Graph().as_default():
      with self.session(use_gpu=False) as sess:
        inputs = array_ops.placeholder(dtypes.float32, shape=[3, 5])
        values = nn_ops.nth_element(inputs, 3)
        grad = sess.run(
            gradients_impl.gradients(values, inputs, grad_ys=grad_ys),
            feed_dict={inputs: x})
    self.assertAllClose(grad[0], result)



if __name__ == "__main__":
  test.main()
