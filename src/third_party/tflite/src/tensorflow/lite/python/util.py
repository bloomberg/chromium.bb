# Lint as: python2, python3
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
"""Functions used by multiple converter files."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import datetime
import sys

import six
from six.moves import range

from tensorflow.core.protobuf import config_pb2 as _config_pb2
from tensorflow.core.protobuf import graph_debug_info_pb2
from tensorflow.core.protobuf import meta_graph_pb2 as _meta_graph_pb2
from tensorflow.lite.python.op_hint import convert_op_hints_to_stubs
from tensorflow.lite.python.op_hint import find_all_hinted_output_nodes
from tensorflow.lite.toco import types_pb2 as _types_pb2
from tensorflow.python.eager import function
from tensorflow.python.framework import convert_to_constants as _convert_to_constants
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import error_interpolation as _error_interpolation
from tensorflow.python.framework import graph_util as tf_graph_util
from tensorflow.python.grappler import tf_optimizer
from tensorflow.python.training.saver import export_meta_graph as _export_meta_graph

# Map of tf.dtypes to TFLite types_flag_pb2.
_MAP_TF_TO_TFLITE_TYPES = {
    dtypes.float32: _types_pb2.FLOAT,
    dtypes.float16: _types_pb2.FLOAT16,
    dtypes.float64: _types_pb2.FLOAT64,
    dtypes.int32: _types_pb2.INT32,
    dtypes.int64: _types_pb2.INT64,
    dtypes.string: _types_pb2.STRING,
    dtypes.uint8: _types_pb2.QUANTIZED_UINT8,
    dtypes.int8: _types_pb2.INT8,
    dtypes.int16: _types_pb2.QUANTIZED_INT16,
    dtypes.complex64: _types_pb2.COMPLEX64,
    dtypes.bool: _types_pb2.BOOL,
}


def convert_dtype_to_tflite_type(tf_dtype):
  """Converts tf.dtype to TFLite proto type.

  Args:
    tf_dtype: tf.dtype

  Raises:
    ValueError: Unsupported tf.dtype.

  Returns:
    types_flag_pb2.
  """
  result = _MAP_TF_TO_TFLITE_TYPES.get(tf_dtype)
  if result is None:
    raise ValueError("Unsupported tf.dtype {0}".format(tf_dtype))
  return result


def get_tensor_name(tensor):
  """Returns name of the input tensor.

  Args:
    tensor: tf.Tensor

  Returns:
    str
  """
  parts = six.ensure_str(tensor.name).split(":")
  if len(parts) > 2:
    raise ValueError("Tensor name invalid. Expect 0 or 1 colon, got {0}".format(
        len(parts) - 1))

  # To be consistent with the tensor naming scheme in tensorflow, we need
  # drop the ':0' suffix for the first tensor.
  if len(parts) > 1 and parts[1] != "0":
    return tensor.name
  return parts[0]


def get_tensors_from_tensor_names(graph, tensor_names):
  """Gets the Tensors associated with the `tensor_names` in the provided graph.

  Args:
    graph: TensorFlow Graph.
    tensor_names: List of strings that represent names of tensors in the graph.

  Returns:
    A list of Tensor objects in the same order the names are provided.

  Raises:
    ValueError:
      tensor_names contains an invalid tensor name.
  """
  # Get the list of all of the tensors.
  tensor_name_to_tensor = {}
  for op in graph.get_operations():
    for tensor in op.values():
      tensor_name_to_tensor[get_tensor_name(tensor)] = tensor

  # Get the tensors associated with tensor_names.
  tensors = []
  invalid_tensors = []
  for name in tensor_names:
    if not isinstance(name, six.string_types):
      raise ValueError("Invalid type for a tensor name in the provided graph. "
                       "Expected type for a tensor name is 'str', instead got "
                       "type '{}' for tensor name '{}'".format(
                           type(name), name))

    tensor = tensor_name_to_tensor.get(name)
    if tensor is None:
      invalid_tensors.append(name)
    else:
      tensors.append(tensor)

  # Throw ValueError if any user input names are not valid tensors.
  if invalid_tensors:
    raise ValueError("Invalid tensors '{}' were found.".format(
        ",".join(invalid_tensors)))
  return tensors


def set_tensor_shapes(tensors, shapes):
  """Sets Tensor shape for each tensor if the shape is defined.

  Args:
    tensors: TensorFlow ops.Tensor.
    shapes: Dict of strings representing input tensor names to list of
      integers representing input shapes (e.g., {"foo": : [1, 16, 16, 3]}).

  Raises:
    ValueError:
      `shapes` contains an invalid tensor.
      `shapes` contains an invalid shape for a valid tensor.
  """
  if shapes:
    tensor_names_to_tensor = {
        get_tensor_name(tensor): tensor for tensor in tensors
    }
    for name, shape in shapes.items():
      if name not in tensor_names_to_tensor:
        raise ValueError("Invalid tensor \'{}\' found in tensor shapes "
                         "map.".format(name))
      if shape is not None:
        tensor = tensor_names_to_tensor[name]
        try:
          tensor.set_shape(shape)
        except ValueError as error:
          message = ("The shape of tensor '{0}' cannot be changed from {1} to "
                     "{2}. {3}".format(name, tensor.shape, shape, str(error)))
          raise ValueError(message)


def get_grappler_config(optimizers_list):
  """Creates a tf.compat.v1.ConfigProto for configuring Grappler.

  Args:
    optimizers_list: List of strings that represents the list of optimizers.

  Returns:
    tf.ConfigProto.
  """
  config = _config_pb2.ConfigProto()
  rewrite_options = config.graph_options.rewrite_options
  for optimizer in optimizers_list:
    rewrite_options.optimizers.append(optimizer)
  return config


def run_graph_optimizations(graph_def,
                            input_arrays,
                            output_arrays,
                            config,
                            graph=None):
  """Apply standard TensorFlow optimizations to the graph_def.

  Args:
    graph_def: Frozen GraphDef to be optimized.
    input_arrays: List of arrays that are considered inputs of the graph.
    output_arrays: List of arrays that are considered outputs of the graph.
    config: tf.ConfigProto.
    graph: TensorFlow Graph. Required when Eager mode is enabled. (default None)

  Returns:
    A new, optimized GraphDef.
  """
  meta_graph = _export_meta_graph(graph_def=graph_def, graph=graph)

  signature = _meta_graph_pb2.SignatureDef()
  for array in input_arrays:
    signature.inputs[array.name].name = array.name
    signature.inputs[array.name].dtype = array.dtype.as_datatype_enum
    signature.inputs[array.name].tensor_shape.CopyFrom(array.shape.as_proto())

  for array in output_arrays:
    signature.outputs[array.name].name = array.name
    signature.outputs[array.name].dtype = array.dtype.as_datatype_enum
    signature.outputs[array.name].tensor_shape.CopyFrom(array.shape.as_proto())

  meta_graph.signature_def["not_used_key"].CopyFrom(signature)

  # We need to add a collection called 'train_op' so that grappler
  # knows what the outputs are.
  fetch_collection = _meta_graph_pb2.CollectionDef()
  for array in input_arrays + output_arrays:
    fetch_collection.node_list.value.append(array.name)
  meta_graph.collection_def["train_op"].CopyFrom(fetch_collection)

  return tf_optimizer.OptimizeGraph(config, meta_graph)


def _convert_op_hints_if_present(sess, graph_def, output_tensors,
                                 hinted_outputs_nodes):
  if is_frozen_graph(sess):
    raise ValueError("Try to convert op hints, needs unfrozen graph.")
  output_arrays = [get_tensor_name(tensor) for tensor in output_tensors]
  graph_def = tf_graph_util.convert_variables_to_constants(
      sess, graph_def, output_arrays + hinted_outputs_nodes)
  graph_def = convert_op_hints_to_stubs(graph_def=graph_def)
  return graph_def


def freeze_graph(sess, input_tensors, output_tensors):
  """Returns a frozen GraphDef.

  Runs a Grappler pass and freezes a graph with Variables in it. Otherwise the
  existing GraphDef is returned. The Grappler pass is only run on models that
  are frozen in order to inline the functions in the graph.
  If OpHints is present, it will try to convert the OpHint graph.

  Args:
    sess: TensorFlow Session.
    input_tensors: List of input tensors.
    output_tensors: List of output tensors (only .name is used from this).

  Returns:
    Frozen GraphDef.
  """
  # Runs a Grappler pass in order to inline any functions in the graph.
  # Asides from inlining any simple function, Grappler will also try to lower
  # while loop into switch merge representation which is undesired for Ophints,
  # so we simply remove those attributes to prevent Grappler from doing so.
  graph_def = _convert_to_constants.disable_lower_using_switch_merge(
      sess.graph_def)
  config = get_grappler_config(["function"])
  graph_def = run_graph_optimizations(
      graph_def, input_tensors, output_tensors, config, graph=sess.graph)

  # If ophints are present, just convert them.
  hinted_outputs_nodes = find_all_hinted_output_nodes(sess)
  if hinted_outputs_nodes:
    return _convert_op_hints_if_present(sess, graph_def, output_tensors,
                                        hinted_outputs_nodes)

  if not is_frozen_graph(sess):
    output_node_names = [tensor.name.split(":")[0] for tensor in output_tensors]
    return tf_graph_util.convert_variables_to_constants(sess, graph_def,
                                                        output_node_names)
  else:
    return sess.graph_def


def is_frozen_graph(sess):
  """Determines if the graph is frozen.

  Determines if a graph has previously been frozen by checking for any
  operations of type Variable*. If variables are found, the graph is not frozen.

  Args:
    sess: TensorFlow Session.

  Returns:
    Bool.
  """
  for op in sess.graph.get_operations():
    if six.ensure_str(op.type).startswith("Variable") or six.ensure_str(
        op.type).endswith("VariableOp"):
      return False
  return True


def build_debug_info_func(original_graph):
  """Returns a method to retrieve the `GraphDebugInfo` from the original graph.

  Args:
    original_graph: The original `Graph` containing all the op stack traces.

  Returns:
    A function which retrieves the stack traces from the original graph and
    converts them to a `GraphDebugInfo` for a given set of nodes.
  """

  def f(original_nodes):
    """Function to create `GraphDebugInfo` for the given `original_nodes`."""
    if not original_graph:
      return None
    # For the given nodes, gets all the op definitions in the original graph.
    useful_ops = []
    for func, name in original_nodes:
      try:
        if not func:
          useful_ops.append((func, original_graph.get_operation_by_name(name)))
        else:
          sub_func = original_graph._get_function(func)  # pylint: disable=protected-access
          if isinstance(sub_func, function._EagerDefinedFunction):  # pylint: disable=protected-access
            useful_ops.append(
                (func, sub_func.graph.get_operation_by_name(name)))
          else:
            sys.stderr.write(
                "Use '@tf.function' or '@defun' to decorate the function.")
            continue
      except KeyError:
        # New node created by graph optimizer. No stack trace from source code.
        continue
    # Convert all the op definitions to stack traces in terms of GraphDebugInfo.
    return _error_interpolation.create_graph_debug_info_def(useful_ops)

  return f


def convert_debug_info_func(saved_debug_info):
  """Returns a method to retrieve the `GraphDebugInfo` from the original graph.

  Args:
    saved_debug_info: The `GraphDebugInfo` containing all the debug info.

  Returns:
    A function which retrieves the stack traces from the original graph and
    converts them to a `GraphDebugInfo` for a given set of nodes.
  """

  def f(original_nodes):
    """Function to create `GraphDebugInfo` for the given `original_nodes`."""
    if not saved_debug_info:
      return None

    output_debug_info = graph_debug_info_pb2.GraphDebugInfo()
    # All the files are copied over, so the index wouldn't be changed.
    output_debug_info.files[:] = saved_debug_info.files
    # We only copy over the debug info for the input nodes
    for func, node in original_nodes:
      debug_key = node + "@" + func
      output_debug_info.traces[debug_key].CopyFrom(
          saved_debug_info.traces[debug_key])
    return output_debug_info

  return f


def get_debug_info(nodes_to_debug_info_func, converted_graph):
  """Returns the debug info for the original nodes in the `converted_graph`.

  Args:
    nodes_to_debug_info_func: The method to collect the op debug info for the
      nodes.
    converted_graph: A `GraphDef` after optimization and transformation.

  Returns:
    `GraphDebugInfo` for all the original nodes in `converted_graph`.
  """
  if not nodes_to_debug_info_func:
    return None

  # Collect all the debug info nodes from the converted_graph
  original_nodes = set()
  for node in converted_graph.node:
    debug_nodes = node.experimental_debug_info.original_node_names
    debug_funcs = node.experimental_debug_info.original_func_names
    # If the `original_node_names` are empty, uses the node name directly.
    if not debug_nodes:
      original_nodes.add(("", node.name))
    else:
      for i in range(len(debug_nodes)):
        debug_func = "" if i >= len(debug_funcs) else debug_funcs[i]
        original_nodes.add((debug_func, debug_nodes[i]))

  # Convert the nodes to the debug info proto object.
  return nodes_to_debug_info_func(original_nodes)


def convert_bytes_to_c_source(data,
                              array_name,
                              max_line_width=80,
                              include_guard=None,
                              include_path=None,
                              use_tensorflow_license=False):
  """Returns strings representing a C constant array containing `data`.

  Args:
    data: Byte array that will be converted into a C constant.
    array_name: String to use as the variable name for the constant array.
    max_line_width: The longest line length, for formatting purposes.
    include_guard: Name to use for the include guard macro definition.
    include_path: Optional path to include in the source file.
    use_tensorflow_license: Whether to include the standard TensorFlow Apache2
      license in the generated files.

  Returns:
    Text that can be compiled as a C source file to link in the data as a
    literal array of values.
    Text that can be used as a C header file to reference the literal array.
  """

  starting_pad = "   "
  array_lines = []
  array_line = starting_pad
  for value in bytearray(data):
    if (len(array_line) + 4) > max_line_width:
      array_lines.append(array_line + "\n")
      array_line = starting_pad
    array_line += " 0x%02x," % (value)
  if len(array_line) > len(starting_pad):
    array_lines.append(array_line + "\n")
  array_values = "".join(array_lines)

  if include_guard is None:
    include_guard = "TENSORFLOW_LITE_UTIL_" + array_name.upper() + "_DATA_H_"

  if include_path is not None:
    include_line = "#include \"{include_path}\"\n".format(
        include_path=include_path)
  else:
    include_line = ""

  if use_tensorflow_license:
    license_text = """
