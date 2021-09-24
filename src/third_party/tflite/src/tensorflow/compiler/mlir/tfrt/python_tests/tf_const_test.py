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
"""Tests for Tensorflow -> CPURT compilation."""

import numpy as np

import unittest
from tensorflow.compiler.mlir.tfrt.jit.python_binding import tf_cpurt

cpurt = tf_cpurt.TfCpurtExecutor()


class TfConstTest(googletest.TestCase):

  def test_const_i32(self):
    mlir_function = """
      func @test() -> tensor<1xi32> {
        %0 = "tf.Const"() {
               value = dense<1> : tensor<1xi32>
             } : () -> tensor<1xi32>
        return %0 : tensor<1xi32>
      }"""

    compiled = cpurt.compile(mlir_function, 'test')
    [res] = cpurt.execute(compiled, [])
    np.testing.assert_allclose(res, 1, rtol=0.0)

  def test_constant_folding_i32(self):
    mlir_function = """
      func @test() -> tensor<2xi32> {
        %0 = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
        %1 = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
        %2 = "tf.Pack"(%0, %1) {axis = 0 : i64}
             : (tensor<i32>, tensor<i32>) -> tensor<2xi32>
        return %2 : tensor<2xi32>
      }"""

    compiled = cpurt.compile(mlir_function, 'test')
    [res] = cpurt.execute(compiled, [])
    np.testing.assert_allclose(res, [0, 1], rtol=0.0)

if __name__ == '__main__':
  googletest.main()
