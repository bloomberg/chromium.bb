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
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import tempfile

from absl.testing import parameterized

import tensorflow as tf


def cycle(obj, cycles, signatures=None):
  to_save = obj
  # TODO(vbardiovsky): It would be nice if exported protos reached a fixed
  # point w.r.t. saving/restoring, ideally after 2nd saving.
  for _ in range(cycles):
    path = tempfile.mkdtemp(prefix=tf.compat.v1.test.get_temp_dir())
    # If available, we'll run the save and restore preferring the GPU. This
    # just makes sure we aren't throwing errors and have enough
    # device("CPU") blocks to satisfy the placer.
    device = "/device:GPU:0" if tf.test.is_gpu_available() else "/device:CPU:0"
    with tf.device(device):
      tf.saved_model.save(to_save, path, signatures)
      loaded = tf.saved_model.load(path)
    to_save = loaded
  return loaded


class _ModelWithOptimizer(tf.train.Checkpoint):

  def __init__(self):
    self.dense = tf.keras.layers.Dense(1)
    self.optimizer = tf.keras.optimizers.Adam(0.01)

  @tf.function(
      input_signature=(tf.TensorSpec([None, 2], tf.float32),
                       tf.TensorSpec([None], tf.float32)))
  def call(self, x, y):
    with tf.GradientTape() as tape:
      loss = tf.math.reduce_mean((self.dense(x) - y) ** 2.)
    trainable_variables = self.dense.trainable_variables
    gradients = tape.gradient(loss, trainable_variables)
    self.optimizer.apply_gradients(zip(gradients, trainable_variables))
    return {"loss": loss}


def _import_and_infer(save_dir, inputs, signature_key="serving_default"):
  """Import a SavedModel into a TF 1.x-style graph and run `signature_key`."""
  graph = tf.Graph()
  with graph.as_default(), tf.compat.v1.Session() as session:
    model = tf.compat.v1.saved_model.load(session, ["serve"], save_dir)
    return _run_signature(session, model, inputs, signature_key)


def _run_signature(session, meta_graph_def, inputs, signature_key):
  signature = meta_graph_def.signature_def[signature_key]
  assert set(inputs.keys()) == set(signature.inputs.keys())
  feed_dict = {}
  for arg_name in inputs.keys():
    input_tensor = session.graph.get_tensor_by_name(
        signature.inputs[arg_name].name)
    feed_dict[input_tensor] = inputs[arg_name]
  output_dict = {}
  for output_name, output_tensor_info in signature.outputs.items():
    output_dict[output_name] = session.graph.get_tensor_by_name(
        output_tensor_info.name)
  return session.run(output_dict, feed_dict=feed_dict)


class SaveTest(tf.test.TestCase):

  def test_unbuilt_model_does_not_prevent_saving(self):
    root = tf.train.Checkpoint(
        model=tf.keras.Sequential([tf.keras.layers.Dense(2)]))
    tf.saved_model.save(root, os.path.join(self.get_temp_dir(), "saved_model"))

  def test_optimizer(self):
    x = tf.constant([[3., 4.]])
    y = tf.constant([2.])
    model = _ModelWithOptimizer()
    first_loss = model.call(x, y)
    save_dir = os.path.join(self.get_temp_dir(), "saved_model")
    tf.saved_model.save(model, save_dir, model.call)
    second_loss = model.call(x, y)
    self.assertNotEqual(first_loss, second_loss)
    self.assertAllClose(
        second_loss,
        _import_and_infer(save_dir, {"x": [[3., 4.]], "y": [2.]}))

  def test_single_method_default_signature(self):
    model = _ModelWithOptimizer()
    x = tf.constant([[3., 4.]])
    y = tf.constant([2.])
    model.call(x, y)
    save_dir = os.path.join(self.get_temp_dir(), "saved_model")
    tf.saved_model.save(model, save_dir)
    self.assertIn("loss",
                  _import_and_infer(save_dir,
                                    {"x": [[3., 4.]], "y": [2.]}))


@parameterized.named_parameters(
    dict(testcase_name="ReloadOnce", cycles=1),
    dict(testcase_name="ReloadTwice", cycles=2),
    dict(testcase_name="ReloadThrice", cycles=3))