/* Copyright {year} The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
""".format(year=datetime.date.today().year)
  else:
    license_text = ""

  source_template = """{license_text}
// This is a TensorFlow Lite model file that has been converted into a C data
// array using the tensorflow.lite.util.convert_bytes_to_c_source() function.
// This form is useful for compiling into a binary for devices that don't have a
// file system.

{include_line}
// We need to keep the data array aligned on some architectures.
#ifdef __has_attribute
#define HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define HAVE_ATTRIBUTE(x) 0
#endif
#if HAVE_ATTRIBUTE(aligned) || (defined(__GNUC__) && !defined(__clang__))
#define DATA_ALIGN_ATTRIBUTE __attribute__((aligned(4)))
#else
#define DATA_ALIGN_ATTRIBUTE
#endif

const unsigned char {array_name}[] DATA_ALIGN_ATTRIBUTE = {{
{array_values}}};
const int {array_name}_len = {array_length};
"""

  source_text = source_template.format(
      array_name=array_name,
      array_length=len(data),
      array_values=array_values,
      license_text=license_text,
      include_line=include_line)

  header_template = """
{license_text}

// This is a TensorFlow Lite model file that has been converted into a C data
// array using the tensorflow.lite.util.convert_bytes_to_c_source() function.
// This form is useful for compiling into a binary for devices that don't have a
// file system.

#ifndef {include_guard}
#define {include_guard}

extern const unsigned char {array_name}[];
extern const int {array_name}_len;

#endif  // {include_guard}
"""

  header_text = header_template.format(
      array_name=array_name,
      include_guard=include_guard,
      license_text=license_text)

  return source_text, header_text
