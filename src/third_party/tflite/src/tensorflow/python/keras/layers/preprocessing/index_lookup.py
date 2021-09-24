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
"""Keras index lookup preprocessing layer."""
# pylint: disable=g-classes-have-attributes

import collections
import json
import operator

import numpy as np

from tensorflow.python.eager import context
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_shape
from tensorflow.python.framework import tensor_spec
from tensorflow.python.framework import tensor_util
from tensorflow.python.keras import backend
from tensorflow.python.keras.engine import base_preprocessing_layer
from tensorflow.python.keras.layers.preprocessing import category_encoding
from tensorflow.python.keras.layers.preprocessing import table_utils
from tensorflow.python.keras.saving.saved_model import layer_serialization
from tensorflow.python.keras.utils import layer_utils
from tensorflow.python.keras.utils import tf_utils
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import init_ops
from tensorflow.python.ops import lookup_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import sparse_ops
from tensorflow.python.ops import string_ops
from tensorflow.python.platform import gfile
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.util import compat

INT = "int"
MULTI_HOT = "multi_hot"
ONE_HOT = "one_hot"
COUNT = "count"
TF_IDF = "tf_idf"

_VOCAB_NAME = "vocab"
_IDF_WEIGHTS_NAME = "idf_weights"


class _NullInitializer(lookup_ops.TextFileInitializer):
  """A placeholder initializer for restoring this layer from a SavedModel."""

  def __init__(self, key_dtype, value_dtype):
    """Construct a table initializer object.

    Args:
      key_dtype: Type of the table keys.
      value_dtype: Type of the table values.
    """
    self._key_dtype = dtypes.as_dtype(key_dtype)
    self._value_dtype = dtypes.as_dtype(value_dtype)

  @property
  def key_dtype(self):
    """The expected table key dtype."""
    return self._key_dtype

  @property
  def value_dtype(self):
    """The expected table value dtype."""
    return self._value_dtype

  def initialize(self, table):
    """Returns the table initialization op."""
    pass

  @property
  def _shared_name(self):
    """Returns a shared name to be used by the table."""
    shared_name = "NULL_INITIALIZER_"
    if context.executing_eagerly():
      # Ensure a unique name when eager execution is enabled to avoid spurious
      # sharing issues..
      shared_name += str(backend.get_uid(shared_name))
    return shared_name


