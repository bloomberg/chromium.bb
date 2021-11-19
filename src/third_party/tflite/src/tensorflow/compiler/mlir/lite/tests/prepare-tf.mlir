// RUN: tf-opt -tfl-prepare-tf %s | FileCheck %s
// RUN: tf-opt %s -tf-layout-optimization=force-data-format=NHWC -tfl-prepare-tf  | FileCheck --check-prefix=LAYOUT --dump-input=always %s

module attributes {tf.versions = {bad_consumers = [], min_consumer = 0 : i32, producer = 268 : i32}} {

func @conv(tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>, tensor<256x3x32x32xf32>) -> (tensor<256x8x7x16xf32>, tensor<256x16x32x32xf32>, tensor<256x8x6x16xf32>, tensor<256x32x32x16xf32>, tensor<256x32x32x16xf32>) {
^bb0(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>, %arg2: tensor<256x3x32x32xf32>) :
   // OK
   %0 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x8x7x16xf32>
   // Unsupported data format
   %1 = "tf.Conv2D"(%arg2, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NCHW", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x3x32x32xf32>, tensor<3x3x3x16xf32>) -> tensor<256x16x32x32xf32>
   // OK
   %2 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC",                           padding = "VALID", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x8x6x16xf32>
   // Unsupported padding
   %3 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "EXPLICIT", strides = [1, 1, 1, 1], explicit_paddings = [0, 0, 1, 1, 1, 1, 0, 0]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
   // Unsupported strides
   %4 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [2, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>

  return %0, %1, %2, %3, %4 : tensor<256x8x7x16xf32>, tensor<256x16x32x32xf32>, tensor<256x8x6x16xf32>, tensor<256x32x32x16xf32>, tensor<256x32x32x16xf32>

// CHECK-LABEL: conv
// CHECK-DAG:  %[[CONSTANT:.*]] = arith.constant dense<0.000000e+00> : tensor<16xf32>
// CHECK-DAG:  %[[CONSTANT0:.*]] = arith.constant dense<[3, 0, 1, 2]> : tensor<4xi32>
// CHECK-DAG:  %[[CONSTANT1:.*]] = arith.constant dense<[{{\[}}0, 0], [1, 1], [1, 1], [0, 0]]> : tensor<4x2xi32>
// CHECK:  %0 = "tf.Transpose"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x16xf32>, tensor<4xi32>) -> tensor<16x3x3x3xf32>
// CHECK:  %1 = "tfl.conv_2d"(%arg0, %0, %[[CONSTANT]]) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x8x7x16xf32>
// CHECK:  %2 = "tf.Conv2D"
// CHECK:  %3 = "tf.Transpose"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x16xf32>, tensor<4xi32>) -> tensor<16x3x3x3xf32>
// CHECK:  %4 = "tfl.conv_2d"(%arg0, %3, %[[CONSTANT]]) {dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x8x6x16xf32>
// CHECK:  %5 = "tf.Pad"(%arg0, %[[CONSTANT1]]) : (tensor<256x32x32x3xf32>, tensor<4x2xi32>) -> tensor<*xf32>
// CHECK:  %6 = "tf.Transpose"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x16xf32>, tensor<4xi32>) -> tensor<16x3x3x3xf32>
// CHECK:  %7 = "tfl.conv_2d"(%5, %6, %[[CONSTANT]]) {dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<*xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x32x32x16xf32>
// CHECK:  %8 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [2, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x32x32x16xf32>
}

func @depthwiseConv2D(tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>, tensor<256x3x32x32xf32>) -> (tensor<256x30x30x12xf32>, tensor<256x12x30x30xf32>, tensor<256x30x30x12xf32>, tensor<256x30x30x12xf32>) {
^bb0(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x4xf32>, %arg2: tensor<256x3x32x32xf32>) :
   // OK
   %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32>
   // Unsupported data format
   %1 = "tf.DepthwiseConv2dNative"(%arg2, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NCHW", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x3x32x32xf32>, tensor<3x3x3x4xf32>) -> tensor<256x12x30x30xf32>
   // OK
   %2 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC",                           padding = "VALID", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32>
   // Unsupported strides
   %3 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [2, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xf32>

  return %0, %1, %2, %3 : tensor<256x30x30x12xf32>, tensor<256x12x30x30xf32>, tensor<256x30x30x12xf32>, tensor<256x30x30x12xf32>

// CHECK-LABEL: depthwiseConv2D
// CHECK-DAG:  %[[CONSTANT:.*]] = arith.constant dense<0.000000e+00> : tensor<12xf32>
// CHECK-DAG:  %[[CONSTANT0:.*]] = arith.constant dense<[1, 3, 3, 12]> : tensor<4xi32>
// CHECK:  %0 = "tf.Reshape"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x4xf32>, tensor<4xi32>) -> tensor<1x3x3x12xf32>
// CHECK:  %1 = "tfl.depthwise_conv_2d"(%arg0, %0, %[[CONSTANT]]) {depth_multiplier = 4 : i32, dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<1x3x3x12xf32>, tensor<12xf32>) -> tensor<256x30x30x12xf32>
// CHECK:  %2 = "tf.DepthwiseConv2dNative"
// CHECK:  %3 = "tf.Reshape"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x4xf32>, tensor<4xi32>) -> tensor<1x3x3x12xf32>
// CHECK:  %4 = "tfl.depthwise_conv_2d"(%arg0, %3, %[[CONSTANT]]) {depth_multiplier = 4 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<1x3x3x12xf32>, tensor<12xf32>) -> tensor<256x30x30x12xf32>
// CHECK:  %5 = "tf.DepthwiseConv2dNative"
}

func @Conv2dNCHW(%arg0: tensor<256x3x32x32xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<256x16x32x32xf32> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NCHW", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x3x32x32xf32>, tensor<3x3x3x16xf32>) -> tensor<256x16x32x32xf32>
  return %0 : tensor<256x16x32x32xf32>

  // LAYOUT-LABEL: Conv2dNCHW
  // LAYOUT: "tfl.conv_2d"
}

func @fusedBatchNormV3(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>) {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // OK
  %0:6 = "tf.FusedBatchNormV3"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Training with non-broadcastable shape
  %cst = arith.constant dense<0.0> : tensor<4xf32>
  %1:6 = "tf.FusedBatchNormV3"( %0#0, %cst, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = true}  : (tensor<8x8x8x8xf32>, tensor<4xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Inference with non-broadcastable shape
  %2:6 = "tf.FusedBatchNormV3"( %1#0, %cst, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<4xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Use other output
  %3:6 = "tf.FusedBatchNormV3"( %2#0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)

  return %3, %3#1 : tensor<8x8x8x8xf32>, tensor<8xf32>

// CHECK-LABEL: fusedBatchNormV3
// CHECK-DAG:  %[[CONSTANT:.*]] = arith.constant dense<1.000000e-03>
// CHECK-DAG:  %[[CONSTANT1:.*]] = arith.constant dense<0.000000e+00> : tensor<4xf32>
//              variance + epsilon
// CHECK:  %[[ADD1:.*]] = "tf.Add"(%[[ARG4:.*]], %[[CONSTANT]])
//              rsqrt(variance + epsilon)
// CHECK:  %[[RSQRT:.*]] = "tf.Rsqrt"(%[[ADD1]])
//              scale * rsqrt(variance + epsilon)
// CHECK:  %[[MUL1:.*]] = "tf.Mul"(%[[ARG1:.*]], %[[RSQRT]])
//              x * scale * rsqrt(variance + epsilon)
// CHECK:  %[[MUL2:.*]] = "tf.Mul"(%[[ARG0:.*]], %[[MUL1]])
//              mean * scale * rsqrt(variance + epsilon)
// CHECK:  %[[MUL3:.*]] = "tf.Mul"(%[[ARG3:.*]], %[[MUL1]])
//              offset - mean * scale * rsqrt(variance + epsilon)
// CHECK:  %[[SUB:.*]] = "tf.Sub"(%[[ARG2:.*]], %[[MUL3]])
//              x * scale * rsqrt(variance + epsilon) +
//              offset - mean * scale * rsqrt(variance + epsilon)
// CHECK:  %[[ADD2:.*]] = "tf.Add"(%[[MUL2]], %[[SUB]])
// CHECK:  %[[BATCHNORM1_a:[^,]+]], {{.*}} = "tf.FusedBatchNormV3"(%[[ADD2]], %[[CONSTANT1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
// CHECK:  %[[BATCHNORM1_b:[^,]+]], {{.*}} = "tf.FusedBatchNormV3"(%[[BATCHNORM1_a]], %[[CONSTANT1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
// CHECK:  "tf.FusedBatchNormV3"(%[[BATCHNORM1_b]], %[[ARG1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
}


func @batchNormWithGlobalNormalization(
    %t:tensor<1x10x10x3xf32>, %m:tensor<3xf32>, %v:tensor<3xf32>, %beta:tensor<3xf32>, %gamma:tensor<3xf32>) -> (tensor<1x10x10x3xf32>) {
  %0 = "tf.BatchNormWithGlobalNormalization"(%t, %m, %v, %beta, %gamma) {T = "tfdtype$DT_FLOAT", variance_epsilon = 0.001 : f32, scale_after_normalization = false} : (tensor<1x10x10x3xf32>, tensor<3xf32>, tensor<3xf32>, tensor<3xf32>, tensor<3xf32>) -> (tensor<1x10x10x3xf32>)
  return %0 : tensor<1x10x10x3xf32>
// CHECK-LABEL: batchNormWithGlobalNormalization
// CHECK:  %[[EPSILON:.*]] = arith.constant dense<1.000000e-03>
// CHECK:  %[[VARIANCE:.*]] = "tf.Add"(%[[ARG_V:.*]], %[[EPSILON]])
// CHECK:  %[[RSQRT:.*]] = "tf.Rsqrt"(%[[VARIANCE]])
// CHECK:  %[[MUL1:.*]] = "tf.Mul"(%[[ARG_T:.*]], %[[RSQRT]])
// CHECK:  %[[MUL2:.*]] = "tf.Mul"(%[[ARG_M:.*]], %[[RSQRT]])
// CHECK:  %[[SUB:.*]] = "tf.Sub"(%[[ARG_BETA:.*]], %[[MUL2]])
// CHECK:  %[[RESULT:.*]] = "tf.Add"(%[[MUL1]], %[[SUB]])
// CHECK:  return %[[RESULT]]
}

func @batchNormWithGlobalNormalizationWithScaleAfterNormalization(
    %t:tensor<1x10x10x3xf32>, %m:tensor<3xf32>, %v:tensor<3xf32>, %beta:tensor<3xf32>, %gamma:tensor<3xf32>) -> (tensor<1x10x10x3xf32>) {
  %0 = "tf.BatchNormWithGlobalNormalization"(%t, %m, %v, %beta, %gamma) {T = "tfdtype$DT_FLOAT", variance_epsilon = 0.001 : f32, scale_after_normalization = true} : (tensor<1x10x10x3xf32>, tensor<3xf32>, tensor<3xf32>, tensor<3xf32>, tensor<3xf32>) -> (tensor<1x10x10x3xf32>)
  return %0 : tensor<1x10x10x3xf32>
// CHECK-LABEL: batchNormWithGlobalNormalizationWithScaleAfterNormalization
// CHECK:  %[[EPSILON:.*]] = arith.constant dense<1.000000e-03>
// CHECK:  %[[VARIANCE:.*]] = "tf.Add"(%[[ARG_V:.*]], %[[EPSILON]])
// CHECK:  %[[RSQRT:.*]] = "tf.Rsqrt"(%[[VARIANCE]])
// CHECK:  %[[MUL0:.*]] = "tf.Mul"(%[[RSQRT]], %[[ARG_GAMMA:.*]])
// CHECK:  %[[MUL1:.*]] = "tf.Mul"(%[[ARG_T:.*]], %[[MUL0]])
// CHECK:  %[[MUL2:.*]] = "tf.Mul"(%[[ARG_M:.*]], %[[MUL0]])
// CHECK:  %[[SUB:.*]] = "tf.Sub"(%[[ARG_BETA:.*]], %[[MUL2]])
// CHECK:  %[[RESULT:.*]] = "tf.Add"(%[[MUL1]], %[[SUB]])
// CHECK:  return %[[RESULT]]
}

func @QDQsFollowedByTranspose(tensor<1x2xf32>) -> (tensor<2x1xf32>) {
^bb0(%arg0: tensor<1x2xf32>):
  %cst_0 = arith.constant dense<[1, 0]> : tensor<2xi32>
  %0 = "tfl.quantize"(%arg0){qtype = tensor<1x2x!quant.uniform<u8:f32, 1.0>>}: (tensor<1x2xf32>) -> (tensor<1x2x!quant.uniform<u8:f32, 1.0>>)
  %1 = "tfl.dequantize"(%0): (tensor<1x2x!quant.uniform<u8:f32, 1.0>>) -> (tensor<1x2xf32>)
  %2 = "tf.Transpose"(%1, %cst_0): (tensor<1x2xf32>, tensor<2xi32>) -> tensor<2x1xf32>
  return %2 : tensor<2x1xf32>

// CHECK: %cst = arith.constant
// CHECK: %[[trans:.*]] = "tf.Transpose"
// CHECK-SAME: -> tensor<2x1xf32>
// CHECK: %[[q:.*]] = "tfl.quantize"(%[[trans]]) {qtype = tensor<2x1x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK-SAME: -> tensor<2x1x!quant.uniform<u8:f32, 1.000000e+00>>
// CHECK: %[[dq:.*]] = "tfl.dequantize"(%[[q]])
// CHECK-SAME: -> tensor<2x1xf32>
// CHECK: return %[[dq]]
}

// CHECK-LABEL: QDQFollowedByReshape
func @QDQFollowedByReshape(tensor<1x2xf32>) -> (tensor<2x1xf32>) {
^bb0(%arg0: tensor<1x2xf32>):
  %cst_0 = arith.constant dense<[2, 1]> : tensor<2xi32>
  %0 = "tfl.quantize"(%arg0){qtype = tensor<1x2x!quant.uniform<u8:f32, 1.0>>}: (tensor<1x2xf32>) -> (tensor<1x2x!quant.uniform<u8:f32, 1.0>>)
  %1 = "tfl.dequantize"(%0): (tensor<1x2x!quant.uniform<u8:f32, 1.0>>) -> (tensor<1x2xf32>)
  %2 = "tf.Reshape"(%1, %cst_0): (tensor<1x2xf32>, tensor<2xi32>) -> tensor<2x1xf32>
  return %2 : tensor<2x1xf32>

// CHECK: %cst = arith.constant
// CHECK: %[[rs:.*]] = "tf.Reshape"
// CHECK-SAME: -> tensor<2x1xf32>
// CHECK: %[[q:.*]] = "tfl.quantize"(%[[rs]]) {qtype = tensor<2x1x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK-SAME: -> tensor<2x1x!quant.uniform<u8:f32, 1.000000e+00>>
// CHECK: %[[dq:.*]] = "tfl.dequantize"(%[[q]])
// CHECK-SAME: -> tensor<2x1xf32>
// CHECK: return %[[dq]]
}

// CHECK-LABEL: QDQFollowedByRank
func @QDQFollowedByRank(%arg0: tensor<1x2xf32>) -> (tensor<i32>) {
  %0 = "tfl.quantize"(%arg0){qtype = tensor<1x2x!quant.uniform<u8:f32, 1.0>>}: (tensor<1x2xf32>) -> (tensor<1x2x!quant.uniform<u8:f32, 1.0>>)
  %1 = "tfl.dequantize"(%0): (tensor<1x2x!quant.uniform<u8:f32, 1.0>>) -> (tensor<1x2xf32>)
  %2 = "tf.Rank"(%1): (tensor<1x2xf32>) -> tensor<i32>
  return %2 : tensor<i32>

// CHECK: %[[R:.*]] = arith.constant dense<2>
// CHECK: return %cst : tensor<i32>
}

func @identity(%arg0: tensor<10xi32>, %arg1: tensor<20xi32>, %arg2: tensor<30xi32>) -> (tensor<10xi32>, tensor<20xi32>, tensor<30xi32>) {
  %0 = "tf.Identity"(%arg0) : (tensor<10xi32>) -> tensor<10xi32>
  %1:2 = "tf.IdentityN"(%arg1,%arg2) : (tensor<20xi32>, tensor<30xi32>) -> (tensor<20xi32>, tensor<30xi32>)
  return %0, %1#0, %1#1: tensor<10xi32>, tensor<20xi32>, tensor<30xi32>

// CHECK-LABEL: identity
// CHECK: return %arg0, %arg1, %arg2
}


func @matmulNoTransposeAOrB(%arg0: tensor<1x1280xf32>, %arg1: tensor<1280x1000xf32>) -> tensor<1x1000xf32> {
  %166 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", _output_shapes = ["tfshape$dim { size = 1} dim { size = 1000}"], device = "", name = "matmul", transpose_a = false, transpose_b = false} : (tensor<1x1280xf32>, tensor<1280x1000xf32>) -> tensor<1x1000xf32>
  return %166 : tensor<1x1000xf32>

  // CHECK-LABEL: matmulNoTransposeAOrB
  // CHECK: %[[RES:.*]] = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<?xi32>
  // CHECK: %[[TRANS:.*]] = "tf.Transpose"(%arg1, %[[RES]]) : (tensor<1280x1000xf32>, tensor<?xi32>) -> tensor<*xf32>
  // CHECK: %[[MM:.*]] = "tf.MatMul"(%arg0, %[[TRANS]]) {transpose_a = false, transpose_b = true} : (tensor<1x1280xf32>, tensor<*xf32>) -> tensor<1x1000xf32>
  // CHECK: return %[[MM]] : tensor<1x1000xf32>
 }

func @matmulNoTransposeB(%arg0: tensor<1x1280xf32>, %arg1: tensor<1280x1000xf32>) -> tensor<1x1000xf32> {
  %166 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", _output_shapes = ["tfshape$dim { size = 1} dim { size = 1000}"], device = "", name = "matmul", transpose_a = true, transpose_b = false} : (tensor<1x1280xf32>, tensor<1280x1000xf32>) -> tensor<1x1000xf32>
  return %166 : tensor<1x1000xf32>

  // CHECK-LABEL: matmulNoTransposeB
  // CHECK: %[[RES:.*]] = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<?xi32>
  // CHECK: %[[TRANS1:.*]] = "tf.Transpose"(%arg0, %[[RES]]) : (tensor<1x1280xf32>, tensor<?xi32>) -> tensor<*xf32>
  // CHECK: %[[TRANS2:.*]] = "tf.Transpose"(%arg1, %[[RES]]) : (tensor<1280x1000xf32>, tensor<?xi32>) -> tensor<*xf32>
  // CHECK: %[[MM:.*]] = "tf.MatMul"(%[[TRANS1]], %[[TRANS2]]) {transpose_a = false, transpose_b = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<1x1000xf32>
  // CHECK: return %[[MM]] : tensor<1x1000xf32>

}

func @snapshot(%arg0: tensor<3xi32>) -> tensor<3xi32> {
  %0 = "tf.Snapshot"(%arg0) : (tensor<3xi32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
  // Should be converted to Identity and then from Identity to value
  // CHECK-LABEL: snapshot
  // CHECK:  return %arg0 : tensor<3xi32>
}

func @stop_gradient(%arg0: tensor<3xi32>) -> tensor<3xi32> {
  %0 = "tf.StopGradient"(%arg0) : (tensor<3xi32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
  // Should be converted to Identity and then from Identity to value
  // CHECK-LABEL: stop_gradient
  // CHECK:  return %arg0 : tensor<3xi32>
}

func @CheckNumerics(%arg0: tensor<3xf32>) -> tensor<3xf32> {
  %0 = "tf.CheckNumerics"(%arg0) {message = ""}: (tensor<3xf32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>
  // Should be converted to Identity and then from Identity to value
  // CHECK-LABEL: CheckNumerics
  // CHECK:  return %arg0 : tensor<3xf32>
}

func @placeholder_with_default(%arg0: tensor<3xf32>) -> tensor<3xf32> {
  %0 = "tf.PlaceholderWithDefault"(%arg0): (tensor<3xf32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>
  // Should be converted to Identity and then from Identity to value
  // CHECK-LABEL: placeholder_with_default
  // CHECK:  return %arg0 : tensor<3xf32>
}

// CHECK-LABEL: @StridedSliceEllipsisMaskBefore
func @StridedSliceEllipsisMaskBefore(%arg0: tensor<21x15x7xf32>) -> tensor<21x15x2xf32> {
  %cst = arith.constant dense<0> : tensor<2xi32>
  %cst_0 = arith.constant dense<1> : tensor<2xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 0 : i64, ellipsis_mask = 1 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<21x15x2xf32>
  return %0 : tensor<21x15x2xf32>

  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<0> : tensor<3xi32>
  // CHECK-DAG: %[[CST_0:.*]] = arith.constant dense<1> : tensor<3xi32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST]], %[[CST_0]]) {begin_mask = 3 : i64, ellipsis_mask = 0 : i64, end_mask = 3 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<21x15x2xf32>
}

// CHECK-LABEL: @StridedSliceEllipsisMaskBeforeWithBeginAndEndMask
func @StridedSliceEllipsisMaskBeforeWithBeginAndEndMask(%arg0: tensor<4x5x4xf32>) -> tensor<4x4x4xf32> {
  %cst = arith.constant dense<[0, 1, 0]> : tensor<3xi32>
  %cst_0 = arith.constant dense<0> : tensor<3xi32>
  %cst_1 = arith.constant dense<1> : tensor<3xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst_0, %cst_1) {begin_mask = 6 : i64, ellipsis_mask = 1 : i64, end_mask = 4 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<4x5x4xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<4x4x4xf32>
  return %0 : tensor<4x4x4xf32>

  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<[0, 1, 0]> : tensor<3xi32>
  // CHECK-DAG: %[[CST_0:.*]] = arith.constant dense<0> : tensor<3xi32>
  // CHECK-DAG: %[[CST_1:.*]] = arith.constant dense<1> : tensor<3xi32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST_0]], %[[CST_1]]) {begin_mask = 7 : i64, ellipsis_mask = 0 : i64, end_mask = 5 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<4x5x4xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<4x4x4xf32>
}

// CHECK-LABEL: @StridedSliceEllipsisMaskAfter
func @StridedSliceEllipsisMaskAfter(%arg0: tensor<21x15x7xf32>) -> tensor<5x15x7xf32> {
  %cst = arith.constant dense<0> : tensor<2xi32>
  %cst_0 = arith.constant dense<1> : tensor<2xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 0 : i64, ellipsis_mask = 2 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<5x15x7xf32>
  return %0 : tensor<5x15x7xf32>

  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<0> : tensor<3xi32>
  // CHECK-DAG: %[[CST_0:.*]] = arith.constant dense<1> : tensor<3xi32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST]], %[[CST_0]]) {begin_mask = 6 : i64, ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<5x15x7xf32>
}

// CHECK-LABEL: @NoStridedSliceEllipsisMask
func @NoStridedSliceEllipsisMask(%arg0: tensor<*xf32>) -> tensor<21x15x2xf32> {
  %cst = arith.constant dense<0> : tensor<2xi32>
  %cst_0 = arith.constant dense<1> : tensor<2xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 0 : i64, ellipsis_mask = 1 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<*xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<21x15x2xf32>
  return %0 : tensor<21x15x2xf32>

  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<0> : tensor<2xi32>
  // CHECK-DAG: %[[CST_0:.*]] = arith.constant dense<1> : tensor<2xi32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST]], %[[CST_0]]) {begin_mask = 0 : i64, ellipsis_mask = 1 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<*xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<21x15x2xf32>
}

// CHECK-LABEL: @NoPadStridedSliceNonNewAxisMask
func @NoPadStridedSliceNonNewAxisMask(%arg0: tensor<1x2x3x1xf32>) -> tensor<1x2x3x1xf32> {
  %cst = arith.constant dense<0> : tensor<4xi32>
  %cst_0 = arith.constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 15 : i64, ellipsis_mask = 0 : i64, end_mask = 15 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<1x2x3x1xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
  return %0 : tensor<1x2x3x1xf32>

  // CHECK-DAG: %cst = arith.constant dense<0> : tensor<4xi32>
  // CHECK-DAG: %cst_0 = arith.constant dense<1> : tensor<4xi32>
  // CHECK: %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 15 : i64, ellipsis_mask = 0 : i64, end_mask = 15 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<1x2x3x1xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
}

// CHECK-LABEL: @PadStridedSliceNewAxisMask1
func @PadStridedSliceNewAxisMask1(%arg0: tensor<2x3xf32>) -> tensor<1x2x3x1xf32> {
  %cst = arith.constant dense<0> : tensor<4xi32>
  %cst_0 = arith.constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 6 : i64, ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 9 : i64, shrink_axis_mask = 0 : i64} : (tensor<2x3xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
  return %0 : tensor<1x2x3x1xf32>

  // CHECK-DAG: %[[CST0:.*]] = arith.constant dense<0> : tensor<4xi32>
  // CHECK-DAG: %[[CST1:.*]] = arith.constant dense<1> : tensor<4xi32>
  // CHECK-DAG: %[[cst_1:.*]] = arith.constant dense<[1, 2, 3, 1]> : tensor<4xi32>
  // CHECK: %0 = "tf.Reshape"(%arg0, %[[cst_1]]) : (tensor<2x3xf32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
  // CHECK: %1 = "tf.StridedSlice"(%0, %[[CST0]], %[[CST0]], %[[CST1]]) {begin_mask = 15 : i64, ellipsis_mask = 0 : i64, end_mask = 15 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<1x2x3x1xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
}

// CHECK-LABEL: @PadStridedSliceNewAxisMask2
func @PadStridedSliceNewAxisMask2(%arg0: tensor<4x64x64x1xf32>) -> tensor<1x4x64x64xf32> {
  %cst = arith.constant dense<0> : tensor<3xi32>
  %cst_0 = arith.constant dense<1> : tensor<3xi32>
  %0 = "tf.Squeeze"(%arg0) {T = f32, _output_shapes = ["tfshape$dim { size: 4 } dim { size: 64 } dim { size: 64 }"], device = "", squeeze_dims = []} : (tensor<4x64x64x1xf32>) -> tensor<4x64x64xf32>
  %1 = "tf.StridedSlice"(%0, %cst, %cst, %cst_0) {Index = i32, T = f32, _output_shapes = ["tfshape$dim { size: 1 } dim { size: 4 } dim { size: 64 } dim { size: 64 }"], begin_mask = 6 : i64, device = "", ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 1 : i64, shrink_axis_mask = 0 : i64} : (tensor<4x64x64xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<1x4x64x64xf32>
  return %1 : tensor<1x4x64x64xf32>
}

// CHECK-LABEL: @AvoidPadStridedSliceNewAxisMaskOnUnknownShapes
func @AvoidPadStridedSliceNewAxisMaskOnUnknownShapes(%arg0: tensor<?x?xf32>) -> tensor<1x?x?x1xf32> {
  %cst = arith.constant dense<0> : tensor<4xi32>
  %cst_0 = arith.constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 6 : i64, ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 9 : i64, shrink_axis_mask = 0 : i64} : (tensor<?x?xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x?x?x1xf32>
  return %0 : tensor<1x?x?x1xf32>

  // CHECK-NOT: "tf.Reshape"
  // CHECK: "tf.StridedSlice"
}

// CHECK-LABEL: @StridedSliceRewriteMasks
func @StridedSliceRewriteMasks(%arg0: tensor<8x4x16x2xf32>) -> tensor<8x4x16x1xf32> {
  %cst = "tf.Const"() {device = "", value = dense<[1, 0, 1]> : tensor<3xi32>} : () -> tensor<3xi32>
  %cst_0 = "tf.Const"() {device = "", value = dense<[1, 0, 0]> : tensor<3xi32>} : () -> tensor<3xi32>
  %cst_1 = "tf.Const"() {device = "", value = dense<1> : tensor<3xi32>} : () -> tensor<3xi32>

  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<[1, 0, 0, 1]> : tensor<4xi32>
  // CHECK-DAG: %[[CST0:.*]] = arith.constant dense<[1, 0, 0, 0]> : tensor<4xi32>
  // CHECK-DAG: %[[CST1:.*]] = arith.constant dense<1> : tensor<4xi32>
  // CHECK: %[[RESULT:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST0]], %[[CST1]])
  // CHECK-SAME: begin_mask = 7 : i64
  // CHECK-SAME: ellipsis_mask = 0 : i64
  // CHECK-SAME: end_mask = 14 : i64
  // CHECK-SAME: new_axis_mask = 0 : i64
  // CHECK-SAME: shrink_axis_mask = 0 : i64

  %0 = "tf.StridedSlice"(%arg0, %cst, %cst_0, %cst_1) {begin_mask = 1 : i64, device = "", ellipsis_mask = 2 : i64, end_mask = 4 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<8x4x16x2xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<8x4x16x1xf32>
  return %0 : tensor<8x4x16x1xf32>
}

func @strided_slice_with_constant_attributes(%arg0: tensor<10x10x10xf32>, %arg1: tensor<1xi32>, %arg2: tensor<1xi32>, %arg3: tensor<1xi32>) -> tensor<10x10xf32> {
  %cst = arith.constant dense<-1> : tensor<1xi32>
  %cst_1 = arith.constant dense<0> : tensor<1xi32>
  %cst_2 = arith.constant dense<1> : tensor<1xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst_1, %cst_2) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 1 : i64} : (tensor<10x10x10xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<10x10xf32>
  return %0 : tensor<10x10xf32>
  // CHECK-LABEL: strided_slice_with_constant_attributes
  // CHECK-DAG: [[BEGIN:%cst.*]] = arith.constant dense<[-1, 0, 0]> : tensor<3xi32>
  // CHECK-DAG: [[END:%cst.*]] = arith.constant dense<[0, 10, 10]> : tensor<3xi32>
  // CHECK-DAG: [[STRIDES:%cst.*]] = arith.constant dense<1> : tensor<3xi32>
  // CHECK-NEXT: "tf.StridedSlice"(%arg0, [[BEGIN]], [[END]], [[STRIDES]]) {begin_mask = 6 : i64, ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 1 : i64} : (tensor<10x10x10xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<10x10xf32>
}

// CHECK-LABEL: @StridedSliceEllipsisAndNewAxisMaskBothSet
func @StridedSliceEllipsisAndNewAxisMaskBothSet(%arg0: tensor<6x7x8xf32>) -> tensor<2x1x7x8x1xf32> {
  %begin = arith.constant dense<0> : tensor<4xi32>
  %end = arith.constant dense<[2,3,4,5]> : tensor<4xi32>
  %step = arith.constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %begin, %end, %step) {
    begin_mask = 0 : i64, ellipsis_mask = 4 : i64, end_mask = 0 : i64, new_axis_mask = 10 : i64, shrink_axis_mask = 0 : i64
  } : (tensor<6x7x8xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<2x1x7x8x1xf32>
  return %0 : tensor<2x1x7x8x1xf32>

  // CHECK-DAG: %[[BEGIN:.*]] = arith.constant dense<0> : tensor<5xi32>
  // CHECK-DAG: %[[END:.*]] = arith.constant dense<[2, 3, 0, 0, 5]> : tensor<5xi32>
  // CHECK-DAG: %[[STEP:.*]] = arith.constant dense<1> : tensor<5xi32>
  // CHECK-DAG: %[[NEW_DIMS:.*]] = arith.constant dense<[6, 1, 7, 8, 1]> : tensor<5xi32>
  // CHECK: %[[RESHAPE:.*]] = "tf.Reshape"(%arg0, %[[NEW_DIMS]]) : (tensor<6x7x8xf32>, tensor<5xi32>) -> tensor<6x1x7x8x1xf32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%[[RESHAPE]], %[[BEGIN]], %[[END]], %[[STEP]]) {begin_mask = 30 : i64, ellipsis_mask = 0 : i64, end_mask = 30 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<6x1x7x8x1xf32>, tensor<5xi32>, tensor<5xi32>, tensor<5xi32>) -> tensor<2x1x7x8x1xf32>
}

// CHECK-LABEL: @StridedSliceShrinkAxisAndNewAxisMaskBothSet
func @StridedSliceShrinkAxisAndNewAxisMaskBothSet(%arg0: tensor<6x7x8xf32>) -> tensor<1x4x1x8xf32> {
  %begin = arith.constant dense<0> : tensor<4xi32>
  %end = arith.constant dense<[2,3,4,5]> : tensor<4xi32>
  %step = arith.constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %begin, %end, %step) {
    begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 10 : i64, shrink_axis_mask = 1 : i64
  } : (tensor<6x7x8xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x4x1x8xf32>
  return %0 : tensor<1x4x1x8xf32>

  // CHECK-DAG: %[[NEW_DIMS:.*]] = arith.constant dense<[6, 1, 7, 1, 8]> : tensor<5xi32>
  // CHECK-DAG: %[[BEGIN:.*]] = arith.constant dense<0> : tensor<5xi32>
  // CHECK-DAG: %[[END:.*]] = arith.constant dense<[2, 3, 4, 5, 8]> : tensor<5xi32>
  // CHECK-DAG: %[[STEP:.*]] = arith.constant dense<1> : tensor<5xi32>
  // CHECK: %[[RESHAPE:.*]] = "tf.Reshape"(%arg0, %[[NEW_DIMS]]) : (tensor<6x7x8xf32>, tensor<5xi32>) -> tensor<6x1x7x1x8xf32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%[[RESHAPE]], %[[BEGIN]], %[[END]], %[[STEP]]) {begin_mask = 26 : i64, ellipsis_mask = 0 : i64, end_mask = 26 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 1 : i64} : (tensor<6x1x7x1x8xf32>, tensor<5xi32>, tensor<5xi32>, tensor<5xi32>) -> tensor<1x4x1x8xf32>
}

func @broadcast_to_f32_low_dim(%arg0: tensor<3xf32>, %arg1: tensor<2xi32>) -> tensor<3x3xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<3xf32>, tensor<2xi32>) -> tensor<3x3xf32>
  return %0: tensor<3x3xf32>

// CHECK-LABEL: broadcast_to_f32_low_dim
// CHECK:  [[CST:%.*]] = arith.constant dense<1.000000e+00> : tensor<3x3xf32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<3xf32>, tensor<3x3xf32>) -> tensor<3x3xf32>
// CHECK:  return [[MUL]] : tensor<3x3xf32>
}

func @broadcast_to_i32_low_dim(%input: tensor<3xi32>, %shape: tensor<2xi32>) -> tensor<3x3xi32> {
  %0 = "tf.BroadcastTo"(%input, %shape) : (tensor<3xi32>, tensor<2xi32>) -> tensor<3x3xi32>
  return %0: tensor<3x3xi32>

// CHECK-LABEL: broadcast_to_i32_low_dim
// CHECK:  [[CST:%.*]] = arith.constant dense<1> : tensor<3x3xi32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<3xi32>, tensor<3x3xi32>) -> tensor<3x3xi32>
// CHECK:  return [[MUL]] : tensor<3x3xi32>
}

func @broadcast_to_i16_low_dim(%input: tensor<3xi16>, %shape: tensor<2xi32>) -> tensor<3x3xi16> {
  %0 = "tf.BroadcastTo"(%input, %shape) : (tensor<3xi16>, tensor<2xi32>) -> tensor<3x3xi16>
  return %0: tensor<3x3xi16>

// CHECK-LABEL: broadcast_to_i16_low_dim
// CHECK:  [[CST:%.*]] = arith.constant dense<1> : tensor<3x3xi16>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<3xi16>, tensor<3x3xi16>) -> tensor<3x3xi16>
// CHECK:  return [[MUL]] : tensor<3x3xi16>
}

func @broadcast_to_low_dim_with_unknown_shape(%arg0: tensor<3xf32>, %arg1: tensor<*xi32>) -> tensor<3x3xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<3xf32>, tensor<*xi32>) -> tensor<3x3xf32>
  return %0: tensor<3x3xf32>

// CHECK-LABEL: broadcast_to_low_dim_with_unknown_shape
// CHECK:  [[CST:%.*]] = arith.constant dense<1.000000e+00> : tensor<3x3xf32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<3xf32>, tensor<3x3xf32>) -> tensor<3x3xf32>
// CHECK:  return [[MUL]] : tensor<3x3xf32>
}

func @broadcast_to_i32_low_dim_with_unknown_output(%input: tensor<3xi32>, %shape: tensor<2xi32>) -> tensor<*xi32> {
  %0 = "tf.BroadcastTo"(%input, %shape) : (tensor<3xi32>, tensor<2xi32>) -> tensor<*xi32>
  return %0: tensor<*xi32>

// CHECK-LABEL: broadcast_to_i32_low_dim_with_unknown_output
// CHECK:  [[CST:%.*]] = arith.constant dense<1> : tensor<i32>
// CHECK:  [[FILL:%.*]] = "tf.Fill"(%arg1, [[CST]]) : (tensor<2xi32>, tensor<i32>) -> tensor<*xi32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[FILL]]) : (tensor<3xi32>, tensor<*xi32>) -> tensor<*xi32>
// CHECK:  return [[MUL]] : tensor<*xi32>
}

func @broadcast_to_high_dim_with_unknown_shape(%arg0: tensor<1x2x3x4x5x6xf32>, %arg1: tensor<*xi32>) -> tensor<7x8x1x2x3x4x5x6xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<1x2x3x4x5x6xf32>, tensor<*xi32>) -> tensor<7x8x1x2x3x4x5x6xf32>
  return %0: tensor<7x8x1x2x3x4x5x6xf32>

// CHECK-LABEL: broadcast_to_high_dim_with_unknown_shape
// CHECK:  [[BCT:%.*]] = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<1x2x3x4x5x6xf32>, tensor<*xi32>) -> tensor<7x8x1x2x3x4x5x6xf32>
// CHECK:  return [[BCT]] : tensor<7x8x1x2x3x4x5x6xf32>
}

func @broadcast_to_high_dim_with_unknown_output(%arg0: tensor<1x2x3x4x5x6xf32>, %arg1: tensor<8xi32>) -> tensor<*xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<1x2x3x4x5x6xf32>, tensor<8xi32>) -> tensor<*xf32>
  return %0: tensor<*xf32>

// CHECK-LABEL: broadcast_to_high_dim_with_unknown_output
// CHECK:  [[BCT:%.*]] = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<1x2x3x4x5x6xf32>, tensor<8xi32>) -> tensor<*xf32>
// CHECK:  return [[BCT]] : tensor<*xf32>
}

func @broadcast_to_with_unknown_shape_and_output(%arg0: tensor<1x2x3x4x5x6xf32>, %arg1: tensor<*xi32>) -> tensor<*xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<1x2x3x4x5x6xf32>, tensor<*xi32>) -> tensor<*xf32>
  return %0: tensor<*xf32>

// CHECK-LABEL: broadcast_to_with_unknown_shape_and_output
// CHECK:  "tf.BroadcastTo"(%arg0, %arg1)
}

func @broadcast_to_ui32(%arg0: tensor<ui32>, %arg1: tensor<1xi64>) -> tensor<10xui32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<ui32>, tensor<1xi64>) -> tensor<10xui32>
  return %0: tensor<10xui32>

// CHECK-LABEL: broadcast_to_ui32
// CHECK:  [[CST:%.*]] = arith.constant dense<1> : tensor<10xui32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<ui32>, tensor<10xui32>) -> tensor<10xui32>
// CHECK:  return [[MUL]] : tensor<10xui32>
}

// CHECK-LABEL: xla_conv
func @xla_conv(%arg0: tensor<4x8x8x16xf32>) -> tensor<4x8x8x16xf32> {
  %0 = "tf.Const"() {value = dense<1.000000e+00> : tensor<3x3x16x16xf32>} : () -> tensor<3x3x16x16xf32> loc("Const_1")
  %1 = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32> loc("XlaConv/feature_group_count")
  %2 = "tf.Const"() {value = dense<1> : tensor<2x2xi32>} : () -> tensor<2x2xi32> loc("XlaConv/padding")
  %3 = "tf.Const"() {value = dense<1> : tensor<2xi32>} : () -> tensor<2xi32> loc("XlaConv/window_strides")
  %4 = "tf.XlaConv"(%arg0, %0, %3, %2, %3, %3, %1) {device = "", dimension_numbers = "\18\02 \032\02\00\01@\03P\03Z\02\01\02b\02\01\02", precision_config = ""} : (tensor<4x8x8x16xf32>, tensor<3x3x16x16xf32>, tensor<2xi32>, tensor<2x2xi32>, tensor<2xi32>, tensor<2xi32>, tensor<i32>) -> tensor<4x8x8x16xf32>
  return %4 : tensor<4x8x8x16xf32>
  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<0.000000e+00> : tensor<16xf32>
  // CHECK-DAG: %[[CST0:.*]] = arith.constant dense<1.000000e+00> : tensor<16x3x3x16xf32>
  // CHECK: %[[RES:.*]] = "tfl.conv_2d"(%arg0, %[[CST0]], %[[CST]]) {dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<4x8x8x16xf32>, tensor<16x3x3x16xf32>, tensor<16xf32>) -> tensor<4x8x8x16xf32>
  // CHECK: return %[[RES]]
}

func @broadcast_to_f32(%arg0: tensor<3xf32>, %arg1: tensor<2xi32>) -> tensor<3x3xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<3xf32>, tensor<2xi32>) -> tensor<3x3xf32>
  return %0: tensor<3x3xf32>

