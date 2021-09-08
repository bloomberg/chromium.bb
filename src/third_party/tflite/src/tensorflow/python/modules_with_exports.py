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
"""Imports modules that should be scanned during API generation.

This file should eventually contain everything we need to scan looking for
tf_export decorators.
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

# go/tf-wildcard-import
# pylint: disable=wildcard-import,g-bad-import-order,g-import-not-at-top
# pylint: disable=unused-import,g-importing-member

# Protocol buffers
from tensorflow.core.framework.graph_pb2 import *
from tensorflow.core.framework.node_def_pb2 import *
from tensorflow.core.framework.summary_pb2 import *
from tensorflow.core.framework.attr_value_pb2 import *
from tensorflow.core.protobuf.meta_graph_pb2 import TensorInfo
from tensorflow.core.protobuf.meta_graph_pb2 import MetaGraphDef
from tensorflow.core.protobuf.config_pb2 import *
from tensorflow.core.util.event_pb2 import *

# Framework
from tensorflow.python.framework.framework_lib import *  # pylint: disable=redefined-builtin
from tensorflow.python.framework.versions import *
from tensorflow.python.framework import config
from tensorflow.python.framework import errors
from tensorflow.python.framework import graph_util

# Session
from tensorflow.python.client.client_lib import *

# Ops
from tensorflow.python.ops.standard_ops import *

# Namespaces
from tensorflow.python.ops import initializers_ns as initializers

from tensorflow.python.util.tf_export import tf_export

# Export protos
# pylint: disable=undefined-variable
tf_export(v1=['AttrValue'])(AttrValue)
tf_export(v1=['ConfigProto'])(ConfigProto)
tf_export(v1=['Event', 'summary.Event'])(Event)
tf_export(v1=['GPUOptions'])(GPUOptions)
tf_export(v1=['GraphDef'])(GraphDef)
tf_export(v1=['GraphOptions'])(GraphOptions)
tf_export(v1=['HistogramProto'])(HistogramProto)
tf_export(v1=['LogMessage'])(LogMessage)
tf_export(v1=['MetaGraphDef'])(MetaGraphDef)
tf_export(v1=['NameAttrList'])(NameAttrList)
tf_export(v1=['NodeDef'])(NodeDef)
tf_export(v1=['OptimizerOptions'])(OptimizerOptions)
tf_export(v1=['RunMetadata'])(RunMetadata)
tf_export(v1=['RunOptions'])(RunOptions)
tf_export(v1=['SessionLog', 'summary.SessionLog'])(SessionLog)
tf_export(v1=['Summary', 'summary.Summary'])(Summary)
tf_export(v1=['summary.SummaryDescription'])(SummaryDescription)
tf_export(v1=['SummaryMetadata'])(SummaryMetadata)
tf_export(v1=['summary.TaggedRunMetadata'])(TaggedRunMetadata)
tf_export(v1=['TensorInfo'])(TensorInfo)
# pylint: enable=undefined-variable
