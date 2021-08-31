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
"""Tools for deserializing `Function`s."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import re

from tensorflow.core.framework import function_pb2
from tensorflow.python.eager import def_function
from tensorflow.python.eager import function as function_lib
from tensorflow.python.framework import func_graph as func_graph_lib
from tensorflow.python.framework import function_def_to_graph as function_def_lib
from tensorflow.python.framework import op_def_registry
from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_spec
from tensorflow.python.framework import type_spec
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.saved_model import nested_structure_coder
from tensorflow.python.util import compat
from tensorflow.python.util import nest
from tensorflow.python.util import tf_decorator
from tensorflow.python.util import tf_inspect


def _is_tensor(t):
  return isinstance(t, (ops.Tensor, resource_variable_ops.BaseResourceVariable))


# TODO(edloper): Update this to just use ConcreteFunction.__call__ with the
# structured signature.
def _call_concrete_function(function, inputs):
  """Calls a restored Function with structured inputs.

  This differs from `function.__call__` in that inputs and outputs are
  structured and that it casts inputs to tensors if needed.

  Note: this does not checks that non-tensor inputs match. That should be
  done before via `_concrete_function_callable_with`.

  Args:
    function: ConcreteFunction to call.
    inputs: Structured inputs compatible with
        `function.graph.structured_input_signature`.

  Returns:
    The structured function output.
  """
  expected_structure = function.graph.structured_input_signature
  flatten_inputs = nest.flatten_up_to(
      expected_structure, inputs, expand_composites=True)
  flatten_expected = nest.flatten(expected_structure, expand_composites=True)
  tensor_inputs = []
  for arg, expected in zip(flatten_inputs, flatten_expected):
    if isinstance(expected, tensor_spec.TensorSpec):
      tensor_inputs.append(
          ops.convert_to_tensor(arg, dtype_hint=expected.dtype))
  result = function._call_flat(tensor_inputs, function._captured_inputs)  # pylint: disable=protected-access
  if isinstance(result, ops.Operation):
    return None
  return result


def _try_convert_to_tensor_spec(arg, dtype_hint):
  """Returns None or TensorSpec obtained if `arg` is converted to tensor."""
  try:
    # Note: try conversion in a FuncGraph to avoid polluting current context.
    with func_graph_lib.FuncGraph(name="guess_conversion").as_default():
      result = ops.convert_to_tensor(arg, dtype_hint=dtype_hint)
      return tensor_spec.TensorSpec(shape=result.shape, dtype=result.dtype)
  except (TypeError, ValueError):
    return None


def _concrete_function_callable_with(function, inputs, allow_conversion):
  """Returns whether concrete `function` can be called with `inputs`."""
  expected_structure = function.graph.structured_input_signature
  try:
    flatten_inputs = nest.flatten_up_to(expected_structure, inputs)
  except (TypeError, ValueError):
    return False

  for arg, expected in zip(flatten_inputs, nest.flatten(expected_structure)):
    if isinstance(expected, tensor_spec.TensorSpec):
      if allow_conversion:
        arg = _try_convert_to_tensor_spec(arg, dtype_hint=expected.dtype)
      if not _is_tensor(arg) and not isinstance(arg, tensor_spec.TensorSpec):
        return False
      if arg.dtype != expected.dtype:
        return False
      if not expected.shape.is_compatible_with(arg.shape):
        return False
    elif isinstance(expected, type_spec.TypeSpec):
      if not expected.is_compatible_with(arg):
        return False
    elif _is_tensor(arg):
      if id(arg) != id(expected):
        return False
    else:
      if arg != expected:
        return False
  return True


def _deserialize_function_spec_as_nonmethod(function_spec_proto, coder):
  """Deserialize a FunctionSpec object from its proto representation."""
  typeless_fullargspec = coder.decode_proto(function_spec_proto.fullargspec)

  # Convert a method function into a non method.
  if function_spec_proto.is_method:
    if not typeless_fullargspec.args:
      raise NotImplementedError(
          "Missing support to deserialize a method function without a named "
          "'self' argument.")
    args = typeless_fullargspec.args[1:]
  else:
    args = typeless_fullargspec.args

  fullargspec = tf_inspect.FullArgSpec(
      args=args,
      varargs=typeless_fullargspec.varargs,
      varkw=typeless_fullargspec.varkw,
      defaults=typeless_fullargspec.defaults,
      kwonlyargs=typeless_fullargspec.kwonlyargs,
      kwonlydefaults=typeless_fullargspec.kwonlydefaults,
      annotations=typeless_fullargspec.annotations)
  input_signature = coder.decode_proto(function_spec_proto.input_signature)
  return function_lib.FunctionSpec(fullargspec=fullargspec,
                                   is_method=False,
                                   input_signature=input_signature)


# TODO(allenl): The fact that we can't derive ConcreteFunction calling
# conventions from the serialized input spec right now is unfortunate. Merging
# these would be good, maybe by adding TensorSpec names to cache keys so renamed
# keyword arguments would yield different ConcreteFunctions.
def setup_bare_concrete_function(saved_bare_concrete_function,
                                 concrete_functions):
  """Makes a restored bare concrete function callable."""
  # Bare concrete functions accept only flat lists of Tensors with unique
  # names.
  concrete_function = concrete_functions[
      saved_bare_concrete_function.concrete_function_name]
  # pylint: disable=protected-access
  concrete_function._arg_keywords = (
      saved_bare_concrete_function.argument_keywords)
  concrete_function._num_positional_args = (
      saved_bare_concrete_function.allowed_positional_arguments)
  # pylint: enable=protected-access
  concrete_function.add_to_graph()
  return concrete_function


class RestoredFunction(def_function.Function):
  """Wrapper class for a function that has been restored from saved state.

  See `def_function.Function`.
  """

  def __init__(self, python_function, name, function_spec, concrete_functions):
    # TODO(mdan): We may enable autograph once exceptions are supported.
    super(RestoredFunction, self).__init__(
        python_function, name, autograph=False)
    self.concrete_functions = concrete_functions
    self._function_spec = function_spec

  def _list_all_concrete_functions_for_serialization(self):
    return self.concrete_functions

  def _defun_with_scope(self, scope):
    func = super(RestoredFunction, self)._defun_with_scope(scope)
    func._function_spec = self._function_spec  # pylint: disable=protected-access
    return func


def recreate_function(saved_function, concrete_functions):
  """Creates a `Function` from a `SavedFunction`.

  Args:
    saved_function: `SavedFunction` proto.
    concrete_functions: map from function name to `ConcreteFunction`.
      As a side effect of this function, the `FunctionSpec` from
      `saved_function` is added to each `ConcreteFunction` in this map.

  Returns:
    A `Function`.
  """
  # TODO(andresp): Construct a `Function` with the cache populated
  # instead of creating a new `Function` backed by a Python layer to
  # glue things together. Current approach is nesting functions deeper for each
  # serialization cycle.

  coder = nested_structure_coder.StructureCoder()

  # Note: handling method functions is tricky since make_decorator does not
  # allows control of "ismethod". Additionally since restored functions do
  # not behave as methods i.e. they always use the same captured tensors
  # independent of the object they are bound to, there is little value on
  # propagating that correctly.
  #
  # Ideally this conversion should happen at serialization time. But since
  # there are SavedModels which have "ismethod" populated and have an extra
  # argument that they expect to be ignored, we do it at deserialization.
  function_spec = _deserialize_function_spec_as_nonmethod(
      saved_function.function_spec,
      coder)

  def restored_function_body(*args, **kwargs):
    """Calls a restored function."""
    # This is the format of function.graph.structured_input_signature. At this
    # point, the args and kwargs have already been canonicalized.
    inputs = (args, kwargs)

    # First try to find a concrete function that can be called without input
    # conversions. This allows one to pick a more specific trace in case there
    # was also a more expensive one that supported tensors.
    for allow_conversion in [False, True]:
      for function_name in saved_function.concrete_functions:
        function = concrete_functions[function_name]
        if _concrete_function_callable_with(function, inputs, allow_conversion):
          return _call_concrete_function(function, inputs)

    signature_descriptions = []

    def _pretty_format_positional(positional):
      return "Positional arguments ({} total):\n    * {}".format(
          len(positional), "\n    * ".join(str(a) for a in positional))

    for index, function_name in enumerate(saved_function.concrete_functions):
      concrete_function = concrete_functions[function_name]
      positional, keyword = concrete_function.structured_input_signature
      signature_descriptions.append(
          "Option {}:\n  {}\n  Keyword arguments: {}"
          .format(index + 1, _pretty_format_positional(positional), keyword))
    raise ValueError(
        "Could not find matching function to call loaded from the SavedModel. "
        "Got:\n  {}\n  Keyword arguments: {}\n\nExpected "
        "these arguments to match one of the following {} option(s):\n\n{}"
        .format(_pretty_format_positional(args), kwargs,
                len(saved_function.concrete_functions),
                "\n\n".join(signature_descriptions)))

  concrete_function_objects = []
  for concrete_function_name in saved_function.concrete_functions:
    concrete_function_objects.append(concrete_functions[concrete_function_name])

  for cf in concrete_function_objects:
    cf._set_function_spec(function_spec)  # pylint: disable=protected-access

  restored_function = RestoredFunction(
      restored_function_body,
      restored_function_body.__name__,
      function_spec,
      concrete_function_objects)

  return tf_decorator.make_decorator(
      restored_function_body,
      restored_function,
      decorator_argspec=function_spec.fullargspec)


def load_function_def_library(library, load_shared_name_suffix=None):
  """Load a set of functions as concrete functions without captured inputs.

  Functions names are manipulated during load such that they do not overlap
  with previously created ones.

  Args:
    library: FunctionDefLibrary proto message.
    load_shared_name_suffix: If specified, used to uniquify shared
      names. Otherwise, a unique name is generated.

  Returns:
    Map of original function names in the library to instances of
    `ConcreteFunction` without captured inputs.

  Raises:
    ValueError: if functions dependencies have a cycle.
  """
  library_function_names = set(fdef.signature.name for fdef in library.function)
  functions = {}
  renamed_functions = {}

  # Our graph building code currently requires functions to be registered with
  # some tf.Graph in order to import functions using the
  # op-name-is-function-name calling convention. To avoid leaking memory into
  # the global default graph when executing eagerly, we create a temporary
  # Graph.
  #
  # TODO(allenl): Make this Graph creation unnecessary when executing eagerly by
  # fixing function_def_to_graph_def.
  if ops.executing_eagerly_outside_functions():
    graph = ops.Graph()
  else:
    graph = ops.get_default_graph()

  if load_shared_name_suffix is None:
    load_shared_name_suffix = "_load_{}".format(ops.uid())
  for fdef in _sort_function_defs(library, library_function_names):
    copy = _fix_fdef(fdef, functions, load_shared_name_suffix)

    # There is no need to copy all functions into the function def graph. It
    # leads to a O(n^2) increase of memory when importing functions and the
    # extra function definitions are a no-op since they already imported as a
    # function before and passed in explicitly (due to the topologic sort
    # import).
    with graph.as_default():
      func_graph = function_def_lib.function_def_to_graph(copy)
    _restore_gradient_functions(func_graph, renamed_functions)

    for dep in _list_function_deps(fdef, library_function_names):
      functions[dep].add_to_graph(func_graph)

    # We do not initialize the new ConcreteFunction's function_spec or
    # arg_keywords here (which are used to parse the structured and flat
    # signatures, respectively).  function_spec is set up later by
    # recreate_function(); and arg_keywords by setup_bare_concrete_function().
    func = function_lib.ConcreteFunction(func_graph)
    func.add_to_graph(graph)

    functions[fdef.signature.name] = func
    renamed_functions[func.name] = func
    if any(op.type == "TRTEngineOp" for op in func_graph.get_operations()):
      # TODO(b/150708051): Remove this hack once TensorRT SavedModel integration
      # is fixed. Currently it's leaking memory to maintain bug compatibility
      # with previous behavior.
      func.add_to_graph(ops.get_default_graph())

  return functions


def _restore_gradient_functions(func_graph, renamed_functions):
  """Populate function op's _gradient_function with default gradient."""
  for op in func_graph.get_operations():
    # TODO(andresp): This code assumes that the gradient registered for this
    # function call is the default gradient for the function and not a custom
    # one.
    if op.type in ["StatefulPartitionedCall", "PartitionedCall"]:
      function = renamed_functions[compat.as_bytes(
          op.node_def.attr["f"].func.name)]
      op._gradient_function = function._get_gradient_function()  # pylint: disable=protected-access


