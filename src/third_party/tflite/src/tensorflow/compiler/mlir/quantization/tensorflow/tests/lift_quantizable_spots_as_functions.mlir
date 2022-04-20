// Copyright 2022 The TensorFlow Runtime Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// RUN: tf-quant-opt %s -split-input-file -tf-fused-kernel-matcher -quant-lift-quantizable-spots-as-functions | FileCheck %s

// CHECK-LABEL: float_conv
func.func @float_conv(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x2xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) {
  %cst = "tf.Const"() {value = dense<0.000000e+00> : tensor<2xf32>} : () -> tensor<2xf32>
  %0 = "tf.Conv2D"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  %1 = "tf.BiasAdd"(%0, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  %2 = "tf.Relu6"(%1) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %3 = "tf.Conv2D"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  %4 = "tf.BiasAdd"(%3, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  %5 = "tf.Relu"(%4) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %6 = "tf.Conv2D"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  %7 = "tf.BiasAdd"(%6, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  func.return %2, %5, %7 : tensor<*xf32>, tensor<*xf32>, tensor<*xf32>

// CHECK: %[[CONST_0:.*]] = "tf.Const"() {value = dense<0.000000e+00> : tensor<2xf32>} : () -> tensor<2xf32>
// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: {_tfl_quant_trait = "fully_quantizable",
// CHECK-SAME: f = @fused_conv2d_with_bias_and_relu6_fn_1}
// CHECK: %[[PARTITIONEDCALL_1:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: f = @fused_conv2d_with_bias_and_relu_fn_1}
// CHECK: %[[PARTITIONEDCALL_2:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: f = @fused_conv2d_with_bias_fn_1}
// CHECK: return %[[PARTITIONEDCALL_0]], %[[PARTITIONEDCALL_1]], %[[PARTITIONEDCALL_2]]
// CHECK: }

// CHECK-LABEL: private @fused_conv2d_with_bias_and_relu6_fn_1
// CHECK-NEXT: %[[conv:.*]] = "tf._FusedConv2D"(%arg0, %arg1, %arg2)
// CHECK-SAME: {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations"
// CHECK-SAME: data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], epsilon = 0.000000e+00 : f32, explicit_paddings = [], fused_ops = ["BiasAdd", "Relu6"], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true}
// CHECK-NEXT: return %[[conv]]

// CHECK-LABEL: private @fused_conv2d_with_bias_and_relu_fn_1
// CHECK-NEXT: tf._FusedConv2D"(%arg0, %arg1, %arg2)
// CHECK-SAME: fused_ops = ["BiasAdd", "Relu"]

// CHECK-LABEL: private @fused_conv2d_with_bias_fn_1
// CHECK-NEXT: tf._FusedConv2D"(%arg0, %arg1, %arg2)
// CHECK-SAME: fused_ops = ["BiasAdd"]
}

// -----

func.func @float_conv_strides_equals_to_dilations(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x2xf32>) -> tensor<*xf32> {
  %cst = "tf.Const"() {value = dense<0.000000e+00> : tensor<2xf32>} : () -> tensor<2xf32>
  %0 = "tf.Conv2D"(%arg0, %arg1) {data_format = "NHWC", device = "", dilations = [1, 1, 2, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  %1 = "tf.BiasAdd"(%0, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  %2 = "tf.Relu6"(%1) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>
  func.return %2 : tensor<*xf32>
}

// CHECK-LABEL: func @float_conv_strides_equals_to_dilations(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x2xf32>) -> tensor<*xf32> {
// CHECK: %[[CONST_0:.*]] = "tf.Const"() {value = dense<0.000000e+00> : tensor<2xf32>} : () -> tensor<2xf32>
// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]]) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @fused_conv2d_with_bias_and_relu6_fn_1} : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>, tensor<2xf32>) -> tensor<*xf32>
// CHECK: return %[[PARTITIONEDCALL_0]] : tensor<*xf32>
// CHECK: }
// CHECK-LABEL: func private @fused_conv2d_with_bias_and_relu6_fn_1(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x2xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.fused_function} {
// CHECK-NEXT: %[[CONV2D_0:.*]] = "tf._FusedConv2D"(%arg0, %arg1, %arg2)
// CHECK-SAME: {attr_map = "0:dilations,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations"
// CHECK-SAME: data_format = "NHWC", device = "", dilations = [1, 1, 2, 1], epsilon = 0.000000e+00 : f32, explicit_paddings = [], fused_ops = ["BiasAdd", "Relu6"], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true}
// CHECK-NEXT: return %[[CONV2D_0]] : tensor<*xf32>

// -----

// CHECK-LABEL: float_depthwise_conv
func.func @float_depthwise_conv(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x1xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) {
  %cst = "tf.Const"() {value = dense<0.000000e+00> : tensor<2xf32>} : () -> tensor<2xf32>
  %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1]
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x1xf32>) -> tensor<*xf32>
  %1 = "tf.BiasAdd"(%0, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  %2 = "tf.Relu6"(%1) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %3 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1]
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x1xf32>) -> tensor<*xf32>
  %4 = "tf.BiasAdd"(%3, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  %5 = "tf.Relu"(%4) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %6 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1]
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x1xf32>) -> tensor<*xf32>
  %7 = "tf.BiasAdd"(%6, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
  func.return %2, %5, %7 : tensor<*xf32>, tensor<*xf32>, tensor<*xf32>

// CHECK: %[[CONST_0:.*]] = "tf.Const"() {value = dense<0.000000e+00> : tensor<2xf32>} : () -> tensor<2xf32>
// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: {_tfl_quant_trait = "fully_quantizable",
// CHECK-SAME: f = @fused_depthwise_conv2d_with_bias_and_relu6_fn_1}
// CHECK: %[[PARTITIONEDCALL_1:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: f = @fused_depthwise_conv2d_with_bias_and_relu_fn_1}
// CHECK: %[[PARTITIONEDCALL_2:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: f = @fused_depthwise_conv2d_with_bias_fn_1}
// CHECK: return %[[PARTITIONEDCALL_0]], %[[PARTITIONEDCALL_1]], %[[PARTITIONEDCALL_2]]
// CHECK: }
}

// -----

// CHECK-LABEL: float_matmul
func.func @float_matmul(
  %arg0: tensor<1x10xf32>, %arg1: tensor<10x10xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) {
  %cst = "tf.Const"() {value = dense<0.000000e+00> : tensor<10xf32>} : () -> tensor<10xf32>
  %0 = "tf.MatMul"(%arg0, %arg1) {
    transpose_a = false, transpose_b = false
  } : (tensor<1x10xf32>, tensor<10x10xf32>) -> tensor<*xf32>
  %1 = "tf.BiasAdd"(%0, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<10xf32>) -> tensor<*xf32>
  %2 = "tf.Relu6"(%1) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>

  %3 = "tf.MatMul"(%arg0, %arg1) {
    transpose_a = true, transpose_b = false
  } : (tensor<1x10xf32>, tensor<10x10xf32>) -> tensor<*xf32>
  %4 = "tf.BiasAdd"(%3, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<10xf32>) -> tensor<*xf32>
  %5 = "tf.Relu"(%4) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>

  %6 = "tf.MatMul"(%arg0, %arg1) {
    transpose_a = false, transpose_b = true
  } : (tensor<1x10xf32>, tensor<10x10xf32>) -> tensor<*xf32>
  %7 = "tf.BiasAdd"(%6, %cst) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<10xf32>) -> tensor<*xf32>
  func.return %2, %5, %7 : tensor<*xf32>, tensor<*xf32>, tensor<*xf32>

// CHECK: %[[CONST_0:.*]] = "tf.Const"() {value = dense<0.000000e+00> : tensor<10xf32>}
// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: {_tfl_quant_trait = "fully_quantizable",
// CHECK-SAME: f = @fused_matmul_with_bias_and_relu6_fn_1}
// CHECK: %[[PARTITIONEDCALL_1:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: f = @fused_matmul_with_bias_and_relu_fn_1}
// CHECK: %[[PARTITIONEDCALL_2:.*]] = "tf.PartitionedCall"(%arg0, %arg1, %[[CONST_0]])
// CHECK-SAME: f = @fused_matmul_with_bias_fn_1}
// CHECK: return %[[PARTITIONEDCALL_0]], %[[PARTITIONEDCALL_1]], %[[PARTITIONEDCALL_2]]
// CHECK: }

// CHECK-LABEL: private @fused_matmul_with_bias_and_relu6_fn_1
// CHECK-NEXT: %[[matmul:.*]] = "tf._FusedMatMul"(%arg0, %arg1, %arg2)
// CHECK-SAME: {attr_map = "0:transpose_a,1:transpose_a"
// CHECK-NEXT: return %[[matmul]]

// CHECK-LABEL: private @fused_matmul_with_bias_and_relu_fn_1
// CHECK-NEXT: tf._FusedMatMul"(%arg0, %arg1, %arg2)
// CHECK-SAME: fused_ops = ["BiasAdd", "Relu"]

// CHECK-LABEL: private @fused_matmul_with_bias_fn_1
// CHECK-NEXT: tf._FusedMatMul"(%arg0, %arg1, %arg2)
// CHECK-SAME: fused_ops = ["BiasAdd"]
}

// -----

// CHECK-LABEL: float_conv_no_bias
func.func @float_conv_no_bias(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x2xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) {
  %0 = "tf.Conv2D"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  %1 = "tf.Relu6"(%0) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %3 = "tf.Conv2D"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  %4 = "tf.Relu"(%3) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %6 = "tf.Conv2D"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<*xf32>
  func.return %1, %4, %6 : tensor<*xf32>, tensor<*xf32>, tensor<*xf32>

// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: {_tfl_quant_trait = "fully_quantizable",
// CHECK-SAME: f = @fused_conv2d_with_relu6_fn_1}
// CHECK: %[[PARTITIONEDCALL_1:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: f = @fused_conv2d_with_relu_fn_1}
// CHECK: %[[PARTITIONEDCALL_2:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: f = @fused_conv2d_fn_1}
// CHECK: return %[[PARTITIONEDCALL_0]], %[[PARTITIONEDCALL_1]], %[[PARTITIONEDCALL_2]]
// CHECK: }

// CHECK-LABEL: private @fused_conv2d_with_relu6_fn_1
// CHECK-LABEL: private @fused_conv2d_with_relu_fn_1
// CHECK-LABEL: private @fused_conv2d_fn_1
}

// -----

// CHECK-LABEL: float_depthwise_conv_no_bias
func.func @float_depthwise_conv_no_bias(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x1xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) {
  %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1]
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x1xf32>) -> tensor<*xf32>
  %1 = "tf.Relu6"(%0) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %3 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1]
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x1xf32>) -> tensor<*xf32>
  %4 = "tf.Relu"(%3) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>


  %6 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {
    data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [],
    padding = "SAME", strides = [1, 1, 2, 1]
  } : (tensor<1x3x4x3xf32>, tensor<2x3x3x1xf32>) -> tensor<*xf32>
  func.return %1, %4, %6 : tensor<*xf32>, tensor<*xf32>, tensor<*xf32>

// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: {_tfl_quant_trait = "fully_quantizable",
// CHECK-SAME: f = @fused_depthwise_conv2d_with_relu6_fn_1}
// CHECK: %[[PARTITIONEDCALL_1:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: f = @fused_depthwise_conv2d_with_relu_fn_1}
// CHECK: %[[PARTITIONEDCALL_2:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: f = @fused_depthwise_conv2d_fn_1}
// CHECK: return %[[PARTITIONEDCALL_0]], %[[PARTITIONEDCALL_1]], %[[PARTITIONEDCALL_2]]
// CHECK: }

// CHECK-LABEL: private @fused_depthwise_conv2d_with_relu6_fn_1
// CHECK-LABEL: private @fused_depthwise_conv2d_with_relu_fn_1
// CHECK-LABEL: private @fused_depthwise_conv2d_fn_1
}

// -----

// CHECK-LABEL: float_matmul_no_bias
func.func @float_matmul_no_bias(
  %arg0: tensor<1x10xf32>, %arg1: tensor<10x10xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<*xf32>) {
  %0 = "tf.MatMul"(%arg0, %arg1) {
    transpose_a = false, transpose_b = false
  } : (tensor<1x10xf32>, tensor<10x10xf32>) -> tensor<*xf32>
  %1 = "tf.Relu6"(%0) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>

  %3 = "tf.MatMul"(%arg0, %arg1) {
    transpose_a = true, transpose_b = false
  } : (tensor<1x10xf32>, tensor<10x10xf32>) -> tensor<*xf32>
  %4 = "tf.Relu"(%3) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>

  %6 = "tf.MatMul"(%arg0, %arg1) {
    transpose_a = false, transpose_b = true
  } : (tensor<1x10xf32>, tensor<10x10xf32>) -> tensor<*xf32>
  func.return %1, %4, %6 : tensor<*xf32>, tensor<*xf32>, tensor<*xf32>

// CHECK: %[[PARTITIONEDCALL_0:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: {_tfl_quant_trait = "fully_quantizable",
// CHECK-SAME: f = @fused_matmul_with_relu6_fn_1}
// CHECK: %[[PARTITIONEDCALL_1:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: f = @fused_matmul_with_relu_fn_1}
// CHECK: %[[PARTITIONEDCALL_2:.*]] = "tf.PartitionedCall"(%arg0, %arg1)
// CHECK-SAME: f = @fused_matmul_fn_1}
// CHECK: return %[[PARTITIONEDCALL_0]], %[[PARTITIONEDCALL_1]], %[[PARTITIONEDCALL_2]]
// CHECK: }

// CHECK-LABEL: private @fused_matmul_with_relu6_fn_1
// CHECK-LABEL: private @fused_matmul_with_relu_fn_1
// CHECK-LABEL: private @fused_matmul_fn_1
}
