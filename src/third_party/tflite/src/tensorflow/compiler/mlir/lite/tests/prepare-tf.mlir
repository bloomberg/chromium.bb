// RUN: tf-opt -tfl-prepare-tf %s | FileCheck %s

func @conv(tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>, tensor<256x3x32x32xf32>) -> (tensor<256x30x30x16xf32>, tensor<256x16x30x30xf32>, tensor<256x30x30x16xf32>, tensor<256x30x30x16xf32>, tensor<256x30x30x16xf32>) {
^bb0(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>, %arg2: tensor<256x3x32x32xf32>) :
   // OK
   %0 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
   // Unsupported data format
   %1 = "tf.Conv2D"(%arg2, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NCHW", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x3x32x32xf32>, tensor<3x3x3x16xf32>) -> tensor<256x16x30x30xf32>
   // OK
   %2 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC",                           padding = "VALID", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
   // Unsupported padding
   %3 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "EXPLICIT", strides = [1, 1, 1, 1], explicit_paddings = [0, 0, 1, 1, 1, 1, 0, 0]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
   // Unsupported strides
   %4 = "tf.Conv2D"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [2, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>

  return %0, %1, %2, %3, %4 : tensor<256x30x30x16xf32>, tensor<256x16x30x30xf32>, tensor<256x30x30x16xf32>, tensor<256x30x30x16xf32>, tensor<256x30x30x16xf32>

// CHECK-LABEL: conv
// CHECK:  %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<16xf32>
// CHECK:  %[[CONSTANT0:.*]] = constant dense<[3, 0, 1, 2]> : tensor<4xi32>
// CHECK:  %0 = "tf.Transpose"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x16xf32>, tensor<4xi32>) -> tensor<16x3x3x3xf32>
// CHECK:  %1 = "tfl.conv_2d"(%arg0, %0, %[[CONSTANT]]) {dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
// CHECK:  %2 = "tf.Conv2D"
// CHECK:  %3 = "tf.Transpose"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x16xf32>, tensor<4xi32>) -> tensor<16x3x3x3xf32>
// CHECK:  %4 = "tfl.conv_2d"(%arg0, %3, %[[CONSTANT]]) {dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<16x3x3x3xf32>, tensor<16xf32>) -> tensor<256x30x30x16xf32>
// CHECK:  %5 = "tf.Conv2D"
// CHECK:  %6 = "tf.Conv2D"
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
// CHECK:  %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<12xf32>
// CHECK:  %[[CONSTANT0:.*]] = constant dense<[1, 3, 3, 12]> : tensor<4xi32>
// CHECK:  %0 = "tf.Reshape"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x4xf32>, tensor<4xi32>) -> tensor<1x3x3x12xf32>
// CHECK:  %1 = "tfl.depthwise_conv_2d"(%arg0, %0, %[[CONSTANT]]) {depth_multiplier = 4 : i32, dilation_h_factor = 2 : i32, dilation_w_factor = 3 : i32, fused_activation_function = "NONE", padding = "SAME", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<1x3x3x12xf32>, tensor<12xf32>) -> tensor<256x30x30x12xf32>
// CHECK:  %2 = "tf.DepthwiseConv2dNative"
// CHECK:  %3 = "tf.Reshape"(%arg1, %[[CONSTANT0]]) : (tensor<3x3x3x4xf32>, tensor<4xi32>) -> tensor<1x3x3x12xf32>
// CHECK:  %4 = "tfl.depthwise_conv_2d"(%arg0, %3, %[[CONSTANT]]) {depth_multiplier = 4 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 4 : i32, stride_w = 5 : i32} : (tensor<256x32x32x3xf32>, tensor<1x3x3x12xf32>, tensor<12xf32>) -> tensor<256x30x30x12xf32>
// CHECK:  %5 = "tf.DepthwiseConv2dNative"
}

func @fusedBatchNorm(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>) {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // OK
  %0:5 = "tf.FusedBatchNorm"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Unsupported training
  %1:5 = "tf.FusedBatchNorm"( %0#0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = true}  : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Use other output
  %2:5 = "tf.FusedBatchNorm"( %1#0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)

  return %2, %2#1 : tensor<8x8x8x8xf32>, tensor<8xf32>

// CHECK-LABEL: fusedBatchNorm
// CHECK:  %[[CONSTANT:.*]] = constant dense<1.000000e-03>
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

// CHECK:  %[[BATCHNORM1_a:[^,]+]], {{.*}} = "tf.FusedBatchNorm"(%[[ADD2]], %[[ARG1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
// CHECK:  "tf.FusedBatchNorm"(%[[BATCHNORM1_a]], %[[ARG1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
}

func @fusedBatchNormV3(tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>) {
^bb0(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>, %arg3: tensor<8xf32>, %arg4: tensor<8xf32>):
  // OK
  %0:6 = "tf.FusedBatchNormV3"(%arg0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Unsupported training
  %1:6 = "tf.FusedBatchNormV3"( %0#0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = true}  : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)
  // Use other output
  %2:6 = "tf.FusedBatchNormV3"( %1#0, %arg1, %arg2, %arg3, %arg4) {T = "tfdtype$DT_FLOAT", U = "tfdtype$DT_FLOAT", data_format = "NHWC", epsilon = 0.001 : f32, is_training = false} : (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>) -> (tensor<8x8x8x8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>, tensor<8xf32>)

  return %2, %2#1 : tensor<8x8x8x8xf32>, tensor<8xf32>

// CHECK-LABEL: fusedBatchNormV3
// CHECK:  %[[CONSTANT:.*]] = constant dense<1.000000e-03>
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

// CHECK:  %[[BATCHNORM1_a:[^,]+]], {{.*}} = "tf.FusedBatchNormV3"(%[[ADD2]], %[[ARG1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
// CHECK:  "tf.FusedBatchNormV3"(%[[BATCHNORM1_a]], %[[ARG1]], %[[ARG2]], %[[ARG3]], %[[ARG4]])
}

func @batchNormWithGlobalNormalization(
    %t:tensor<1x10x10x3xf32>, %m:tensor<3xf32>, %v:tensor<3xf32>, %beta:tensor<3xf32>, %gamma:tensor<3xf32>) -> (tensor<1x10x10x3xf32>) {
  %0 = "tf.BatchNormWithGlobalNormalization"(%t, %m, %v, %beta, %gamma) {T = "tfdtype$DT_FLOAT", variance_epsilon = 0.001 : f32, scale_after_normalization = false} : (tensor<1x10x10x3xf32>, tensor<3xf32>, tensor<3xf32>, tensor<3xf32>, tensor<3xf32>) -> (tensor<1x10x10x3xf32>)
  return %0 : tensor<1x10x10x3xf32>
// CHECK-LABEL: batchNormWithGlobalNormalization
// CHECK:  %[[EPSILON:.*]] = constant dense<1.000000e-03>
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
// CHECK:  %[[EPSILON:.*]] = constant dense<1.000000e-03>
// CHECK:  %[[VARIANCE:.*]] = "tf.Add"(%[[ARG_V:.*]], %[[EPSILON]])
// CHECK:  %[[RSQRT:.*]] = "tf.Rsqrt"(%[[VARIANCE]])
// CHECK:  %[[MUL0:.*]] = "tf.Mul"(%[[RSQRT]], %[[ARG_GAMMA:.*]])
// CHECK:  %[[MUL1:.*]] = "tf.Mul"(%[[ARG_T:.*]], %[[MUL0]])
// CHECK:  %[[MUL2:.*]] = "tf.Mul"(%[[ARG_M:.*]], %[[MUL0]])
// CHECK:  %[[SUB:.*]] = "tf.Sub"(%[[ARG_BETA:.*]], %[[MUL2]])
// CHECK:  %[[RESULT:.*]] = "tf.Add"(%[[MUL1]], %[[SUB]])
// CHECK:  return %[[RESULT]]
}

// CHECK-LABEL: fakeQuantPerChannelForActivation
func @fakeQuantPerChannelForActivation(%arg0: tensor<8x3xf32>) -> (tensor<8x3xf32>) {
  %arg1 = constant dense<[0.0, -1.0, 1.0]> : tensor<3xf32>
  %arg2 = constant dense<[255.0, 254.0, 256.0]> : tensor<3xf32>
  %0 = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %arg1, %arg2) {num_bits = 3, narrow_range = false} : (tensor<8x3xf32>, tensor<3xf32>, tensor<3xf32>) -> tensor<8x3xf32>
  return %0 : tensor<8x3xf32>

// CHECK:  %[[fq:.*]] = "tf.FakeQuantWithMinMaxVarsPerChannel"(%arg0, %cst, %cst_0)
// CHECK:  %[[q:.*]] = "tfl.quantize"(%[[fq]]) {qtype = tensor<8x3x!quant.uniform<u8:f32:1, {1.000000e+00,1.000000e+00:1,1.000000e+00}>>}
// CHECK:  %[[dq:.*]] = "tfl.dequantize"(%[[q]])
// CHECK:  return %[[dq]]
}

// CHECK-LABEL: fakeQuantForActivation
func @fakeQuantForActivation(tensor<8xf32>) -> (tensor<8xf32>) {
^bb0(%arg0: tensor<8xf32>):
  %arg1 = constant dense<0.0> : tensor<f32>
  %arg2 = constant dense<255.0> : tensor<f32>
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {num_bits = 3, narrow_range = false} : (tensor<8xf32>, tensor<f32>, tensor<f32>) -> tensor<8xf32>
  return %0 : tensor<8xf32>

// CHECK:  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %cst, %cst_0)
// CHECK:  %1 = "tfl.quantize"(%0) {qtype = tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK:  %2 = "tfl.dequantize"(%1)
// CHECK:  return %2
}

// CHECK-LABEL: fakeQuantForActivationNoDuplication
func @fakeQuantForActivationNoDuplication(tensor<8xf32>) -> (tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>) {
^bb0(%arg0: tensor<8xf32>):
  %arg1 = constant dense<0.0> : tensor<f32>
  %arg2 = constant dense<255.0> : tensor<f32>
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {num_bits = 3, narrow_range = false} : (tensor<8xf32>, tensor<f32>, tensor<f32>) -> tensor<8xf32>
  %1 = "tfl.quantize"(%0) {qtype = tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>} : (tensor<8xf32>) -> tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>
  return %1 : tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>

// CHECK:  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %cst, %cst_0) {narrow_range = false, num_bits = 3 : i64}
// CHECK:  %1 = "tfl.quantize"(%0) {qtype = tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK:  return %1
}

// CHECK-LABEL: fakeQuantFolded
func @fakeQuantFolded() -> (tensor<8xf32>) {
  %in = constant dense<0.0> : tensor<8xf32>
  %min = constant dense<0.0> : tensor<f32>
  %max = constant dense<255.0> : tensor<f32>
  %mini = "tf.Identity"(%min) : (tensor<f32>) -> tensor<f32>
  %maxi = "tf.Identity"(%max) : (tensor<f32>) -> tensor<f32>
  %rst = "tf.FakeQuantWithMinMaxVars"(%in, %mini, %maxi) {num_bits = 3, narrow_range = false} : (tensor<8xf32>, tensor<f32>, tensor<f32>) -> tensor<8xf32>
  return %rst : tensor<8xf32>

// CHECK: %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<8xf32>
// CHECK: %[[QUANTIZE:.*]] = "tfl.quantize"(%[[CONSTANT]]) {qtype = tensor<8x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK: %[[DEQUANTIZE:.*]] = "tfl.dequantize"(%[[QUANTIZE]])
// CHECK: return %[[DEQUANTIZE]] : tensor<8xf32>
}

// CHECK-LABEL: fakeQuantNotFolded
func @fakeQuantNotFolded(tensor<8xf32>, tensor<f32>, tensor<f32>) -> (tensor<8xf32>) {
^bb0(%arg0: tensor<8xf32>, %arg3: tensor<f32>, %arg4: tensor<f32>):
  %1 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg3, %arg4) {num_bits = 3, narrow_range = false} : (tensor<8xf32>, tensor<f32>, tensor<f32>) -> tensor<8xf32>
  return %1 : tensor<8xf32>

// CHECK: %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2)
// CHECK: return %0 : tensor<8xf32>
}

// CHECK-LABEL: fakeQuantFollowedByTranspose
func @fakeQuantFollowedByTranspose(tensor<1x2xf32>, tensor<f32>, tensor<f32>) -> (tensor<2x1xf32>) {
^bb0(%arg0: tensor<1x2xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>):
  %cst_0 = constant dense<[1, 0]> : tensor<2xi32>
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {num_bits = 3, narrow_range = false} : (tensor<1x2xf32>, tensor<f32>, tensor<f32>) -> tensor<1x2xf32>
  %1 = "tf.Transpose"(%0, %cst_0): (tensor<1x2xf32>, tensor<2xi32>) -> tensor<2x1xf32>
  return %1 : tensor<2x1xf32>

// CHECK:  %cst = constant
// CHECK:  %0 = "tf.Transpose"(%arg0, %cst)
// CHECK:  %1 = "tf.FakeQuantWithMinMaxVars"(%0, %arg1, %arg2)
// CHECK:  return %1
}

// CHECK-LABEL: fakeQuantFollowedByReshape
func @fakeQuantFollowedByReshape(tensor<1x2xf32>, tensor<f32>, tensor<f32>) -> (tensor<2x1xf32>) {
^bb0(%arg0: tensor<1x2xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>):
  %cst_0 = constant dense<[2, -1]> : tensor<2xi64>
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {num_bits = 3, narrow_range = false} : (tensor<1x2xf32>, tensor<f32>, tensor<f32>) -> tensor<1x2xf32>
  %1 = "tf.Reshape"(%0, %cst_0) : (tensor<1x2xf32>, tensor<2xi64>) -> tensor<2x1xf32>
  return %1 : tensor<2x1xf32>

// CHECK:  %cst = constant
// CHECK:  %0 = "tf.Reshape"(%arg0, %cst)
// CHECK-SAME: tensor<2x1xf32>
// CHECK:  %1 = "tf.FakeQuantWithMinMaxVars"(%0, %arg1, %arg2)
// CHECK:  return %1
}

// CHECK-LABEL: QDQsFollowedByTranspose
func @QDQsFollowedByTranspose(tensor<1x2xf32>) -> (tensor<2x1xf32>) {
^bb0(%arg0: tensor<1x2xf32>):
  %cst_0 = constant dense<[1, 0]> : tensor<2xi32>
  %0 = "tfl.quantize"(%arg0){qtype = tensor<1x2x!quant.uniform<u8:f32, 1.0>>}: (tensor<1x2xf32>) -> (tensor<1x2x!quant.uniform<u8:f32, 1.0>>)
  %1 = "tfl.dequantize"(%0): (tensor<1x2x!quant.uniform<u8:f32, 1.0>>) -> (tensor<1x2xf32>)
  %2 = "tf.Transpose"(%1, %cst_0): (tensor<1x2xf32>, tensor<2xi32>) -> tensor<2x1xf32>
  return %2 : tensor<2x1xf32>

// CHECK: %cst = constant
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
  %cst_0 = constant dense<[2, 1]> : tensor<2xi32>
  %0 = "tfl.quantize"(%arg0){qtype = tensor<1x2x!quant.uniform<u8:f32, 1.0>>}: (tensor<1x2xf32>) -> (tensor<1x2x!quant.uniform<u8:f32, 1.0>>)
  %1 = "tfl.dequantize"(%0): (tensor<1x2x!quant.uniform<u8:f32, 1.0>>) -> (tensor<1x2xf32>)
  %2 = "tf.Reshape"(%1, %cst_0): (tensor<1x2xf32>, tensor<2xi32>) -> tensor<2x1xf32>
  return %2 : tensor<2x1xf32>

// CHECK: %cst = constant
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

// CHECK: %[[R:.*]] = constant dense<2>
// CHECK: return %cst : tensor<i32>
}

// CHECK-LABEL: fakeQuantWithConv2D
func @fakeQuantWithConv2D(tensor<256x32x32x3xf32>) -> (tensor<256x30x30x16xf32>) {
^bb0(%arg: tensor<256x32x32x3xf32>) :
  %in = constant dense<0.0> : tensor<3x3x3x16xf32>
  %min = constant dense<0.0> : tensor<f32>
  %max = constant dense<255.0> : tensor<f32>
  %mini = "tf.Identity"(%min) : (tensor<f32>) -> tensor<f32>
  %maxi = "tf.Identity"(%max) : (tensor<f32>) -> tensor<f32>
  %fq = "tf.FakeQuantWithMinMaxVars"(%in, %mini, %maxi) {num_bits = 3, narrow_range = false} : (tensor<3x3x3x16xf32>, tensor<f32>, tensor<f32>) -> tensor<3x3x3x16xf32>
  %rst = "tf.Conv2D"(%arg, %fq) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %rst : tensor<256x30x30x16xf32>

// CHECK: %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<16xf32>
// CHECK: %[[CONSTANT0:.*]] = constant dense<0.000000e+00> : tensor<16x3x3x3xf32>
// CHECK: %[[QUANTIZE:.*]] = "tfl.quantize"(%[[CONSTANT0]]) {qtype = tensor<16x3x3x3x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK: %[[DEQUANTIZE:.*]] = "tfl.dequantize"(%[[QUANTIZE]])
// CHECK: %[[CONV:.*]] = "tfl.conv_2d"(%arg0, %[[DEQUANTIZE]], %[[CONSTANT]])
// CHECK: return %[[CONV]]
}

// CHECK-LABEL: perChannelFakeQuantWithConv2D
func @perChannelFakeQuantWithConv2D(tensor<256x32x32x3xf32>) -> (tensor<256x30x30x16xf32>) {
^bb0(%arg: tensor<256x32x32x3xf32>) :
  %in = constant dense<0.0> : tensor<3x3x3x16xf32>
  %min = constant dense<0.0> : tensor<16xf32>
  %max = constant dense<255.0> : tensor<16xf32>
  %mini = "tf.Identity"(%min) : (tensor<16xf32>) -> tensor<16xf32>
  %maxi = "tf.Identity"(%max) : (tensor<16xf32>) -> tensor<16xf32>
  %fq = "tf.FakeQuantWithMinMaxVarsPerChannel"(%in, %mini, %maxi) {num_bits = 3, narrow_range = false} : (tensor<3x3x3x16xf32>, tensor<16xf32>, tensor<16xf32>) -> tensor<3x3x3x16xf32>
  %rst = "tf.Conv2D"(%arg, %fq) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %rst : tensor<256x30x30x16xf32>

// CHECK: %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<16xf32>
// CHECK: %[[CONSTANT0:.*]] = constant dense<0.000000e+00> : tensor<16x3x3x3xf32>
// CHECK: %[[QUANTIZE:.*]] = "tfl.quantize"(%[[CONSTANT0]]) {qtype = tensor<16x3x3x3x!quant.uniform<u8:f32:0,
// CHECK-SAME: {1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00}>>
// CHECK: %[[DEQUANTIZE:.*]] = "tfl.dequantize"(%[[QUANTIZE]])
// CHECK: %[[CONV:.*]] = "tfl.conv_2d"(%arg0, %[[DEQUANTIZE]], %[[CONSTANT]])
// CHECK: return %[[CONV]] : tensor<256x30x30x16xf32>
}

// CHECK-LABEL: fakeQuantWithDepthwiseConv2D
func @fakeQuantWithDepthwiseConv2D(tensor<256x32x32x3xf32>) -> (tensor<256x30x30x16xf32>) {
^bb0(%arg: tensor<256x32x32x3xf32>) :
  %in = constant dense<0.0> : tensor<3x3x3x16xf32>
  %min = constant dense<0.0> : tensor<f32>
  %max = constant dense<255.0> : tensor<f32>
  %mini = "tf.Identity"(%min) : (tensor<f32>) -> tensor<f32>
  %maxi = "tf.Identity"(%max) : (tensor<f32>) -> tensor<f32>
  %fq = "tf.FakeQuantWithMinMaxVars"(%in, %mini, %maxi) {num_bits = 3, narrow_range = false} : (tensor<3x3x3x16xf32>, tensor<f32>, tensor<f32>) -> tensor<3x3x3x16xf32>
  %rst = "tf.DepthwiseConv2dNative"(%arg, %fq) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %rst : tensor<256x30x30x16xf32>

// CHECK: %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<48xf32>
// CHECK: %[[CONSTANT0:.*]] = constant dense<0.000000e+00> : tensor<1x3x3x48xf32>
// CHECK: %[[QUANTIZE:.*]] = "tfl.quantize"(%[[CONSTANT0]]) {qtype = tensor<1x3x3x48x!quant.uniform<u8:f32, 1.000000e+00>>}
// CHECK: %[[DEQUANTIZE:.*]] = "tfl.dequantize"(%[[QUANTIZE]])
// CHECK: %[[CONV:.*]] = "tfl.depthwise_conv_2d"(%arg0, %[[DEQUANTIZE]], %[[CONSTANT]])
// CHECK: return %[[CONV]]
}

// CHECK-LABEL: perChannelFakeQuantWithDepthwiseConv2D
func @perChannelFakeQuantWithDepthwiseConv2D(tensor<256x32x32x3xf32>) -> (tensor<256x30x30x16xf32>) {
^bb0(%arg: tensor<256x32x32x3xf32>) :
  %in = constant dense<0.0> : tensor<3x3x3x16xf32>
  %min = constant dense<0.0> : tensor<16xf32>
  %max = constant dense<255.0> : tensor<16xf32>
  %mini = "tf.Identity"(%min) : (tensor<16xf32>) -> tensor<16xf32>
  %maxi = "tf.Identity"(%max) : (tensor<16xf32>) -> tensor<16xf32>
  %fq = "tf.FakeQuantWithMinMaxVarsPerChannel"(%in, %mini, %maxi) {num_bits = 3, narrow_range = false} : (tensor<3x3x3x16xf32>, tensor<16xf32>, tensor<16xf32>) -> tensor<3x3x3x16xf32>
  %rst = "tf.DepthwiseConv2dNative"(%arg, %fq) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", dilations = [1, 2, 3, 1], padding = "SAME", strides = [1, 4, 5, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<256x30x30x16xf32>
  return %rst : tensor<256x30x30x16xf32>

// CHECK: %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<48xf32>
// CHECK: %[[CONSTANT0:.*]] = constant dense<0.000000e+00> : tensor<1x3x3x48xf32>
// CHECK: %[[QUANTIZE:.*]] = "tfl.quantize"(%[[CONSTANT0]]) {qtype = tensor<1x3x3x48x!quant.uniform<u8:f32:3,
// CHECK-SAME: {1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,
// CHECK-SAME:  1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,
// CHECK-SAME:  1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00}>>}
// CHECK: %[[DEQUANTIZE:.*]] = "tfl.dequantize"(%[[QUANTIZE]])
// CHECK: %[[CONV:.*]] = "tfl.depthwise_conv_2d"(%arg0, %[[DEQUANTIZE]], %[[CONSTANT]])
// CHECK: return %[[CONV]]
}

// CHECK-LABEL: perChannelFakeQuantWithDepthwiseConv2DWithReshape
func @perChannelFakeQuantWithDepthwiseConv2DWithReshape(%arg: tensor<1x160x160x48xf32>) -> (tensor<1x160x160x48xf32>) {
  %in = constant dense<0.0> : tensor<3x3x48x1xf32>
  %min = constant dense<0.0> : tensor<48xf32>
  %max = constant dense<255.0> : tensor<48xf32>
  %mini = "tf.Identity"(%min) : (tensor<48xf32>) -> tensor<48xf32>
  %maxi = "tf.Identity"(%max) : (tensor<48xf32>) -> tensor<48xf32>
  %s1 = constant dense<[3, 3, 48]> : tensor<3xi32>
  %s2 = constant dense<[3, 3, 48, 1]> : tensor<4xi32>
  %r1 = "tf.Reshape"(%in, %s1) {T = f32, Tshape = i32, device = ""} : (tensor<3x3x48x1xf32>, tensor<3xi32>) -> tensor<3x3x48xf32>
  %fq = "tf.FakeQuantWithMinMaxVarsPerChannel"(%r1, %mini, %maxi) {num_bits = 3, narrow_range = false} : (tensor<3x3x48xf32>, tensor<48xf32>, tensor<48xf32>) -> tensor<3x3x48xf32>
  %r2 = "tf.Reshape"(%fq, %s2) {T = f32, Tshape = i32, device = ""} : (tensor<3x3x48xf32>, tensor<4xi32>) -> tensor<3x3x48x1xf32>
  %rst = "tf.DepthwiseConv2dNative"(%arg, %r2) {T = f32, data_format = "NHWC", dilations = [1, 1, 1, 1], padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<1x160x160x48xf32>, tensor<3x3x48x1xf32>) -> tensor<1x160x160x48xf32>
  return %rst : tensor<1x160x160x48xf32>

// CHECK: %[[CONSTANT:.*]] = constant dense<0.000000e+00> : tensor<48xf32>
// CHECK: %[[CONSTANT0:.*]] = constant dense<0.000000e+00> : tensor<1x3x3x48xf32>
// CHECK: %[[QUANTIZE:.*]] = "tfl.quantize"(%[[CONSTANT0]]) {qtype = tensor<1x3x3x48x!quant.uniform<u8:f32:3,
// CHECK-SAME: {1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,
// CHECK-SAME:  1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,
// CHECK-SAME:  1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00,1.000000e+00}>>}
// CHECK: %[[DEQUANTIZE:.*]] = "tfl.dequantize"(%[[QUANTIZE]])
// CHECK: %[[CONV:.*]] = "tfl.depthwise_conv_2d"(%arg0, %[[DEQUANTIZE]], %[[CONSTANT]])
// CHECK: return %[[CONV]]
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
  // CHECK: %0 = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<?xi32>
  // CHECK: %1 = "tf.Transpose"(%arg1, %0) : (tensor<1280x1000xf32>, tensor<?xi32>) -> tensor<*xf32>
  // CHECK: %2 = "tf.MatMul"(%arg0, %1) {transpose_a = false, transpose_b = true} : (tensor<1x1280xf32>, tensor<*xf32>) -> tensor<1x1000xf32>
  // CHECK: return %2 : tensor<1x1000xf32>
 }

func @matmulNoTransposeB(%arg0: tensor<1x1280xf32>, %arg1: tensor<1280x1000xf32>) -> tensor<1x1000xf32> {
  %166 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", _output_shapes = ["tfshape$dim { size = 1} dim { size = 1000}"], device = "", name = "matmul", transpose_a = true, transpose_b = false} : (tensor<1x1280xf32>, tensor<1280x1000xf32>) -> tensor<1x1000xf32>
  return %166 : tensor<1x1000xf32>

  // CHECK-LABEL: matmulNoTransposeB
  // CHECK: %0 = "tf.Const"() {value = dense<[1, 0]> : tensor<2xi32>} : () -> tensor<?xi32>
  // CHECK: %1 = "tf.Transpose"(%arg0, %0) : (tensor<1x1280xf32>, tensor<?xi32>) -> tensor<*xf32>
  // CHECK: %2 = "tf.Transpose"(%arg1, %0) : (tensor<1280x1000xf32>, tensor<?xi32>) -> tensor<*xf32>
  // CHECK: %3 = "tf.MatMul"(%1, %2) {transpose_a = false, transpose_b = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<1x1000xf32>
  // CHECK: return %3 : tensor<1x1000xf32>

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
  %cst = constant dense<0> : tensor<2xi32>
  %cst_0 = constant dense<1> : tensor<2xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 0 : i64, ellipsis_mask = 1 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<21x15x2xf32>
  return %0 : tensor<21x15x2xf32>

  // CHECK: %[[CST:.*]] = constant dense<0> : tensor<3xi32>
  // CHECK: %[[CST_0:.*]] = constant dense<1> : tensor<3xi32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST]], %[[CST_0]]) {begin_mask = 3 : i64, ellipsis_mask = 0 : i64, end_mask = 3 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<21x15x2xf32>
}

// CHECK-LABEL: @StridedSliceEllipsisMaskAfter
func @StridedSliceEllipsisMaskAfter(%arg0: tensor<21x15x7xf32>) -> tensor<5x15x7xf32> {
  %cst = constant dense<0> : tensor<2xi32>
  %cst_0 = constant dense<1> : tensor<2xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 0 : i64, ellipsis_mask = 2 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<5x15x7xf32>
  return %0 : tensor<5x15x7xf32>

  // CHECK: %[[CST:.*]] = constant dense<0> : tensor<3xi32>
  // CHECK: %[[CST_0:.*]] = constant dense<1> : tensor<3xi32>
  // CHECK: %[[STRIDED_SLICE:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST]], %[[CST_0]]) {begin_mask = 6 : i64, ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<21x15x7xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<5x15x7xf32>
}

// CHECK-LABEL: @NoPadStridedSliceNonNewAxisMask
func @NoPadStridedSliceNonNewAxisMask(%arg0: tensor<1x2x3x1xf32>) -> tensor<1x2x3x1xf32> {
  %cst = constant dense<0> : tensor<4xi32>
  %cst_0 = constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 15 : i64, ellipsis_mask = 0 : i64, end_mask = 15 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<1x2x3x1xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
  return %0 : tensor<1x2x3x1xf32>

  // CHECK: %cst = constant dense<0> : tensor<4xi32>
  // CHECK: %cst_0 = constant dense<1> : tensor<4xi32>
  // CHECK: %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 15 : i64, ellipsis_mask = 0 : i64, end_mask = 15 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<1x2x3x1xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
}

// CHECK-LABEL: @PadStridedSliceNewAxisMask1
func @PadStridedSliceNewAxisMask1(%arg0: tensor<2x3xf32>) -> tensor<1x2x3x1xf32> {
  %cst = constant dense<0> : tensor<4xi32>
  %cst_0 = constant dense<1> : tensor<4xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst, %cst_0) {begin_mask = 6 : i64, ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 9 : i64, shrink_axis_mask = 0 : i64} : (tensor<2x3xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
  return %0 : tensor<1x2x3x1xf32>

  // CHECK: %cst = constant dense<0> : tensor<4xi32>
  // CHECK: %cst_0 = constant dense<1> : tensor<4xi32>
  // CHECK: %[[cst_1:.*]] = constant dense<[1, 2, 3, 1]> : tensor<4xi32>
  // CHECK: %0 = "tf.Reshape"(%arg0, %[[cst_1]]) : (tensor<2x3xf32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
  // CHECK: %1 = "tf.StridedSlice"(%0, %cst, %cst, %cst_0) {begin_mask = 15 : i64, ellipsis_mask = 0 : i64, end_mask = 15 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<1x2x3x1xf32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>) -> tensor<1x2x3x1xf32>
}

// CHECK-LABEL: @PadStridedSliceNewAxisMask2
func @PadStridedSliceNewAxisMask2(%arg0: tensor<4x64x64x1xf32>) -> tensor<1x4x64x64xf32> {
  %cst = constant dense<0> : tensor<3xi32>
  %cst_0 = constant dense<1> : tensor<3xi32>
  %0 = "tf.Squeeze"(%arg0) {T = f32, _output_shapes = ["tfshape$dim { size: 4 } dim { size: 64 } dim { size: 64 }"], device = "", squeeze_dims = []} : (tensor<4x64x64x1xf32>) -> tensor<4x64x64xf32>
  %1 = "tf.StridedSlice"(%0, %cst, %cst, %cst_0) {Index = i32, T = f32, _output_shapes = ["tfshape$dim { size: 1 } dim { size: 4 } dim { size: 64 } dim { size: 64 }"], begin_mask = 6 : i64, device = "", ellipsis_mask = 0 : i64, end_mask = 6 : i64, new_axis_mask = 1 : i64, shrink_axis_mask = 0 : i64} : (tensor<4x64x64xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<1x4x64x64xf32>
  return %1 : tensor<1x4x64x64xf32>
}

// CHECK-LABEL: @StridedSliceRewriteMasks
func @StridedSliceRewriteMasks(%arg0: tensor<8x4x16x2xf32>) -> tensor<8x4x16x1xf32> {
  %cst = "tf.Const"() {device = "", value = dense<[1, 0, 1]> : tensor<3xi32>} : () -> tensor<3xi32>
  %cst_0 = "tf.Const"() {device = "", value = dense<[1, 0, 0]> : tensor<3xi32>} : () -> tensor<3xi32>
  %cst_1 = "tf.Const"() {device = "", value = dense<1> : tensor<3xi32>} : () -> tensor<3xi32>

  // CHECK: %[[CST:.*]] = constant dense<[1, 0, 0, 1]> : tensor<4xi32>
  // CHECK: %[[CST0:.*]] = constant dense<[1, 0, 0, 0]> : tensor<4xi32>
  // CHECK: %[[CST1:.*]] = constant dense<1> : tensor<4xi32>
  // CHECK: %[[RESULT:.*]] = "tf.StridedSlice"(%arg0, %[[CST]], %[[CST0]], %[[CST1]])
  // CHECK-SAME: begin_mask = 7 : i64
  // CHECK-SAME: ellipsis_mask = 0 : i64
  // CHECK-SAME: end_mask = 14 : i64
  // CHECK-SAME: new_axis_mask = 0 : i64
  // CHECK-SAME: shrink_axis_mask = 0 : i64

  %0 = "tf.StridedSlice"(%arg0, %cst, %cst_0, %cst_1) {begin_mask = 1 : i64, device = "", ellipsis_mask = 2 : i64, end_mask = 4 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<8x4x16x2xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<8x4x16x1xf32>
  return %0 : tensor<8x4x16x1xf32>
}

// CHECK-LABEL: @MatrixSetDiagV2Conversion
func @MatrixSetDiagV2Conversion(%arg0: tensor<3x3xi32>, %arg1: tensor<3xi32>) -> tensor<3x3xi32> {
  %cst = constant dense<0> : tensor<i32>
  %0 = "tf.MatrixSetDiagV2"(%arg0, %arg1, %cst) : (tensor<3x3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi32>
  return %0 : tensor<3x3xi32>

  // CHECK: %[[RES:.*]] = "tf.MatrixSetDiag"(%arg0, %arg1) : (tensor<3x3xi32>, tensor<3xi32>) -> tensor<3x3xi32>
  // CHECK: return %[[RES]]
}

// CHECK-LABEL: @MatrixSetDiagV2NonZeroK
func @MatrixSetDiagV2NonZeroK(%arg0: tensor<3x3xi32>, %arg1: tensor<3xi32>) -> tensor<3x3xi32> {
  %cst = constant dense<1> : tensor<i32>
  %0 = "tf.MatrixSetDiagV2"(%arg0, %arg1, %cst) : (tensor<3x3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi32>
  return %0 : tensor<3x3xi32>

  // CHECK: %[[CST:.*]] = constant dense<1> : tensor<i32>
  // CHECK: %[[RES:.*]] = "tf.MatrixSetDiagV2"(%arg0, %arg1, %[[CST]]) : (tensor<3x3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi32>
  // CHECK: return %[[RES]]
}

// CHECK-LABEL: @MatrixSetDiagV3Conversion
func @MatrixSetDiagV3Conversion(%arg0: tensor<3x3xi32>, %arg1: tensor<3xi32>) -> tensor<3x3xi32> {
  %cst = constant dense<0> : tensor<i32>
  %0 = "tf.MatrixSetDiagV3"(%arg0, %arg1, %cst) : (tensor<3x3xi32>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi32>
  return %0 : tensor<3x3xi32>

  // CHECK: %[[RES:.*]] = "tf.MatrixSetDiag"(%arg0, %arg1) : (tensor<3x3xi32>, tensor<3xi32>) -> tensor<3x3xi32>
  // CHECK: return %[[RES]]
}