def _sort_function_defs(library, library_function_names):
  """Return a topologic sort of FunctionDefs in a library."""
  edges = collections.defaultdict(list)
  in_count = collections.defaultdict(lambda: 0)

  for fdef in library.function:
    for dep in _list_function_deps(fdef, library_function_names):
      edges[dep].append(fdef.signature.name)
      in_count[fdef.signature.name] += 1

  ready = [
      fdef.signature.name
      for fdef in library.function
      if in_count[fdef.signature.name] == 0
  ]
  output = []
  while ready:
    node = ready.pop()
    output.append(node)
    for dest in edges[node]:
      in_count[dest] -= 1
      if not in_count[dest]:
        ready.append(dest)

  if len(output) != len(library.function):
    failed_to_resolve = sorted(set(in_count.keys()) - set(output))
    raise ValueError("There is a cyclic-dependency between functions. ",
                     "Could not resolve %r." % (failed_to_resolve,))

  reverse = {fdef.signature.name: fdef for fdef in library.function}
  return [reverse[x] for x in output]


def fix_node_def(node_def, functions, shared_name_suffix, debug_name):
  """Replace functions calls and shared names in `node_def`."""
  if ("_gradient_op_type" in node_def.attr and
      node_def.op not in ["StatefulPartitionedCall", "PartitionedCall"]):
    logging.warning(
        "Importing a function (%s) with ops with custom gradients. Will likely "
        "fail if a gradient is requested.", debug_name)
  if node_def.op in functions:
    node_def.op = functions[node_def.op].name
  for _, attr_value in node_def.attr.items():
    if attr_value.WhichOneof("value") == "func":
      attr_value.func.name = functions[attr_value.func.name].name
    elif attr_value.WhichOneof("value") == "list":
      for fn in attr_value.list.func:
        fn.name = functions[fn.name].name

  # Fix old table creation bug.
  if node_def.op == "HashTableV2":
    if ("use_node_name_sharing" not in node_def.attr or
        not node_def.attr["use_node_name_sharing"].b):
      node_def.attr["use_node_name_sharing"].b = True
      # We are turning on node mame sharing, so have to make sure we don't
      # accidentally share a table resource.
      shared_name_suffix += "_{}".format(ops.uid())

  # TODO(b/124205571): Avoid accidental sharing and destruction of restored
  # resources. For now uniquify "shared_name" when loading functions to avoid
  # sharing.
  # TODO: Add regression test for b/150826922.
  op_def = op_def_registry.get(node_def.op)
  if op_def:
    attr = next((a for a in op_def.attr if a.name == "shared_name"), None)
    if attr:
      shared_name = None
      if "shared_name" in node_def.attr and node_def.attr["shared_name"].s:
        shared_name = node_def.attr["shared_name"].s
      elif attr.default_value.s:
        shared_name = compat.as_bytes(attr.default_value.s)
      if not shared_name:
        shared_name = compat.as_bytes(node_def.name)

      node_def.attr["shared_name"].s = (
          shared_name + compat.as_bytes(shared_name_suffix))