// CHECK-LABEL: broadcast_to_f32
// CHECK:  [[CST:%.*]] = arith.constant dense<1.000000e+00> : tensor<3x3xf32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<3xf32>, tensor<3x3xf32>) -> tensor<3x3xf32>
// CHECK:  return [[MUL]] : tensor<3x3xf32>
}

func @broadcast_to_i32(%input: tensor<3xi32>, %shape: tensor<2xi32>) -> tensor<3x3xi32> {
  %0 = "tf.BroadcastTo"(%input, %shape) : (tensor<3xi32>, tensor<2xi32>) -> tensor<3x3xi32>
  return %0: tensor<3x3xi32>

// CHECK-LABEL: broadcast_to_i32
// CHECK:  [[CST:%.*]] = arith.constant dense<1> : tensor<3x3xi32>
// CHECK:  [[MUL:%.*]] = "tf.Mul"(%arg0, [[CST]]) : (tensor<3xi32>, tensor<3x3xi32>) -> tensor<3x3xi32>
// CHECK:  return [[MUL]] : tensor<3x3xi32>
}

// CHECK-LABEL: lower_rfft_to_rfft2d
func @lower_rfft_to_rfft2d(%input: tensor<10x20x30xf32>, %fft_len: tensor<1xi32>) -> tensor<10x20x30xcomplex<f64>> {
  %0 = "tf.RFFT"(%input, %fft_len) : (tensor<10x20x30xf32>, tensor<1xi32>) -> tensor<10x20x30xcomplex<f64>>
  return %0: tensor<10x20x30xcomplex<f64>>

// CHECK-DAG:  %[[CST:.*]] = arith.constant dense<-2> : tensor<i32>
// CHECK-DAG:  %[[CST0:.*]] = arith.constant dense<1> : tensor<1xi32>
// CHECK-DAG:  %[[CST1:.*]] = arith.constant dense<0> : tensor<i32>
// CHECK:  %[[EXP:.*]] = "tf.ExpandDims"(%arg0, %[[CST]]) : (tensor<10x20x30xf32>, tensor<i32>) -> tensor<10x20x1x30xf32>
// CHECK:  %[[CON:.*]] = "tf.ConcatV2"(%[[CST0]], %arg1, %[[CST1]]) : (tensor<1xi32>, tensor<1xi32>, tensor<i32>) -> tensor<2xi32>
// CHECK:  %[[RFF:.*]] = "tf.RFFT2D"(%[[EXP]], %[[CON]]) : (tensor<10x20x1x30xf32>, tensor<2xi32>) -> tensor<10x20x1x30xcomplex<f64>>
// CHECK:  %[[SQE:.*]] = "tf.Squeeze"(%[[RFF]]) {squeeze_dims = [-2]} : (tensor<10x20x1x30xcomplex<f64>>) -> tensor<10x20x30xcomplex<f64>>
}

