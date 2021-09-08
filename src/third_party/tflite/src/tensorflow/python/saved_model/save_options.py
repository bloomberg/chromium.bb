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
"""Options for saving SavedModels."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import six

from tensorflow.python.util import compat
from tensorflow.python.util.tf_export import tf_export


@tf_export("saved_model.SaveOptions")
class SaveOptions(object):
  """Options for saving to SavedModel.

  This function may be used in the `options` argument in functions that
  save a SavedModel (`tf.saved_model.save`, `tf.keras.models.save_model`).
  """

  # Define object attributes in __slots__ for improved memory and performance.
  __slots__ = ("namespace_whitelist", "save_debug_info", "function_aliases",
               "experimental_io_device")

  def __init__(self,
               namespace_whitelist=None,
               save_debug_info=False,
               function_aliases=None,
               experimental_io_device=None):
    """Creates an object that stores options for SavedModel saving.

    Args:
      namespace_whitelist: List of strings containing op namespaces to whitelist
        when saving a model. Saving an object that uses namespaced ops must
        explicitly add all namespaces to the whitelist. The namespaced ops must
        be registered into the framework when loading the SavedModel.
      save_debug_info: Boolean indicating whether debug information is saved. If
        True, then a debug/saved_model_debug_info.pb file will be written with
        the contents of a GraphDebugInfo binary protocol buffer containing stack
        trace information for all ops and functions that are saved.
      function_aliases: Python dict. Mapping from string to object returned by
        @tf.function. A single tf.function can generate many ConcreteFunctions.
        If a downstream tool wants to refer to all concrete functions generated
        by a single tf.function you can use the `function_aliases` argument to
        store a map from the alias name to all concrete function names.
        E.g.
        ```python
        class MyModel:
        @tf.function
        def func():
          ...

        @tf.function
        def serve():
          ...
          func()

        model = MyModel()
        signatures = {
            'serving_default': model.serve.get_concrete_function(),
        }
        options = tf.saved_model.SaveOptions(function_aliases={
            'my_func': func,
        })
        tf.saved_model.save(model, export_dir, signatures, options)
        ```
      experimental_io_device: string. Applies in a distributed setting.
        Tensorflow device to use to access the filesystem. If `None` (default)
        then for each variable the filesystem is accessed from the CPU:0 device
        of the host where that variable is assigned. If specified, the
        filesystem is instead accessed from that device for all variables.

        This is for example useful if you want to save to a local directory,
        such as "/tmp" when running in a distributed setting. In that case pass
        a device for the host where the "/tmp" directory is accessible.
    """
    self.namespace_whitelist = _validate_namespace_whitelist(
        namespace_whitelist)
    self.save_debug_info = save_debug_info
    self.function_aliases = function_aliases if function_aliases else dict()
    self.experimental_io_device = experimental_io_device


def _validate_namespace_whitelist(namespace_whitelist):
  """Validates namespace whitelist argument."""
  if namespace_whitelist is None:
    return []
  if not isinstance(namespace_whitelist, list):
    raise TypeError("Namespace whitelist must be a list of strings.")

  processed = []
  for namespace in namespace_whitelist:
    if not isinstance(namespace, six.string_types):
      raise ValueError("Whitelisted namespace must be a string. Got: {} of type"
                       " {}.".format(namespace, type(namespace)))
    processed.append(compat.as_str(namespace))
  return processed