def _fix_fdef(orig_fdef, functions, shared_name_suffix):
  """Fixes a FunctionDef proto to be loaded in current context.

  In particular, when loading a function library into an eager context, one
  must rename the functions to avoid conflicts with existent functions.

  Args:
    orig_fdef: FunctionDef proto to fix. It is not modified.
    functions: map from function name to a ConcreteFunction instance.
    shared_name_suffix: A unique string for this load which helps to avoid
      `shared_name` collisions across loads. Two functions from the same load
      using the same `shared_name` still need to share, but functions from
      different loads with the same `shared_name` should not.

  Returns:
    A fixed copy of the original FunctionDef.
  """
  fdef = function_pb2.FunctionDef()
  fdef.CopyFrom(orig_fdef)
  for node_def in fdef.node_def:
    fix_node_def(node_def, functions, shared_name_suffix, fdef.signature.name)

  fdef.signature.name = _clean_function_name(fdef.signature.name)
  return fdef


def _list_function_deps(fdef, library_function_names):
  """Find functions referenced in `fdef`."""
  # TODO(andresp): Recurse into list attributes and into NameAttrList attrs both
  # when listing deps and when fixing them. `function_def_to_graph` also
  # requires fixes.
  deps = set()
  for node_def in fdef.node_def:
    if node_def.op in library_function_names:
      deps.add(node_def.op)
    else:
      for _, attr_value in node_def.attr.items():
        if attr_value.WhichOneof("value") == "func":
          deps.add(attr_value.func.name)
        elif attr_value.WhichOneof("value") == "list":
          for fn in attr_value.list.func:
            deps.add(fn.name)

  return deps


_FUNCTION_WRAPPER_NAME_REGEX = r"^%s(.*)_\d+$" % (function_lib._INFERENCE_PREFIX
                                                 )  # pylint:disable=protected-access


def _clean_function_name(name):
  """Vanity function to keep the function names comprehensible."""
  # Note: each time a function is wrapped into `function_lib.ConcreteFunction`
  # its name becomes "__inference_<orig>_xyz".
  match = re.search(_FUNCTION_WRAPPER_NAME_REGEX, name)
  if match:
    return match.group(1)
  else:
    return name