// CHECK-LABEL: xla_gather_to_slice
func @xla_gather_to_slice(%arg0 : tensor<1x9x104x768xf32>) -> tensor<*xf32> {
  %0 = "tf.Const"() {value = dense<0> : tensor<1xi32>} : () -> tensor<1xi32>
  %1 = "tf.Const"() {value = dense<[1, 9, 23, 768]> : tensor<4xi32>} : () -> tensor<4xi32>
  %2 = "tf.XlaGather"(%arg0, %0, %1) {device = "", dimension_numbers = "\0A\04\00\01\02\03\1A\01\02", indices_are_sorted = false} : (tensor<1x9x104x768xf32>, tensor<1xi32>, tensor<4xi32>) -> tensor<*xf32>
  return %2 : tensor<*xf32>

// CHECK-DAG: %[[CST:.*]] = arith.constant dense<0> : tensor<4xi64>
// CHECK-DAG: %[[CST0:.*]] = arith.constant dense<[1, 9, 23, 768]> : tensor<4xi64>
// CHECK: %[[V0:.*]] = "tf.Slice"(%arg0, %[[CST]], %[[CST0]]) : (tensor<1x9x104x768xf32>, tensor<4xi64>, tensor<4xi64>) -> tensor<*xf32>
// CHECK: return %[[V0]] : tensor<*xf32>
}