class LoadTest(tf.test.TestCase, parameterized.TestCase):

  def test_optimizer(self, cycles):

    class _HasOptimizer(tf.Module):

      def __init__(self):
        super(_HasOptimizer, self).__init__()
        self.layer = tf.keras.layers.Dense(1)
        self.optimizer = tf.keras.optimizers.Adam(0.01)

      @tf.function
      def __call__(self, x):
        return self.layer(x)

      @tf.function
      def train(self, x, y):
        with tf.GradientTape() as tape:
          predicted = self(x)
          loss = tf.math.reduce_sum(tf.math.abs(y - predicted))
        train_vars = self.layer.trainable_variables
        grads = tape.gradient(loss, train_vars)
        self.optimizer.apply_gradients(zip(grads, train_vars))

    root = _HasOptimizer()
    train_input = dict(x=tf.constant([[1.]]),
                       y=tf.constant([[2.]]))
    root.train(**train_input)
    imported = cycle(root, cycles)
    self.assertAllClose(root.optimizer.learning_rate.numpy(),
                        imported.optimizer.learning_rate.numpy())
    self.assertAllClose(root(tf.constant([[-0.5]])),
                        imported(tf.constant([[-0.5]])))
    root.train(**train_input)
    imported.train(**train_input)
    self.assertAllClose(root(tf.constant([[-0.5]])),
                        imported(tf.constant([[-0.5]])))

  def test_model_with_custom_function_attached(self, cycles):
    root = tf.train.Checkpoint(
        model=tf.keras.Sequential([tf.keras.layers.Dense(2)]))

    @tf.function
    def _use_sequential(x):
      return root.model.call(x)

    root.model.traced_call = _use_sequential

    original = root.model.traced_call(tf.zeros([1, 1])).numpy()
    root = cycle(root, cycles)
    self.assertAllEqual(
        original,
        root.model.traced_call(tf.zeros([1, 1])).numpy())


@parameterized.named_parameters(
    dict(testcase_name="ReloadOnce", cycles=1),
    dict(testcase_name="ReloadTwice", cycles=2),
    dict(testcase_name="ReloadThrice", cycles=3))
class KerasLoadTest(tf.test.TestCase, parameterized.TestCase):

  def test_dense_features_layer(self, cycles):
    columns = [
        tf.feature_column.numeric_column("x"),
        tf.feature_column.numeric_column("y")
    ]
    layer = tf.keras.layers.DenseFeatures(columns)
    model = tf.keras.Sequential([layer])
    model_input = {"x": tf.constant([[1.]]),
                   "y": tf.constant([[2.]])}
    self.assertAllClose([[1., 2.]], model.predict(model_input, steps=1))
    loaded = cycle(model, cycles)
    output, = loaded._default_save_signature(model_input).values()
    self.assertAllClose([[1., 2.]], output)
    signature_output, = loaded.signatures["serving_default"](
        **model_input).values()
    self.assertAllClose([[1., 2.]], signature_output)

  def test_dense_features_layer_fit(self, cycles):
    columns = [tf.feature_column.numeric_column("x")]
    model = tf.keras.Sequential(
        [tf.keras.layers.DenseFeatures(columns),
         tf.keras.layers.Dense(1)])
    model_input = {"x": tf.constant([[1.]])}
    model.compile(optimizer="adam", loss="mse", run_eagerly=True)
    model.fit(model_input, tf.constant([[3.]]))
    loaded = cycle(model, cycles)
    loaded._default_save_signature(model_input)
    loaded.signatures["serving_default"](**model_input)

  def test_multi_output_layer(self, cycles):

    inp = tf.keras.Input(name="inp", shape=(None,), dtype=tf.float32)

    class _MultiOutput(tf.keras.layers.Layer):

      def call(self, x):
        return x + 1., x + 2.

    out = _MultiOutput(name="out")(inp)  # pylint: disable=not-callable
    model = tf.keras.Model(inp, out)
    loaded = cycle(model, cycles)
    self.assertAllClose(
        dict(out=2., out_1=3.),
        loaded.signatures["serving_default"](tf.constant(1.)))

  def test_functional_model_with_conv(self, cycles):
    x = tf.keras.Input(name="x", shape=(None, None, 3), dtype=tf.float32)
    conved = tf.keras.layers.Conv2D(
        filters=3, kernel_size=3, dilation_rate=2)(x)
    model = tf.keras.Model([x], conved)
    model_input = tf.ones((1, 10, 10, 3))
    initial_output = model.predict([model_input])
    model = cycle(model, cycles)
    self.assertAllClose(
        [initial_output],
        list(model.signatures["serving_default"](model_input).values()))


if __name__ == "__main__":
  tf.test.main()
