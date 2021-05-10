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
"""Keras string lookup preprocessing layer."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.framework import dtypes
from tensorflow.python.keras.layers.preprocessing import index_lookup
from tensorflow.python.keras.layers.preprocessing import table_utils
from tensorflow.python.util.tf_export import keras_export


@keras_export("keras.layers.experimental.preprocessing.StringLookup", v1=[])
class StringLookup(index_lookup.IndexLookup):
  """Maps strings from a vocabulary to integer indices.

  This layer translates a set of arbitrary strings into an integer output via a
  table-based lookup, with optional out-of-vocabulary handling.

  If desired, the user can call this layer's `adapt()` method on a data set,
  which will analyze the data set, determine the frequency of individual string
  values, and create a vocabulary from them. This vocabulary can have
  unlimited size or be capped, depending on the configuration options for this
  layer; if there are more unique values in the input than the maximum
  vocabulary size, the most frequent terms will be used to create the
  vocabulary.

  Attributes:
    max_tokens: The maximum size of the vocabulary for this layer. If None,
      there is no cap on the size of the vocabulary. Note that this vocabulary
      includes the OOV and mask tokens, so the effective number of tokens is
      (max_tokens - num_oov_indices - (1 if mask_token else 0))
    num_oov_indices: The number of out-of-vocabulary tokens to use; defaults to
      1. If this value is more than 1, OOV inputs are hashed to determine their
      OOV value; if this value is 0, passing an OOV input will result in a '-1'
      being returned for that value in the output tensor. (Note that, because
      the value is -1 and not 0, this will allow you to effectively drop OOV
      values from categorical encodings.)
    mask_token: A token that represents masked values, and which is mapped to
      index 0. Defaults to the empty string "". If set to None, no mask term
      will be added and the OOV tokens, if any, will be indexed from
      (0...num_oov_indices) instead of (1...num_oov_indices+1).
    oov_token: The token representing an out-of-vocabulary value. Defaults to
      "[UNK]".
    vocabulary: An optional list of vocabulary terms, or a path to a text file
      containing a vocabulary to load into this layer. The file should contain
      one token per line. If the list or file contains the same token multiple
      times, an error will be thrown.
    encoding: The Python string encoding to use. Defaults to `'utf-8'`.
    invert: If true, this layer will map indices to vocabulary items instead
      of mapping vocabulary items to indices.

  Examples:

  Creating a lookup layer with a known vocabulary

  This example creates a lookup layer with a pre-existing vocabulary.

  >>> vocab = ["a", "b", "c", "d"]
  >>> data = tf.constant([["a", "c", "d"], ["d", "z", "b"]])
  >>> layer = StringLookup(vocabulary=vocab)
  >>> layer(data)
  <tf.Tensor: shape=(2, 3), dtype=int64, numpy=
  array([[2, 4, 5],
         [5, 1, 3]])>


  Creating a lookup layer with an adapted vocabulary

  This example creates a lookup layer and generates the vocabulary by analyzing
  the dataset.

  >>> data = tf.constant([["a", "c", "d"], ["d", "z", "b"]])
  >>> layer = StringLookup()
  >>> layer.adapt(data)
  >>> layer.get_vocabulary()
  ['', '[UNK]', 'd', 'z', 'c', 'b', 'a']

  Note how the mask token '' and the OOV token [UNK] have been added to the
  vocabulary. The remaining tokens are sorted by frequency ('d', which has
  2 occurrences, is first) then by inverse sort order.

  >>> data = tf.constant([["a", "c", "d"], ["d", "z", "b"]])
  >>> layer = StringLookup()
  >>> layer.adapt(data)
  >>> layer(data)
  <tf.Tensor: shape=(2, 3), dtype=int64, numpy=
  array([[6, 4, 2],
         [2, 3, 5]])>

  Lookups with multiple OOV tokens.

  This example demonstrates how to use a lookup layer with multiple OOV tokens.
  When a layer is created with more than one OOV token, any OOV values are
  hashed into the number of OOV buckets, distributing OOV values in a
  deterministic fashion across the set.

  >>> vocab = ["a", "b", "c", "d"]
  >>> data = tf.constant([["a", "c", "d"], ["m", "z", "b"]])
  >>> layer = StringLookup(vocabulary=vocab, num_oov_indices=2)
  >>> layer(data)
  <tf.Tensor: shape=(2, 3), dtype=int64, numpy=
  array([[3, 5, 6],
         [1, 2, 4]])>

  Note that the output for OOV value 'm' is 1, while the output for OOV value
  'z' is 2. The in-vocab terms have their output index increased by 1 from
  earlier examples (a maps to 3, etc) in order to make space for the extra OOV
  value.

  Inverse lookup

  This example demonstrates how to map indices to strings using this layer. (You
  can also use adapt() with inverse=True, but for simplicity we'll pass the
  vocab in this example.)

  >>> vocab = ["a", "b", "c", "d"]
  >>> data = tf.constant([[1, 3, 4], [4, 5, 2]])
  >>> layer = StringLookup(vocabulary=vocab, invert=True)
  >>> layer(data)
  <tf.Tensor: shape=(2, 3), dtype=string, numpy=
  array([[b'a', b'c', b'd'],
         [b'd', b'[UNK]', b'b']], dtype=object)>

  Note that the integer 5, which is out of the vocabulary space, returns an OOV
  token.


  Forward and inverse lookup pairs

  This example demonstrates how to use the vocabulary of a standard lookup
  layer to create an inverse lookup layer.

  >>> vocab = ["a", "b", "c", "d"]
  >>> data = tf.constant([["a", "c", "d"], ["d", "z", "b"]])
  >>> layer = StringLookup(vocabulary=vocab)
  >>> i_layer = StringLookup(vocabulary=layer.get_vocabulary(), invert=True)
  >>> int_data = layer(data)
  >>> i_layer(int_data)
  <tf.Tensor: shape=(2, 3), dtype=string, numpy=
  array([[b'a', b'c', b'd'],
         [b'd', b'[UNK]', b'b']], dtype=object)>

  In this example, the input value 'z' resulted in an output of '[UNK]', since
  1000 was not in the vocabulary - it got represented as an OOV, and all OOV
  values are returned as '[OOV}' in the inverse layer. Also, note that for the
  inverse to work, you must have already set the forward layer vocabulary
  either directly or via fit() before calling get_vocabulary().
  """

  def __init__(self,
               max_tokens=None,
               num_oov_indices=1,
               mask_token="",
               oov_token="[UNK]",
               vocabulary=None,
               encoding=None,
               invert=False,
               **kwargs):
    allowed_dtypes = [dtypes.string]

    if "dtype" in kwargs and kwargs["dtype"] not in allowed_dtypes:
      raise ValueError("StringLookup may only have a dtype in %s." %
                       allowed_dtypes)

    if "dtype" not in kwargs:
      kwargs["dtype"] = dtypes.string

    if encoding is None:
      encoding = "utf-8"

    if vocabulary is not None:
      if isinstance(vocabulary, str):
        vocabulary = table_utils.get_vocabulary_from_file(vocabulary, encoding)

    self.encoding = encoding

    super(StringLookup, self).__init__(
        max_tokens=max_tokens,
        num_oov_indices=num_oov_indices,
        mask_token=mask_token,
        oov_token=oov_token,
        vocabulary=vocabulary,
        invert=invert,
        **kwargs)

  def get_config(self):
    config = {"encoding": self.encoding}
    base_config = super(StringLookup, self).get_config()
    return dict(list(base_config.items()) + list(config.items()))

  def get_vocabulary(self):
    if self._table_handler.vocab_size() == 0:
      return []

    keys, values = self._table_handler.data()
    # This is required because the MutableHashTable doesn't preserve insertion
    # order, but we rely on the order of the array to assign indices.
    return [x.decode(self.encoding) for _, x in sorted(zip(values, keys))]
