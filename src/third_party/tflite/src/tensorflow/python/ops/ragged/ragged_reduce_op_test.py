# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for ragged_math_ops.reduce_<AGGREGATE> ops."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import parameterized
import numpy as np

from tensorflow.python.eager import context
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import test_util
from tensorflow.python.ops import array_ops
from tensorflow.python.ops.ragged import ragged_factory_ops
from tensorflow.python.ops.ragged import ragged_math_ops
from tensorflow.python.platform import googletest

_MAX_INT32 = dtypes.int32.max
_MIN_INT32 = dtypes.int32.min
_NAN = np.nan


def mean(*values):
  return 1.0 * sum(values) / len(values)


def variance(*values):
  return np.var(values, dtype=np.float64)


def std(*values):
  return np.std(values, dtype=np.float64)


@test_util.run_all_in_graph_and_eager_modes
class RaggedReduceOpsTest(test_util.TensorFlowTestCase, parameterized.TestCase):

  @parameterized.parameters(
      #=========================================================================
      # Docstring examples.  RaggedTensor for testing is:
      #   [[3, 1, 4],
      #    [1, 5,  ],
      #    [9,     ],
      #    [2, 6   ]]
      #=========================================================================
      # keepdims=True
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=False,
          expected=[15, 12, 4]  # = [3+1+9+2, 1+5+6, 4]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=-2,
          keepdims=False,
          expected=[15, 12, 4]  # = [3+1+9+2, 1+5+6, 4]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=False,
          expected=[8, 6, 9, 8]  # = [3+1+4, 1+5, 9, 2+6]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=-1,
          keepdims=False,
          expected=[8, 6, 9, 8]  # = [3+1+4, 1+5, 9, 2+6]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=False,
          expected=[54, 30, 4]  # = [3*1*9*2, 1*5*6, 4]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=False,
          expected=[12, 5, 9, 12]  # = [3*1*4, 1*5, 9, 2*6]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=False,
          expected=[1, 1, 4]  # = [min(3, 1, 9, 2), min(1, 5, 6), 4]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=False,
          expected=[1, 1, 9, 2]  # = [min(3, 1, 4), min(1, 5), 9, min(2, 6)]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=False,
          expected=[9, 6, 4]  # = [max(3, 1, 9, 2), max(1, 5, 6), 4]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=False,
          expected=[4, 5, 9, 6]  # = [max(3, 1, 4), max(1, 5), 9, max(2, 6)]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=False,
          expected=[3.75, 4, 4]  # = [mean(3, 1, 9, 2), mean(1, 5, 6), 4]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[3, 1, 4], [1, 1], [9], [2, 1]],
          axis=0,
          keepdims=False,
          expected=[9.6875, 0.0, 0.0]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[3, 1, 4], [3, 1], [2], [2, 1]],
          axis=0,
          keepdims=False,
          expected=[0.5, 0., 0.]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_any,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=0,
          keepdims=False,
          expected=[True, True, False, True]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_any,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=1,
          keepdims=False,
          expected=[True, True, True]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_all,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=0,
          keepdims=False,
          expected=[False, True, False, True]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_all,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=1,
          keepdims=False,
          expected=[True, False, False]),
      # keepdims=True
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=True,
          expected=[[15, 12, 4]]  # = [[3+1+9+2, 1+5+6, 4]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=-2,
          keepdims=True,
          expected=[[15, 12, 4]]  # = [[3+1+9+2, 1+5+6, 4]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=True,
          expected=[[8], [6], [9], [8]]  # = [[3+1+4], [1+5], [9], [2+6]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=-1,
          keepdims=True,
          expected=[[8], [6], [9], [8]]  # = [[3+1+4], [1+5], [9], [2+6]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=True,
          expected=[[54, 30, 4]]  # = [[3*1*9*2, 1*5*6, 4]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=True,
          expected=[[12], [5], [9], [12]]  # = [[3*1*4], [1*5], [9], [2*6]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=True,
          expected=[[1, 1, 4]]  # = [[min(3, 1, 9, 2), min(1, 5, 6), 4]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=True,
          expected=[[1], [1], [9],
                    [2]]  # = [[min(3, 1, 4)], [min(1, 5)], [9], [min(2, 6)]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=True,
          expected=[[9, 6, 4]]  # = [[max(3, 1, 9, 2), max(1, 5, 6), 4]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=1,
          keepdims=True,
          expected=[[4], [5], [9],
                    [6]]  # = [[max(3, 1, 4)], [max(1, 5)], [9], [max(2, 6)]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[3, 1, 4], [1, 5], [9], [2, 6]],
          axis=0,
          keepdims=True,
          expected=[[3.75, 4, 4]]  # = [[mean(3, 1, 9, 2), mean(1, 5, 6), 4]]
      ),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[3, 1, 4], [1, 1], [9], [2, 1]],
          axis=0,
          keepdims=True,
          expected=[[9.6875, 0., 0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[3, 1, 4], [3, 1], [2], [2, 1]],
          axis=0,
          keepdims=True,
          expected=[[0.5, 0., 0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_any,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=0,
          keepdims=True,
          expected=[[True, True, False, True]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_any,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=1,
          keepdims=True,
          expected=[[True], [True], [True]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_all,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=0,
          keepdims=True,
          expected=[[False, True, False, True]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_all,
          rt_input=[[True, True], [True, True, False, True], [False, True]],
          axis=1,
          keepdims=True,
          expected=[[True], [False], [False]]),

      #=========================================================================
      # Examples with the following RaggedTensor (ragged_rank=1):
      #   [[0, 1, 2, 3],
      #    [4         ],
      #    [          ],
      #    [5, 6      ],
      #    [7         ],
      #    [8, 9      ]]
      #=========================================================================

      # axis=None
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=0 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=0 * 1 * 2 * 3 * 4 * 5 * 6 * 7 * 8 * 9),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=min(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=max(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=mean(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=variance(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=None,
          keepdims=False,
          expected=std(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),
      # axis=0
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=0,
          keepdims=False,
          expected=[0 + 4 + 5 + 7 + 8, 1 + 6 + 9, 2, 3]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=0,
          keepdims=False,
          expected=[0 * 4 * 5 * 7 * 8, 1 * 6 * 9, 2, 3]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=0,
          keepdims=False,
          expected=[min(0, 4, 5, 7, 8), min(1, 6, 9), 2, 3]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=0,
          keepdims=False,
          expected=[max(0, 4, 5, 7, 8), max(1, 6, 9), 2, 3]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=0,
          keepdims=False,
          expected=[mean(0, 4, 5, 7, 8),
                    mean(1, 6, 9), 2, 3]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[0, 1, 2, 3], [1], [], [2, 1], [3], [4, 1]],
          axis=0,
          keepdims=False,
          expected=[variance(0, 1, 2, 3, 4),
                    variance(1, 1, 1), 0, 0]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[1, 1, 2, 3], [1], [], [1, 1], [1], [1, 1]],
          axis=0,
          keepdims=False,
          expected=[std(1, 1, 1, 1, 1), std(1, 1, 1), 0, 0]),
      # axis=1
      # Note: we don't test mean here because it gives a NaN, and this will
      # cause assertEqual to fail (since NaN != NaN).  See testMeanNan().
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=1,
          keepdims=False,
          expected=[0 + 1 + 2 + 3, 4, 0, 5 + 6, 7, 8 + 9]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_prod,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=1,
          keepdims=False,
          expected=[0 * 1 * 2 * 3, 4, 1, 5 * 6, 7, 8 * 9]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_min,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=1,
          keepdims=False,
          expected=[min(0, 1, 2, 3), 4, _MAX_INT32,
                    min(5, 6), 7,
                    min(8, 9)]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_max,
          rt_input=[[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]],
          axis=1,
          keepdims=False,
          expected=[max(0, 1, 2, 3), 4, _MIN_INT32,
                    max(5, 6), 7,
                    max(8, 9)]),

      #=========================================================================
      # Examples with ragged_rank=2:
      # [[[1, 2], [ ], [3, 4, 5]],
      #  [[6, 7], [ ], [8      ]],
      #  [                      ],
      #  [[9   ]                ]]
      #=========================================================================
      # keepdims=False
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[],
          keepdims=False,
          expected=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=None,
          keepdims=False,
          expected=sum([1, 2, 3, 4, 5, 6, 7, 8, 9])),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=0,
          keepdims=False,
          expected=[[1 + 6 + 9, 2 + 7], [], [3 + 8, 4, 5]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=1,
          keepdims=False,
          expected=[[1 + 3, 2 + 4, 5], [6 + 8, 7], [], [9]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=2,
          keepdims=False,
          expected=[[1 + 2, 0, 3 + 4 + 5], [6 + 7, 0, 8], [], [9]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[0, 1],
          keepdims=False,
          expected=[1 + 3 + 6 + 8 + 9, 2 + 4 + 7, 5]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[0, 2],
          keepdims=False,
          expected=[1 + 6 + 9 + 2 + 7, 0, 3 + 8 + 4 + 5]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[1, 2],
          keepdims=False,
          expected=[1 + 2 + 3 + 4 + 5, 6 + 7 + 8, 0, 9]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[0, 1, 2],
          keepdims=False,
          expected=sum([1, 2, 3, 4, 5, 6, 7, 8, 9])),
      # keepdims=True
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[],
          keepdims=True,
          expected=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=None,
          keepdims=True,
          expected=[[[sum([1, 2, 3, 4, 5, 6, 7, 8, 9])]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=0,
          keepdims=True,
          expected=[[[1 + 6 + 9, 2 + 7], [], [3 + 8, 4, 5]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=1,
          keepdims=True,
          expected=[[[1 + 3, 2 + 4, 5]], [[6 + 8, 7]], [[]], [[9]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=2,
          keepdims=True,
          expected=[[[1 + 2], [0], [3 + 4 + 5]], [[6 + 7], [0], [8]], [],
                    [[9]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[0, 1],
          keepdims=True,
          expected=[[[1 + 3 + 6 + 8 + 9, 2 + 4 + 7, 5]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[0, 2],
          keepdims=True,
          expected=[[[1 + 6 + 9 + 2 + 7], [0], [3 + 8 + 4 + 5]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[1, 2],
          keepdims=True,
          expected=[[[1 + 2 + 3 + 4 + 5]], [[6 + 7 + 8]], [[0]], [[9]]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[0, 1, 2],
          keepdims=True,
          expected=[[[sum([1, 2, 3, 4, 5, 6, 7, 8, 9])]]]),

      #=========================================================================
      # Examples for ragged_reduce_mean ragged_rank=2:
      # [[[1, 2], [3, 4, 5]],
      #  [[6, 7], [8      ]],
      #  [[9   ]          ]]
      #=========================================================================
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[[1, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=0,
          keepdims=False,
          expected=[[mean(1, 6, 9), mean(2, 7)], [mean(3, 8), 4, 5]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[[1, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=1,
          keepdims=False,
          expected=[[mean(1, 3), mean(2, 4), 5], [mean(6, 8), 7], [9]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_mean,
          rt_input=[[[1, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=2,
          keepdims=False,
          expected=[[mean(1, 2), mean(3, 4, 5)], [mean(6, 7), 8], [9]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[[6, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=0,
          keepdims=False,
          expected=[[variance(6, 6, 9), variance(2, 7)],
                    [variance(3, 8), 0., 0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[[6, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=1,
          keepdims=False,
          expected=[[variance(6, 3), variance(2, 4), 0.], [variance(6, 8), 0.],
                    [0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_variance,
          rt_input=[[[6, 2], [6, 9, 9]], [[6, 7], [8]], [[9]]],
          axis=2,
          keepdims=False,
          expected=[[variance(6, 2), variance(6, 9, 9)], [variance(6, 7), 0.],
                    [0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[[6, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=0,
          keepdims=False,
          expected=[[std(6, 6, 9), std(2, 7)], [std(3, 8), 0., 0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[[6, 2], [3, 4, 5]], [[6, 7], [8]], [[9]]],
          axis=1,
          keepdims=False,
          expected=[[std(6, 3), std(2, 4), 0.], [std(6, 8), 0.], [0.]]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_std,
          rt_input=[[[6, 2], [6, 9, 9]], [[6, 7], [8]], [[9]]],
          axis=2,
          keepdims=False,
          expected=[[std(6, 2), std(6, 9, 9)], [std(6, 7), 0.], [0.]]),

      # Test case for GitHub issue 27497, multiple negative axes.
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[-2, -1],
          keepdims=False,
          expected=[1 + 2 + 3 + 4 + 5, 6 + 7 + 8, 0, 9]),
      dict(
          ragged_reduce_op=ragged_math_ops.reduce_sum,
          rt_input=[[[1, 2], [], [3, 4, 5]], [[6, 7], [], [8]], [], [[9]]],
          axis=[-3, -2, -1],
          keepdims=False,
          expected=sum([1, 2, 3, 4, 5, 6, 7, 8, 9])),
  )
  def testReduce(self, ragged_reduce_op, rt_input, axis, keepdims, expected):
    rt_input = ragged_factory_ops.constant(rt_input)
    reduced = ragged_reduce_op(rt_input, axis, keepdims=keepdims)
    self.assertAllEqual(reduced, expected)

  def testReduceKeepsInnerDimensionShape(self):
    # Test for bug [b/139823356].
    rt = ragged_factory_ops.constant([[[[1, 1]]]], ragged_rank=2)
    self.assertEqual(rt.shape.as_list(), [1, None, None, 2])
    reduced = ragged_math_ops.reduce_sum(rt, axis=2)
    self.assertEqual(reduced.shape.as_list(), [1, None, 2])

  def assertEqualWithNan(self, actual, expected):
    """Like assertEqual, but NaN==NaN."""
    self.assertTrue(
        ((actual == expected) | (np.isnan(actual) & np.isnan(expected))).all())

  def testMeanNan(self):
    rt_as_list = [[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]]
    expected = (
        np.array([0 + 1 + 2 + 3, 4, 0, 5 + 6, 7, 8 + 9]) /
        np.array([4, 1, 0, 2, 1, 2]))
    rt_input = ragged_factory_ops.constant(rt_as_list)
    reduced = ragged_math_ops.reduce_mean(rt_input, axis=1)
    self.assertEqualWithNan(self.evaluate(reduced), expected)

  def testVarianceNan(self):
    rt_as_list = [[0, 1, 2, 3], [4], [], [5, 6], [7], [8, 9]]
    expected = ([
        variance(0, 1, 2, 3),
        variance(4),
        variance(),
        variance(5, 6),
        variance(7),
        variance(8, 9)
    ])
    rt_input = ragged_factory_ops.constant(rt_as_list)
    reduced = ragged_math_ops.reduce_variance(rt_input, axis=1)
    self.assertEqualWithNan(self.evaluate(reduced), expected)

  def testStdNan(self):
    rt_as_list = [[0, 1, 1, 0], [4], [], [5, 6], [7], [8, 9]]
    expected = ([std(0, 1, 1, 0), std(4), std(), std(5, 6), std(7), std(8, 9)])
    rt_input = ragged_factory_ops.constant(rt_as_list)
    reduced = ragged_math_ops.reduce_std(rt_input, axis=1)
    self.assertEqualWithNan(self.evaluate(reduced), expected)

  def testMeanWithTensorInputs(self):
    tensor = [[1.0, 2.0, 3.0], [10.0, 20.0, 30.0]]
    expected = [2.0, 20.0]
    reduced = ragged_math_ops.reduce_mean(tensor, axis=1)
    self.assertAllEqual(reduced, expected)

  def testVarianceWithTensorInputs(self):
    tensor = [[6.0, 9.0, 6.0], [60.0, 90.0, 60.0]]
    expected = [2., 200.]
    reduced = ragged_math_ops.reduce_variance(tensor, axis=1)
    self.assertAllEqual(reduced, expected)

  def testStdWithTensorInputs(self):
    tensor = [[1.0, 2.0, 2.0, 1.0], [10.0, 20.0, 20.0, 10.0]]
    expected = [0.5, 5.]
    reduced = ragged_math_ops.reduce_std(tensor, axis=1)
    self.assertAllEqual(reduced, expected)

  def testErrors(self):
    rt_input = ragged_factory_ops.constant([[1, 2, 3], [4, 5]])
    axis = array_ops.placeholder_with_default(constant_op.constant([0]), None)

    if not context.executing_eagerly():
      self.assertRaisesRegex(ValueError,
                             r'axis must be known at graph construction time.',
                             ragged_math_ops.reduce_sum, rt_input, axis)
    self.assertRaisesRegex(TypeError, r'axis must be an int; got str.*',
                           ragged_math_ops.reduce_sum, rt_input, ['x'])


if __name__ == '__main__':
  googletest.main()