class IndexLookup(base_preprocessing_layer.CombinerPreprocessingLayer):
  """Maps values from a vocabulary to integer indices.

  This layer translates a set of arbitrary hashables into an integer output via
  a table-based lookup, with optional out-of-vocabulary handling. This is the
  basis layer for both IntegerLookup and StringLookup; it holds the common
  logic but is not intended to be exported as part of the Keras API.

  Args:
    max_tokens: The maximum size of the vocabulary for this layer. If None,
      there is no cap on the size of the vocabulary. Note that this size
      includes the OOV and mask tokens.
    num_oov_indices: The number of out-of-vocabulary tokens to use. If this
      value is more than 1, OOV inputs are hashed to determine their OOV value.
      If this value is 0, OOV inputs will cause an error when calling the layer.
    mask_token: A token that represents masked inputs. When `output_mode` is
      `"int"`, the token is included in vocabulary and mapped to index 0. In
      other output modes, the token will not appear in the vocabulary and
      instances of the mask token in the input will be dropped. If set to None,
      no mask term will be added.
    oov_token: Only used when `invert` is True. The token to return for OOV
      indices.
    vocabulary: An optional list of vocabulary terms. If the list contains the
      same token multiple times, an error will be thrown.
    invert: Only valid when `output_mode` is `"int"`. If True, this layer will
      map indices to vocabulary items instead of mapping vocabulary items to
      indices. Default to False.
    output_mode: Specification for the output of the layer. Defaults to `"int"`.
      Values can be `"int"`, `"one_hot"`, `"multi_hot"`, `"count"`, or
      `"tf_idf"` configuring the layer as follows:
        - `"int"`: Return the raw integer indices of the input tokens.
        - `"one_hot"`: Encodes each individual element in the input into an
          array the same size as the vocabulary, containing a 1 at the element
          index. If the last dimension is size 1, will encode on that dimension.
          If the last dimension is not size 1, will append a new dimension for
          the encoded output.
        - `"multi_hot"`: Encodes each sample in the input into a single array
          the same size as the vocabulary, containing a 1 for each vocabulary
          term present in the sample. Treats the last dimension as the sample
          dimension, if input shape is (..., sample_length), output shape will
          be (..., num_tokens).
        - `"count"`: As `"multi_hot"`, but the int array contains a count of the
          number of times the token at that index appeared in the sample.
        - `"tf_idf"`: As `"multi_hot"`, but the TF-IDF algorithm is applied to
          find the value in each token slot.
    pad_to_max_tokens: Only valid when `output_mode` is `"multi_hot"`,
      `"count"`, or `"tf_idf"`. If True, the output will have its feature axis
      padded to `max_tokens` even if the number of unique tokens in the
      vocabulary is less than max_tokens, resulting in a tensor of shape
      [batch_size, max_tokens] regardless of vocabulary size. Defaults to False.
    sparse: Boolean. Only applicable to `"multi_hot"` and `"count"` output
      modes. If True, returns a `SparseTensor` instead of a dense `Tensor`.
      Defaults to False.
  """

  def __init__(self,
               max_tokens,
               num_oov_indices,
               mask_token,
               oov_token,
               vocabulary=None,
               invert=False,
               output_mode=INT,
               sparse=False,
               pad_to_max_tokens=False,
               **kwargs):
    # If max_tokens is set, the value must be greater than 1 - otherwise we
    # are creating a 0-element vocab, which doesn't make sense.
    if max_tokens is not None and max_tokens <= 1:
      raise ValueError("If set, `max_tokens` must be greater than 1. "
                       "You passed {}".format(max_tokens))

    if num_oov_indices < 0:
      raise ValueError("`num_oov_indices` must be greater than or equal to 0. "
                       "You passed {}".format(num_oov_indices))

    # Support deprecated names for output_modes.
    if output_mode == "binary":
      output_mode = MULTI_HOT
    if output_mode == "tf-idf":
      output_mode = TF_IDF
    # 'output_mode' must be one of (INT, ONE_HOT, MULTI_HOT, COUNT, TF_IDF)
    layer_utils.validate_string_arg(
        output_mode,
        allowable_strings=(INT, ONE_HOT, MULTI_HOT, COUNT, TF_IDF),
        layer_name=self.__class__.__name__,
        arg_name="output_mode")

    if invert and output_mode != INT:
      raise ValueError("`output_mode` must be {} when `invert` is true. You "
                       "passed {}".format(INT, output_mode))

    self.invert = invert
    self.max_tokens = max_tokens
    self.num_oov_indices = num_oov_indices
    self.output_mode = output_mode
    self.sparse = sparse
    self.pad_to_max_tokens = pad_to_max_tokens
    self._called = False

    # A note on vocab_size: we need to always keep a non-Tensor representation
    # of vocab_size around to use in graph building. Because we might be
    # in a tf.function, we can't rely on evaluating the actual tables to
    # find the value either.
    self._vocab_size = None
    # We need to keep track our current vocab size outside of our layer weights
    # to support a static output shape when `output_mode != INT`. The bincount
    # ops do not set shape on their outputs, which means we have to set it
    # ourselves. We persist the current vocab size as a hidden part of the
    # config when serializing our model.
    if "vocabulary_size" in kwargs:
      self._vocab_size = kwargs["vocabulary_size"]
      del kwargs["vocabulary_size"]

    restore_from_static_table = kwargs.pop("has_static_table", False)

    # Make sure the mask token and oov token are truly of the dtype we want. We
    # can ignore strings here, because they have only one dtype.
    dtype = kwargs["dtype"]
    if dtype == dtypes.int32:
      mask_token = None if mask_token is None else np.int32(mask_token)
      oov_token = None if oov_token is None else np.int32(oov_token)
    elif dtype == dtypes.int64:
      mask_token = None if mask_token is None else np.int64(mask_token)
      oov_token = None if oov_token is None else np.int64(oov_token)
    self.mask_token = mask_token
    self.oov_token = oov_token

    if max_tokens is not None:
      available_vocab_size = max_tokens - self._token_start_index()
    else:
      available_vocab_size = None

    super(IndexLookup, self).__init__(
        combiner=_IndexLookupCombiner(
            vocab_size=available_vocab_size,
            mask_value=mask_token,
            oov_value=oov_token,
            compute_idf=(output_mode == TF_IDF)),
        **kwargs)

    # We need to save the key dtype so that we know if we're expecting int64
    # keys. If we are, we will cast int32 inputs to int64 as well.
    if invert:
      self._key_dtype = dtypes.int64
      self._value_dtype = self.dtype
      self._mask_key = 0
      self._mask_value = mask_token
      key_index = lookup_ops.TextFileIndex.LINE_NUMBER
      value_index = lookup_ops.TextFileIndex.WHOLE_LINE
      default_value = self.oov_token
      oov_indices = None
    else:
      self._key_dtype = self.dtype
      self._value_dtype = dtypes.int64
      self._mask_key = mask_token
      key_index = lookup_ops.TextFileIndex.WHOLE_LINE
      value_index = lookup_ops.TextFileIndex.LINE_NUMBER
      # Masks should map to 0 for int output and be dropped otherwise. Max ints
      # will be dropped from the bincount op.
      self._mask_value = 0 if self.output_mode == INT else dtypes.int64.max
      oov_start = self._oov_start_index()
      token_start = self._token_start_index()
      if self.num_oov_indices == 0:
        # If there are no OOV indices, we map OOV tokens to -1 and error out
        # during call if we find a negative index.
        default_value = -1
        oov_indices = None
      elif self.num_oov_indices == 1:
        # If there is only one OOV index, we can set that index as the default
        # value of the index_lookup table.
        default_value = oov_start
        oov_indices = None
      else:
        # If we hav multiple OOV values, we need to do a further hashing step;
        # to make this easier, we set the OOV value to -1. (This lets us do a
        # vectorized add and cast to boolean to determine locations where we
        # need to do extra hashing.)
        default_value = -1
        oov_indices = list(range(oov_start, token_start))

    self._static_vocabulary_path = None
    has_vocab_path = (vocabulary is not None and isinstance(vocabulary, str))
    if has_vocab_path or restore_from_static_table:
      self._has_static_table = True
      if vocabulary is None:
        # If we're restoring a layer that was saved with a static table
        # initializer, we create a fake initializer object to let the code
        # progress. The savedmodel restoration code will handle restoring
        # the actual data.
        initializer = _NullInitializer(self._key_dtype, self._value_dtype)
      else:
        if not gfile.Exists(vocabulary):
          raise ValueError("Vocabulary file %s does not exist." % (vocabulary,))
        self._static_vocabulary_path = vocabulary
        num_tokens = table_utils.num_tokens_in_file(vocabulary)
        self._vocab_size = self._token_start_index() + num_tokens

        initializer = lookup_ops.TextFileInitializer(
            filename=vocabulary,
            key_dtype=self._key_dtype,
            key_index=key_index,
            value_dtype=self._value_dtype,
            value_index=value_index,
            value_index_offset=self._token_start_index())

      self._table = lookup_ops.StaticHashTable(
          initializer, default_value=default_value)
      self._table_handler = table_utils.TableHandler(
          table=self._table,
          mask_token=self._mask_key if self.mask_token is not None else None,
          mask_value=self._mask_value,
          oov_tokens=oov_indices)

      tracked_table = self._add_trackable(self._table, trainable=False)

    else:
      self._has_static_table = False
      self._table = lookup_ops.MutableHashTable(
          key_dtype=self._key_dtype,
          value_dtype=self._value_dtype,
          default_value=default_value,
          name=(self._name + "_index_table"))
      self._table_handler = table_utils.TableHandler(
          table=self._table,
          oov_tokens=oov_indices)
      if vocabulary is not None:
        self.set_vocabulary(vocabulary)
      tracked_table = self._add_trackable(self._table, trainable=False)

    if self.output_mode == TF_IDF:
      # The TF-IDF weight may have a (None,) tensorshape. This creates
      # a 1D variable with arbitrary shape, which we can assign any weight to
      # so long as it has 1 dimension. In order to properly initialize this
      # weight in Keras, we need to provide a custom callable initializer which
      # does not depend on the shape of the weight (as all other initializers
      # do) since the weight is not known. Hence the lambda shape, dtype: [0].
      if not self.pad_to_max_tokens or max_tokens is None:
        initializer = lambda shape, dtype: [0]
      else:
        initializer = init_ops.zeros_initializer

      # We are adding these here instead of in build() since they do not depend
      # on the input shape at all.
      idf_shape = (max_tokens,) if self.pad_to_max_tokens else (None,)
      self.tf_idf_weights = self._add_state_variable(
          name="idf",
          shape=tensor_shape.TensorShape(idf_shape),
          dtype=backend.floatx(),
          initializer=initializer)

    # This is a workaround for summary() on this layer. Because the table is
    # not mutable during training, the effective number of parameters (and so
    # the weight shape) is 0; we add this as an attr so that the parameter
    # counting code in the Model object doesn't throw an attribute error.
    tracked_table.shape = tensor_shape.TensorShape((0,))

  def compute_output_shape(self, input_shape):
    if self.output_mode == INT:
      return input_shape
    if self._vocab_size and not self.pad_to_max_tokens:
      out_depth = self._vocab_size
    else:
      out_depth = self.max_tokens
    return tensor_shape.TensorShape([input_shape[0], out_depth])

  def compute_output_signature(self, input_spec):
    output_shape = self.compute_output_shape(input_spec.shape.as_list())
    output_dtype = (self._value_dtype if self.output_mode == INT
                    else backend.floatx())
    return tensor_spec.TensorSpec(shape=output_shape, dtype=output_dtype)

  def adapt(self, data, reset_state=True):
    """Fits the state of the preprocessing layer to the dataset.

    Overrides the default adapt method to apply relevant preprocessing to the
    inputs before passing to the combiner.

    Args:
      data: The data to train on. It can be passed either as a tf.data Dataset,
        or as a numpy array.
      reset_state: Optional argument specifying whether to clear the state of
        the layer at the start of the call to `adapt`. This must be True for
        this layer, which does not support repeated calls to `adapt`.
    """
    if not reset_state:
      raise ValueError("IndexLookup does not support streaming adapts.")
    super(IndexLookup, self).adapt(data, reset_state)

  def get_vocabulary(self, include_special_tokens=True):
    """Returns the current vocabulary of the layer.

    Args:
      include_special_tokens: If True, the returned vocabulary will include mask
        and OOV tokens, and a term's index in the vocabulary will equal the
        term's index when calling the layer. If False, the returned vocabulary
        will not include any mask or OOV tokens.
    """
    if self.vocabulary_size() is None:
      return []

    # The MutableHashTable data will not be sorted, so we will create a inverted
    # lookup here, and use that to lookup a range of indices [0, vocab_size).
    keys, values = self._table.export()
    vocab, indices = (values, keys) if self.invert else (keys, values)
    lookup = collections.defaultdict(
        lambda: self.oov_token,
        zip(indices.numpy(), self._tensor_vocab_to_numpy(vocab)))
    vocab = [lookup[x] for x in range(self.vocabulary_size())]
    if self.mask_token is not None and self.output_mode == INT:
      vocab[0] = self.mask_token
    if not include_special_tokens:
      vocab = vocab[self._token_start_index():]
    return vocab

  def vocabulary_size(self):
    """Gets the current size of the layer's vocabulary.

    Returns:
      The integer size of the voculary, including optional mask and oov indices.
    """
    return self._vocab_size

  def vocab_size(self):
    logging.warning("vocab_size is deprecated, please use vocabulary_size.")
    return self.vocabulary_size()

  def get_config(self):
    if self._has_static_table:
      vocabulary_path = self._static_vocabulary_path
    else:
      vocabulary_path = None

    config = {
        "invert": self.invert,
        "max_tokens": self.max_tokens,
        "num_oov_indices": self.num_oov_indices,
        "oov_token": self.oov_token,
        "mask_token": self.mask_token,
        "output_mode": self.output_mode,
        "pad_to_max_tokens": self.pad_to_max_tokens,
        "vocabulary_size": self.vocabulary_size(),
        "vocabulary": vocabulary_path,
    }
    if self._has_static_table:
      config["has_static_table"] = True

    base_config = super(IndexLookup, self).get_config()
    return dict(list(base_config.items()) + list(config.items()))

  def count_params(self):
    # This method counts the number of scalars in the weights of this layer.
    # Since this layer doesn't have any /actual/ weights (in that there's
    # nothing in this layer that can be trained - we only use the weight
    # abstraction for ease of saving!) we return 0.
    return 0

  def set_vocabulary(self, vocabulary, idf_weights=None):
    """Sets vocabulary (and optionally document frequency) data for this layer.

    This method sets the vocabulary and idf weights for this layer directly,
    instead of analyzing a dataset through `adapt`. It should be used whenever
    the vocab (and optionally document frequency) information is already known.
    If vocabulary data is already present in the layer, this method will replace
    it.

    Args:
      vocabulary: An array, numpy array, or tensor of hashable tokens.
      idf_weights: An array, numpy array, or tensor of inverse document
        frequency weights with equal length to vocab. Only necessary if the
        layer output_mode is TF_IDF.

    Raises:
      ValueError: If there are too many inputs, the inputs do not match, or
        input data is missing.
      RuntimeError: If the vocabulary cannot be set when this function is
        called. This happens when `"multi_hot"`, `"count"`, and `"tfidf"` modes,
        if `pad_to_max_tokens` is False and the layer itself has already been
        called.
      RuntimeError: If a tensor vocabulary is passed outside of eager execution.
    """
    if self._has_static_table:
      raise RuntimeError("Layer {} was created with a static file-based table "
                         "because a file path was passed to the layer "
                         "init. Layers created with static file-based tables "
                         "do not support changing the vocabulary after "
                         "creation.".format(self.name))

    if self.output_mode != TF_IDF and idf_weights is not None:
      raise ValueError("`idf_weights` should only be set if output_mode is "
                       "TF_IDF. output_mode is {}.".format(self.output_mode))

    if (self.output_mode in [MULTI_HOT, COUNT, TF_IDF] and self._called and
        not self.pad_to_max_tokens):
      raise RuntimeError("When using {} mode and `pad_to_max_tokens` is "
                         "False, the vocabulary cannot be changed after the "
                         "layer is called.".format(self.output_mode))

    if not context.executing_eagerly() and (tensor_util.is_tensor(vocabulary) or
                                            tensor_util.is_tensor(idf_weights)):
      raise RuntimeError(
          "Cannot set a tensor vocabulary on {} layer {} when not executing "
          "eagerly. Create this layer or call `set_vocabulary` outside of "
          "any `tf.function`s and with eager execution enabled.".format(
              self.__class__.__name__, self.name))

    # TODO(mattdangerw): for better performance we should rewrite this entire
    # function to operate on tensors and convert vocabulary to a tensor here.
    if tensor_util.is_tensor(vocabulary):
      vocabulary = self._tensor_vocab_to_numpy(vocabulary)
    if tensor_util.is_tensor(idf_weights):
      idf_weights = idf_weights.numpy()

    oov_start = self._oov_start_index()
    token_start = self._token_start_index()
    should_have_mask = (oov_start > 0)
    has_mask = should_have_mask and vocabulary[0] == self.mask_token

    should_have_oov = (self.num_oov_indices > 0)
    expected_oov = [self.oov_token] * self.num_oov_indices
    found_oov = vocabulary[oov_start:token_start]
    has_oov = should_have_oov and found_oov == expected_oov
    # If we get a numpy array, then has_oov may end up being a numpy array
    # instead of a bool. Fix this by collapsing the variable if it's not bool.
    if not isinstance(has_oov, bool):
      has_oov = any(has_oov)

    if all([should_have_mask, has_mask, should_have_oov]) and not has_oov:
      raise ValueError(
          "Invalid vocabulary format. The layer was created with "
          "`mask_token={mask}` and `oov_token={oov}`. These tokens should be "
          "included in the provided vocabulary. The passed vocabulary has the "
          "correct mask token `{mask}` at index 0, but does not have the OOV "
          "token `{oov}` in indices [{start}:{end}]. Instead, we found "
          "`{found}`. Was this vocabulary generated by a layer with "
          "incompatible settings?".format(
              mask=self.mask_token,
              oov=self.oov_token,
              start=oov_start,
              end=token_start,
              found=found_oov))

    if all([should_have_oov, has_oov, should_have_mask]) and not has_mask:
      raise ValueError(
          "Invalid vocabulary format. The layer was created with "
          "`mask_token={mask}` and `oov_token={oov}`. These tokens should be "
          "included in the provided vocabulary. The passed vocabulary has the "
          "correct OOV token `{oov}` at indices [{start}:{end}], but does not "
          "have the mask token `{mask}` in index 0. Instead, we found "
          "`{found}`. Was this vocabulary generated by a layer with "
          "incompatible settings?".format(
              mask=self.mask_token,
              oov=self.oov_token,
              start=oov_start,
              end=token_start,
              found=vocabulary[0]))

    found_special_tokens = has_oov or has_mask
    if found_special_tokens:
      tokens = vocabulary[token_start:]
    else:
      tokens = vocabulary

    repeated_tokens = table_utils.find_repeated_tokens(tokens)
    if repeated_tokens:
      raise ValueError("The passed vocabulary has at least one repeated "
                       "term. Please uniquify your dataset. The repeated terms "
                       "are {}".format(repeated_tokens))

    if self.mask_token in tokens:
      raise ValueError("Reserved mask token {} was found in the passed "
                       "vocabulary at index {}. Please either remove the "
                       "reserved token from the vocabulary or change the "
                       "mask token for this layer.".format(
                           self.mask_token, tokens.index(self.mask_token)))
    if self.oov_token in tokens:
      raise ValueError("Reserved OOV token {} was found in the passed "
                       "vocabulary at index {}. Please either remove the "
                       "reserved token from the vocabulary or change the "
                       "OOV token for this layer.".format(
                           self.oov_token, tokens.index(self.oov_token)))

    self._vocab_size = token_start + len(tokens)
    if self.max_tokens is not None and self._vocab_size > self.max_tokens:
      raise ValueError(
          "Attempted to set a vocabulary larger than the maximum vocab size. "
          "Passed vocab size is {}, max vocab size is {}.".format(
              self._vocab_size, self.max_tokens))

    if self.output_mode == TF_IDF:
      if idf_weights is None:
        raise ValueError("`idf_weights` must be set if output_mode is TF_IDF")
      if len(vocabulary) != len(idf_weights):
        raise ValueError("`idf_weights` must be the same length as vocabulary. "
                         "len(idf_weights) is {}, len(vocabulary) is {}".format(
                             len(vocabulary), len(idf_weights)))
      idf_weights = self._convert_to_ndarray(idf_weights)
      if idf_weights.ndim != 1:
        raise ValueError(
            "TF-IDF data must be a 1-index array, but received {}".format(
                type(idf_weights)))

    # We add the non-special vocab tokens and optionally the mask_token to our
    # hash table. OOV tokens are handled with the hash table default value and
    # not added directly.
    self._table_handler.clear()
    indices = np.arange(token_start, len(tokens) + token_start, dtype=np.int64)
    if self.invert:
      self._table_handler.insert(indices, tokens)
    else:
      self._table_handler.insert(tokens, indices)
    if self.mask_token is not None:
      self._table_handler.insert([self._mask_key], [self._mask_value])

    if self.output_mode == TF_IDF:
      # If the passed vocabulary has no special tokens, we need to pad the front
      # of idf_weights. We don't have real document frequencies for these tokens
      # so we will use an average of all idf_weights passed in as a reasonable
      # default.
      if found_special_tokens:
        front_padding = 0
        front_padding_value = 0
      else:
        front_padding = token_start
        front_padding_value = np.average(idf_weights)
      # If pad_to_max_tokens is true, and max_tokens is greater than our total
      # vocab size, we need to pad the back of idf_weights with zeros as well.
      back_padding_value = 0
      if self.pad_to_max_tokens and self.max_tokens is not None:
        back_padding = self.max_tokens - front_padding - len(idf_weights)
      else:
        back_padding = 0
      idf_weights = np.pad(
          idf_weights, (front_padding, back_padding),
          "constant",
          constant_values=(front_padding_value, back_padding_value))
      backend.set_value(self.tf_idf_weights, idf_weights)

  def _set_state_variables(self, updates):
    if not self.built:
      raise RuntimeError("_set_state_variables() must be called after build().")
    self.set_vocabulary(
        updates[_VOCAB_NAME], idf_weights=updates[_IDF_WEIGHTS_NAME])

  def call(self, inputs):
    if isinstance(inputs, (list, tuple, np.ndarray)):
      inputs = ops.convert_to_tensor_v2_with_dispatch(inputs)

    if not self.max_tokens and self._vocab_size is None:
      raise ValueError("You must set the layer's vocabulary before calling it. "
                       "Either pass a `vocabulary` argument to the layer, or "
                       "call `layer.adapt(dataset)` with some sample data.")
    self._called = True
    if self._key_dtype == dtypes.int64 and inputs.dtype == dtypes.int32:
      inputs = math_ops.cast(inputs, dtypes.int64)
    lookup_result = self._table_handler.lookup(inputs)

    lookup_checks = []

    if self.num_oov_indices == 0 and not self.invert:
      if tf_utils.is_sparse(inputs):
        lookup_values = lookup_result.values
        input_values = inputs.values
      elif tf_utils.is_ragged(inputs):
        lookup_values = lookup_result.flat_values
        input_values = inputs.flat_values
      else:
        lookup_values = lookup_result
        input_values = inputs
      oov_indices = array_ops.where_v2(math_ops.equal(lookup_values, -1))
      oov_inputs = array_ops.gather_nd(input_values, oov_indices)
      msg = string_ops.string_format(
          "When `num_oov_indices=0` all inputs should be in vocabulary, "
          "found OOV values {}, consider setting `num_oov_indices=1`.",
          (oov_inputs,))
      assertion = control_flow_ops.Assert(
          math_ops.equal(array_ops.size(oov_indices), 0), [msg])
      lookup_checks.append(assertion)

    with ops.control_dependencies(lookup_checks):
      if self.output_mode == INT:
        return array_ops.identity(lookup_result)
      else:
        return self._encode_output(lookup_result)

  def _encode_output(self, lookup_result):
    def expand_dims(inputs, axis):
      if tf_utils.is_sparse(inputs):
        return sparse_ops.sparse_expand_dims(inputs, axis)
      else:
        return array_ops.expand_dims(inputs, axis)

    original_shape = lookup_result.shape
    # In all cases, we should uprank scalar input to a single sample.
    if lookup_result.shape.rank == 0:
      lookup_result = expand_dims(lookup_result, -1)
    # One hot will unprank only if the final output dimension is not already 1.
    if self.output_mode == ONE_HOT:
      if lookup_result.shape[-1] != 1:
        lookup_result = expand_dims(lookup_result, -1)

    # TODO(b/190445202): remove output rank restriction.
    if lookup_result.shape.rank > 2:
      raise ValueError(
          "Received input shape {}, which would result in output rank {}. "
          "Currently only outputs up to rank 2 are supported for "
          "`output_mode={}`.".format(original_shape, lookup_result.shape.rank,
                                     self.output_mode))

    binary_output = self.output_mode in (MULTI_HOT, ONE_HOT)
    if self._vocab_size and not self.pad_to_max_tokens:
      out_depth = self._vocab_size
    else:
      out_depth = self.max_tokens
    if self.sparse:
      bincounts = category_encoding.sparse_bincount(lookup_result, out_depth,
                                                    binary_output)
    else:
      bincounts = category_encoding.dense_bincount(lookup_result, out_depth,
                                                   binary_output)

    if self.output_mode == TF_IDF:
      return math_ops.multiply(bincounts, self.tf_idf_weights)

    return bincounts

  def _convert_to_ndarray(self, x):
    return np.array(x) if isinstance(x, (list, tuple)) else x

  def _oov_start_index(self):
    return 1 if self.mask_token is not None and self.output_mode == INT else 0

  def _token_start_index(self):
    return self._oov_start_index() + self.num_oov_indices

  @property
  def _trackable_saved_model_saver(self):
    return layer_serialization.IndexLookupLayerSavedModelSaver(self)

  # Override points for IntegerLookup and StringLookup.
  def _tensor_vocab_to_numpy(self, vocabulary):
    """Converts a tensor vocabulary to a numpy vocabulary."""
    return vocabulary.numpy()