// CHECK-LABEL: DontMatchFusedBatchNormV3
func @DontMatchFusedBatchNormV3(%arg0 :tensor<?x576x1x1xf32>, %arg1 : tensor<576xf32>, %arg2 : tensor<576xf32>, %arg3 : tensor<576xf32>,%arg4 : tensor<576xf32>) -> (tensor<?x576x1x1xf32>) {
  %result:6 = "tf.FusedBatchNormV3"(%arg0, %arg1, %arg2, %arg3, %arg4) {data_format = "NHWC", device = "", epsilon = 0.001 : f32, exponential_avg_factor = 1.0 : f32, is_training = false} : (tensor<?x576x1x1xf32>, tensor<576xf32>, tensor<576xf32>, tensor<576xf32>, tensor<576xf32>) -> (tensor<?x576x1x1xf32>, tensor<576xf32>, tensor<576xf32>, tensor<576xf32>, tensor<576xf32>, tensor<*xf32>)
  return %result : tensor<?x576x1x1xf32>
  // CHECK: "tf.FusedBatchNormV3"
}

// CHECK-LABEL: DoNotConvertConv2DWhenFilterTypeDimIsNotDecided
func @DoNotConvertConv2DWhenFilterTypeDimIsNotDecided(%arg0 : tensor<?x?x?x96xf32>, %arg1 : tensor<3x3x96x?xf32>) -> tensor<?x?x?x?xf32> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 1, 1], use_cudnn_on_gpu = true} : (tensor<?x?x?x96xf32>, tensor<3x3x96x?xf32>) -> tensor<?x?x?x?xf32>
  return %0 : tensor<?x?x?x?xf32>
