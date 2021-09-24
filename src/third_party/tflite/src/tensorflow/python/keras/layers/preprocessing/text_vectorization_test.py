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
"""Tests for Keras text vectorization preprocessing layer."""

import gc
import os

from absl.testing import parameterized
import numpy as np

from tensorflow.python import keras
from tensorflow.python import tf2

from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.distribute import one_device_strategy
from tensorflow.python.eager import context
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.keras import backend
from tensorflow.python.keras import keras_parameterized
from tensorflow.python.keras import testing_utils
from tensorflow.python.keras.layers import convolutional
from tensorflow.python.keras.layers import core
from tensorflow.python.keras.layers import embeddings
from tensorflow.python.keras.layers.preprocessing import preprocessing_test_utils
from tensorflow.python.keras.layers.preprocessing import text_vectorization
from tensorflow.python.keras.utils import generic_utils
from tensorflow.python.ops import gen_string_ops
from tensorflow.python.ops.ragged import ragged_factory_ops
from tensorflow.python.ops.ragged import ragged_string_ops
from tensorflow.python.platform import gfile
from tensorflow.python.platform import test


def _get_end_to_end_test_cases():
  test_cases = (
      {
          "testcase_name":
              "test_simple_tokens_int_mode",
          # Create an array where 'earth' is the most frequent term, followed by
          # 'wind', then 'and', then 'fire'. This ensures that the vocab
          # is sorting by frequency.
          "vocab_data":
              np.array([["fire"], ["earth"], ["earth"], ["earth"], ["earth"],
                        ["wind"], ["wind"], ["wind"], ["and"], ["and"]]),
          "input_data":
              np.array([["earth"], ["wind"], ["and"], ["fire"], ["fire"],
                        ["and"], ["earth"], ["michigan"]]),
          "kwargs": {
              "max_tokens": None,
              "standardize": None,
              "split": None,
              "output_mode": text_vectorization.INT
          },
          "expected_output": [[2], [3], [4], [5], [5], [4], [2], [1]],
      },
      {
          "testcase_name":
              "test_simple_tokens_int_mode_hard_cap",
          # Create an array where 'earth' is the most frequent term, followed by
          # 'wind', then 'and', then 'fire'. This ensures that the vocab
          # is sorting by frequency.
          "vocab_data":
              np.array([["fire"], ["earth"], ["earth"], ["earth"], ["earth"],
                        ["wind"], ["wind"], ["wind"], ["and"], ["and"]]),
          "input_data":
              np.array([["earth"], ["wind"], ["and"], ["fire"], ["fire"],
                        ["and"], ["earth"], ["michigan"]]),
          "kwargs": {
              "max_tokens": 6,
              "standardize": None,
              "split": None,
              "output_mode": text_vectorization.INT
          },
          "expected_output": [[2], [3], [4], [5], [5], [4], [2], [1]],
      },
      {
          "testcase_name":
              "test_special_tokens_int_mode",
          # Mask tokens in the vocab data should be ingored, and mapped to 0 in
          # from the input data.
          "vocab_data":
              np.array([["fire"], ["earth"], ["earth"], ["earth"], ["earth"],
                        [""], [""], [""], ["[UNK]"], ["[UNK]"], ["[UNK]"],
                        ["wind"], ["wind"], ["wind"], ["and"], ["and"]]),
          "input_data":
              np.array([["earth"], [""], ["wind"], ["[UNK]"], ["and"], [""],
                        ["fire"], ["and"], ["[UNK]"], ["michigan"]]),
          "kwargs": {
              "max_tokens": None,
              "standardize": None,
              "split": None,
              "output_mode": text_vectorization.INT
          },
          "expected_output": [[2], [0], [3], [1], [4], [0], [5], [4], [1], [1]],
      },
      {
          "testcase_name":
              "test_documents_int_mode",
          "vocab_data":
              np.array([["fire earth earth"], ["earth earth"], ["wind wind"],
                        ["and wind and"]]),
          "input_data":
              np.array([["earth wind and"], ["fire fire"], ["and earth"],
                        ["michigan"]]),
          "kwargs": {
              "max_tokens": None,
              "standardize": None,
              "split": text_vectorization.SPLIT_ON_WHITESPACE,
              "output_mode": text_vectorization.INT
          },
          "expected_output": [[2, 3, 4], [5, 5, 0], [4, 2, 0], [1, 0, 0]],
      },
      {
          "testcase_name":
              "test_documents_1d_input_int_mode",
          "vocab_data":
              np.array([
                  "fire earth earth", "earth earth", "wind wind", "and wind and"
              ]),
          "input_data":
              np.array([["earth wind and"], ["fire fire"], ["and earth"],
                        ["michigan"]]),
          "kwargs": {
              "max_tokens": None,
              "standardize": None,
              "split": text_vectorization.SPLIT_ON_WHITESPACE,
              "output_mode": text_vectorization.INT
          },
          "expected_output": [[2, 3, 4], [5, 5, 0], [4, 2, 0], [1, 0, 0]],
      },
      {
          "testcase_name":
              "test_simple_tokens_binary_mode",
          "vocab_data":
              np.array([["fire"], ["earth"], ["earth"], ["earth"], ["earth"],
                        ["wind"], ["wind"], ["wind"], ["and"], ["and"]]),
          "input_data":
              np.array([["earth"], ["wind"], ["and"], ["fire"], ["fire"],
                        ["and"], ["earth"], ["michigan"]]),
          "kwargs": {
              "max_tokens": 5,
              "standardize": None,
              "split": None,
              "output_mode": text_vectorization.MULTI_HOT
          },
          "expected_output": [[0, 1, 0, 0, 0], [0, 0, 1, 0, 0], [0, 0, 0, 1, 0],
                              [0, 0, 0, 0, 1], [0, 0, 0, 0, 1], [0, 0, 0, 1, 0],
                              [0, 1, 0, 0, 0], [1, 0, 0, 0, 0]],
      },
      {
          "testcase_name":
              "test_documents_binary_mode",
          "vocab_data":
              np.array([["fire earth earth"], ["earth earth"], ["wind wind"],
                        ["and wind and"]]),
          "input_data":
              np.array([["earth wind"], ["and"], ["fire fire"],
                        ["earth michigan"]]),
          "kwargs": {
              "max_tokens": 5,
              "standardize": None,
              "split": text_vectorization.SPLIT_ON_WHITESPACE,
              "output_mode": text_vectorization.MULTI_HOT
          },
          "expected_output": [[0, 1, 1, 0, 0], [0, 0, 0, 1, 0], [0, 0, 0, 0, 1],
                              [1, 1, 0, 0, 0]],
      },
      {
          "testcase_name":
              "test_simple_tokens_count_mode",
          "vocab_data":
              np.array([["fire"], ["earth"], ["earth"], ["earth"], ["earth"],
                        ["wind"], ["wind"], ["wind"], ["and"], ["and"]]),
          "input_data":
              np.array([["earth"], ["wind"], ["and"], ["fire"], ["fire"],
                        ["and"], ["earth"], ["michigan"]]),
          "kwargs": {
              "max_tokens": 5,
              "standardize": None,
              "split": None,
              "output_mode": text_vectorization.COUNT
          },
          "expected_output": [[0, 1, 0, 0, 0], [0, 0, 1, 0, 0], [0, 0, 0, 1, 0],
                              [0, 0, 0, 0, 1], [0, 0, 0, 0, 1], [0, 0, 0, 1, 0],
                              [0, 1, 0, 0, 0], [1, 0, 0, 0, 0]],
      },
      {
          "testcase_name":
              "test_documents_count_mode",
          "vocab_data":
              np.array([["fire earth earth"], ["earth earth"], ["wind wind"],
                        ["and wind and"]]),
          "input_data":
              np.array([["earth wind"], ["and"], ["fire fire"],
                        ["earth michigan"]]),
          "kwargs": {
              "max_tokens": 5,
              "standardize": None,
              "split": text_vectorization.SPLIT_ON_WHITESPACE,
              "output_mode": text_vectorization.COUNT
          },
          "expected_output": [[0, 1, 1, 0, 0], [0, 0, 0, 1, 0], [0, 0, 0, 0, 2],
                              [1, 1, 0, 0, 0]],
      },
      {
          "testcase_name":
              "test_tokens_idf_mode",
          "vocab_data":
              np.array([["fire"], ["earth"], ["earth"], ["earth"], ["earth"],
                        ["wind"], ["wind"], ["wind"], ["and"], ["and"]]),
          "input_data":
              np.array([["earth"], ["wind"], ["and"], ["fire"], ["fire"],
                        ["and"], ["earth"], ["michigan"]]),
          "kwargs": {
              "max_tokens": 5,
              "standardize": None,
              "split": None,
              "output_mode": text_vectorization.TF_IDF
          },
          "expected_output": [[0, 1.098612, 0, 0, 0], [0, 0, 1.252763, 0, 0],
                              [0, 0, 0, 1.466337, 0], [0, 0, 0, 0, 1.7917595],
                              [0, 0, 0, 0, 1.7917595], [0, 0, 0, 1.4663371, 0],
                              [0, 1.098612, 0, 0, 0], [1.402368, 0, 0, 0, 0]],
      },
      {
          "testcase_name":
              "test_documents_idf_mode",
          "vocab_data":
              np.array([["fire earth earth"], ["earth earth"], ["wind wind"],
                        ["and wind and"]]),
          "input_data":
              np.array([["earth wind"], ["and"], ["fire fire"],
                        ["earth michigan"]]),
          "kwargs": {
              "max_tokens": 5,
              "standardize": None,
              "split": text_vectorization.SPLIT_ON_WHITESPACE,
              "output_mode": text_vectorization.TF_IDF
          },
          "expected_output": [[0., 0.847298, 0.847298, 0., 0.],
                              [0., 0., 0., 1.098612, 0.],
                              [0., 0., 0., 0., 2.197225],
                              [0.972955, 0.847298, 0., 0., 0.]],
      },
  )

  crossed_test_cases = []
  # Cross above test cases with use_dataset in (True, False)
  for use_dataset in (True, False):
    for case in test_cases:
      case = case.copy()
      if use_dataset:
        case["testcase_name"] = case["testcase_name"] + "_with_dataset"
      case["use_dataset"] = use_dataset
      crossed_test_cases.append(case)

  return crossed_test_cases


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationLayerTest(keras_parameterized.TestCase,
                                 preprocessing_test_utils.PreprocessingLayerTest
                                ):

  @parameterized.named_parameters(*_get_end_to_end_test_cases())
  def test_layer_end_to_end_with_adapt(self, vocab_data, input_data, kwargs,
                                       use_dataset, expected_output):
    cls = text_vectorization.TextVectorization
    if kwargs.get("output_mode") == text_vectorization.INT:
      expected_output_dtype = dtypes.int64
    else:
      expected_output_dtype = dtypes.float32
    input_shape = input_data.shape

    if use_dataset:
      # Keras APIs expect batched datasets.
      # TODO(rachelim): `model.predict` predicts the result on each
      # dataset batch separately, then tries to concatenate the results
      # together. When the results have different shapes on the non-concat
      # axis (which can happen in the output_mode = INT case for
      # TextVectorization), the concatenation fails. In real use cases, this may
      # not be an issue because users are likely to pipe the preprocessing layer
      # into other keras layers instead of predicting it directly. A workaround
      # for these unit tests is to have the dataset only contain one batch, so
      # no concatenation needs to happen with the result. For consistency with
      # numpy input, we should make `predict` join differently shaped results
      # together sensibly, with 0 padding.
      input_data = dataset_ops.Dataset.from_tensor_slices(input_data).batch(
          input_shape[0])
      vocab_data = dataset_ops.Dataset.from_tensor_slices(vocab_data).batch(
          input_shape[0])

    output_data = testing_utils.layer_test(
        cls,
        kwargs=kwargs,
        input_shape=input_shape,
        input_data=input_data,
        input_dtype=dtypes.string,
        expected_output_dtype=expected_output_dtype,
        validate_training=False,
        adapt_data=vocab_data)
    self.assertAllClose(expected_output, output_data)

  def test_scalar_input_int_mode_no_len_limit(self):
    vocab_data = [
        "fire earth earth", "earth earth", "wind wind", "and wind and"
    ]
    input_data = "earth wind and fire fire and earth michigan"
    layer = text_vectorization.TextVectorization()
    layer.adapt(vocab_data)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [2, 3, 4, 5, 5, 4, 2, 1])
    layer.set_vocabulary(["earth", "wind", "and", "fire"])
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [2, 3, 4, 5, 5, 4, 2, 1])

  def test_scalar_input_int_mode_trim_to_len_limit(self):
    vocab_data = [
        "fire earth earth", "earth earth", "wind wind", "and wind and"
    ]
    input_data = "earth wind and fire fire and earth michigan"
    layer = text_vectorization.TextVectorization(output_sequence_length=3)
    layer.adapt(vocab_data)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [2, 3, 4])
    layer.set_vocabulary(["earth", "wind", "and", "fire"])
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [2, 3, 4])

  def test_scalar_input_int_pad_to_len_limit(self):
    vocab_data = [
        "fire earth earth", "earth earth", "wind wind", "and wind and"
    ]
    input_data = "earth wind and fire fire and earth michigan"
    layer = text_vectorization.TextVectorization(output_sequence_length=10)
    layer.adapt(vocab_data)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [2, 3, 4, 5, 5, 4, 2, 1, 0, 0])
    layer.set_vocabulary(["earth", "wind", "and", "fire"])
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [2, 3, 4, 5, 5, 4, 2, 1, 0, 0])

  def test_list_inputs_1d(self):
    vocab_data = ["two two two", "two three three", "three four four five"]
    input_data = ["two three", "four five"]
    layer = text_vectorization.TextVectorization()
    layer.adapt(vocab_data)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])
    layer.set_vocabulary(["two", "three", "four", "five"])
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])

  def test_tensor_inputs(self):
    vocab_data = constant_op.constant(
        ["two two two", "two three three", "three four four five"])
    input_data = constant_op.constant(["two three", "four five"])
    layer = text_vectorization.TextVectorization()
    layer.adapt(vocab_data)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])
    layer.set_vocabulary(["two", "three", "four", "five"])
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])

  def test_list_inputs_2d(self):
    vocab_data = [
        ["two two two"], ["two three three"], ["three four four five"]]
    input_data = [["two three"], ["four five"]]
    layer = text_vectorization.TextVectorization()
    layer.adapt(vocab_data)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])
    layer.set_vocabulary(["two", "three", "four", "five"])
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])

  def test_dataset_of_single_strings(self):
    vocab_data = ["two two two", "two three three", "three four four five"]
    input_data = ["two three", "four five"]
    vocab_ds = dataset_ops.Dataset.from_tensor_slices(vocab_data)  # unbatched
    layer = text_vectorization.TextVectorization()
    layer.adapt(vocab_ds)
    out = layer(input_data)
    if context.executing_eagerly():
      self.assertAllClose(out.numpy(), [[2, 3], [4, 5]])

  @parameterized.named_parameters(
      {
          "testcase_name": "1d",
          "data": ["0", "a", "b", "c", "d", "e", "a", "b", "c", "d", "f"],
          "expected": [1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1]
      },
      {
          "testcase_name": "2d",
          "data": [["0", "a", "b", "c", "d"], ["e", "a", "b", "c", "d"], ["f"]],
          "expected": [[1, 2, 3, 4, 5], [1, 2, 3, 4, 5], [1, 0, 0, 0, 0]]
      },
      {
          "testcase_name":
              "3d",
          "data": [[["0", "a", "b"], ["c", "d"]], [["e", "a"], ["b", "c", "d"]],
                   [["f"]]],
          "expected": [[[1, 2, 3], [4, 5, 0]], [[1, 2, 0], [3, 4, 5]],
                       [[1, 0, 0], [0, 0, 0]]]
      },
  )
  def test_layer_dimensionality_handling(self, data, expected):
    vocab = ["a", "b", "c", "d"]
    vectorization = text_vectorization.TextVectorization(
        max_tokens=None, standardize=None, split=None, pad_to_max_tokens=False)
    vectorization.set_vocabulary(vocab)
    output = vectorization(ragged_factory_ops.constant(data))
    self.assertAllEqual(expected, output)

  @parameterized.named_parameters(
      {
          "testcase_name": "1d",
          "data": ["0 a b c d e a b c d f"],
          "expected": [[1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1]]
      },
      {
          "testcase_name":
              "3d",
          "data": [[["0 a b"], ["c d"]], [["e a"], ["b c d"]], [["f"]]],
          "expected": [[[1, 2, 3], [4, 5, 0]], [[1, 2, 0], [3, 4, 5]],
                       [[1, 0, 0], [0, 0, 0]]]
      },
  )
  def test_layer_dimensionality_handling_with_split(self, data, expected):
    vocab = ["a", "b", "c", "d"]
    vectorization = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        pad_to_max_tokens=False)
    vectorization.set_vocabulary(vocab)
    output = vectorization(ragged_factory_ops.constant(data, inner_shape=(1,)))
    self.assertAllEqual(expected, output)


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationPreprocessingTest(
    keras_parameterized.TestCase,
    preprocessing_test_utils.PreprocessingLayerTest):

  def _write_to_temp_file(self, file_name, vocab_list):
    vocab_path = os.path.join(self.get_temp_dir(), file_name + ".txt")
    with gfile.GFile(vocab_path, "w") as writer:
      for vocab in vocab_list:
        writer.write(vocab + "\n")
      writer.flush()
      writer.close()
    return vocab_path

  def test_summary_before_adapt(self):
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=text_vectorization.LOWER_AND_STRIP_PUNCTUATION,
        split=None,
        ngrams=None,
        output_mode=text_vectorization.TF_IDF)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    # We are testing that model.summary() can be called without erroring out.
    # (b/145726907)
    model.summary()

  def test_normalization(self):
    input_array = np.array([["Earth", "wInD", "aNd", "firE"],
                            ["fire|", "an<>d", "{earth}", "michigan@%$"]])
    expected_output = np.array([[b"earth", b"wind", b"and", b"fire"],
                                [b"fire", b"and", b"earth", b"michigan"]])

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=text_vectorization.LOWER_AND_STRIP_PUNCTUATION,
        split=None,
        ngrams=None,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_normalization_ragged_inputs(self):
    input_array = ragged_factory_ops.constant([["Earth", "wInD", "aNd", "firE"],
                                               ["fire|", "an<>d", "{earth}"]])
    expected_output = [[b"earth", b"wind", b"and", b"fire"],
                       [b"fire", b"and", b"earth"]]

    input_data = keras.Input(shape=(None,), ragged=True, dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=text_vectorization.LOWER_AND_STRIP_PUNCTUATION,
        split=None,
        ngrams=None,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_custom_normalization(self):
    input_array = np.array([["Earth", "wInD", "aNd", "firE"],
                            ["fire|", "an<>d", "{earth}", "michigan@%$"]])
    expected_output = np.array(
        [[b"earth", b"wind", b"and", b"fire"],
         [b"fire|", b"an<>d", b"{earth}", b"michigan@%$"]])

    custom_standardization = gen_string_ops.string_lower
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=custom_standardization,
        split=None,
        ngrams=None,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_string_splitting(self):
    input_array = np.array([["earth wind and fire"],
                            ["\tfire\tand\nearth    michigan  "]])
    expected_output = [[b"earth", b"wind", b"and", b"fire"],
                       [b"fire", b"and", b"earth", b"michigan"]]

    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        ngrams=None,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_custom_string_splitting(self):
    input_array = np.array([["earth>wind>and fire"],
                            ["\tfire>and\nearth>michigan"]])
    expected_output = [[b"earth", b"wind", b"and fire"],
                       [b"\tfire", b"and\nearth", b"michigan"]]

    custom_split = lambda x: ragged_string_ops.string_split_v2(x, sep=">")
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=custom_split,
        ngrams=None,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_single_ngram_value_ragged_inputs(self):
    input_array = ragged_factory_ops.constant([["earth", "wind", "and", "fire"],
                                               ["fire", "and", "earth"]])
    # pyformat: disable
    expected_output = [[b"earth", b"wind", b"and", b"fire",
                        b"earth wind", b"wind and", b"and fire",
                        b"earth wind and", b"wind and fire"],
                       [b"fire", b"and", b"earth",
                        b"fire and", b"and earth",
                        b"fire and earth"]]
    # pyformat: enable

    input_data = keras.Input(shape=(None,), ragged=True, dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        ngrams=3,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_single_ngram_value(self):
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    # pyformat: disable
    expected_output = [[b"earth", b"wind", b"and", b"fire",
                        b"earth wind", b"wind and", b"and fire",
                        b"earth wind and", b"wind and fire"],
                       [b"fire", b"and", b"earth", b"michigan",
                        b"fire and", b"and earth", b"earth michigan",
                        b"fire and earth", b"and earth michigan"]]
    # pyformat: enable

    input_data = keras.Input(shape=(4,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        ngrams=3,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_multiple_ngram_values(self):
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    # pyformat: disable
    expected_output = [[b"earth wind", b"wind and", b"and fire",
                        b"earth wind and", b"wind and fire"],
                       [b"fire and", b"and earth", b"earth michigan",
                        b"fire and earth", b"and earth michigan"]]
    # pyformat: enable

    input_data = keras.Input(shape=(4,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        ngrams=(2, 3),
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_string_multiple_preprocessing_steps(self):
    input_array = np.array([["earth wInD and firE"],
                            ["\tfire\tand\nearth!!    michig@n  "]])
    expected_output = [[
        b"earth",
        b"wind",
        b"and",
        b"fire",
        b"earth wind",
        b"wind and",
        b"and fire",
    ],
                       [
                           b"fire",
                           b"and",
                           b"earth",
                           b"michign",
                           b"fire and",
                           b"and earth",
                           b"earth michign",
                       ]]

    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=text_vectorization.LOWER_AND_STRIP_PUNCTUATION,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        ngrams=2,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_string_splitting_with_non_1d_array_fails(self):
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=None)
    with self.assertRaisesRegex(RuntimeError,
                                ".*tokenize strings, the innermost dime.*"):
      _ = layer(input_data)

  def test_string_splitting_with_non_1d_raggedarray_fails(self):
    input_data = keras.Input(shape=(None,), ragged=True, dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        vocabulary=["a"],
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=None)
    with self.assertRaisesRegex(RuntimeError,
                                ".*tokenize strings, the innermost dime.*"):
      _ = layer(input_data)

  def test_standardization_with_invalid_standardize_arg(self):
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(vocabulary=["a"])
    layer._standardize = "unsupported"
    with self.assertRaisesRegex(ValueError,
                                ".*is not a supported standardization.*"):
      _ = layer(input_data)

  def test_splitting_with_invalid_split_arg(self):
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(vocabulary=["a"])
    layer._split = "unsupported"
    with self.assertRaisesRegex(ValueError, ".*is not a supported splitting.*"):
      _ = layer(input_data)

  def test_vocab_setting_via_init(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT,
        vocabulary=vocab_data)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_vocab_setting_via_init_file(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    vocab_path = self._write_to_temp_file("vocab_file", vocab_data)
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT,
        vocabulary=vocab_path)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_vocab_setting_via_setter(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    vocab_path = self._write_to_temp_file("vocab_file", vocab_data)
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT)
    layer.set_vocabulary(vocab_path)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_vocab_setting_with_oov_via_setter(self):
    vocab_data = ["", "[UNK]", "earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    vocab_path = self._write_to_temp_file("vocab_file", vocab_data)
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT)
    layer.set_vocabulary(vocab_path)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationDistributionTest(
    keras_parameterized.TestCase,
    preprocessing_test_utils.PreprocessingLayerTest):

  def test_distribution_strategy_output(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    strategy = one_device_strategy.OneDeviceStrategy("/cpu:0")
    with strategy.scope():
      input_data = keras.Input(shape=(None,), dtype=dtypes.string)
      layer = text_vectorization.TextVectorization(
          max_tokens=None,
          standardize=None,
          split=None,
          output_mode=text_vectorization.INT)
      layer.set_vocabulary(vocab_data)
      int_data = layer(input_data)
      model = keras.Model(inputs=input_data, outputs=int_data)

    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationOutputTest(
    keras_parameterized.TestCase,
    preprocessing_test_utils.PreprocessingLayerTest):

  def test_int_output(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_int_output_densifies_with_zeros(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # Create an input array that has 5 elements in the first example and 4 in
    # the second. This should output a 2x5 tensor with a padding value in the
    # second example.
    input_array = np.array([["earth wind and also fire"],
                            ["fire and earth michigan"]])
    expected_output = [[2, 3, 4, 1, 5], [5, 4, 2, 1, 0]]

    # This test doesn't explicitly set an output shape, so the 2nd dimension
    # should stay 'None'.
    expected_output_shape = [None, None]

    # The input shape here is explicitly 1 because we're tokenizing.
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=text_vectorization.INT)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_int_output_densifies_with_zeros_and_pads(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # Create an input array that has 5 elements in the first example and 4 in
    # the second. This should output a 2x6 tensor with a padding value in the
    # second example, since output_sequence_length is set to 6.
    input_array = np.array([["earth wind and also fire"],
                            ["fire and earth michigan"]])
    expected_output = [[2, 3, 4, 1, 5, 0], [5, 4, 2, 1, 0, 0]]

    output_sequence_length = 6
    expected_output_shape = [None, output_sequence_length]

    # The input shape here is explicitly 1 because we're tokenizing.
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=text_vectorization.INT,
        output_sequence_length=output_sequence_length)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_int_output_densifies_with_zeros_and_strips(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # Create an input array that has 5 elements in the first example and 4 in
    # the second. This should output a 2x3 tensor with a padding value in the
    # second example, since output_sequence_length is set to 3.
    input_array = np.array([["earth wind and also fire"],
                            ["fire and earth michigan"]])
    expected_output = [[2, 3, 4], [5, 4, 2]]
    output_sequence_length = 3
    expected_output_shape = [None, output_sequence_length]

    # The input shape here is explicitly 1 because we're tokenizing.
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=text_vectorization.INT,
        output_sequence_length=output_sequence_length)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_int_output_dynamically_strips_and_pads(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # Create an input array that has 5 elements in the first example and 4 in
    # the second. This should output a 2x3 tensor with a padding value in the
    # second example, since output_sequence_length is set to 3.
    input_array = np.array([["earth wind and also fire"],
                            ["fire and earth michigan"]])
    expected_output = [[2, 3, 4], [5, 4, 2]]
    output_sequence_length = 3
    expected_output_shape = [None, output_sequence_length]

    # The input shape here is explicitly 1 because we're tokenizing.
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=text_vectorization.INT,
        output_sequence_length=output_sequence_length)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

    # Create an input array that has 1 element in the first example and 2 in
    # the second. This should output a 2x3 tensor with a padding value in the
    # second example, since output_sequence_length is set to 3.
    input_array_2 = np.array([["wind"], ["fire and"]])
    expected_output_2 = [[3, 0, 0], [5, 4, 0]]
    output_dataset = model.predict(input_array_2)
    self.assertAllEqual(expected_output_2, output_dataset)

  def test_binary_output_hard_maximum(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 1, 1, 1, 0, 0],
                       [1, 1, 0, 1, 0, 0]]
    # pyformat: enable
    max_tokens = 6
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=max_tokens,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=True)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_binary_output_soft_maximum(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 1, 1, 1, 0],
                       [1, 1, 0, 1, 0]]
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=False)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bag_output_hard_maximum_set_vocabulary_after_build(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 1, 1, 1, 0],
                       [1, 1, 0, 1, 0]]
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=max_tokens,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=True)
    int_data = layer(input_data)
    layer.set_vocabulary(vocab_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bag_output_hard_maximum_adapt_after_build(self):
    vocab_data = np.array([
        "earth", "earth", "earth", "earth", "wind", "wind", "wind", "and",
        "and", "fire"
    ])
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 1, 1, 1, 0],
                       [1, 1, 0, 1, 0]]
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=max_tokens,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=True)
    int_data = layer(input_data)
    layer.adapt(vocab_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bag_output_hard_maximum_set_state_variables_after_build(self):
    state_variables = {
        text_vectorization._VOCAB_NAME: ["earth", "wind", "and", "fire"]
    }
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 1, 1, 1, 0],
                       [1, 1, 0, 1, 0]]
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=max_tokens,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=True)
    int_data = layer(input_data)
    layer._set_state_variables(state_variables)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bag_output_hard_maximum_multiple_adapts(self):
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])
    adapt_data = ["earth", "earth", "earth", "earth", "wind", "wind", "wind"]
    first_expected_output = [
        [1, 1, 1, 0, 0],
        [1, 1, 0, 0, 0],
    ]
    second_adapt_data = [
        "earth", "earth", "earth", "earth", "wind", "wind", "wind", "and",
        "and", "fire"
    ]
    second_expected_output = [
        [0, 1, 1, 1, 0],
        [1, 1, 0, 1, 0],
    ]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=5,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=True)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    # Test the first adapt
    layer.adapt(adapt_data)
    first_output = model.predict(input_array)
    # Test the second adapt
    layer.adapt(second_adapt_data)
    second_output = model.predict(input_array)
    self.assertAllEqual(first_expected_output, first_output)
    self.assertAllEqual(second_expected_output, second_output)

  def test_bag_output_soft_maximum_set_state_after_build(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 1, 1, 1, 0],
                       [1, 1, 0, 1, 0]]
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=False)
    layer.build(input_data.shape)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_bag_output_soft_maximum_set_vocabulary_after_call_fails(self):
    vocab_data = ["earth", "wind", "and", "fire"]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=False)
    layer.adapt(vocab_data)
    _ = layer(input_data)
    with self.assertRaisesRegex(RuntimeError, "vocabulary cannot be changed"):
      layer.set_vocabulary(vocab_data)

  def test_bag_output_soft_maximum_set_state_variables_after_call_fails(self):
    state_variables = {
        text_vectorization._VOCAB_NAME: ["earth", "wind", "and", "fire"]
    }

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT,
        pad_to_max_tokens=False)
    layer.adapt(["earth", "wind"])
    _ = layer(input_data)
    with self.assertRaisesRegex(RuntimeError, "vocabulary cannot be changed"):
      layer._set_state_variables(state_variables)

  def test_count_output_hard_maximum(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 2, 1, 1, 0, 0],
                       [2, 1, 0, 1, 0, 0]]
    # pyformat: enable
    max_tokens = 6
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=6,
        standardize=None,
        split=None,
        output_mode=text_vectorization.COUNT,
        pad_to_max_tokens=True)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_count_output_soft_maximum(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[0, 2, 1, 1, 0],
                       [2, 1, 0, 1, 0]]
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=None,
        split=None,
        output_mode=text_vectorization.COUNT,
        pad_to_max_tokens=False)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

  def test_tfidf_output_hard_maximum(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # OOV idf weight (bucket 0) should 0.5, the average of passed weights.
    idf_weights = [.4, .25, .75, .6]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "fire", "earth", "michigan"]])

    # pyformat: disable
    # pylint: disable=bad-whitespace
    expected_output = [[ 0, .8, .25, .75,  0, 0],
                       [ 1, .4,   0,   0, .6, 0]]
    # pylint: enable=bad-whitespace
    # pyformat: enable
    max_tokens = 6
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=6,
        standardize=None,
        split=None,
        output_mode=text_vectorization.TF_IDF,
        pad_to_max_tokens=True)
    layer.set_vocabulary(vocab_data, idf_weights=idf_weights)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllClose(expected_output, output_dataset)

  def test_tfidf_output_soft_maximum(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # OOV idf weight (bucket 0) should 0.5, the average of passed weights.
    idf_weights = [.4, .25, .75, .6]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "fire", "earth", "michigan"]])

    # pyformat: disable
    # pylint: disable=bad-whitespace
    expected_output = [[ 0, .8, .25, .75,  0],
                       [ 1, .4,   0,   0, .6]]
    # pylint: enable=bad-whitespace
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=None,
        split=None,
        output_mode=text_vectorization.TF_IDF,
        pad_to_max_tokens=False)
    layer.set_vocabulary(vocab_data, idf_weights=idf_weights)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllClose(expected_output, output_dataset)

  def test_tfidf_output_set_oov_weight(self):
    vocab_data = ["[UNK]", "earth", "wind", "and", "fire"]
    idf_weights = [.1, .4, .25, .75, .6]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "fire", "earth", "michigan"]])

    # pyformat: disable
    # pylint: disable=bad-whitespace
    expected_output = [[  0, .8, .25, .75,  0],
                       [ .2, .4,   0,   0, .6]]
    # pylint: enable=bad-whitespace
    # pyformat: enable
    max_tokens = 5
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=None,
        split=None,
        output_mode=text_vectorization.TF_IDF,
        pad_to_max_tokens=False)
    layer.set_vocabulary(vocab_data, idf_weights=idf_weights)
    int_data = layer(input_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())

    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllClose(expected_output, output_dataset)

  def test_accept_1D_input(self):
    input_array = np.array(["earth wind and fire",
                            "fire and earth michigan"])
    layer = text_vectorization.TextVectorization(
        standardize=None, split=None, output_mode="int")
    layer.adapt(input_array)
    _ = layer(input_array)


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationModelBuildingTest(
    keras_parameterized.TestCase,
    preprocessing_test_utils.PreprocessingLayerTest):

  @parameterized.named_parameters(
      {
          "testcase_name": "count_hard_max",
          "pad_to_max_tokens": True,
          "output_mode": text_vectorization.COUNT
      }, {
          "testcase_name": "count_soft_max",
          "pad_to_max_tokens": False,
          "output_mode": text_vectorization.COUNT
      }, {
          "testcase_name": "binary_hard_max",
          "pad_to_max_tokens": True,
          "output_mode": text_vectorization.MULTI_HOT
      }, {
          "testcase_name": "binary_soft_max",
          "pad_to_max_tokens": False,
          "output_mode": text_vectorization.MULTI_HOT
      }, {
          "testcase_name": "tfidf_hard_max",
          "pad_to_max_tokens": True,
          "output_mode": text_vectorization.TF_IDF
      }, {
          "testcase_name": "tfidf_soft_max",
          "pad_to_max_tokens": False,
          "output_mode": text_vectorization.TF_IDF
      })
  def test_end_to_end_bagged_modeling(self, output_mode, pad_to_max_tokens):
    vocab_data = ["earth", "wind", "and", "fire"]
    idf_weights = [.5, .25, .2, .125]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=10,
        standardize=None,
        split=None,
        output_mode=output_mode,
        pad_to_max_tokens=pad_to_max_tokens)
    if output_mode == text_vectorization.TF_IDF:
      layer.set_vocabulary(vocab_data, idf_weights=idf_weights)
    else:
      layer.set_vocabulary(vocab_data)

    int_data = layer(input_data)
    float_data = backend.cast(int_data, dtype="float32")
    output_data = core.Dense(64)(float_data)
    model = keras.Model(inputs=input_data, outputs=output_data)
    _ = model.predict(input_array)

  def test_end_to_end_vocab_modeling(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth wind and also fire"],
                            ["fire and earth michigan"]])
    output_sequence_length = 6
    max_tokens = 5

    # The input shape here is explicitly 1 because we're tokenizing.
    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=text_vectorization.SPLIT_ON_WHITESPACE,
        output_mode=text_vectorization.INT,
        output_sequence_length=output_sequence_length)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    embedded_data = embeddings.Embedding(
        input_dim=max_tokens + 1, output_dim=32)(
            int_data)
    output_data = convolutional.Conv1D(
        250, 3, padding="valid", activation="relu", strides=1)(
            embedded_data)

    model = keras.Model(inputs=input_data, outputs=output_data)
    _ = model.predict(input_array)


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationVocbularyTest(
    keras_parameterized.TestCase,
    preprocessing_test_utils.PreprocessingLayerTest,
):

  def test_get_vocabulary(self):
    vocab = ["earth", "wind", "and", "fire"]

    layer = text_vectorization.TextVectorization(vocabulary=vocab)
    self.assertAllEqual(layer.get_vocabulary(),
                        ["", "[UNK]", "earth", "wind", "and", "fire"])

  def test_get_vocabulary_adapt(self):
    vocab = np.array([["earth earth earth earth wind wind wind and and fire"]])

    layer = text_vectorization.TextVectorization()
    layer.adapt(vocab)
    self.assertAllEqual(layer.get_vocabulary(),
                        ["", "[UNK]", "earth", "wind", "and", "fire"])

  def test_get_vocabulary_no_special_tokens(self):
    vocab = ["earth", "wind", "and", "fire"]

    layer = text_vectorization.TextVectorization(vocabulary=vocab)
    self.assertAllEqual(
        layer.get_vocabulary(include_special_tokens=False),
        ["earth", "wind", "and", "fire"])


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationErrorTest(keras_parameterized.TestCase,
                                 preprocessing_test_utils.PreprocessingLayerTest
                                ):

  def test_too_long_vocab_fails_in_single_setting(self):
    vocab_data = ["earth", "wind", "and", "fire"]

    layer = text_vectorization.TextVectorization(
        max_tokens=4,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT)
    with self.assertRaisesRegex(ValueError,
                                "vocabulary larger than the maximum vocab.*"):
      layer.set_vocabulary(vocab_data)

  def test_setting_vocab_without_idf_weights_fails_in_tfidf_mode(self):
    vocab_data = ["earth", "wind", "and", "fire"]

    layer = text_vectorization.TextVectorization(
        max_tokens=5,
        standardize=None,
        split=None,
        output_mode=text_vectorization.TF_IDF)
    with self.assertRaisesRegex(
        ValueError, "`idf_weights` must be set if output_mode is TF_IDF"):
      layer.set_vocabulary(vocab_data)

  def test_idf_weights_length_mismatch_fails(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    idf_weights = [1, 2, 3]
    layer = text_vectorization.TextVectorization(
        max_tokens=5,
        standardize=None,
        split=None,
        output_mode=text_vectorization.TF_IDF)
    with self.assertRaisesRegex(
        ValueError, "`idf_weights` must be the same length as vocab"):
      layer.set_vocabulary(vocab_data, idf_weights)

  def test_set_tfidf_in_non_tfidf_fails(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    idf_weights = [1, 2, 3, 4]
    layer = text_vectorization.TextVectorization(
        max_tokens=5,
        standardize=None,
        split=None,
        output_mode=text_vectorization.MULTI_HOT)
    with self.assertRaisesRegex(ValueError,
                                "`idf_weights` should only be set if"):
      layer.set_vocabulary(vocab_data, idf_weights)

  def test_zero_max_tokens_fails(self):
    with self.assertRaisesRegex(ValueError, "max_tokens.*"):
      _ = text_vectorization.TextVectorization(max_tokens=0)

  def test_non_string_dtype_fails(self):
    with self.assertRaisesRegex(ValueError, "dtype of string.*"):
      _ = text_vectorization.TextVectorization(dtype=dtypes.int64)

  def test_unknown_standardize_arg_fails(self):
    with self.assertRaisesRegex(ValueError,
                                "standardize arg.*unsupported_value"):
      _ = text_vectorization.TextVectorization(standardize="unsupported_value")

  def test_unknown_split_arg_fails(self):
    with self.assertRaisesRegex(ValueError, "split arg.*unsupported_value"):
      _ = text_vectorization.TextVectorization(split="unsupported_value")

  def test_unknown_output_mode_arg_fails(self):
    with self.assertRaisesRegex(ValueError,
                                "output_mode arg.*unsupported_value"):
      _ = text_vectorization.TextVectorization(output_mode="unsupported_value")

  def test_unknown_ngrams_arg_fails(self):
    with self.assertRaisesRegex(ValueError, "ngrams.*unsupported_value"):
      _ = text_vectorization.TextVectorization(ngrams="unsupported_value")

  def test_float_ngrams_arg_fails(self):
    with self.assertRaisesRegex(ValueError, "ngrams.*2.9"):
      _ = text_vectorization.TextVectorization(ngrams=2.9)

  def test_float_tuple_ngrams_arg_fails(self):
    with self.assertRaisesRegex(ValueError, "ngrams.*(1.3, 2.9)"):
      _ = text_vectorization.TextVectorization(ngrams=(1.3, 2.9))

  def test_non_int_output_sequence_length_dtype_fails(self):
    with self.assertRaisesRegex(ValueError, "output_sequence_length.*2.0"):
      _ = text_vectorization.TextVectorization(
          output_mode="int", output_sequence_length=2.0)

  def test_non_none_output_sequence_length_fails_if_output_type_not_int(self):
    with self.assertRaisesRegex(ValueError,
                                "`output_sequence_length` must not be set"):
      _ = text_vectorization.TextVectorization(
          output_mode="count", output_sequence_length=2)


# Custom functions for the custom callable serialization test. Declared here
# to avoid multiple registrations from run_all_keras_modes().
@generic_utils.register_keras_serializable(package="Test")
def custom_standardize_fn(x):
  return gen_string_ops.string_lower(x)


@generic_utils.register_keras_serializable(package="Test")
def custom_split_fn(x):
  return ragged_string_ops.string_split_v2(x, sep=">")


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationSavingTest(
    keras_parameterized.TestCase,
    preprocessing_test_utils.PreprocessingLayerTest):

  def tearDown(self):
    keras.backend.clear_session()
    gc.collect()
    super(TextVectorizationSavingTest, self).tearDown()

  def test_saving(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    # Build and validate a golden model.
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    # Save the model to disk.
    output_path = os.path.join(self.get_temp_dir(), "tf_keras_saved_model")

    model.save(output_path, save_format="tf")

    # Delete the session and graph to ensure that the loaded model is generated
    # from scratch.
    # TODO(b/149526183): Can't clear session when TF2 is disabled.
    if tf2.enabled():
      keras.backend.clear_session()

    loaded_model = keras.models.load_model(output_path)
    self.assertAllEqual(loaded_model.predict(input_array), expected_output)

  def test_saving_when_nested(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    input_array = np.array([["earth", "wind", "and", "fire"],
                            ["fire", "and", "earth", "michigan"]])
    expected_output = [[2, 3, 4, 5], [5, 4, 2, 1]]

    # Build and validate a golden model.
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=None,
        split=None,
        output_mode=text_vectorization.INT)
    layer.set_vocabulary(vocab_data)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)

    outer_input = keras.Input(shape=(None,), dtype=dtypes.string)
    outer_output = model(outer_input)
    outer_model = keras.Model(inputs=outer_input, outputs=outer_output)

    # Save the model to disk.
    output_path = os.path.join(self.get_temp_dir(), "tf_keras_saved_model")
    outer_model.save(output_path, save_format="tf")

    # Delete the session and graph to ensure that the loaded model is generated
    # from scratch.
    # TODO(b/149526183): Can't clear session when TF2 is disabled.
    if tf2.enabled():
      keras.backend.clear_session()

    loaded_model = keras.models.load_model(output_path)
    self.assertAllEqual(loaded_model.predict(input_array), expected_output)

  def test_saving_with_tfidf(self):
    vocab_data = ["earth", "wind", "and", "fire"]
    # OOV idf weight (bucket 0) should 0.5, the average of passed weights.
    idf_weights = [.4, .25, .75, .6]
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "fire", "earth", "michigan"]])

    # pyformat: disable
    # pylint: disable=bad-whitespace
    expected_output = [[ 0, .8, .25, .75,  0],
                       [ 1, .4,   0,   0, .6]]
    vocab_data = ["earth", "wind", "and", "fire"]
    # pylint: enable=bad-whitespace
    # pyformat: enable

    # Build and validate a golden model.
    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=5,
        standardize=None,
        split=None,
        output_mode=text_vectorization.TF_IDF)
    layer.set_vocabulary(vocab_data, idf_weights=idf_weights)

    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllClose(output_dataset, expected_output)

    # Save the model to disk.
    output_path = os.path.join(self.get_temp_dir(), "tf_keras_saved_model")
    model.save(output_path, save_format="tf")
    loaded_model = keras.models.load_model(output_path)

    # Ensure that the loaded model is unique (so that the save/load is real)
    self.assertIsNot(model, loaded_model)

    # Validate correctness of the new model.
    new_output_dataset = loaded_model.predict(input_array)
    self.assertAllClose(new_output_dataset, expected_output)

  def test_serialization_with_custom_callables(self):
    input_array = np.array([["earth>wind>and Fire"],
                            ["\tfire>And\nearth>michigan"]])
    expected_output = [[b"earth", b"wind", b"and fire"],
                       [b"\tfire", b"and\nearth", b"michigan"]]

    input_data = keras.Input(shape=(1,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=None,
        standardize=custom_standardize_fn,
        split=custom_split_fn,
        ngrams=None,
        output_mode=None)
    int_data = layer(input_data)
    model = keras.Model(inputs=input_data, outputs=int_data)
    output_dataset = model.predict(input_array)
    self.assertAllEqual(expected_output, output_dataset)

    serialized_model_data = model.get_config()
    new_model = keras.Model.from_config(serialized_model_data)
    new_output_dataset = new_model.predict(input_array)
    self.assertAllEqual(expected_output, new_output_dataset)


@keras_parameterized.run_all_keras_modes(always_skip_v1=True)
class TextVectorizationE2ETest(keras_parameterized.TestCase,
                               preprocessing_test_utils.PreprocessingLayerTest):

  def test_keras_vocab_trimming_example(self):
    vocab_data = np.array([
        "earth", "earth", "earth", "earth", "wind", "wind", "wind", "and",
        "and", "fire"
    ])
    input_array = np.array([["earth", "wind", "and", "earth"],
                            ["ohio", "and", "earth", "michigan"]])

    # pyformat: disable
    expected_output = [[1, 2, 1],
                       [3, 1, 0]]
    # pyformat: enable
    max_tokens = 3
    expected_output_shape = [None, max_tokens]

    input_data = keras.Input(shape=(None,), dtype=dtypes.string)
    layer = text_vectorization.TextVectorization(
        max_tokens=max_tokens,
        standardize=None,
        split=None,
        output_mode=text_vectorization.COUNT,
        pad_to_max_tokens=True)
    int_data = layer(input_data)
    layer.adapt(vocab_data)
    self.assertAllEqual(expected_output_shape, int_data.shape.as_list())
    model = keras.Model(input_data, int_data)
    output = model.predict(input_array)
    self.assertAllEqual(expected_output, output)


if __name__ == "__main__":
  test.main()
