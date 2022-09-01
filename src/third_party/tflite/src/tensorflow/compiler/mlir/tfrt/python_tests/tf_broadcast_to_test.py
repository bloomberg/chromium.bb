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
"""Tests for Tensorflow -> jitrt compilation."""

import numpy as np

from tensorflow.compiler.mlir.tfrt.jit.python_binding import tf_jitrt
from tensorflow.python.platform import test

jitrt = tf_jitrt.TfJitRtExecutor()


class TfBroadcastToTest(test.TestCase):

  def test_broadcast_return(self):
    mlir_function = """
      func.func @test(%arg0: tensor<?xf32>, %arg1: tensor<2xi32>)
           -> (tensor<?x?xf32>, tensor<?x?xf32>) {
        %1 = "tf.BroadcastTo"(%arg0, %arg1)
             : (tensor<?xf32>, tensor<2xi32>) -> tensor<?x?xf32>
        %2 = "tf.Add"(%1, %1)
             : (tensor<?x?xf32>, tensor<?x?xf32>) -> tensor<?x?xf32>
        func.return %1, %2 : tensor<?x?xf32>, tensor<?x?xf32>
      }"""

    compiled = jitrt.compile(mlir_function, 'test')

    arg0 = np.random.uniform(0, 10.0, size=1).astype(np.float32)
    arg1 = np.random.uniform(0, 10, size=2).astype(np.int32)

    [res1, res2] = jitrt.execute(compiled, [arg0, arg1])
    np.testing.assert_allclose(res1, np.broadcast_to(arg0, arg1), atol=0.0)
    np.testing.assert_allclose(res2, np.broadcast_to(arg0, arg1) * 2, atol=0.0)


if __name__ == '__main__':
  test.main()