// CHECK: tf.Conv2D
}

// CHECK-LABEL: conv2d_f16
func @conv2d_f16(%arg0 : tensor<?x224x224x3xf16>, %arg1 : tensor<3x3x3x16xf16>) -> tensor<?x112x112x16xf16> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 2, 2, 1], use_cudnn_on_gpu = true} : (tensor<?x224x224x3xf16>, tensor<3x3x3x16xf16>) -> tensor<?x112x112x16xf16>
  return %0 : tensor<?x112x112x16xf16>
  // CHECK: "tf.Conv2D"
}

// CHECK-LABEL: fused_batch_norm_v3_f16
func @fused_batch_norm_v3_f16(%arg0 : tensor<?x112x112x16xf16>, %arg1 : tensor<16xf32>, %arg2 : tensor<16xf32>, %arg3 : tensor<16xf32>, %arg4 : tensor<16xf32>) -> tensor<?x112x112x16xf16> {
  %0, %1, %2, %3, %4, %5 = "tf.FusedBatchNormV3"(%arg0, %arg1, %arg2, %arg3, %arg4) {data_format = "NHWC", device = "", epsilon = 1.000000e-03 : f32, exponential_avg_factor = 1.000000e+00 : f32, is_training = false} : (tensor<?x112x112x16xf16>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>) -> (tensor<?x112x112x16xf16>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<*xf32>)
  return %0 : tensor<?x112x112x16xf16>
  // CHECK: "tf.FusedBatchNormV3"
}

