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
"""Benchmarks for `tf.data.Dataset.map()`."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.data.benchmarks import benchmark_base
from tensorflow.python.data.experimental.ops import stats_aggregator
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.framework import constant_op
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import map_fn as map_fn
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import random_ops


# TODO(b/119837791): Add eager benchmarks.
class MapBenchmark(benchmark_base.DatasetBenchmarkBase):
  """Benchmarks for `tf.data.Dataset.map()`."""

  def benchmark_chain_of_maps(self):

    def benchmark_helper(chain_length, fn, use_inter_op_parallelism, label):
      dataset = dataset_ops.Dataset.range(10000)
      for _ in range(chain_length):
        dataset = dataset_ops.MapDataset(
            dataset, fn, use_inter_op_parallelism=use_inter_op_parallelism)
      self.run_and_report_benchmark(
          dataset,
          num_elements=10000,
          name="chain_length_%d%s" % (chain_length, label))

    chain_lengths = [0, 1, 2, 5, 10, 20, 50]
    for chain_length in chain_lengths:
      benchmark_helper(chain_length, lambda x: x + 1, True, "")
      benchmark_helper(chain_length, lambda x: x + 1, False, "_single_threaded")
      benchmark_helper(chain_length, lambda x: x, True, "_short_circuit")

  def benchmark_map_fan_out(self):
    fan_outs = [1, 2, 5, 10, 20, 50, 100]

    def benchmark_helper(fan_out, fn, use_inter_op_parallelism, label):
      dataset = dataset_ops.Dataset.from_tensors(
          tuple(0 for _ in range(fan_out))).repeat(None)
      dataset = dataset_ops.MapDataset(
          dataset, fn, use_inter_op_parallelism=use_inter_op_parallelism)
      self.run_and_report_benchmark(
          dataset,
          num_elements=10000,
          name="fan_out_%d%s" % (fan_out, label))

    for fan_out in fan_outs:
      benchmark_helper(fan_out, lambda *xs: [x + 1 for x in xs], True, "")
      benchmark_helper(fan_out, lambda *xs: [x + 1 for x in xs], False,
                       "_single_threaded")
      benchmark_helper(fan_out, lambda *xs: xs, True, "_short_circuit")

  def benchmark_stats(self):
    for stats in [True, False]:
      dataset = dataset_ops.Dataset.range(1000).repeat()
      dataset = dataset.map(lambda x: x + 1, num_parallel_calls=32)
      options = dataset_ops.Options()
      options.experimental_deterministic = False
      if stats:
        aggregator = stats_aggregator.StatsAggregator()
        options.experimental_stats.aggregator = aggregator
      dataset = dataset.with_options(options)
      self.run_and_report_benchmark(
          dataset, num_elements=10000, name="stats_%s" % stats)

  def benchmark_sequential_control_flow(self):
    dataset = dataset_ops.Dataset.from_tensors(100000)

    def fn(x):
      i = constant_op.constant(0)

      def body(i, x):
        return math_ops.add(i, 1), x

      return control_flow_ops.while_loop(math_ops.less, body, [i, x])

    dataset = dataset.map(fn)
    self.run_and_report_benchmark(
        dataset,
        num_elements=1,
        name="sequential_control_flow",
        apply_default_optimizations=True)

  def benchmark_parallel_control_flow(self):
    dataset = dataset_ops.Dataset.from_tensors(
        random_ops.random_uniform([100, 10000000]))

    def fn(x):
      return map_fn.map_fn(
          lambda y: y * array_ops.transpose(y), x, parallel_iterations=10)

    dataset = dataset.map(fn)
    self.run_and_report_benchmark(
        dataset,
        num_elements=1,
        name="parallel_control_flow",
        apply_default_optimizations=True)


if __name__ == "__main__":
  benchmark_base.test.main()