class _IndexLookupAccumulator(
    collections.namedtuple("Accumulator",
                           ["data", "count_dict", "per_doc_count_dict"])):
  pass


class _IndexLookupCombiner(base_preprocessing_layer.Combiner):
  """Combiner for the IndexLookup preprocessing layer.

  This class encapsulates the logic for computing a vocabulary based on the
  frequency of each token.

  Attributes:
    vocab_size: (Optional) If set, only the top `vocab_size` tokens (based on
      frequency across the dataset) are retained in the vocabulary. If None, or
      set to a value greater than the total number of distinct tokens in the
      dataset, all tokens are retained.
  """

  def __init__(self,
               vocab_size=None,
               mask_value=None,
               oov_value=None,
               compute_idf=False):
    self._vocab_size = vocab_size
    self._mask_value = mask_value
    self._oov_value = oov_value
    self._compute_idf = compute_idf

  def compute(self, values, accumulator=None):
    """Compute a step in this computation, returning a new accumulator."""
    values = base_preprocessing_layer.convert_to_list(
        values, sparse_default_value=self._mask_value)

    if accumulator is None:
      accumulator = self._create_accumulator()

    # TODO(momernick): Benchmark improvements to this algorithm.
    if not isinstance(values, list):
      values = [values]
    for document in values:
      if not isinstance(document, list):
        document = [document]
      if self._compute_idf:
        current_doc_id = accumulator.data["next_doc_id"]
        accumulator.data["next_doc_id"] += 1
      for token in document:
        accumulator.count_dict[token] += 1
        if self._compute_idf:
          doc_count = accumulator.per_doc_count_dict[token]
          if doc_count["last_doc_id"] != current_doc_id:
            doc_count["count"] += 1
            doc_count["last_doc_id"] = current_doc_id

    return accumulator

  def merge(self, accumulators):
    """Merge several accumulators to a single accumulator."""
    if not accumulators:
      return accumulators

    base_accumulator = accumulators[0]
    for accumulator in accumulators[1:]:
      for token, value in accumulator.count_dict.items():
        base_accumulator.count_dict[token] += value

      if self._compute_idf:
        base_accumulator.data["next_doc_id"] += accumulator.data["next_doc_id"]
        if self._compute_idf:
          for token, value in accumulator.per_doc_count_dict.items():
            # Any newly created token counts in 'base_accumulator''s
            # per_doc_count_dict will have a last_doc_id of -1. This is always
            # less than the next doc id (which are strictly positive), so any
            # future occurrences are guaranteed to be counted.
            base_accumulator.per_doc_count_dict[token]["count"] += value[
                "count"]

    return base_accumulator

  def extract(self, accumulator):
    """Convert an accumulator into a dict of output values.

    Args:
      accumulator: An accumulator aggregating over the full dataset.

    Returns:
      A dict of:
        "vocab": A list of the retained items in the vocabulary.
    """
    vocab_counts = accumulator.count_dict

    # Drop special tokens from our vocab.
    if self._mask_value in vocab_counts:
      del vocab_counts[self._mask_value]
    if self._oov_value in vocab_counts:
      del vocab_counts[self._oov_value]
    # Data processed by the accumulator could be tensors, numpy arrays or lists.
    # For tensor string input, values will have been converted into bytes. We
    # need to check the bytes version of special tokens in this case.
    if isinstance(self._mask_value, str):
      mask_value_bytes = compat.as_bytes(self._mask_value)
      if mask_value_bytes in vocab_counts:
        del vocab_counts[mask_value_bytes]
    if isinstance(self._oov_value, str):
      oov_value_bytes = compat.as_bytes(self._oov_value)
      if oov_value_bytes in vocab_counts:
        del vocab_counts[oov_value_bytes]

    sorted_counts = sorted(
        vocab_counts.items(), key=operator.itemgetter(1, 0), reverse=True)
    vocab_data = (
        sorted_counts[:self._vocab_size] if self._vocab_size else sorted_counts)
    vocab = [data[0] for data in vocab_data]

    if self._compute_idf:
      num_documents = accumulator.data["next_doc_id"]
      document_counts = accumulator.per_doc_count_dict
      doc_counts = [document_counts[token]["count"] for token in vocab]
      idf_weights = self._inverse_document_frequency(doc_counts, num_documents)
    else:
      idf_weights = None

    return {_VOCAB_NAME: vocab, _IDF_WEIGHTS_NAME: idf_weights}

  def restore(self, output):
    """Create an accumulator based on 'output'."""
    raise NotImplementedError(
        "IndexLookup does not restore or support streaming updates.")

  def serialize(self, accumulator):
    """Serialize an accumulator for a remote call."""
    output_dict = {}
    output_dict["vocab"] = list(accumulator.count_dict.keys())
    output_dict["vocab_counts"] = list(accumulator.count_dict.values())

    if self._compute_idf:
      output_dict["data"] = accumulator.data
      output_dict["idf_vocab"] = list(accumulator.per_doc_count_dict.keys())
      output_dict["idf_counts"] = [
          counter["count"]
          for counter in accumulator.per_doc_count_dict.values()
      ]
    return compat.as_bytes(json.dumps(output_dict))

  def deserialize(self, encoded_accumulator):
    """Deserialize an accumulator received from 'serialize()'."""
    accumulator_dict = json.loads(compat.as_text(encoded_accumulator))

    accumulator = self._create_accumulator()
    count_dict = dict(
        zip(accumulator_dict["vocab"], accumulator_dict["vocab_counts"]))
    accumulator.count_dict.update(count_dict)

    if self._compute_idf:
      accumulator.data = accumulator_dict["data"]
      create_dict = lambda x: {"count": x, "last_doc_id": -1}
      idf_count_dicts = [
          create_dict(count) for count in accumulator_dict["idf_counts"]
      ]
      idf_dict = dict(zip(accumulator_dict["idf_vocab"], idf_count_dicts))
      accumulator.per_doc_count_dict.update(idf_dict)
    return accumulator

  def _create_accumulator(self):
    """Accumulate a sorted array of vocab tokens and corresponding counts."""

    if self._compute_idf:
      create_default_dict = lambda: {"count": 0, "last_doc_id": -1}
      per_doc_count_dict = collections.defaultdict(create_default_dict)
      data = {"next_doc_id": 0}
    else:
      per_doc_count_dict = None
      data = None

    count_dict = collections.defaultdict(int)
    return _IndexLookupAccumulator(data, count_dict, per_doc_count_dict)

  def _inverse_document_frequency(self, document_counts, num_documents):
    """Computes the inverse-document-frequency (IDF) component of TF-IDF.

    Uses the default weighting scheme described in
    https://en.wikipedia.org/wiki/Tf%E2%80%93idf.

    Args:
      document_counts: An array of the # of documents each token appears in.
      num_documents: An int representing the total number of documents

    Returns:
      An array of "inverse document frequency" weights.
    """
    return np.log(1 + num_documents / (1 + np.array(document_counts)))
