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

import functools
import itertools
import pickle
import re
import sys
import unittest
import weakref

from absl.testing import parameterized
from six.moves import range

from tensorflow.python.autograph.core import converter
from tensorflow.python.eager import backprop
from tensorflow.python.eager import def_function
from tensorflow.python.eager import lift_to_graph
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import errors
from tensorflow.python.framework import extension_type
from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_spec
from tensorflow.python.framework import test_util
from tensorflow.python.module import module
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import cond_v2
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import random_ops
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.ops import variables
from tensorflow.python.platform import test
from tensorflow.python.saved_model import save_context
from tensorflow.python.saved_model import save_options
from tensorflow.python.saved_model.load import load
from tensorflow.python.saved_model.save import save
from tensorflow.python.training.tracking.util import Checkpoint


def undecorated_function(x):
  return x * 3.


class _HasDecoratedMethod(object):

  @def_function.function
  def f(self, x):
    return x * 3.


class DefFunctionTest(test.TestCase, parameterized.TestCase):

  def testNoVariables(self):

    @def_function.function
    def fn(x):
      return 2 * x

    self.assertAllEqual(fn(constant_op.constant(4.0)), 8.0)

  def testFailIfVariablesAreCreatedMoreThanOnce(self):

    @def_function.function
    def fn(x):
      return variables.Variable(1.0) + x

    with self.assertRaises(ValueError):
      fn(1.0)

  def testFailIfVariablesAreCreatedMoreThanOnceNoWeakRef(self):
    state = []

    @def_function.function
    def fn(x):
      state.append(variables.Variable(1.0))
      return state[-1] + x

    with self.assertRaises(ValueError):
      fn(1.0)

  def testRange(self):

    @def_function.function
    def f(unused_x):
      return 1.0

    self.assertAllEqual(f(range(5)), 1.0)

  def testCorrectVariableCreation(self):

    state = []

    @def_function.function
    def fn(x):
      if not state:
        state.append(variables.Variable(2.0))
      return state[0] * x

    self.assertAllEqual(fn(constant_op.constant(1.0)), 2.0)
    self.assertAllEqual(fn(constant_op.constant(3.0)), 6.0)

  def testFunctionInitializer(self):

    state = []

    @def_function.function
    def fn(x):
      if not state:
        state.append(variables.Variable(lambda: 2.0))
      return state[0] * x

    self.assertAllEqual(fn(constant_op.constant(1.0)), 2.0)

  def testFunctionMultipleVariableInitializer(self):

    state = []

    @def_function.function
    def fn(x):
      if not state:
        state.append(variables.Variable(lambda: 2.0))
        state.append(variables.Variable(lambda: 5.0))
      return state[0] * x, state[1] * x

    self.assertAllEqual(fn(constant_op.constant(1.0)), [2.0, 5.0])

  def testFunctionInitializationFunction(self):

    state = []

    @def_function.function
    def fn(x):
      if not state:
        state.append(variables.Variable(2.0))
      return state[0] * x

    init_fn = fn.get_initialization_function(constant_op.constant(1.0))
    self.assertLen(state, 1)
    self.assertFalse(
        resource_variable_ops.var_is_initialized_op(state[0].handle))
    init_fn()
    self.assertEqual(state[0].numpy(), 2.0)

  def testVariableInitializerNotConstant(self):

    state = []

    @def_function.function
    def fn(x):
      if not state:
        state.append(variables.Variable(2.0 * x))
      return state[0] * x

    self.assertAllEqual(fn(constant_op.constant(1.0)), 2.0)
    self.assertAllEqual(fn(constant_op.constant(3.0)), 6.0)

  def testLegacyGraphModeVariables(self):
    with ops.Graph().as_default(), self.test_session() as sess:
      state = []

      @def_function.function
      def fn(x):
        if not state:
          state.append(variables.Variable(2.0))
        return state[0] * x

      result = fn(3.0)

      self.evaluate(variables.global_variables_initializer())
      self.assertAllEqual(sess.run(state[0]), 2.0)
      self.assertAllEqual(self.evaluate(result), 6.0)

  def testLegacyGraphModeVariablesNonTrivialInitializer(self):
    with ops.Graph().as_default(), self.test_session() as sess:
      state = []

      @def_function.function
      def fn(x):
        if not state:
          two = constant_op.constant(2.0)
          four = two * two
          two_again = math_ops.sqrt(four)
          state.append(variables.Variable(two_again + four))
        return state[0] * x

      result = fn(3.0)

      self.evaluate(variables.global_variables_initializer())
      self.assertAllEqual(sess.run(state[0]), 6.0)
      self.assertAllEqual(self.evaluate(result), 18.0)

  def testLegacyGraphModeInputDependentInitializerFails(self):
    with ops.Graph().as_default():
      state = []

      @def_function.function
      def fn(x):
        if not state:
          state.append(variables.Variable(2.0 * x))
        return state[0] * x

      with self.assertRaisesRegex(lift_to_graph.UnliftableError,
                                  r'transitively.* mul .* x'):
        fn(constant_op.constant(3.0))

  def testMethod(self):

    class MyModel(object):

      def __init__(self):
        self.var = None

      @def_function.function
      def apply(self, x):
        if self.var is None:
          self.var = variables.Variable(2.0)
        return self.var * x

    m0 = MyModel()
    self.assertAllEqual(m0.apply(3.0), 6.0)
    # Calling twice to exercise that we do not recreate variables.
    m0.var.assign(3.0)
    self.assertAllEqual(m0.apply(3.0), 9.0)

    m1 = MyModel()
    self.assertAllEqual(m1.apply(3.0), 6.0)

  @unittest.expectedFailure
  def testMethodAllowDynamicVariableWithoutGuards(self):

    class Foo:

      def __init__(self):
        self._var = 0

      def __call__(self, val):
        self.compute(val)
        return self._var

      @def_function.function
      def compute(self, val):
        self._var = variables.Variable(val)

    def_function.ALLOW_DYNAMIC_VARIABLE_CREATION = True
    foo = Foo()
    self.assertAllEqual(foo(0.3), 0.3)
    self.assertAllEqual(
        foo(0.9), 0.9, 'https://github.com/tensorflow/tensorflow/issues/27120')

  def testMethodAllowDynamicVariable(self):

    class Foo:

      def __init__(self):
        self._flag_keyed_vars = {}
        self.trace_count = 0

      def __call__(self, var_creation_flag):
        self.compute(var_creation_flag)
        return self._flag_keyed_vars[var_creation_flag]

      @def_function.function
      def compute(self, var_creation_flag):
        self.trace_count += 1
        if var_creation_flag not in self._flag_keyed_vars:
          if var_creation_flag:
            self._flag_keyed_vars[var_creation_flag] = variables.Variable(1.0)
          else:
            self._flag_keyed_vars[var_creation_flag] = variables.Variable(2.0)

    def_function.ALLOW_DYNAMIC_VARIABLE_CREATION = True
    foo = Foo()
    self.assertAllEqual(foo(True), 1.0)
    self.assertEqual(foo.trace_count, 2)
    self.assertAllEqual(foo(True), 1.0)
    self.assertEqual(foo.trace_count, 2)
    self.assertAllEqual(foo(False), 2.0)
    self.assertEqual(foo.trace_count, 3)

  def testMethodNotAllowDynamicVariable(self):

    class Foo:

      def __init__(self):
        self._flag_keyed_vars = {}
        self.trace_count = 0

      def __call__(self, var_creation_flag):
        self.compute(var_creation_flag)
        return self._flag_keyed_vars[var_creation_flag]

      @def_function.function
      def compute(self, var_creation_flag):
        self.trace_count += 1
        if var_creation_flag not in self._flag_keyed_vars:
          if var_creation_flag:
            self._flag_keyed_vars[var_creation_flag] = variables.Variable(1.0)
          else:
            self._flag_keyed_vars[var_creation_flag] = variables.Variable(2.0)

    def_function.ALLOW_DYNAMIC_VARIABLE_CREATION = False
    foo = Foo()
    self.assertAllEqual(foo(True), 1.0)
    self.assertEqual(foo.trace_count, 2)
    self.assertAllEqual(foo(True), 1.0)
    self.assertEqual(foo.trace_count, 2)
    msg = 'singleton tf.Variable.*on the first call'
    with self.assertRaisesRegex(ValueError, msg):
      foo(False)
    self.assertEqual(foo.trace_count, 3)

  def testMethodExtensionType(self):

    class MaskedTensor(extension_type.ExtensionType):
      values: ops.Tensor
      mask: ops.Tensor

      @def_function.function
      def with_default(self, default_value):
        return array_ops.where_v2(self.mask, self.values, default_value)

      @def_function.function
      def sum(self):
        # Use a loop & conditional to test that autograph works correctly.
        result = 0
        for i in range(array_ops.size(self.values)):
          if self.mask[i]:
            result += self.values[i]
        return result

    mt = MaskedTensor([1, 2, 3], [True, False, True])
    self.assertAllEqual(mt.with_default(-1), [1, -1, 3])
    self.assertAllEqual(mt.sum(), 4)

  def test_functools_partial(self):
    self.assertAllClose(
        3.,
        def_function.function(functools.partial(lambda x, y: x + y, 1.))(
            constant_op.constant(2.)))

  def test_functools_partial_new_default(self):
    def f(x=3, y=7):
      return x + y

    func = def_function.function(functools.partial(f, y=6))
    self.assertEqual(func().numpy(), 9)
    self.assertEqual(func(y=8).numpy(), 11)

  def test_functools_partial_keywords(self):
    def f(x, y):
      return x + y

    func = def_function.function(
        functools.partial(f, x=array_ops.zeros([1]), y=array_ops.zeros([1])))
    self.assertAllEqual(func(), [0.0])

  def test_functools_partial_single_positional(self):
    def f(x, y):
      return x + y

    func = def_function.function(
        functools.partial(f, constant_op.constant(1)))
    self.assertAllEqual(func(5), 6)

  def test_complicated_partial_with_defaults(self):

    def identity(*args):
      return args

    def dynamic_unroll(core_fn,
                       input_sequence,
                       initial_state,
                       sequence_length=None,
                       parallel_iterations=1,
                       swap_memory=False):
      del core_fn
      self.assertIs(None, sequence_length)
      self.assertEqual(1, parallel_iterations)
      self.assertTrue(swap_memory)
      return input_sequence, initial_state

    input_sequence = random_ops.random_uniform([1, 1, 1])
    initial_state = random_ops.random_uniform([1, 1])

    func = def_function.function(
        functools.partial(dynamic_unroll, identity, swap_memory=True))
    func(input_sequence, initial_state)

  def test_unspecified_default_argument(self):
    wrapped = def_function.function(
        lambda x, y=2: x + y,
        input_signature=[tensor_spec.TensorSpec((), dtypes.int32)])
    self.assertEqual(3, wrapped(constant_op.constant(1)).numpy())

  def test_concrete_function_from_signature(self):

    @def_function.function(
        input_signature=[tensor_spec.TensorSpec(None, dtypes.float32)])
    def compute(x):
      return 2. * x

    concrete = compute.get_concrete_function()
    self.assertAllClose(1., concrete(constant_op.constant(0.5)))
    concrete = compute.get_concrete_function(
        tensor_spec.TensorSpec(None, dtypes.float32))
    self.assertAllClose(4., concrete(constant_op.constant(2.)))
    signature_args, _ = concrete.structured_input_signature
    self.assertEqual(signature_args,
                     (tensor_spec.TensorSpec(
                         None, dtypes.float32, name='x'),))

  def testInputSignatureMissingTensorSpecsMethod(self):

    class MyModule(module.Module):

      def f1(self, arg1, arg2, arg3):
        pass

      def f2(self, arg1, arg2, arg3, **kwargs):
        pass

      def f3(self, arg1, arg2, arg3, arg4=4, **kwargs):
        pass

      def f4(self, arg1, arg2, arg3, *args):
        pass

      def f5(self, arg1, arg2, arg3, *args, **kwargs):
        pass

      def f6(self, arg1, arg4=4, **kwargs):
        return arg1 + arg4

    m = MyModule()
    tf_func_dec = def_function.function(
        input_signature=(tensor_spec.TensorSpec([], dtypes.int32),))
    error_msg = 'TensorSpecs are still required.*arg2.*arg3'
    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(m.f1)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(m.f2)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(m.f3)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(m.f4)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(m.f5)(1, 2, 3)

    self.assertEqual(tf_func_dec(m.f6)(1).numpy(), 5)

  def testInputSignatureMissingTensorSpecsFunction(self):
    tf_func_dec = def_function.function(
        input_signature=(tensor_spec.TensorSpec([], dtypes.int32),))
    error_msg = 'TensorSpecs are still required.*arg2.*arg3'
    # pylint: disable=unused-argument
    def f1(arg1, arg2, arg3):
      pass

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(f1)(1, 2, 3)

    def f2(arg1, arg2, arg3, **kwargs):
      pass

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(f2)(1, 2, 3)

    def f3(arg1, arg2, arg3, arg4=4, **kwargs):
      pass

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(f3)(1, 2, 3)

    def f4(arg1, arg2, arg3, *args):
      pass

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(f4)(1, 2, 3)

    def f5(arg1, arg2, arg3, *args, **kwargs):
      pass

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(f5)(1, 2, 3)
    # pyline: enable=unused-argument

    def f6(arg1, arg4=4, **kwargs):
      return arg1 + arg4
    self.assertEqual(tf_func_dec(f6)(1).numpy(), 5)

  def testInputSignatureMissingTensorSpecsLambdaFunction(self):
    tf_func_dec = def_function.function(
        input_signature=(tensor_spec.TensorSpec([], dtypes.int32),))
    error_msg = 'TensorSpecs are still required.*arg2.*arg3'
    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(lambda ar1, arg2, arg3: None)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(lambda arg1, arg2, arg3, **kwargs: None)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(lambda arg1, arg2, arg3, arg4=4, **kwargs: None)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(lambda arg1, arg2, arg3, *args: None)(1, 2, 3)

    with self.assertRaisesRegex(TypeError, error_msg):
      tf_func_dec(lambda arg1, arg2, arg3, *args, **kwargs: None)(1, 2, 3)

    self.assertEqual(
        tf_func_dec(lambda arg1, arg4=4, **kwargs: arg1 + arg4)(1).numpy(), 5)

  @parameterized.named_parameters(('_method', 'method'),
                                  ('_function', 'function'),
                                  ('_lambda_function', 'lambda_function'))
  def testInputSignaturePartialFuncMissingTensorSpecs(self, func_type):
    if func_type == 'method':
      class MyModule(module.Module):

        def f(self, arg1, arg2, arg3, arg4=4):
          return arg1 + arg2 + arg3 + arg4
      f = MyModule().f
    elif func_type == 'function':
      def f(arg1, arg2, arg3, arg4=4):
        return arg1 + arg2 + arg3 + arg4
    else:  # lambda_function
      f = lambda arg1, arg2, arg3, arg4=4: arg1 + arg2 + arg3 + arg4

    tf_func_dec = def_function.function(
        input_signature=(tensor_spec.TensorSpec([], dtypes.int32),))
    with self.assertRaisesRegex(TypeError,
                                'TensorSpecs are still required.*arg3'):
      tf_func_dec(functools.partial(f, 1))(2, 3)

    with self.assertRaisesRegex(TypeError,
                                'TensorSpecs are still required.*arg2.*arg3'):
      tf_func_dec(functools.partial(f, arg4=5))(1, 2, 3)

    with self.assertRaisesRegex(TypeError,
                                'TensorSpecs are still required.*arg3'):
      tf_func_dec(functools.partial(f, 1, arg4=5))(2, 3)

    self.assertAllEqual(tf_func_dec(functools.partial(f, 1, 2, arg4=5))(3),
                        array_ops.constant(11))

  @test_util.run_in_graph_and_eager_modes
  def test_variable_naming(self):
    class HasVars(module.Module):

      def __init__(self):
        self.x = None
        self.y = None
        self.z = None

      @def_function.function
      def make_x(self):
        if self.x is None:
          self.x = variables.Variable(1., name='v')

      def make_y(self):
        if self.y is None:
          self.y = variables.Variable(1., name='v')

      def make_z(self):
        if self.z is None:
          with ops.name_scope('z_scope', skip_on_eager=False):
            self.z = variables.Variable(1., name='z')

    root = HasVars()
    root.make_x()
    root.make_y()
    root.make_z()
    self.assertEqual('v:0', root.x.name)
    self.assertEqual('z_scope/z:0', root.z.name)

  def test_concrete_function_keyword_arguments(self):
    @def_function.function
    def f(x):
      return x

    conc = f.get_concrete_function(
        tensor_spec.TensorSpec(None, dtypes.float32, 'y'))
    conc(y=constant_op.constant(3.0))
    signature_args, _ = conc.structured_input_signature
    self.assertEqual('y', signature_args[0].name)

    conc = f.get_concrete_function(tensor_spec.TensorSpec(None, dtypes.float32))
    conc(x=constant_op.constant(3.0))
    signature_args, _ = conc.structured_input_signature
    self.assertEqual('x', signature_args[0].name)

    @def_function.function
    def g(x):
      return x[0]

    conc = g.get_concrete_function(
        [tensor_spec.TensorSpec(None, dtypes.float32, 'z'), 2])
    conc(z=constant_op.constant(3.0))
    signature_args, _ = conc.structured_input_signature
    self.assertEqual('z', signature_args[0][0].name)

  def testRuntimeErrorNotSticky(self):

    @def_function.function
    def fail(i):
      control_flow_ops.Assert(math_ops.equal(i, 0), ['ick'])

    fail(constant_op.constant(0))  # OK
    with self.assertRaises(errors.InvalidArgumentError):
      fail(constant_op.constant(1))  # InvalidArgument: "ick"
    fail(constant_op.constant(0))  # OK

  def testUnderscoreName(self):

    @def_function.function
    def f(_):
      return _ + _

    self.assertAllEqual(2.0, f(constant_op.constant(1.0)))

  def test_serialization_signature_cache(self):

    @def_function.function
    def f(x, y):
      return x, y

    f(constant_op.constant([[3., 4.]]), constant_op.constant([2.]))
    f(constant_op.constant([[3, 4, 5]]), constant_op.constant([2]))

    signatures_args = set()
    concrete_functions = f._list_all_concrete_functions_for_serialization()
    for concrete_function in concrete_functions:
      args, kwargs = concrete_function.structured_input_signature
      signatures_args.add(args)
      self.assertEqual(dict(), kwargs)

    self.assertEqual(
        signatures_args,
        set(((tensor_spec.TensorSpec([1, 2], dtypes.float32, name='x'),
              tensor_spec.TensorSpec([1], dtypes.float32, name='y')),
             (tensor_spec.TensorSpec([1, 3], dtypes.int32, name='x'),
              tensor_spec.TensorSpec([1], dtypes.int32, name='y')))))

  @test_util.assert_no_garbage_created
  def testFunctionReferenceCycles(self):
    fn = def_function.function(lambda x: 2. * x)
    fn(constant_op.constant(4.0))
    weak_fn = weakref.ref(fn)
    del fn
    # Tests that the weak reference we made to the function is now dead, which
    # means the object has been deleted. This should be true as long as the
    # function itself is not involved in a reference cycle.
    self.assertIs(None, weak_fn())

  @test_util.assert_no_garbage_created
  def testMethodReferenceCycles(self):
    has_decorated_method = _HasDecoratedMethod()
    has_decorated_method.f(constant_op.constant(5.))
    weak_fn = weakref.ref(has_decorated_method.f)
    del has_decorated_method
    # Tests that the weak reference we made to the function is now dead, which
    # means the object has been deleted. This should be true as long as the
    # function itself is not involved in a reference cycle.
    self.assertIs(None, weak_fn())

  @test_util.assert_no_new_pyobjects_executing_eagerly
  def testErrorMessageWhenGraphTensorIsPassedToEager(self):

    @def_function.function
    def failing_function():
      a = constant_op.constant(1.)

      with ops.init_scope():
        _ = a + a

    with self.assertRaisesRegex(
        TypeError, re.compile('def_function_test.*out of scope', re.DOTALL)):
      failing_function()

  def testSymbolicTensorIllegalCaptureCallTimeError(self):
    x = None

    @def_function.function
    def f1(a):
      nonlocal x
      x = a
      return a

    @def_function.function
    def f2(b):
      return b + x

    f1(constant_op.constant(1))
    with self.assertRaisesRegex(
        TypeError, re.compile('def_function_test.*out of scope', re.DOTALL)):
      f2(constant_op.constant(2))

  def testSymbolicTensorIllegalCaptureTraceTimeError(self):

    @def_function.function
    def f(inputs):
      num_steps, _ = inputs.shape[:2]
      outputs = []
      for t in math_ops.range(num_steps):
        outputs.append(inputs[t])
      return outputs

    with self.assertRaisesRegex(errors.InaccessibleTensorError, 'out of scope'):
      f(array_ops.zeros(shape=(8, 42, 3)))

  def testNonUniqueNamesGetConcreteFunction(self):
    @def_function.function
    def non_unique_arg_names(x, **kwargs):
      a, b, c = x
      d = kwargs['d']
      return a + b + c + d

    concrete = non_unique_arg_names.get_concrete_function(
        (tensor_spec.TensorSpec(None, dtypes.float32),
         tensor_spec.TensorSpec(None, dtypes.float32),
         tensor_spec.TensorSpec(None, dtypes.float32)),
        d=tensor_spec.TensorSpec(None, dtypes.float32))
    self.assertAllClose(
        10.,
        concrete(x=constant_op.constant(1.),
                 x_1=constant_op.constant(2.),
                 x_2=constant_op.constant(3.),
                 d=constant_op.constant(4.)))
    self.assertAllClose(
        10.,
        concrete(constant_op.constant(1.),
                 constant_op.constant(2.),
                 constant_op.constant(3.),
                 constant_op.constant(4.)))

  def testVariableCreatorScope(self):
    created_variables = []
    captured_variables = []

    @def_function.function
    def f():
      if not created_variables:
        created_variables.append(variables.Variable(1.))
      return created_variables[0] + 1.

    def capture_creator(next_creator, **kwargs):
      created = next_creator(**kwargs)
      captured_variables.append(created)
      return created

    with variable_scope.variable_creator_scope(capture_creator):
      f()
    self.assertEqual(created_variables, captured_variables)

  def testVarAlreadyInitializedNoClobbering(self):
    v_holder = []

    @def_function.function
    def add_var(x):
      if not v_holder:
        v = variables.Variable([1., 2.])
        v_holder.append(v)
        already_initialized = variables.Variable(3.)
        with ops.init_scope():
          already_initialized.assign(10.)
        v_holder.append(already_initialized)
      return v_holder[0] + v_holder[1] + x

    add_var.get_concrete_function(constant_op.constant(2.))
    self.assertAllClose([13., 14.], add_var(constant_op.constant(2.)))

  def testSameVariableTwice(self):
    v = variables.Variable(1.0)

    @def_function.function
    def add(a, b):
      return a + b

    self.assertAllEqual(add(v, v), 2.0)

  def testVariableUpdate(self):
    v1 = variables.Variable(1.0)
    v2 = variables.Variable(2.0)
    v3 = variables.Variable(4, dtype=dtypes.int32)

    trace_count = [0]

    @def_function.function
    def double_variable(x):
      trace_count[0] += 1
      x.assign_add(x.read_value())

    self.assertEqual(trace_count[0], 0)
    double_variable(v1)
    self.assertEqual(trace_count[0], 1)
    self.assertEqual(self.evaluate(v1), 2.0)
    double_variable(v2)
    # No retracing because v2's data type and shape are the same as v1
    self.assertEqual(trace_count[0], 1)
    self.assertEqual(self.evaluate(v2), 4.0)
    double_variable(v3)
    # Retracing because of data type change
    self.assertEqual(trace_count[0], 2)
    self.assertEqual(self.evaluate(v3), 8)

  def testShapeCache(self):
    @def_function.function
    def func(x):
      return 2 * x

    func_a = func.get_concrete_function(
        tensor_spec.TensorSpec([None], dtypes.int32))
    func_b = func.get_concrete_function(
        tensor_spec.TensorSpec([None], dtypes.int32))

    self.assertIs(func_a, func_b)

  def testCacheWithinSaveContext(self):

    @def_function.function
    def func(x):
      return 2 * x

    func_a = func.get_concrete_function(constant_op.constant(2.))
    func_b = func.get_concrete_function(constant_op.constant(2.))

    self.assertIs(func_a, func_b)

    with save_context.save_context(
        save_options.SaveOptions(experimental_variable_policy=save_options
                                 .VariablePolicy.EXPAND_DISTRIBUTED_VARIABLES)):
      func_c = func.get_concrete_function(constant_op.constant(2.))

    with save_context.save_context(
        save_options.SaveOptions(
            experimental_variable_policy=save_options.VariablePolicy.NONE)):
      func_d = func.get_concrete_function(constant_op.constant(2.))

    self.assertIsNot(func_a, func_c)
    self.assertIsNot(func_a, func_d)

  def testInitializationInNestedCall(self):
    v_holder = []

    @def_function.function
    def add_var(x):
      if not v_holder:
        v = variables.Variable([1., 2.])
        v_holder.append(v)
        already_initialized = variables.Variable(3.)
        with ops.init_scope():
          already_initialized.assign(10.)
        v_holder.append(already_initialized)
      return v_holder[0] + v_holder[1] + x

    @def_function.function
    def wrapper(x):
      return add_var(x)

    self.assertAllClose([13., 14.], wrapper(constant_op.constant(2.)))
    v_holder[1].assign(11.)
    self.assertAllClose([14., 15.], wrapper(constant_op.constant(2.)))

  @test_util.run_gpu_only
  def testDeviceAnnotationRespected(self):
    a = []

    @def_function.function()
    def create_variable():
      with ops.init_scope():
        initial_value = random_ops.random_uniform(
            (2, 2), maxval=1000000, dtype=dtypes.int64)

      if not a:
        with ops.device('CPU:0'):
          a.append(resource_variable_ops.ResourceVariable(initial_value))

      return a[0].read_value()

    create_variable()
    self.assertRegex(a[0].device, 'CPU')

  @test_util.run_gpu_only
  def testDeviceAnnotationForInitializerRespected(self):
    a = []
    initial_value = []

    def initial_value_fn():
      initial_value.append(random_ops.random_uniform((2, 3)))
      return initial_value[0]

    @def_function.function()
    def create_variable():
      with ops.init_scope():
        if not a:
          a.append(variables.Variable(initial_value_fn))

    with ops.device('CPU:0'):
      create_variable()
    self.assertRegex(a[0].device, 'CPU')
    self.assertRegex(initial_value[0].device, 'CPU')

  def testDecorate(self):
    func = def_function.function(lambda: 1)
    def decorator(f):
      return lambda: 1 + f()

    func._decorate(decorator)
    self.assertEqual(func().numpy(), 2)

  @parameterized.parameters(*itertools.product(
      (None, (tensor_spec.TensorSpec([]),)),  # input_signature
      (True, False),                          # autograph
      (None, converter.Feature.ALL),          # autograph_options
      (None, 'foo.bar'),                      # implements
      (None, True, False),                    # relax_shapes
      (True, False),                          # compile
      (True, False),                          # override_function
  ))

  def testClone(self, input_signature, autograph, autograph_options, implements,
                relax_shapes, compile_, override_function):
    original_py_function = lambda x: x

    compile_ = False
    func = def_function.function(
        func=original_py_function,
        input_signature=input_signature,
        autograph=autograph,
        experimental_implements=implements,
        experimental_autograph_options=autograph_options,
        reduce_retracing=relax_shapes,
        jit_compile=compile_)

    if override_function:
      cloned_py_function = lambda x: x + 1
    else:
      cloned_py_function = original_py_function

    cloned = func._clone(python_function=cloned_py_function)

    self.assertEqual(cloned_py_function, cloned._python_function)
    self.assertEqual(func._name, cloned._name)
    self.assertEqual(input_signature, cloned._input_signature)
    self.assertEqual(autograph, cloned._autograph)
    self.assertEqual(implements, cloned._implements)
    self.assertEqual(autograph_options, cloned._experimental_autograph_options)
    self.assertEqual(relax_shapes, cloned._reduce_retracing)
    self.assertEqual(compile_, cloned._jit_compile)

    # This test does not run with XLA JIT support linked in so we can only check
    # the output of the function if compile is disabled.
    if not compile_:
      x = array_ops.zeros([])
      self.assertEqual(self.evaluate(cloned(x)),
                       self.evaluate(cloned_py_function(x)))

  def testLiftPlaceholderInitializedVariable(self):
    with ops.Graph().as_default():
      var_list = []

      @def_function.function
      def use_variable():
        if not var_list:
          initial_value = array_ops.placeholder(shape=[], dtype=dtypes.float32)
          v = variables.Variable(initial_value)
          var_list.append(v)
        return var_list[0] + 1.

      var_plus_one = use_variable()
      with self.session() as session:
        init_op = var_list[0].initializer
        session.run(init_op, feed_dict={init_op.inputs[1]: 2.})
        self.assertEqual(3., session.run(var_plus_one))

  def testDecorate_rejectedAfterTrace(self):
    func = def_function.function(lambda: 1)
    self.assertEqual(func().numpy(), 1)
    msg = 'Functions cannot be decorated after they have been traced.'
    with self.assertRaisesRegex(ValueError, msg):
      func._decorate(lambda f: f)

  def testGetConcreteFunctionGraphLifetime(self):

    @def_function.function
    def func():
      pass

    graph = func.get_concrete_function().graph
    del func

    # If the graph is deleted, then an exception is raised on reading `captures`
    self.assertEmpty(graph.captures)

  @parameterized.parameters(*itertools.product(
      (None, (tensor_spec.TensorSpec([]),)),  # input_signature
      (True, False),  # autograph
      (None, converter.Feature.ALL),  # autograph_options
      (None, 'foo.bar'),  # implements
      (None, True, False),  # relax_shapes
  ))

  def test_pickle(self, input_signature, autograph, autograph_options,
                  implements, relax_shapes):
    """@function objects can be pickled and unpickled."""
    original_py_function = undecorated_function

    func = def_function.function(
        func=original_py_function,
        input_signature=input_signature,
        autograph=autograph,
        experimental_implements=implements,
        experimental_autograph_options=autograph_options,
        reduce_retracing=relax_shapes,
    )

    cloned = pickle.loads(pickle.dumps(func))

    self.assertEqual(func._name, cloned._name)
    self.assertEqual(input_signature, cloned._input_signature)
    self.assertEqual(autograph, cloned._autograph)
    self.assertEqual(implements, cloned._implements)
    self.assertEqual(autograph_options, cloned._experimental_autograph_options)
    self.assertEqual(relax_shapes, cloned._reduce_retracing)

    x = array_ops.ones([])
    self.assertEqual(self.evaluate(cloned(x)), self.evaluate(func(x)))

  def test_frequent_retracing_warning(self):
    if sys.version_info[0] < 3:
      self.skipTest('self.assertLogs() call is not available in Python 2.')

    @def_function.function
    def f(x):
      return x

    with self.assertLogs(level='WARN') as logs:
      f(1)
      f(2)
      f(3)
      f(4)
      self.assertEmpty(logs.output)
      f(5)

    self.assertLen(logs.output, 1)
    self.assertIn('Tracing is expensive', logs.output[0])

  def test_frequent_retracing_warning_lambda(self):
    if sys.version_info[0] < 3:
      self.skipTest('self.assertLogs() call is not available in Python 2.')

    f = def_function.function(lambda x: x)

    with self.assertLogs(level='WARN') as logs:
      f(1)
      f(2)
      f(3)
      f(4)
      f(5)

    self.assertLen(logs.output, 1)
    self.assertIn('Tracing is expensive', logs.output[0])

  def test_frequent_retracing_warning_method(self):
    if sys.version_info[0] < 3:
      self.skipTest('self.assertLogs() call is not available in Python 2.')

    class Foo(object):

      @def_function.function
      def f(self, x):
        return x

    f = Foo().f

    with self.assertLogs(level='WARN') as logs:
      f(1)
      f(2)
      f(3)
      f(4)
      f(5)

    self.assertLen(logs.output, 1)
    self.assertIn('Tracing is expensive', logs.output[0])

  def test_frequent_retracing_warning_two_independent_tf_functions(self):
    if sys.version_info[0] < 3:
      self.skipTest('self.assertLogs() call is not available in Python 2.')

    @def_function.function
    def f(x):
      return x

    @def_function.function
    def g(x):
      return x

    with self.assertLogs(level='WARN') as logs:
      f(1)
      f(2)
      f(3)
      f(4)
      g(1)
      g(2)
      g(3)
      g(4)
      g(5)

    self.assertLen(logs.output, 1)
    self.assertIn('Tracing is expensive', logs.output[0])

  def test_frequent_retracing_warning_nested(self):
    if sys.version_info[0] < 3:
      self.skipTest('self.assertLogs() call is not available in Python 2.')

    @def_function.function
    def inner(x):
      return x + 1

    @def_function.function
    def outer1(x):
      return inner(x) * 2

    @def_function.function
    def outer2(x):
      return inner(x) * 3

    with self.assertLogs(level='WARN') as logs:
      inner(1)
      inner(2)
      inner(3)
      inner(4)

      outer1(5)
      outer1(6)
      outer1(7)
      outer1(8)

      outer2(9)
      outer2(10)
      outer2(11)
      outer2(12)

      self.assertEmpty(logs.output)

      outer2(13)

      self.assertLen(logs.output, 1)
      self.assertIn('Tracing is expensive', logs.output[0])

  def test_frequent_retracing_warning_on_reinstantiation(self):
    if sys.version_info[0] < 3:
      self.skipTest('self.assertLogs() call is not available in Python 2.')

    with self.assertLogs(level='WARN') as logs:
      for i in range(5):

        @def_function.function
        def f(x):
          return x

        f(i)

        if i < 4:
          self.assertEmpty(logs.output)

    self.assertLen(logs.output, 1)
    self.assertIn('Tracing is expensive', logs.output[0])

  def test_restored_function_retracing_warning(self):

    class Foo(Checkpoint):

      @def_function.function
      def __call__(self, x):
        return x

    f_flexible = Foo()
    _ = f_flexible.__call__.get_concrete_function(
        tensor_spec.TensorSpec(shape=[None], dtype=dtypes.int32))
    tmp_dir = self.create_tempdir()
    save(f_flexible, tmp_dir.full_path)
    restored_f_flexible = load(tmp_dir.full_path)

    f_fixed_shape = Foo()

    with self.assertLogs(level='WARN') as logs:
      restored_f_flexible(constant_op.constant([1], dtypes.int32))
      restored_f_flexible(constant_op.constant([1, 2], dtypes.int32))
      restored_f_flexible(constant_op.constant([1, 2, 3], dtypes.int32))
      restored_f_flexible(constant_op.constant([1, 2, 3, 4], dtypes.int32))
      restored_f_flexible(constant_op.constant([1, 2, 3, 4, 5], dtypes.int32))
      self.assertEmpty(logs.output)

      f_fixed_shape(constant_op.constant([1], dtypes.int32))
      f_fixed_shape(constant_op.constant([1, 2], dtypes.int32))
      f_fixed_shape(constant_op.constant([1, 2, 3], dtypes.int32))
      f_fixed_shape(constant_op.constant([1, 2, 3, 4], dtypes.int32))
      f_fixed_shape(constant_op.constant([1, 2, 3, 4, 5], dtypes.int32))
      self.assertLen(logs.output, 1)
      self.assertIn('Tracing is expensive', logs.output[0])

  def test_retracing_warning_limits(self):

    @def_function.function
    def my_func(x):
      return x

    with self.assertLogs(level='WARN') as logs:
      for i in range(10):
        my_func(i)

      self.assertLen(logs.output, 2)

  def test_experimental_get_tracing_count_function(self):

    @def_function.function
    def double(a):
      return a + a

    double(constant_op.constant(1))
    double(constant_op.constant(2))
    self.assertAllEqual(double.experimental_get_tracing_count(), 1)
    double(constant_op.constant('a'))
    self.assertAllEqual(double.experimental_get_tracing_count(), 2)

  def test_experimental_get_tracing_count_method(self):

    class TestClass():

      @def_function.function
      def testDouble(self, a):
        return a + a

    obj1 = TestClass()
    obj1.testDouble(constant_op.constant(1))
    obj1.testDouble(constant_op.constant(2))
    obj1.testDouble(constant_op.constant(1.1))
    self.assertAllEqual(obj1.testDouble.experimental_get_tracing_count(), 2)
    obj2 = TestClass()
    obj2.testDouble(constant_op.constant(1))
    obj2.testDouble(constant_op.constant(1.1))
    obj2.testDouble(constant_op.constant('a'))
    self.assertAllEqual(obj2.testDouble.experimental_get_tracing_count(), 3)
    self.assertAllEqual(obj1.testDouble.experimental_get_tracing_count(), 2)

  def test_recursive_tf_function(self):

    @def_function.function
    def recursive_fn(n):
      if n > 0:
        return recursive_fn(n - 1)
      return 1

    self.assertEqual(recursive_fn(5).numpy(), 1)

  def test_recursive_tf_function_with_gradients(self):

    @def_function.function
    def recursive_fn(n, x):
      if n > 0:
        return n * recursive_fn(n - 1, x)
      else:
        return x

    x = variables.Variable(1.0)
    with backprop.GradientTape() as tape:
      g = recursive_fn(5, x)

    dg_dx = tape.gradient(g, x)
    self.assertEqual(dg_dx.numpy(), 120)

  def test_recursive_python_function(self):

    def recursive_py_fn(n):
      if n > 0:
        return recursive_py_fn(n - 1)
      return 1

    @def_function.function
    def recursive_fn(n):
      return recursive_py_fn(n)

    self.assertEqual(recursive_fn(5).numpy(), 1)

  def test_recursive_python_function_with_gradients(self):

    def recursive_py_fn(n, x):
      if n > 0:
        return n * recursive_py_fn(n - 1, x)
      return x

    @def_function.function
    def recursive_fn(n, x):
      return recursive_py_fn(n, x)

    x = variables.Variable(1.0)
    with backprop.GradientTape() as tape:
      g = recursive_fn(5, x)

    dg_dx = tape.gradient(g, x)
    self.assertEqual(dg_dx.numpy(), 120)

  def test_recursive_tf_function_call_each_other(self):

    @def_function.function
    def recursive_fn1(n):
      if n <= 1:
        return 1
      return recursive_fn2(n - 1)

    @def_function.function
    def recursive_fn2(n):
      if n <= 1:
        return 2
      return recursive_fn1(n - 1)

    self.assertEqual(recursive_fn1(5).numpy(), 1)
    self.assertEqual(recursive_fn1(6).numpy(), 2)
    self.assertEqual(recursive_fn2(5).numpy(), 2)
    self.assertEqual(recursive_fn2(6).numpy(), 1)

  def test_recursive_tf_function_call_each_other_with_gradients(self):

    @def_function.function
    def recursive_fn1(n, x):
      if n <= 1:
        return x
      return n * recursive_fn2(n - 1, x)

    @def_function.function
    def recursive_fn2(n, x):
      if n <= 1:
        return 2 * x
      return n * recursive_fn1(n - 1, x)

    x = variables.Variable(1.0)
    with backprop.GradientTape() as tape:
      g1 = recursive_fn1(5, x)

    dg1_dx = tape.gradient(g1, x)
    self.assertEqual(dg1_dx.numpy(), 120)

    with backprop.GradientTape() as tape:
      g2 = recursive_fn2(5, x)

    dg2_dx = tape.gradient(g2, x)
    self.assertEqual(dg2_dx.numpy(), 240)

  def test_recursive_tf_function_with_cond(self):
    @def_function.function(autograph=False)
    def recursive_fn(n):
      return cond_v2.cond_v2(n > 0, recursive_fn(n - 1), 1)

    with self.assertRaises(RecursionError):
      recursive_fn(constant_op.constant(5))


if __name__ == '__main__':
  ops.enable_eager_execution()
  test.main()
