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
"""Converts a model's graph def into a tflite model with MLIR-based conversion."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import tempfile

import numpy as np
import tensorflow.compat.v1 as tf
from tensorflow.lite.python import test_util as tflite_test_util
from tensorflow.lite.testing import zip_test_utils
from tensorflow.python.platform import resource_loader


def mlir_convert(options, graph_def, input_tensors, output_tensors, **kwargs):
  """Convert a model's graph def into a tflite model with MLIR-based conversion.

  Args:
    options: A lite.testing.generate_examples_lib.Options instance.
    graph_def: A GraphDef object.
    input_tensors: List of input tensor tuples `(name, shape, type)`.
    output_tensors: List of output tensors (names).
    **kwargs: Extra parameters.

  Returns:
    output tflite model, log_txt from conversion
    or None, log_txt if it did not convert properly.
  """
  test_params = kwargs.get("test_params", {})
  # TODO(b/146025965): Rename ExtraTocoOptions to ExtraConvertOptions or
  #                    something else.
  extra_toco_options = kwargs.get("extra_toco_options",
                                  zip_test_utils.ExtraTocoOptions())
  input_arrays = [x[0] for x in input_tensors]
  input_shapes = zip_test_utils.get_input_shapes_map(input_tensors)

  tflite_model = None
  log = ""

  with tempfile.NamedTemporaryFile() as graphdef_file:
    graphdef_file.write(graph_def.SerializeToString())
    graphdef_file.flush()
    converter = tf.lite.TFLiteConverter.from_frozen_graph(
        graphdef_file.name, input_arrays, output_tensors, input_shapes)
    converter.allow_custom_ops = extra_toco_options.allow_custom_ops
    converter.experimental_new_quantizer = options.mlir_quantizer

    if options.run_with_flex:
      converter.supported_ops = set([
          tf.lite.OpsSet.TFLITE_BUILTINS, tf.lite.OpsSet.SELECT_TF_OPS])

    if test_params.get("dynamic_range_quantize", False):
      converter.optimizations = [tf.lite.Optimize.DEFAULT]

    if test_params.get("fully_quantize", False):
      converter.optimizations = [tf.lite.Optimize.DEFAULT]

      # Read the input range for the representative dataset from parameters.
      min_value, max_value = test_params.get("input_range", (-1, 1))

      def representative_dataset(input_tensors):
        calibration_inputs = []
        for _, shape, _ in input_tensors:
          if shape:
            dims = [1 if dim.value is None else dim.value for dim in shape.dims]
            calibration_inputs.append(
                np.random.uniform(min_value, max_value,
                                  tuple(dims)).astype(np.float32))
        return calibration_inputs

      def representative_dataset_gen():
        for _ in range(100):
          yield representative_dataset(input_tensors)

      if test_params.get("quant_16x8", False):
        converter.target_spec.supported_ops = [
            tf.lite.OpsSet.\
            EXPERIMENTAL_TFLITE_BUILTINS_ACTIVATIONS_INT16_WEIGHTS_INT8
        ]
      else:
        converter.target_spec.supported_ops = [
            tf.lite.OpsSet.TFLITE_BUILTINS_INT8
        ]

      converter.representative_dataset = representative_dataset_gen
      if extra_toco_options.inference_input_type:
        converter.inference_input_type = (
            extra_toco_options.inference_input_type)

      if extra_toco_options.inference_output_type:
        converter.inference_output_type = (
            extra_toco_options.inference_output_type)

    try:
      tflite_model = converter.convert()
      if options.expected_ops_in_converted_model:
        ops_list = tflite_test_util.get_ops_list(tflite_model)
        for expected_op in options.expected_ops_in_converted_model:
          if expected_op not in ops_list:
            # Force the test to fail.
            tflite_model = None
            raise ValueError(
                "{} op not found in the converted model".format(expected_op))
    except Exception as e:  # pylint: disable=broad-except
      log = str(e)

  return tflite_model, log


def mlir_convert_file(graph_def_filename,
                      input_tensors,
                      output_tensors,
                      quantization_params=None,
                      additional_flags=""):
  """Convert a graphdef file into a tflite model with MLIR-based conversion.

  NOTE: this currently shells out to the MLIR binary binary, but we would like
  convert to Python API tooling in the future.

  Args:
    graph_def_filename: A GraphDef file.
    input_tensors: List of input tensor tuples `(name, shape, type)`. name
      should be a string. shape should be a tuple of integers. type should be a
      string, for example 'DT_FLOAT'
    output_tensors: List of output tensors (names).
    quantization_params: parameters `(inference_type, min_values, max_values)`
      to quantize the model.
    additional_flags: A string of additional command line flags to be passed
      to MLIR converter.

  Returns:
    output tflite model, log_txt from conversion
    or None, log_txt if it did not convert properly.
  """
  bin_path = resource_loader.get_path_to_datafile(
      "../../../../compiler/mlir/lite/tf_tfl_translate")

  with tempfile.NamedTemporaryFile() as output_file, \
       tempfile.NamedTemporaryFile("w+") as stdout_file:
    input_shapes = []
    for input_tensor in input_tensors:
      shape = input_tensor[1]
      input_shapes.append(",".join([str(dim) for dim in shape]))
    input_shapes_str = ":".join(input_shapes)

    input_types = ",".join([x[2] for x in input_tensors])

    quant_flags = ""
    if quantization_params is not None:
      min_vals = ",".join([str(val) for val in quantization_params[1]])
      max_vals = ",".join([str(val) for val in quantization_params[2]])
      quant_flags = ("-tf-inference-type=" + quantization_params[0] +
                     " -tf-input-min-values='" + min_vals +
                     "' -tf-input-max-values='" + max_vals + "' " +
                     "-emit-quant-adaptor-ops ")
    cmd = ("%s -tf-input-arrays=%s -tf-input-data-types=%s -tf-input-shapes=%s "
           "-tf-output-arrays=%s " + quant_flags + additional_flags +
           "%s -o %s")
    cmd = cmd % (
        bin_path,
        ",".join([x[0] for x in input_tensors]),
        input_types,
        input_shapes_str,
        ",".join(output_tensors),
        graph_def_filename,
        output_file.name,
    )
    exit_code = os.system(cmd)
    log = (
        cmd + "exited with code %d" % exit_code + "\n------------------\n" +
        stdout_file.read())
    return (None if exit_code != 0 else output_file.read()), log


def mlir_convert_saved_model(saved_model_dir,
                             is_signature_def_saved_model,
                             tags=(),
                             exported_names=(),
                             additional_flags=""):
  """Convert a saved_model into a tflite model with MLIR-based conversion.

  Args:
    saved_model_dir: Saved model dir.
    is_signature_def_saved_model: Whether the SavedModel SignatureDef importer
      or ObjectGraph importer should be used.
    tags: Set of tags identifying the MetaGraphDef within the SavedModel to
      analyze. All tags in the tag set must be present.
    exported_names: Names to export from SavedModel.
    additional_flags: A string of additional command line flags to be passed to
      MLIR converter.

  Returns:
    output tflite model, log_txt from conversion
    or None, log_txt if it did not convert properly.
  """
  bin_path = resource_loader.get_path_to_datafile(
      "../../../../compiler/mlir/lite/tf_tfl_translate")
  with tempfile.NamedTemporaryFile() as output_file, \
       tempfile.NamedTemporaryFile("w+") as stdout_file:
    tags_str = ",".join(tags)
    exported_names_str = ",".join(exported_names)

    saved_model_flag = "-savedmodel-objectgraph-to-mlir"
    if is_signature_def_saved_model:
      saved_model_flag = "-savedmodel-signaturedefs-to-mlir"

    cmd = ("%s %s --tf-savedmodel-tags=%s --tf-savedmodel-exported-names=%s " +
           additional_flags + " %s --o=%s")
    cmd = cmd % (
        bin_path,
        saved_model_flag,
        tags_str,
        exported_names_str,
        saved_model_dir,
        output_file.name,
    )
    exit_code = os.system(cmd)
    log = (
        cmd + "exited with code %d" % exit_code + "\n------------------\n" +
        stdout_file.read())
    return (None if exit_code != 0 else output_file.read()), log


def mlir_convert_jax(input_file_name, is_hlotxt_format, additional_flags=""):
  """Convert a jax model file into a tflite model with MLIR-based conversion.

  Args:
    input_file_name: Jax hlo proto file.
    is_hlotxt_format: Whether the input proto is in hlotxt format.
    additional_flags: A string of additional command line flags to be passed to
      MLIR converter.

  Returns:
    output tflite model, log_txt from conversion
    or None, log_txt if it did not convert properly.
  """
  bin_path = resource_loader.get_path_to_datafile(
      "../../../../compiler/mlir/lite/tf_tfl_translate")
  hlo_import_type_str = "proto"
  if is_hlotxt_format:
    hlo_import_type_str = "hlotxt"
  with tempfile.NamedTemporaryFile() as output_file, \
       tempfile.NamedTemporaryFile("w+") as stdout_file:
    cmd = ("%s --import-hlo --enable-hlo-to-tf-conversion "
           "--hlo-import-type=%s" + additional_flags + " %s --o=%s")
    cmd = cmd % (
        bin_path,
        hlo_import_type_str,
        input_file_name,
        output_file.name,
    )
    exit_code = os.system(cmd)
    log = (
        cmd + "exited with code %d" % exit_code + "\n------------------\n" +
        stdout_file.read())
    return (None if exit_code != 0 else output_file.read()), log