// CHECK-LABEL: depthwise_conv2d_native_f16
func @depthwise_conv2d_native_f16(%arg0 : tensor<?x112x112x16xf16>, %arg1 : tensor<3x3x16x1xf16>) -> tensor<?x112x112x16xf16> {
  %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<?x112x112x16xf16>, tensor<3x3x16x1xf16>) -> tensor<?x112x112x16xf16>
  return %0 : tensor<?x112x112x16xf16>
  // CHECK: "tf.DepthwiseConv2dNative"
}

// CHECK-LABEL: conv_2d_bf16
func @conv_2d_bf16(%arg0 : tensor<256x32x32x3xbf16>, %arg1 : tensor<3x3x3x16xbf16>) -> tensor<256x8x7x16xbf16> {
  %0 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xbf16>, tensor<3x3x3x16xbf16>) -> tensor<256x8x7x16xbf16>
  return %0 : tensor<256x8x7x16xbf16>
  // CHECK: "tf.Conv2D"
}

// CHECK-LABEL: fused_batch_norm_v3_bf16
func @fused_batch_norm_v3_bf16(%arg0 : tensor<?x112x112x16xbf16>, %arg1 : tensor<16xf32>, %arg2 : tensor<16xf32>, %arg3 : tensor<16xf32>, %arg4 : tensor<16xf32>) -> tensor<?x112x112x16xbf16> {
  %0, %1, %2, %3, %4, %5 = "tf.FusedBatchNormV3"(%arg0, %arg1, %arg2, %arg3, %arg4) {data_format = "NHWC", device = "", epsilon = 1.000000e-03 : f32, exponential_avg_factor = 1.000000e+00 : f32, is_training = false} : (tensor<?x112x112x16xbf16>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>) -> (tensor<?x112x112x16xbf16>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<*xf32>)
  return %0 : tensor<?x112x112x16xbf16>
  // CHECK: "tf.FusedBatchNormV3"
}

