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
"""Tests for mfcc_ops."""

from absl.testing import parameterized

from tensorflow.python.eager import context
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import tensor_shape
from tensorflow.python.framework import test_util
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import random_ops
from tensorflow.python.ops.signal import mfcc_ops
from tensorflow.python.platform import test


# TODO(rjryan): We have no open source tests for MFCCs at the moment. Internally
# at Google, this code is tested against a reference implementation that follows
# HTK conventions.
@test_util.run_all_in_graph_and_eager_modes
class MFCCTest(test.TestCase, parameterized.TestCase):

  def test_error(self):
    # num_mel_bins must be positive.
    with self.assertRaises(ValueError):
      signal = array_ops.zeros((2, 3, 0))
      mfcc_ops.mfccs_from_log_mel_spectrograms(signal)

  @parameterized.parameters(dtypes.float32, dtypes.float64)
  def test_basic(self, dtype):
    """A basic test that the op runs on random input."""
    signal = random_ops.random_normal((2, 3, 5), dtype=dtype)
    self.evaluate(mfcc_ops.mfccs_from_log_mel_spectrograms(signal))

  def test_unknown_shape(self):
    """A test that the op runs when shape and rank are unknown."""
    if context.executing_eagerly():
      return
    signal = array_ops.placeholder_with_default(
        random_ops.random_normal((2, 3, 5)), tensor_shape.TensorShape(None))
    self.assertIsNone(signal.shape.ndims)
    self.evaluate(mfcc_ops.mfccs_from_log_mel_spectrograms(signal))

if __name__ == "__main__":
  test.main()
