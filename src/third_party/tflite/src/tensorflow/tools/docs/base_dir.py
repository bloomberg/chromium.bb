# Lint as: python3
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
"""Opensource base_dir configuration for tensorflow doc-generator."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import distutils
from os import path

import tensorboard
import tensorflow as tf
import tensorflow_estimator


def get_base_dirs_and_prefixes(code_url_prefix):
  """Returns the base_dirs and code_prefixes for OSS TensorFlow api gen."""
  base_dir = path.dirname(tf.__file__)

  if distutils.version.LooseVersion(tf.__version__) >= "2.2":
    base_dirs = [
        base_dir,
        path.dirname(tensorboard.__file__),
        path.dirname(tensorflow_estimator.__file__),
    ]
  else:
    base_dirs = [
        path.normpath(path.join(base_dir, "../tensorflow_core")),
        path.dirname(tensorboard.__file__),
        path.dirname(tensorflow_estimator.__file__),
    ]

  code_url_prefixes = (
      code_url_prefix,
      "https://github.com/tensorflow/tensorboard/tree/master/tensorboard",
      "https://github.com/tensorflow/estimator/tree/master/tensorflow_estimator",
  )

  return base_dirs, code_url_prefixes