// CHECK-LABEL: depthwise_conv_2d_bf16
func @depthwise_conv_2d_bf16(%arg0 : tensor<256x32x32x3xbf16>, %arg1 : tensor<3x3x3x4xf32>, %arg2 : tensor<256x3x32x32xf32>) -> tensor<256x30x30x12xbf16> {
  %0 = "tf.DepthwiseConv2dNative"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xbf16>, tensor<3x3x3x4xf32>) -> tensor<256x30x30x12xbf16>
  return %0 : tensor<256x30x30x12xbf16>
  // CHECK: "tf.DepthwiseConv2dNative"
}

// CHECK-LABEL: strided_slice_unranked_input
func @strided_slice_unranked_input(%arg0 : tensor<*xf32>) -> tensor<*xf32> {
  %18 = "tf.Const"() {value = dense<1> : tensor<4xi32>} : () -> tensor<4xi32>
  %57 = "tf.Const"() {value = dense<0> : tensor<4xi32>} : () -> tensor<4xi32>
  %534 = "tf.StridedSlice"(%arg0, %57, %57, %18) {begin_mask = 11 : i64, device = "", ellipsis_mask = 0 : i64, end_mask = 11 : i64, new_axis_mask = 4 : i64, shrink_axis_mask = 0 : i64} : (tensor<*xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<*xf32>
  return %534 : tensor<*xf32>
  // CHECK: "tf.StridedSlice"
}

func @fused_batch_norm_v3_training(%arg0 : tensor<1x1x6x2xf32>, %arg1 : tensor<2xf32>, %arg2 : tensor<2xf32>, %arg3 : tensor<2xf32>, %arg4 : tensor<2xf32>) -> tensor<1x1x6x2xf32> {
  %0, %1, %2, %3, %4, %5 = "tf.FusedBatchNormV3"(%arg0, %arg1, %arg2, %arg3, %arg4) {data_format = "NHWC", epsilon = 1.000000e-03 : f32, exponential_avg_factor = 1.000000e+00 : f32, is_training = true} : (tensor<1x1x6x2xf32>, tensor<2xf32>, tensor<2xf32>, tensor<2xf32>, tensor<2xf32>) -> (tensor<1x1x6x2xf32>, tensor<2xf32>, tensor<2xf32>, tensor<2xf32>, tensor<2xf32>, tensor<*xf32>)
  return %0 : tensor<1x1x6x2xf32>
  // CHECK-LABEL: fused_batch_norm_v3_training
  // CHECK-DAG: %[[CST:.*]] = arith.constant dense<[0, 1, 2]> : tensor<3xi32>
  // CHECK-DAG: %[[CST1:.*]] = arith.constant dense<1.000000e-03> : tensor<f32>
  // CHECK:  %[[MEAN:.*]] = "tf.Mean"(%arg0, %[[CST]]) {keep_dims = false} : (tensor<1x1x6x2xf32>, tensor<3xi32>) -> tensor<2xf32>
  // CHECK:  %[[SQ:.*]] = "tf.SquaredDifference"(%arg0, %[[MEAN]]) : (tensor<1x1x6x2xf32>, tensor<2xf32>) -> tensor<1x1x6x2xf32>
  // CHECK:  %[[MEAN0:.*]] = "tf.Mean"(%[[SQ]], %[[CST]]) {keep_dims = false} : (tensor<1x1x6x2xf32>, tensor<3xi32>) -> tensor<2xf32>
  // CHECK:  %[[ADD:.*]] = "tf.Add"(%[[MEAN0]], %[[CST1]]) : (tensor<2xf32>, tensor<f32>) -> tensor<2xf32>
  // CHECK:  %[[RSQRT:.*]] = "tf.Rsqrt"(%[[ADD]]) : (tensor<2xf32>) -> tensor<2xf32>
  // CHECK:  %[[MUL1:.*]] = "tf.Mul"(%arg1, %[[RSQRT]]) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  // CHECK:  %[[MUL2:.*]] = "tf.Mul"(%arg0, %[[MUL1]]) : (tensor<1x1x6x2xf32>, tensor<2xf32>) -> tensor<1x1x6x2xf32>
  // CHECK:  %[[MUL3:.*]] = "tf.Mul"(%[[MEAN]], %[[MUL1]]) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  // CHECK:  %[[SUB:.*]] = "tf.Sub"(%arg2, %[[MUL3]]) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  // CHECK:  %[[ADD0:.*]] = "tf.Add"(%[[MUL2]], %[[SUB]]) : (tensor<1x1x6x2xf32>, tensor<2xf32>) -> tensor<1x1x6x2xf32>
  // CHECK:  return %[[ADD0]] : tensor<1x1x6x2xf32>
}

func @scatter_nd_add(%arg0: tensor<7xi64>, %arg1: tensor<1x1xi32>, %arg2: tensor<1xi64>) -> tensor<7xi64> {
  %0 = "tf.TensorScatterAdd"(%arg0, %arg1, %arg2) : (tensor<7xi64>, tensor<1x1xi32>, tensor<1xi64>) -> tensor<7xi64>
  return %0 : tensor<7xi64>

  // CHECK-LABEL: scatter_nd_add
  // CHECK:  %[[GATHER:.*]] = "tf.GatherNd"(%arg0, %arg1) : (tensor<7xi64>, tensor<1x1xi32>) -> tensor<1xi64>
  // CHECK:  %[[ADD:.*]] = "tf.Add"(%arg2, %[[GATHER]]) : (tensor<1xi64>, tensor<1xi64>) -> tensor<1xi64>
  // CHECK:  %[[SCATTER:.*]] = "tf.TensorScatterUpdate"(%arg0, %arg1, %[[ADD]]) : (tensor<7xi64>, tensor<1x1xi32>, tensor<1xi64>) -> tensor<7xi64>
  // CHECK:  return %[[SCATTER]] : tensor<7xi64>
}

func @add_v2_uint32(%arg0: tensor<ui32>, %arg1: tensor<ui32>) -> tensor<ui32> {
  %0 = "tf.AddV2"(%arg0, %arg1) : (tensor<ui32>, tensor<ui32>) -> tensor<ui32>
  return %0 : tensor<ui32>

  // CHECK-LABEL: add_v2_uint32
  // CHECK:  %[[CAST:.*]] = "tf.Cast"(%arg0) {Truncate = false} : (tensor<ui32>) -> tensor<i32>
  // CHECK:  %[[CAST1:.*]] = "tf.Cast"(%arg1) {Truncate = false} : (tensor<ui32>) -> tensor<i32>
  // CHECK:  %[[ADD:.*]] = "tf.AddV2"(%[[CAST]], %[[CAST1]]) : (tensor<i32>, tensor<i32>) -> tensor<i32>
  // CHECK:  %[[CAST2:.*]] = "tf.Cast"(%[[ADD]]) {Truncate = false} : (tensor<i32>) -> tensor<ui32>
  // CHECK:  return %[[CAST2]] : tensor<ui32>
}

}

