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
"""Benchmark for KPL implementation of embedding column with dense inputs."""

from tensorflow.python import keras
from tensorflow.python.compat import v2_compat
from tensorflow.python.eager.def_function import function as tf_function
from tensorflow.python.feature_column import feature_column_v2 as fcv2
from tensorflow.python.framework import dtypes as dt
from tensorflow.python.keras.layers.preprocessing.benchmarks import feature_column_benchmark as fc_bm
from tensorflow.python.ops import math_ops
from tensorflow.python.platform import test as tf_test

# This is required as of 3/2021 because otherwise we drop into graph mode.
v2_compat.enable_v2_behavior()

NUM_REPEATS = 10
BATCH_SIZES = [32, 256]


### KPL AND FC IMPLEMENTATION BENCHMARKS ###
def embedding_varlen(batch_size, max_length):
  """Benchmark a variable-length embedding."""
  # Data and constants.
  embedding_size = 32768
  data = fc_bm.create_data(
      max_length, batch_size * NUM_REPEATS, embedding_size - 1, dtype=int)

  # Keras implementation
  model = keras.Sequential()
  model.add(keras.Input(shape=(None,), name="data", dtype=dt.int64))
  model.add(keras.layers.Embedding(embedding_size, 256))
  model.add(keras.layers.Lambda(lambda x: math_ops.reduce_mean(x, axis=-1)))

  # FC implementation
  fc = fcv2.embedding_column(
      fcv2.categorical_column_with_identity(
          "data", num_buckets=embedding_size - 1),
      dimension=256)

  # Wrap the FC implementation in a tf.function for a fair comparison
  @tf_function()
  def fc_fn(tensors):
    fc.transform_feature(fcv2.FeatureTransformationCache(tensors), None)

  # Benchmark runs
  keras_data = {"data": data.to_tensor(default_value=0)}
  k_avg_time = fc_bm.run_keras(keras_data, model, batch_size, NUM_REPEATS)

  fc_data = {"data": data.to_tensor(default_value=0)}
  fc_avg_time = fc_bm.run_fc(fc_data, fc_fn, batch_size, NUM_REPEATS)

  return k_avg_time, fc_avg_time


class BenchmarkLayer(fc_bm.LayerBenchmark):
  """Benchmark the layer forward pass."""

  def benchmark_layer(self):
    for batch in BATCH_SIZES:
      name = "embedding|dense|batch_%s" % batch
      k_time, f_time = embedding_varlen(batch_size=batch, max_length=256)
      self.report(name, k_time, f_time, NUM_REPEATS)


if __name__ == "__main__":
  tf_test.main()
