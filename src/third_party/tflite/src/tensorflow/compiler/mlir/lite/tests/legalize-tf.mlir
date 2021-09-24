// RUN: tf-opt %s -tfl-legalize-tf --cse | FileCheck %s

func @add(%arg0: tensor<1xf32>, %arg1: tensor<1xf32>) -> tensor<1xf32> {
  %0 = "tf.Add"(%arg0, %arg1) : (tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32>
  return %0: tensor<1xf32>

// CHECK-LABEL: add
// CHECK:  tfl.add %arg0, %arg1 {fused_activation_function = "NONE"} : tensor<1xf32>
// CHECK:  return
}

func @sub(%arg0: tensor<1xi64>, %arg1: tensor<1xi64>) -> tensor<1xi64> {
  %0 = "tf.Sub"(%arg0, %arg1) : (tensor<1xi64>, tensor<1xi64>) -> tensor<1xi64>
  return %0: tensor<1xi64>

// CHECK-LABEL: sub
// CHECK:  tfl.sub %arg0, %arg1 {fused_activation_function = "NONE"} : tensor<1xi64>
// CHECK:  return
}

// CHECK-LABEL: testAddHighDimsHaveSameShape
func @testAddHighDimsHaveSameShape(%arg0: tensor<1x2x3x4x5x6x7x8xi32>, %arg1: tensor<1x2x3x4x5x6x7x8xi32>) -> tensor<1x2x3x4x5x6x7x8xi32> {
  // CHECK: tfl.add %arg0, %arg1 {fused_activation_function = "NONE"}
  %0 = "tf.Add"(%arg0, %arg1) : (tensor<1x2x3x4x5x6x7x8xi32>, tensor<1x2x3x4x5x6x7x8xi32>) -> tensor<1x2x3x4x5x6x7x8xi32>
  return %0 : tensor<1x2x3x4x5x6x7x8xi32>
}

func @LeakyRelu(%arg0: tensor<1xf32>) -> tensor<1xf32> {
  %2 = "tf.LeakyRelu"(%arg0) {alpha = 0.1 : f32} : (tensor<1xf32>) -> tensor<1xf32>
  return %2: tensor<1xf32>

// CHECK-LABEL: LeakyRelu
// CHECK:  "tfl.leaky_relu"(%arg0) {alpha = 1.000000e-01 : f32} : (tensor<1xf32>) -> tensor<1xf32>
}

func @biasAdd(%arg0: tensor<1x10x10x32xf32>, %arg1: tensor<32xf32>) -> tensor<1x10x10x32xf32> {
  %0 = "tf.BiasAdd"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC"} : (tensor<1x10x10x32xf32>, tensor<32xf32>) -> tensor<1x10x10x32xf32>
  return %0 : tensor<1x10x10x32xf32>

// CHECK-LABEL: biasAdd
// CHECK: tfl.add(%arg0, %arg1) {fused_activation_function = "NONE"} : (tensor<1x10x10x32xf32>, tensor<32xf32>) -> tensor<1x10x10x32xf32>
}

func @biasAddInt(%arg0: tensor<1x10x10x32xi32>, %arg1: tensor<32xi32>) -> tensor<1x10x10x32xi32> {
  %0 = "tf.BiasAdd"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", data_format = "NHWC"} : (tensor<1x10x10x32xi32>, tensor<32xi32>) -> tensor<1x10x10x32xi32>
  return %0 : tensor<1x10x10x32xi32>

// CHECK-LABEL: biasAddInt
// CHECK:  "tf.BiasAdd"(%arg0, %arg1)
}

func @squeezeAndReshape(%arg0: tensor<1x1x10xf32>, %arg1: tensor<?x10xf32>) -> i32 {
  %0 = "tf.Squeeze"(%arg0) {squeeze_dims = [0]} : (tensor<1x1x10xf32>) -> tensor<1x10xf32>
  %1 = "tf.Squeeze"(%arg1) : (tensor<?x10xf32>) -> tensor<*xf32>
  %2 = "tf.Const"() { value = dense<[2, 5]> : tensor<2xi32> } : () -> tensor<2xi32>
  %3 = "tf.Reshape" (%0, %2) : (tensor<1x10xf32>, tensor<2xi32>) -> tensor<2x5xf32>
  %4 = "tf.some_op"(%1, %3) : (tensor<*xf32>, tensor<2x5xf32>) -> i32
  return %4 : i32
// CHECK-LABEL: squeezeAndReshape
// CHECK:  "tfl.squeeze"(%arg0) {squeeze_dims = [0]} : (tensor<1x1x10xf32>) -> tensor<1x10xf32>
// CHECK:  %1 = "tfl.squeeze"(%arg1) {squeeze_dims = []} : (tensor<?x10xf32>) -> tensor<*xf32>
// CHECK:  %cst = constant dense<[2, 5]> : tensor<2xi32>
// CHECK:  %2 = "tfl.reshape"(%0, %cst) : (tensor<1x10xf32>, tensor<2xi32>) -> tensor<2x5xf32>
// CHECK:  %3 = "tf.some_op"(%1, %2) : (tensor<*xf32>, tensor<2x5xf32>) -> i32
// CHECK:  return
}

func @dynamicReshape(%arg0: tensor<*xf32>, %arg1: tensor<2xi32>) -> tensor<?x?xf32> {
  %0 = "tf.Reshape"(%arg0, %arg1) : (tensor<*xf32>, tensor<2xi32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>

// CHECK-LABEL: dynamicReshape
// CHECK-NEXT:  %[[reshape:.*]] = "tfl.reshape"(%arg0, %arg1) : (tensor<*xf32>, tensor<2xi32>) -> tensor<?x?xf32>
// CHECK-NEXT:  return %[[reshape]] : tensor<?x?xf32>
}

func @dynamicReshapeI64(%arg0: tensor<*xf32>, %arg1: tensor<2xi64>) -> tensor<?x?xf32> {
  %0 = "tf.Reshape"(%arg0, %arg1) : (tensor<*xf32>, tensor<2xi64>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>

// CHECK-LABEL: dynamicReshapeI64
// CHECK-NEXT:  %[[cast:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
// CHECK-NEXT:  %[[reshape:.*]] = "tfl.reshape"(%arg0, %[[cast]]) : (tensor<*xf32>, tensor<2xi32>) -> tensor<?x?xf32>
// CHECK-NEXT:  return %[[reshape]] : tensor<?x?xf32>
}

func @dynamicReshapeI64Fold(%arg0: tensor<*xf32>) -> tensor<1x2xf32> {
  %cst = constant dense<[1, 2]> : tensor<2xi64>
  %0 = "tf.Reshape"(%arg0, %cst) : (tensor<*xf32>, tensor<2xi64>) -> tensor<1x2xf32>
  return %0 : tensor<1x2xf32>

// CHECK-LABEL: dynamicReshapeI64Fold
// CHECK:  %[[cst:.*]] = constant dense<[1, 2]> : tensor<2xi32>
// CHECK-NEXT:  %[[reshape:.*]] = "tfl.reshape"(%arg0, %[[cst]]) : (tensor<*xf32>, tensor<2xi32>) -> tensor<1x2xf32>
// CHECK-NEXT:  return %[[reshape]] : tensor<1x2xf32>
}

func @dynamicReshapeI64Unranked(%arg0: tensor<*xf32>, %arg1: tensor<*xi64>) -> tensor<*xf32> {
  %0 = "tf.Reshape"(%arg0, %arg1) : (tensor<*xf32>, tensor<*xi64>) -> tensor<*xf32>
  return %0 : tensor<*xf32>

// CHECK-LABEL: dynamicReshapeI64Unranked
// CHECK-NEXT:  %[[cast:.*]] = "tfl.cast"(%arg1) : (tensor<*xi64>) -> tensor<*xi32>
// CHECK-NEXT:  %[[reshape:.*]] = "tfl.reshape"(%arg0, %[[cast]]) : (tensor<*xf32>, tensor<*xi32>) -> tensor<*xf32>
// CHECK-NEXT:  return %[[reshape]] : tensor<*xf32>
}

func @avgPool2D(%arg0: tensor<1x6x6x16xf32>) -> tensor<1x1x1x16xf32> {
  // OK
  %0 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [1, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 1]} : (tensor<1x6x6x16xf32>) -> tensor<1x1x1x16xf32>
  // Unsupported data format
  %1 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NCHW", ksize = [1, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 1]} : (tensor<1x6x6x16xf32>) -> tensor<1x1x1x16xf32>
  // Unsupported ksize
  %2 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [3, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 1]} : (tensor<1x6x6x16xf32>) -> tensor<1x1x1x16xf32>
  // Unsupported strides
  %3 = "tf.AvgPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [1, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 3]} : (tensor<1x6x6x16xf32>) -> tensor<1x1x1x16xf32>

  %5 = addf %0, %1 : tensor<1x1x1x16xf32>
  %6 = addf %2, %3 : tensor<1x1x1x16xf32>
  %7 = addf %5, %6 : tensor<1x1x1x16xf32>
  return %7 : tensor<1x1x1x16xf32>

// CHECK-LABEL: func @avgPool2D
// CHECK:  "tfl.average_pool_2d"(%arg0) {filter_height = 3 : i32, filter_width = 6 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 3 : i32, stride_w = 1 : i32} : (tensor<1x6x6x16xf32>) -> tensor<1x1x1x16xf32>
// CHECK:  %1 = "tf.AvgPool"(%arg0)
// CHECK:  %2 = "tf.AvgPool"(%arg0)
// CHECK:  %3 = "tf.AvgPool"(%arg0)
}

func @softmax(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Softmax"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL: softmax
// CHECK:  "tfl.softmax"(%arg0) {beta = 1.000000e+00 : f32} : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @softplus(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Softplus"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL: softplus
// CHECK:  %[[exp:.*]] = "tfl.exp"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
// CHECK:  %[[cst:.*]] = constant dense<1.000000e+00> : tensor<f32>
// CHECK:  %[[add:.*]] = tfl.add(%[[exp]], %[[cst]]) {fused_activation_function = "NONE"} : (tensor<8x16xf32>, tensor<f32>) -> tensor<8x16xf32>
// CHECK:  %[[log:.*]] = "tfl.log"(%[[add]]) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @fakeQuantArgsFalse(%arg0: tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
  %0 = "tf.FakeQuantWithMinMaxArgs"(%arg0) {min = -0.1 : f32, max = 0.2 : f32, num_bits = 3, narrow_range = false} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>

  // CHECK-LABEL: fakeQuantArgsFalse
  // CHECK: "tfl.quantize"(%arg0) {qtype = tensor<8x8x8x8x!quant.uniform<u8:f32, 0.0011764706057660721:85>>}
  // CHECK: %1 = "tfl.dequantize"(%0) : (tensor<8x8x8x8x!quant.uniform<u8:f32, 0.0011764706057660721:85>>) -> tensor<8x8x8x8xf32>
}

func @fakeQuantArgsTrue(%arg0: tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
  %0 = "tf.FakeQuantWithMinMaxArgs"(%arg0) {min = -0.1 : f32, max = 0.2 : f32, num_bits = 3, narrow_range = true} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>

  // CHECK-LABEL: fakeQuantArgsTrue
  // CHECK: "tfl.quantize"(%arg0) {qtype = tensor<8x8x8x8x!quant.uniform<u8<1:255>:f32, 0.001181102379804521:86>>} : (tensor<8x8x8x8xf32>) -> tensor<8x8x8x8x!quant.uniform<u8<1:255>:f32, 0.001181102379804521:86>>
  // CHECK: %1 = "tfl.dequantize"(%0) : (tensor<8x8x8x8x!quant.uniform<u8<1:255>:f32, 0.001181102379804521:86>>) -> tensor<8x8x8x8xf32>
}

func @fakeQuantVarsFalse(%arg0: tensor<8x8x8x8xf32>) -> tensor<8x8x8x8xf32> {
  %arg1 = "tf.Const"() { value = dense<-0.1> : tensor<f32> } : () -> tensor<f32>
  %arg2 = "tf.Const"() { value = dense<0.2> : tensor<f32> } : () -> tensor<f32>
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {num_bits = 3, narrow_range = false} : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>

  // CHECK-LABEL: fakeQuantVarsFalse
  // CHECK: "tfl.quantize"(%arg0) {qtype = tensor<8x8x8x8x!quant.uniform<u8:f32, 0.0011764706057660721:85>>}
  // CHECK: %1 = "tfl.dequantize"(%0) : (tensor<8x8x8x8x!quant.uniform<u8:f32, 0.0011764706057660721:85>>) -> tensor<8x8x8x8xf32>
}

func @fakeQuantVarsTrue(%arg0: tensor<8x8x8x8xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>) -> tensor<8x8x8x8xf32> {
  %0 = "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {min = 0.0 : f32, max = 1.0 : f32, num_bits = 3, narrow_range = true} : (tensor<8x8x8x8xf32>, tensor<f32>, tensor<f32>) -> tensor<8x8x8x8xf32>
  return %0 : tensor<8x8x8x8xf32>

  // CHECK-LABEL: fakeQuantVarsTrue
  // CHECK: "tf.FakeQuantWithMinMaxVars"(%arg0, %arg1, %arg2) {max = 1.000000e+00 : f32, min = 0.000000e+00 : f32, narrow_range = true, num_bits = 3 : i64}
}

func @const() -> tensor<2xi32> {
  %0 = "tf.Const"() {device = "", name = "weights_quant/min", dtype = "tfdtype$DT_INT32", value = opaque<"tf", "0x746674656E736F722464747970653A2044545F494E5433320A74656E736F725F7368617065207B0A202064696D207B0A2020202073697A653A20320A20207D0A7D0A74656E736F725F636F6E74656E743A20225C3230305C3030305C3030305C3030305C3230305C3030305C3030305C303030220A"> : tensor<2xi32>} : () -> (tensor<2xi32>)
  return %0: tensor<2xi32>

// CHECK-LABEL: @const
// CHECK: "tfl.pseudo_const"() {value = opaque<"tf", "0x746674656E736F722464747970653A2044545F494E5433320A74656E736F725F7368617065207B0A202064696D207B0A2020202073697A653A20320A20207D0A7D0A74656E736F725F636F6E74656E743A20225C3230305C3030305C3030305C3030305C3230305C3030305C3030305C303030220A"> : tensor<2xi32>} : () -> tensor<2xi32>
}

func @shape(%arg0: tensor<?x1001xf32>) -> tensor<2xi32> {
  %0 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT", out_type = "tfdtype$DT_INT32"} : (tensor<?x1001xf32>) -> tensor<2xi32>
  %1 = "tf.Shape"(%arg0) {T = "tfdtype$DT_FLOAT"} : (tensor<?x1001xf32>) -> tensor<2xi32>
  %2 = "tf.Add"(%0, %1) : (tensor<2xi32>, tensor<2xi32>) -> tensor<2xi32>
  return %2: tensor<2xi32>

// CHECK-LABEL: shape
// CHECK:  "tfl.shape"(%arg0) : (tensor<?x1001xf32>) -> tensor<2xi32>
}

func @fill(%arg0: tensor<3xi32>, %arg1: tensor<f32>) -> tensor<?x?x?xf32> {
  %0 = "tf.Fill"(%arg0, %arg1) : (tensor<3xi32>, tensor<f32>) -> tensor<?x?x?xf32>
  return %0 : tensor<?x?x?xf32>

// CHECK-LABEL:fill
// CHECK:  "tfl.fill"(%arg0, %arg1) : (tensor<3xi32>, tensor<f32>) -> tensor<?x?x?xf32>
}

func @argmin(%arg0: tensor<3xi32>, %arg1: tensor<i32>) -> tensor<i32> {
  %0 = "tf.ArgMin"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<i32>
  return %0 : tensor<i32>

// CHECK-LABEL: argmin
// CHECK:  "tfl.arg_min"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<i32>
}

func @sigmoid(%arg0: tensor<?x88xf32>) -> tensor<?x88xf32> {
  %0 = "tf.Sigmoid"(%arg0) : (tensor<?x88xf32>) -> tensor<?x88xf32>
  return %0 : tensor<?x88xf32>
// CHECK-LABEL: sigmoid
// CHECK:  "tfl.logistic"(%arg0) : (tensor<?x88xf32>) -> tensor<?x88xf32>
}

func @sqrt(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Sqrt"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
// CHECK-LABEL: sqrt
// CHECK:  "tfl.sqrt"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @square(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Square"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
// CHECK-LABEL: square
// CHECK:  "tfl.square"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @neg(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Neg"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
// CHECK-LABEL: neg
// CHECK:  "tfl.neg"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @log(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Log"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
// CHECK-LABEL: log
// CHECK:  "tfl.log"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @log_softmax(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.LogSoftmax"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
// CHECK-LABEL: log_softmax
// CHECK:  "tfl.log_softmax"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @zeros_like(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.ZerosLike"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>
// CHECK-LABEL: zeros_like
// CHECK:  "tfl.zeros_like"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @div(%arg0: tensor<1xf32>, %arg1: tensor<1xf32>) -> tensor<1xf32> {
  %0 = "tf.Div"(%arg0, %arg1) : (tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32>
  return %0: tensor<1xf32>

// CHECK-LABEL: div
// CHECK:  tfl.div %arg0, %arg1 {fused_activation_function = "NONE"} : tensor<1xf32>
// CHECK:  return
}

func @squaredDifferenceRelu(tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32> {
^bb0(%arg0: tensor<1xf32>, %arg1: tensor<1xf32>):
  %0 = "tf.SquaredDifference"(%arg0, %arg1) : (tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32>
  %1 = "tf.Relu6"(%0) : (tensor<1xf32>) -> tensor<1xf32>
  return %1: tensor<1xf32>

// CHECK-LABEL: squaredDifferenceRelu
// CHECK:  tfl.squared_difference %arg0, %arg1 : tensor<1xf32>
// CHECK:  %1 = "tfl.relu6"(%0) : (tensor<1xf32>) -> tensor<1xf32>
// CHECK:  return
}

func @maxPool2D(%arg0: tensor<1x1x1x16xf32>) -> tensor<1x1x1x16xf32> {
  // OK
  %0 = "tf.MaxPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [1, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 1]} : (tensor<1x1x1x16xf32>) -> tensor<1x1x1x16xf32>

  // Unsupported data_format
  %1 = "tf.MaxPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NCHW", ksize = [1, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 1]} : (tensor<1x1x1x16xf32>) -> tensor<1x1x1x16xf32>

  // Unsupported ksize
  %2 = "tf.MaxPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [3, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 1]} : (tensor<1x1x1x16xf32>) -> tensor<1x1x1x16xf32>

  // Unsupported strides
  %3 = "tf.MaxPool"(%arg0) {T = "tfdtype$DT_FLOAT", data_format = "NHWC", ksize = [1, 3, 6, 1], padding = "VALID", strides = [1, 3, 1, 3]} : (tensor<1x1x1x16xf32>) -> tensor<1x1x1x16xf32>

  %5 = addf %0, %1 : tensor<1x1x1x16xf32>
  %6 = addf %2, %3 : tensor<1x1x1x16xf32>
  %7 = addf %5, %6 : tensor<1x1x1x16xf32>
  return %7 : tensor<1x1x1x16xf32>

// CHECK-LABEL: func @maxPool2D
// CHECK:  "tfl.max_pool_2d"(%arg0) {filter_height = 3 : i32, filter_width = 6 : i32, fused_activation_function = "NONE", padding = "VALID", stride_h = 3 : i32, stride_w = 1 : i32} : (tensor<1x1x1x16xf32>) -> tensor<1x1x1x16xf32>
// CHECK:  %1 = "tf.MaxPool"(%arg0)
// CHECK:  %2 = "tf.MaxPool"(%arg0)
// CHECK:  %3 = "tf.MaxPool"(%arg0)
}

func @abs(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Abs"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL:abs
// CHECK:  "tfl.abs"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @any(%arg0: tensor<2x2xi1>, %arg1: tensor<i32>) -> tensor<i1> {
  %0 = "tf.Any"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i32>) -> tensor<i1>
  return %0 : tensor<i1>

// CHECK-LABEL:any
// CHECK:  "tfl.reduce_any"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i32>) -> tensor<i1>
}

func @any_i64axes(%arg0: tensor<8x16x16xi1>, %arg1: tensor<2xi64>) -> tensor<?xi1> {
  %0 = "tf.Any"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xi1>, tensor<2xi64>) -> tensor<?xi1>
  return %0 : tensor<?xi1>

  // CHECK-LABEL: any_i64axes
  // CHECK: %[[V0:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
  // CHECK: "tfl.reduce_any"(%arg0, %[[V0]]) {keep_dims = false} : (tensor<8x16x16xi1>, tensor<2xi32>) -> tensor<?xi1>
}

func @ceil(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Ceil"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL: ceil
// CHECK:  "tfl.ceil"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
// CHECK:  return
}

func @cos(%arg0: tensor<f32>) -> tensor<f32> {
  %0 = "tf.Cos"(%arg0) : (tensor<f32>) -> tensor<f32>
  return %0 : tensor<f32>

// CHECK-LABEL:cos
// CHECK:  "tfl.cos"(%arg0) : (tensor<f32>) -> tensor<f32>
}

func @elu(%arg0: tensor<11x16xf32>) -> tensor<11x16xf32> {
  %0 = "tf.Elu"(%arg0) : (tensor<11x16xf32>) -> tensor<11x16xf32>
  return %0 : tensor<11x16xf32>

// CHECK-LABEL:elu
// CHECK:  "tfl.elu"(%arg0) : (tensor<11x16xf32>) -> tensor<11x16xf32>
}

func @expandDims(%arg0: tensor<2x2xf32>, %arg1: tensor<i32>) -> tensor<1x2x2xf32> {
  %0 = "tf.ExpandDims"(%arg0, %arg1) : (tensor<2x2xf32>, tensor<i32>) -> tensor<1x2x2xf32>
  return %0 : tensor<1x2x2xf32>

// CHECK-LABEL:expandDims
// CHECK:  "tfl.expand_dims"(%arg0, %arg1) : (tensor<2x2xf32>, tensor<i32>) -> tensor<1x2x2xf32>
}

func @squeezeDefault(%arg0: tensor<1x2x2xf32>) -> tensor<2x2xf32> {
  %0 = "tf.Squeeze"(%arg0) : (tensor<1x2x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>

// CHECK-LABEL:squeezeDefault
// CHECK:  "tfl.squeeze"(%arg0) {squeeze_dims = []} : (tensor<1x2x2xf32>) -> tensor<2x2xf32>
}

func @squeezeSingleAxis(%arg0: tensor<2x1x2xf32>) -> tensor<2x2xf32> {
  %0 = "tf.Squeeze"(%arg0) {squeeze_dims = [1]} : (tensor<2x1x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>

// CHECK-LABEL:squeezeSingleAxis
// CHECK:  "tfl.squeeze"(%arg0) {squeeze_dims = [1]} : (tensor<2x1x2xf32>) -> tensor<2x2xf32>
}

func @squeezeTwoAxes(%arg0: tensor<1x2x1x2xf32>) -> tensor<2x2xf32> {
  %0 = "tf.Squeeze"(%arg0) {squeeze_dims = [0, 2]} : (tensor<1x2x1x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>

// CHECK-LABEL:squeezeTwoAxes
// CHECK:  "tfl.squeeze"(%arg0) {squeeze_dims = [0, 2]} : (tensor<1x2x1x2xf32>) -> tensor<2x2xf32>
}

func @gatherScalarIndices(%arg0 : tensor<3x2xf32>, %arg1 : tensor<i32>) -> tensor<2xf32> {
  %0 = "tf.Gather"(%arg0, %arg1) : (tensor<3x2xf32>, tensor<i32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>

// CHECK-LABEL:gatherScalarIndices
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = 0 : i32, batch_dims = 0 : i32} : (tensor<3x2xf32>, tensor<i32>) -> tensor<2xf32>
}

func @gatherVectorIndices(%arg0 : tensor<2xf32>, %arg1 : tensor<3xi32>) -> tensor<3xf32> {
  %0 = "tf.Gather"(%arg0, %arg1) : (tensor<2xf32>, tensor<3xi32>) -> tensor<3xf32>
  return %0 : tensor<3xf32>

// CHECK-LABEL:gatherVectorIndices
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = 0 : i32, batch_dims = 0 : i32} : (tensor<2xf32>, tensor<3xi32>) -> tensor<3xf32>
}

func @gatherHigherRankIndices(%arg0 : tensor<2x3x6xf32>, %arg1 : tensor<4x5xi32>) -> tensor<4x5x3x6xf32> {
  %0 = "tf.Gather"(%arg0, %arg1) : (tensor<2x3x6xf32>, tensor<4x5xi32>) -> tensor<4x5x3x6xf32>
  return %0 : tensor<4x5x3x6xf32>

// CHECK-LABEL:gatherHigherRankIndices
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = 0 : i32, batch_dims = 0 : i32} : (tensor<2x3x6xf32>, tensor<4x5xi32>) -> tensor<4x5x3x6xf32>
}

func @gatherNdVectorIndices(%arg0 : tensor<3x2x2xf32>, %arg1 : tensor<2xi32>) -> tensor<2xf32> {
  %0 = "tf.GatherNd"(%arg0, %arg1) : (tensor<3x2x2xf32>, tensor<2xi32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>

// CHECK-LABEL:gatherNdVectorIndices
// CHECK:  "tfl.gather_nd"(%arg0, %arg1) : (tensor<3x2x2xf32>, tensor<2xi32>) -> tensor<2xf32>
}

func @gatherNdHigherRankIndices(%arg0 : tensor<4x3x2xf32>, %arg1 : tensor<2x2xi32>) -> tensor<2x2xf32> {
  %0 = "tf.GatherNd"(%arg0, %arg1) : (tensor<4x3x2xf32>, tensor<2x2xi32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>

// CHECK-LABEL:gatherNdHigherRankIndices
// CHECK:  "tfl.gather_nd"(%arg0, %arg1) : (tensor<4x3x2xf32>, tensor<2x2xi32>) -> tensor<2x2xf32>
}

func @scatterNdVectorIndices(%arg0: tensor<5x1xi32>, %arg1: tensor<5x3x2xf32>) -> tensor<10x3x2xf32> {
  %cst = "tf.Const"() { value = dense<[10, 3, 2]> : tensor<3xi32> } : () -> tensor<3xi32>
  %1 = "tf.ScatterNd"(%arg0, %arg1, %cst) : (tensor<5x1xi32>, tensor<5x3x2xf32>, tensor<3xi32>) -> tensor<10x3x2xf32>
  return %1 : tensor<10x3x2xf32>

// CHECK-LABEL:scatterNdVectorIndices
// CHECK: %[[CST:.*]] = constant dense<[10, 3, 2]> : tensor<3xi32>
// CHECK: %[[RES:.*]] = "tfl.scatter_nd"(%arg0, %arg1, %[[CST]]) : (tensor<5x1xi32>, tensor<5x3x2xf32>, tensor<3xi32>) -> tensor<10x3x2xf32>
// CHECK: return %[[RES]]
}

func @scatterNdHigherRankIndices(%arg0: tensor<4x2x2xi32>, %arg1: tensor<4x2x3xf32>, %arg2: tensor<3xi32>) -> tensor<10x2x3xf32> {
  %0 = "tf.ScatterNd"(%arg0, %arg1, %arg2) : (tensor<4x2x2xi32>, tensor<4x2x3xf32>, tensor<3xi32>) -> tensor<10x2x3xf32>
  return %0 : tensor<10x2x3xf32>

// CHECK-LABEL:scatterNdHigherRankIndices
// CHECK: %[[RES:.*]] = "tfl.scatter_nd"(%arg0, %arg1, %arg2) : (tensor<4x2x2xi32>, tensor<4x2x3xf32>, tensor<3xi32>) -> tensor<10x2x3xf32>
// CHECK: return %[[RES]]
}

func @scatter_nd_i64(%arg0: tensor<4x2x2xi64>, %arg1: tensor<4x2x3xf32>, %arg2: tensor<3xi64>) -> tensor<10x2x3xf32> {
  %0 = "tf.ScatterNd"(%arg0, %arg1, %arg2) : (tensor<4x2x2xi64>, tensor<4x2x3xf32>, tensor<3xi64>) -> tensor<10x2x3xf32>
  return %0 : tensor<10x2x3xf32>

// CHECK-LABEL:scatter_nd_i64
// CHECK:  "tfl.cast"
// CHECK:  "tfl.cast"
// CHECK:  "tfl.scatter_nd"
}

func @gatherV2VectorIndices(%arg0 : tensor<1x2x20xf32>, %arg1 : tensor<3x5xi32>) -> tensor<1x3x5x20xf32> {
  %0 = "tf.Const"() { value = dense<[1]> : tensor<1xi32> } : () -> tensor<1xi32>
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) : (tensor<1x2x20xf32>, tensor<3x5xi32>, tensor<1xi32>) -> tensor<1x3x5x20xf32>
  return %1 : tensor<1x3x5x20xf32>

// CHECK-LABEL:gatherV2VectorIndices
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = 1 : i32, batch_dims = 0 : i32} : (tensor<1x2x20xf32>, tensor<3x5xi32>) -> tensor<1x3x5x20xf32>
}

func @gatherV2VectorIndices_I64Axis(%arg0 : tensor<1x2x20xf32>, %arg1 : tensor<3x5xi32>) -> tensor<1x3x5x20xf32> {
  %0 = "tf.Const"() { value = dense<[1]> : tensor<1xi64> } : () -> tensor<1xi64>
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) : (tensor<1x2x20xf32>, tensor<3x5xi32>, tensor<1xi64>) -> tensor<1x3x5x20xf32>
  return %1 : tensor<1x3x5x20xf32>

// CHECK-LABEL:gatherV2VectorIndices_I64Axis
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = 1 : i32, batch_dims = 0 : i32} : (tensor<1x2x20xf32>, tensor<3x5xi32>) -> tensor<1x3x5x20xf32>
}

func @gatherV2VectorIndicesNegAxis(%arg0 : tensor<1x2x20xf32>, %arg1 : tensor<3x5xi32>) -> tensor<1x2x3x5xf32> {
  %0 = "tf.Const"() { value = dense<[-1]> : tensor<1xi32> } : () -> tensor<1xi32>
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) : (tensor<1x2x20xf32>, tensor<3x5xi32>, tensor<1xi32>) -> tensor<1x2x3x5xf32>
  return %1 : tensor<1x2x3x5xf32>

// CHECK-LABEL:gatherV2VectorIndices
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = -1 : i32, batch_dims = 0 : i32} : (tensor<1x2x20xf32>, tensor<3x5xi32>) -> tensor<1x2x3x5xf32>
}

func @gatherWithBatchDims(%arg0 : tensor<2x3x6xf32>, %arg1 : tensor<2x5xi32>) -> tensor<2x5x3x6xf32> {
  %0 = "tf.Const"() { value = dense<[1]> : tensor<1xi32> } : () -> tensor<1xi32>
  %1 = "tf.GatherV2"(%arg0, %arg1, %0) {batch_dims = 1 : i64} : (tensor<2x3x6xf32>, tensor<2x5xi32>, tensor<1xi32>) -> tensor<2x5x3x6xf32>
  return %1 : tensor<2x5x3x6xf32>

// CHECK-LABEL:gatherWithBatchDims
// CHECK:  "tfl.gather"(%arg0, %arg1) {axis = 1 : i32, batch_dims = 1 : i32} : (tensor<2x3x6xf32>, tensor<2x5xi32>) -> tensor<2x5x3x6xf32>
}



func @greater(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xi1> {
  %0 = "tf.Greater"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
  return %0 : tensor<8x16xi1>

// CHECK-LABEL: greater
// CHECK:  tfl.greater(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
// CHECK:  return
}

func @greater_equal(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xi1> {
  %0 = "tf.GreaterEqual"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
  return %0 : tensor<8x16xi1>

// CHECK-LABEL: greater_equal
// CHECK:  tfl.greater_equal(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
// CHECK:  return
}

//TODO(b/136498739): Add failure test for non-broadcastable types, since currently
// we can't catch this error.
func @less_equal(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xi1> {
  %0 = "tf.LessEqual"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
  return %0 : tensor<8x16xi1>

// CHECK-LABEL: less_equal
// CHECK:  tfl.less_equal(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
// CHECK:  return
}

func @rank(%arg0: tensor<*xf32>) -> tensor<1xi32> {
  %0 = "tf.Rank"(%arg0) : (tensor<*xf32>) -> tensor<1xi32>
  return %0 : tensor<1xi32>

// CHECK-LABEL:rank
// CHECK:  "tfl.rank"(%arg0) : (tensor<*xf32>) -> tensor<1xi32>
}

func @floor(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Floor"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL: floor
// CHECK:  "tfl.floor"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
// CHECK:  return
}

func @floor_div(tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32> {
^bb0(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>):
  %0 = "tf.FloorDiv"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL: floor_div
// CHECK:  tfl.floor_div %arg0, %arg1 : tensor<8x16xf32>
// CHECK:  return
}

func @not_equal(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xi1> {
  %0 = "tf.NotEqual"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
  return %0 : tensor<8x16xi1>

// CHECK-LABEL: not_equal
// CHECK:  tfl.not_equal(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
// CHECK:  return
}

func @select(%arg0: tensor<8xi1>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>) -> tensor<8xf32> {
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<8xi1>, tensor<8xf32>, tensor<8xf32>) -> tensor<8xf32>
  return %0: tensor<8xf32>

// CHECK-LABEL: select
// CHECK:  "tfl.select"(%arg0, %arg1, %arg2)
// CHECK:  return
}

func @select_multidim(%arg0: tensor<8xi1>, %arg1: tensor<8x3xf32>, %arg2: tensor<8x3xf32>) -> tensor<8x3xf32> {
  %0 = "tf.Select"(%arg0, %arg1, %arg2) : (tensor<8xi1>, tensor<8x3xf32>, tensor<8x3xf32>) -> tensor<8x3xf32>
  return %0: tensor<8x3xf32>

// CHECK-LABEL: select_multidim
// CHECK:  "tfl.select"(%arg0, %arg1, %arg2)
// CHECK:  return
}

func @select_v2_same_shape(%arg0: tensor<8xi1>, %arg1: tensor<8xf32>, %arg2: tensor<8xf32>) -> tensor<8xf32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<8xi1>, tensor<8xf32>, tensor<8xf32>) -> tensor<8xf32>
  return %0: tensor<8xf32>

// CHECK-LABEL: select_v2_same_shape
// CHECK:  "tfl.select"(%arg0, %arg1, %arg2)
// CHECK:  return
}

func @select_v2_multidim(%arg0: tensor<3xi1>, %arg1: tensor<8x3xf32>, %arg2: tensor<8x3xf32>) -> tensor<8x3xf32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<8x3xf32>, tensor<8x3xf32>) -> tensor<8x3xf32>
  return %0: tensor<8x3xf32>

// CHECK-LABEL: select_v2_multidim
// CHECK:  "tfl.select_v2"(%arg0, %arg1, %arg2)
// CHECK:  return
}

func @select_v2_broadcast(%arg0: tensor<4xi1>, %arg1: tensor<3x4xf32>, %arg2: tensor<8x3x4xf32>) -> tensor<8x3x4xf32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<4xi1>, tensor<3x4xf32>, tensor<8x3x4xf32>) -> tensor<8x3x4xf32>
  return %0: tensor<8x3x4xf32>

// CHECK-LABEL: select_v2_broadcast
// CHECK:  "tfl.select_v2"(%arg0, %arg1, %arg2)
// CHECK:  return
}

func @sin(%arg0: tensor<f32>) -> tensor<f32> {
  %0 = "tf.Sin"(%arg0) : (tensor<f32>) -> tensor<f32>
  return %0 : tensor<f32>

// CHECK-LABEL:sin
// CHECK:  "tfl.sin"(%arg0) : (tensor<f32>) -> tensor<f32>
}

func @topk(%arg0: tensor<8xf32>, %arg1: tensor<i32>) -> (tensor<?xf32>, tensor<?xi32>) {
  %0, %1 = "tf.TopKV2"(%arg0, %arg1) : (tensor<8xf32>, tensor<i32>) -> (tensor<?xf32>, tensor<?xi32>)
  return %0, %1: tensor<?xf32>, tensor<?xi32>

// CHECK-LABEL: topk
// CHECK:  "tfl.topk_v2"(%arg0, %arg1)
// CHECK:  return
}

func @topk_2(%arg0: tensor<8xf32>) -> (tensor<2xf32>, tensor<2xi32>) {
  %0 = "tf.Const"() { value = dense<2> : tensor<i32> } : () -> tensor<i32>
  %1:2 = "tf.TopKV2"(%arg0, %0) : (tensor<8xf32>, tensor<i32>) -> (tensor<2xf32>, tensor<2xi32>)
  return %1#0, %1#1: tensor<2xf32>, tensor<2xi32>

// CHECK-LABEL: topk_2
// CHECK:  "tfl.topk_v2"(%arg0, %cst)
// CHECK:  return
}

func @topk_3(%arg0: tensor<?x8xf32>) -> (tensor<?x2xf32>, tensor<?x2xi32>) {
  %0 = "tf.Const"() { value = dense<2> : tensor<i32> } : () -> tensor<i32>
  %1:2 = "tf.TopKV2"(%arg0, %0) : (tensor<?x8xf32>, tensor<i32>) -> (tensor<?x2xf32>, tensor<?x2xi32>)
  return %1#0, %1#1: tensor<?x2xf32>, tensor<?x2xi32>

// CHECK-LABEL: topk_3
// CHECK:  "tfl.topk_v2"(%arg0, %cst) : (tensor<?x8xf32>, tensor<i32>) -> (tensor<?x2xf32>, tensor<?x2xi32>)
// CHECK:  return
}

func @topk_4(%arg0: tensor<1x2x3x4xf32>) -> (tensor<1x2x3x2xf32>, tensor<1x2x3x2xi32>) {
  %0 = "tf.Const"() { value = dense<2> : tensor<i32> } : () -> tensor<i32>
  %1:2 = "tf.TopKV2"(%arg0, %0) : (tensor<1x2x3x4xf32>, tensor<i32>) -> (tensor<1x2x3x2xf32>, tensor<1x2x3x2xi32>)
  return %1#0, %1#1: tensor<1x2x3x2xf32>, tensor<1x2x3x2xi32>

// CHECK-LABEL: topk_4
// CHECK:  "tfl.topk_v2"(%arg0, %cst)
// CHECK:  return
}

func @topk_5(%arg0: tensor<*xf32>) -> (tensor<*xf32>, tensor<*xi32>) {
  %0 = "tf.Const"() { value = dense<2> : tensor<i32> } : () -> tensor<i32>
  %1:2 = "tf.TopKV2"(%arg0, %0) : (tensor<*xf32>, tensor<i32>) -> (tensor<*xf32>, tensor<*xi32>)
  return %1#0, %1#1: tensor<*xf32>, tensor<*xi32>

// CHECK-LABEL: topk_5
// CHECK:  "tfl.topk_v2"(%arg0, %cst)
// CHECK:  return
}

func @logicalAnd(%arg0: tensor<8xi1>, %arg1: tensor<8xi1>) -> tensor<8xi1> {
  %0 = "tf.LogicalAnd"(%arg0, %arg1) : (tensor<8xi1>, tensor<8xi1>) -> tensor<8xi1>
  return %0: tensor<8xi1>

// CHECK-LABEL: logicalAnd
// CHECK:  tfl.logical_and %arg0, %arg1 : tensor<8xi1>
// CHECK:  return
}

func @logicalNot(%arg0: tensor<8xi1>) -> tensor<8xi1> {
  %0 = "tf.LogicalNot"(%arg0) : (tensor<8xi1>) -> tensor<8xi1>
  return %0 : tensor<8xi1>
// CHECK-LABEL: logicalNot
// CHECK:  "tfl.logical_not"(%arg0) : (tensor<8xi1>) -> tensor<8xi1>
}

func @logicalOr(%arg0: tensor<8xi1>, %arg1: tensor<8xi1>) -> tensor<8xi1> {
  %0 = "tf.LogicalOr"(%arg0, %arg1) : (tensor<8xi1>, tensor<8xi1>) -> tensor<8xi1>
  return %0: tensor<8xi1>

// CHECK-LABEL: logicalOr
// CHECK:  tfl.logical_or %arg0, %arg1 : tensor<8xi1>
// CHECK:  return
}

func @addV2(%arg0: tensor<1xi32>, %arg1: tensor<1xi32>) -> tensor<1xi32> {
  %0 = "tf.AddV2"(%arg0, %arg1) : (tensor<1xi32>, tensor<1xi32>) -> tensor<1xi32>
  return %0 : tensor<1xi32>

// CHECK-LABEL: addV2
// CHECK:  tfl.add %arg0, %arg1 {fused_activation_function = "NONE"} : tensor<1xi32>
}

func @addN(%arg0: tensor<2x3xi32>, %arg1: tensor<2x3xi32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  %0 = "tf.AddN"(%arg0, %arg1, %arg2) : (tensor<2x3xi32>, tensor<2x3xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>

// CHECK-LABEL: addN
// CHECK:  "tfl.add_n"(%arg0, %arg1, %arg2) : (tensor<2x3xi32>, tensor<2x3xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
// CHECK:  return
}

func @reverse_v2(%arg0: tensor<1x2x3x4xf32>, %arg1: tensor<1xi32>) -> tensor<1x2x3x4xf32> {
  %0 = "tf.ReverseV2"(%arg0, %arg1) : (tensor<1x2x3x4xf32>, tensor<1xi32>) -> tensor<1x2x3x4xf32>
  return %0 : tensor<1x2x3x4xf32>

// CHECK-LABEL:reverse_v2
// CHECK:  "tfl.reverse_v2"(%arg0, %arg1) : (tensor<1x2x3x4xf32>, tensor<1xi32>) -> tensor<1x2x3x4xf32>
// CHECK:  return
}

func @reverse_v2_i64(%arg0: tensor<1x2x3x4xf32>, %arg1: tensor<1xi64>) -> tensor<1x2x3x4xf32> {
  %0 = "tf.ReverseV2"(%arg0, %arg1) : (tensor<1x2x3x4xf32>, tensor<1xi64>) -> tensor<1x2x3x4xf32>
  return %0 : tensor<1x2x3x4xf32>

// CHECK-LABEL:reverse_v2_i64
// CHECK:  "tfl.cast"
// CHECK:  "tfl.reverse_v2"
// CHECK:  return
}

func @matrix_diag(%arg0: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
  %0 = "tf.MatrixDiag"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16x16xf32>
  return %0 : tensor<8x16x16xf32>

// CHECK-LABEL:matrix_diag
// CHECK:  "tfl.matrix_diag"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16x16xf32>
}

func @matrix_diag_v2_no_match(%arg0: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
  // this should have been 0.
  %0 = constant dense<[1]> : tensor<1xi32>

  %1 = constant dense<[-1]> : tensor<1xi32>
  %2 = constant dense<[-1]> : tensor<1xi32>
  %3 = constant dense<[0, 0]> : tensor<2xi32>
  %4 = "tf.MatrixDiagV2"(%arg0, %0, %1, %2, %3) : (tensor<8x16xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<2xi32>) -> tensor<8x16x16xf32>
  return %4 : tensor<8x16x16xf32>

// CHECK-LABEL:   func @matrix_diag_v2_no_match(
// CHECK-SAME:                                  [[VAL_0:%.*]]: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
// CHECK:           [[VAL_1:%.*]] = constant dense<1> : tensor<1xi32>
// CHECK:           [[VAL_2:%.*]] = constant dense<-1> : tensor<1xi32>
// CHECK:           [[VAL_3:%.*]] = constant dense<0> : tensor<2xi32>
// CHECK:           [[VAL_4:%.*]] = "tf.MatrixDiagV2"([[VAL_0]], [[VAL_1]], [[VAL_2]], [[VAL_2]], [[VAL_3]]) : (tensor<8x16xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<2xi32>) -> tensor<8x16x16xf32>
// CHECK:           return [[VAL_4]] : tensor<8x16x16xf32>
}

func @matrix_diag_v2(%arg0: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
  %0 = constant dense<[0]> : tensor<1xi32>
  %1 = constant dense<[-1]> : tensor<1xi32>
  %2 = constant dense<[-1]> : tensor<1xi32>
  %3 = constant dense<[0, 0]> : tensor<2xi32>
  %4 = "tf.MatrixDiagV2"(%arg0, %0, %1, %2, %3) : (tensor<8x16xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<2xi32>) -> tensor<8x16x16xf32>
  return %4 : tensor<8x16x16xf32>

// CHECK-LABEL:   func @matrix_diag_v2(
// CHECK-SAME:                         [[VAL_5:%.*]]: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
// CHECK:           [[VAL_6:%.*]] = "tfl.matrix_diag"([[VAL_5]]) : (tensor<8x16xf32>) -> tensor<8x16x16xf32>
// CHECK:           return [[VAL_6]] : tensor<8x16x16xf32>
}

func @matrix_diag_v3_no_match(%arg0: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
  // this should have been 0.
  %0 = constant dense<[1]> : tensor<1xi32>

  %1 = constant dense<[-1]> : tensor<1xi32>
  %2 = constant dense<[-1]> : tensor<1xi32>
  %3 = constant dense<[0, 0]> : tensor<2xi32>
  %4 = "tf.MatrixDiagV3"(%arg0, %0, %1, %2, %3) : (tensor<8x16xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<2xi32>) -> tensor<8x16x16xf32>
  return %4 : tensor<8x16x16xf32>

// CHECK-LABEL:   func @matrix_diag_v3_no_match(
// CHECK-SAME:      [[VAL_0:%.*]]: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
// CHECK:           [[VAL_1:%.*]] = constant dense<1> : tensor<1xi32>
// CHECK:           [[VAL_2:%.*]] = constant dense<-1> : tensor<1xi32>
// CHECK:           [[VAL_3:%.*]] = constant dense<0> : tensor<2xi32>
// CHECK:           [[VAL_4:%.*]] = "tf.MatrixDiagV3"([[VAL_0]], [[VAL_1]], [[VAL_2]], [[VAL_2]], [[VAL_3]]) : (tensor<8x16xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<2xi32>) -> tensor<8x16x16xf32>
// CHECK:           return [[VAL_4]] : tensor<8x16x16xf32>
}

func @matrix_diag_v3(%arg0: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
  %0 = constant dense<[0]> : tensor<1xi32>
  %1 = constant dense<[-1]> : tensor<1xi32>
  %2 = constant dense<[-1]> : tensor<1xi32>
  %3 = constant dense<[0, 0]> : tensor<2xi32>
  %4 = "tf.MatrixDiagV3"(%arg0, %0, %1, %2, %3) : (tensor<8x16xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<2xi32>) -> tensor<8x16x16xf32>
  return %4 : tensor<8x16x16xf32>

// CHECK-LABEL:   func @matrix_diag_v3(
// CHECK-SAME:      [[VAL_5:%.*]]: tensor<8x16xf32>) -> tensor<8x16x16xf32> {
// CHECK:           [[VAL_6:%.*]] = "tfl.matrix_diag"([[VAL_5]]) : (tensor<8x16xf32>) -> tensor<8x16x16xf32>
// CHECK:           return [[VAL_6]] : tensor<8x16x16xf32>
}

func @matrix_set_diag_v3(%arg0: tensor<3x3xi64>, %arg1: tensor<3xi32>) -> tensor<3x3xi64> {
  %cst = constant dense<0> : tensor<i32>
  %0 = "tf.MatrixSetDiagV3"(%arg0, %arg1, %cst) {align = "RIGHT_LEFT"} : (tensor<3x3xi64>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi64>
  return %0 : tensor<3x3xi64>

// CHECK-LABEL: func @matrix_set_diag_v3
// CHECK: "tfl.matrix_set_diag"(%arg0, %arg1) : (tensor<3x3xi64>, tensor<3xi32>) -> tensor<3x3xi64>
}

func @matrix_set_diag_v3_non_zero_k(%arg0: tensor<3x3xi64>, %arg1: tensor<3xi32>) -> tensor<3x3xi64> {
  %cst = constant dense<1> : tensor<i32>
  %0 = "tf.MatrixSetDiagV3"(%arg0, %arg1, %cst) : (tensor<3x3xi64>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi64>
  return %0 : tensor<3x3xi64>

// CHECK-LABEL: @matrix_set_diag_v3_non_zero_k
// CHECK: tf.MatrixSetDiagV3
}

func @matrix_set_diag_v3_default_align(%arg0: tensor<3x3xi64>, %arg1: tensor<3xi32>) -> tensor<3x3xi64> {
  %cst = constant dense<0> : tensor<i32>
  %0 = "tf.MatrixSetDiagV3"(%arg0, %arg1, %cst) : (tensor<3x3xi64>, tensor<3xi32>, tensor<i32>) -> tensor<3x3xi64>
  return %0 : tensor<3x3xi64>

// CHECK-LABEL: @matrix_set_diag_v3_default_align
// CHECK: "tfl.matrix_set_diag"(%arg0, %arg1) : (tensor<3x3xi64>, tensor<3xi32>) -> tensor<3x3xi64>
}

func @maximum(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Maximum"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL:maximum
// CHECK:  "tfl.maximum"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @minimum(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Minimum"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL:minimum
// CHECK:  "tfl.minimum"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
}

func @realDiv(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.RealDiv"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

// CHECK-LABEL: realDiv
// CHECK:  tfl.div %arg0, %arg1 {fused_activation_function = "NONE"} : tensor<8x16xf32>
}

func @equal(%arg0: tensor<8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<8x16xi1> {
  %0 = "tf.Equal"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
  return %0 : tensor<8x16xi1>

// CHECK-LABEL: equal
// CHECK:  "tfl.equal"(%arg0, %arg1) : (tensor<8x16xf32>, tensor<8x16xf32>) -> tensor<8x16xi1>
// CHECK:  return
}

func @pad(tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32> {
^bb0(%arg0: tensor<2x1x3xf32>, %arg1: tensor<3x2xi32>):
  %0 = "tf.Pad"(%arg0, %arg1) : (tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32>
  return %0#0 : tensor<? x f32>

  // CHECK-LABEL: pad
  // CHECK:  "tfl.pad"(%arg0, %arg1) : (tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<?xf32>
  // CHECK:  return
}

func @pad_5D(tensor<2x1x3x1x1xf32>, tensor<5x2xi32>) -> tensor<? x f32> {
^bb0(%arg0: tensor<2x1x3x1x1xf32>, %arg1: tensor<5x2xi32>):
  %0 = "tf.Pad"(%arg0, %arg1) : (tensor<2x1x3x1x1xf32>, tensor<5x2xi32>) -> tensor<? x f32>
  return %0#0 : tensor<? x f32>

  // CHECK-LABEL: pad_5D
  // CHECK:  "tfl.pad"(%arg0, %arg1) : (tensor<2x1x3x1x1xf32>, tensor<5x2xi32>) -> tensor<?xf32>
  // CHECK:  return
}

func @pow(%arg0: tensor<2x1x3xf32>, %arg1: tensor<2x1x1xf32>) -> tensor<2x1x3xf32> {
  %0 = "tf.Pow"(%arg0, %arg1) : (tensor<2x1x3xf32>, tensor<2x1x1xf32>) -> tensor<2x1x3xf32>
  return %0 : tensor<2x1x3xf32>

  // CHECK-LABEL: pow
  // CHECK:  %[[pow:.*]] = tfl.pow(%arg0, %arg1) : (tensor<2x1x3xf32>, tensor<2x1x1xf32>) -> tensor<2x1x3xf32>
  // CHECK:  return
}

func @tile(tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x6xf32> {
^bb0(%arg0: tensor<2x3xf32>, %arg1: tensor<2xi32>):
  %cst = "tf.Const"() { value = dense<[1, 2]> : tensor<2xi32> } : () -> tensor<2xi32>
  %0 = "tf.Tile"(%arg0, %cst) : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x6xf32>
  return %0 : tensor<2x6xf32>

  // CHECK-LABEL: tile
  // CHECK:  "tfl.tile"(%arg0, %cst) : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x6xf32>
  // CHECK:  return
}

func @padv2(tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32> {
^bb0(%arg0: tensor<2x1x3xf32>, %arg1: tensor<3x2xi32>):
  %cst = "tf.Const"() { value = dense<2.0> : tensor<f32> } : () -> tensor<f32>
  %0 = "tf.PadV2"(%arg0, %arg1, %cst) : (tensor<2x1x3xf32>, tensor<3x2xi32>, tensor<f32>) -> tensor<? x f32>
  return %0#0 : tensor<? x f32>

  // CHECK-LABEL: padv2
  // CHECK:  "tfl.padv2"(%arg0, %arg1, %cst) : (tensor<2x1x3xf32>, tensor<3x2xi32>, tensor<f32>) -> tensor<?xf32>
  // CHECK:  return
}

func @padv2_5D(tensor<2x1x3x1x1xf32>, tensor<5x2xi32>) -> tensor<? x f32> {
^bb0(%arg0: tensor<2x1x3x1x1xf32>, %arg1: tensor<5x2xi32>):
  %cst = "tf.Const"() { value = dense<2.0> : tensor<f32> } : () -> tensor<f32>
  %0 = "tf.PadV2"(%arg0, %arg1, %cst) : (tensor<2x1x3x1x1xf32>, tensor<5x2xi32>, tensor<f32>) -> tensor<? x f32>
  return %0#0 : tensor<? x f32>

  // CHECK-LABEL: padv2_5D
  // CHECK:  "tfl.padv2"(%arg0, %arg1, %cst) : (tensor<2x1x3x1x1xf32>, tensor<5x2xi32>, tensor<f32>) -> tensor<?xf32>
  // CHECK:  return
}

func @pack2Tensors(%arg0: tensor<2xi32>, %arg1: tensor<2xi32>) -> tensor<2x2xi32> {
  %0 = "tf.Pack"(%arg0, %arg1) : (tensor<2xi32>, tensor<2xi32>) -> tensor<2x2xi32>
  return %0 : tensor<2x2xi32>

// CHECK-LABEL: pack2Tensors
// CHECK: "tfl.pack"(%arg0, %arg1) {axis = 0 : i32, values_count = 2 : i32} : (tensor<2xi32>, tensor<2xi32>) -> tensor<2x2xi32>
}

func @pack3Tensors(%arg0: tensor<2xi32>, %arg1: tensor<2xi32>, %arg2 : tensor<2xi32>) -> tensor<2x3xi32> {
  %0 = "tf.Pack"(%arg0, %arg1, %arg2) {axis = 1 : i64} : (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>

// CHECK-LABEL: pack3Tensors
// CHECK: "tfl.pack"(%arg0, %arg1, %arg2) {axis = 1 : i32, values_count = 3 : i32} : (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<2x3xi32>
}

func @packStringWithFlex(%arg0: tensor<2x!tf_type.string>, %arg1: tensor<2x!tf_type.string>) -> tensor<2x2x!tf_type.string> {
  %0 = "tf.Pack"(%arg0, %arg1) : (tensor<2x!tf_type.string>, tensor<2x!tf_type.string>) -> tensor<2x2x!tf_type.string>
  return %0 : tensor<2x2x!tf_type.string>

// CHECK-LABEL: packStringWithFlex
// CHECK: "tf.Pack"(%arg0, %arg1) : (tensor<2x!tf_type.string>, tensor<2x!tf_type.string>) -> tensor<2x2x!tf_type.string>
}

func @packNegAxis(%arg0: tensor<2xi32>, %arg1: tensor<2xi32>, %arg2 : tensor<2xi32>) -> tensor<2x3xi32> {
  %0 = "tf.Pack"(%arg0, %arg1, %arg2) {axis = -1 : i64} : (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>

// CHECK-LABEL: packNegAxis
// CHECK: "tfl.pack"(%arg0, %arg1, %arg2) {axis = -1 : i32, values_count = 3 : i32} : (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<2x3xi32>
}

func @unpack2Tensors(%arg0: tensor<2x2xi32>) -> tensor<2xi32> {
  %0:2 = "tf.Unpack"(%arg0) : (tensor<2x2xi32>) -> (tensor<2xi32>, tensor<2xi32>)
  return %0#0 : tensor<2xi32>

// CHECK-LABEL: unpack2Tensors
// CHECK: "tfl.unpack"(%arg0) {axis = 0 : i32, num = 2 : i32} : (tensor<2x2xi32>) -> (tensor<2xi32>, tensor<2xi32>)
}

func @unpack3Tensors(%arg0: tensor<2x3xi32>) -> tensor<2xi32> {
  %0:3 = "tf.Unpack"(%arg0) {axis = 1 : i64} : (tensor<2x3xi32>) -> (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>)
  return %0#0 : tensor<2xi32>

// CHECK-LABEL: unpack3Tensors
// CHECK: "tfl.unpack"(%arg0) {axis = 1 : i32, num = 3 : i32} : (tensor<2x3xi32>) -> (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>)
}

func @unpackNegAxis(%arg0: tensor<2x3xi32>) -> tensor<2xi32> {
  %0:3 = "tf.Unpack"(%arg0) {axis = -1 : i64} : (tensor<2x3xi32>) -> (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>)
  return %0#0 : tensor<2xi32>

// CHECK-LABEL: unpackNegAxis
// CHECK: "tfl.unpack"(%arg0) {axis = -1 : i32, num = 3 : i32} : (tensor<2x3xi32>) -> (tensor<2xi32>, tensor<2xi32>, tensor<2xi32>)
}

func @mean(%arg0: tensor<2x2xf32>, %arg1: tensor<1xi32>) -> tensor<1x2xf32> {
  %0 = "tf.Mean"(%arg0, %arg1) : (tensor<2x2xf32>, tensor<1xi32>) -> tensor<1x2xf32>
  return %0 : tensor<1x2xf32>

// CHECK-LABEL: mean
// CHECK:  "tfl.mean"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xf32>, tensor<1xi32>) -> tensor<1x2xf32>
}

func @mean_true(%arg0: tensor<2x2xf32>, %arg1: tensor<1xi32>) -> tensor<1x2xf32> {
  %0 = "tf.Mean"(%arg0, %arg1) {keep_dims = true} : (tensor<2x2xf32>, tensor<1xi32>) -> tensor<1x2xf32>
  return %0 : tensor<1x2xf32>

// CHECK-LABEL: mean_true
// CHECK:  "tfl.mean"(%arg0, %arg1) {keep_dims = true} : (tensor<2x2xf32>, tensor<1xi32>) -> tensor<1x2xf32>
}

func @sum(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Sum"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: sum
  // CHECK: "tfl.sum"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @sum_true(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Sum"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: sum_true
  // CHECK: "tfl.sum"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @sum_i64axes(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi64>) -> tensor<?xf32> {
  %0 = "tf.Sum"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi64>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: sum_i64axes
  // CHECK: %[[V0:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
  // CHECK: "tfl.sum"(%arg0, %[[V0]]) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_min(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Min"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_min
  // CHECK: "tfl.reduce_min"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_min_true(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Min"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_min_true
  // CHECK: "tfl.reduce_min"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_min_i64axes(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi64>) -> tensor<?xf32> {
  %0 = "tf.Min"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi64>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_min_i64axes
  // CHECK: %[[V0:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
  // CHECK: "tfl.reduce_min"(%arg0, %[[V0]]) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_max(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Max"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_max
  // CHECK: "tfl.reduce_max"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_max_true(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Max"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_max_true
  // CHECK: "tfl.reduce_max"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_max_i64axes(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi64>) -> tensor<?xf32> {
  %0 = "tf.Max"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi64>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_max_i64axes
  // CHECK: %[[V0:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
  // CHECK: "tfl.reduce_max"(%arg0, %[[V0]]) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_prod(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Prod"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_prod
  // CHECK: "tfl.reduce_prod"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_prod_true(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi32>) -> tensor<?xf32> {
  %0 = "tf.Prod"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_prod_true
  // CHECK: "tfl.reduce_prod"(%arg0, %arg1) {keep_dims = true} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @reduce_prod_i64axes(%arg0: tensor<8x16x16xf32>, %arg1: tensor<2xi64>) -> tensor<?xf32> {
  %0 = "tf.Prod"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi64>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: reduce_prod_i64axes
  // CHECK: %[[V0:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
  // CHECK: "tfl.reduce_prod"(%arg0, %[[V0]]) {keep_dims = false} : (tensor<8x16x16xf32>, tensor<2xi32>) -> tensor<?xf32>
}

func @batch_to_space_nd(%arg0: tensor<4x2x2x3xf32>, %arg1: tensor<2xi32>, %arg2: tensor<2x2xi32>) -> tensor<?xf32> {
  %0 = "tf.BatchToSpaceND"(%arg0, %arg1, %arg2) : (tensor<4x2x2x3xf32>, tensor<2xi32>, tensor<2x2xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
  // CHECK-LABEL: batch_to_space_nd
  // CHECK: "tfl.batch_to_space_nd"(%arg0, %arg1, %arg2) : (tensor<4x2x2x3xf32>, tensor<2xi32>, tensor<2x2xi32>) -> tensor<?xf32>
}

func @batch_to_space_nd_unsupported(%arg0: tensor<?x1x1x1x4xf32>, %arg1: tensor<3xi32>, %arg2: tensor<3x2xi32>) -> tensor<?x3x3x3x4xf32> {
  %0 = "tf.BatchToSpaceND"(%arg0, %arg1, %arg2) : (tensor<?x1x1x1x4xf32>, tensor<3xi32>, tensor<3x2xi32>) -> tensor<?x3x3x3x4xf32>
  return %0 : tensor<?x3x3x3x4xf32>
  // CHECK-LABEL: batch_to_space_nd_unsupported
  // CHECK: "tf.BatchToSpaceND"
}

func @batch_to_space_nd_i64(%arg0: tensor<4x2x2x3xf32>, %arg1: tensor<2xi64>, %arg2: tensor<2x2xi64>) -> tensor<?xf32> {
  %0 = "tf.BatchToSpaceND"(%arg0, %arg1, %arg2) : (tensor<4x2x2x3xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
  // CHECK-LABEL: batch_to_space_nd_i64
  // CHECK: "tfl.cast"
  // CHECK: "tfl.cast"
  // CHECK: "tfl.batch_to_space_nd"
}

func @space_to_batch_nd(%arg0: tensor<1x4x4x3xf32>, %arg1: tensor<2xi32>, %arg2: tensor<2x2xi32>) -> tensor<*xf32> {
  %0 = "tf.SpaceToBatchND"(%arg0, %arg1, %arg2) : (tensor<1x4x4x3xf32>, tensor<2xi32>, tensor<2x2xi32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
  // CHECK-LABEL: space_to_batch_nd
  // CHECK: "tfl.space_to_batch_nd"(%arg0, %arg1, %arg2) : (tensor<1x4x4x3xf32>, tensor<2xi32>, tensor<2x2xi32>) -> tensor<*xf32>
}

func @space_to_batch_nd_i64(%arg0: tensor<1x4x4x3xf32>, %arg1: tensor<2xi64>, %arg2: tensor<2x2xi64>) -> tensor<*xf32> {
  %0 = "tf.SpaceToBatchND"(%arg0, %arg1, %arg2) : (tensor<1x4x4x3xf32>, tensor<2xi64>, tensor<2x2xi64>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
  // CHECK-LABEL: space_to_batch_nd_i64
  // CHECK: "tfl.cast"
  // CHECK: "tfl.cast"
  // CHECK: "tfl.space_to_batch_nd"
}

func @split(%arg0: tensor<i32>, %arg1: tensor<1x4x3x3xf32>) -> tensor<1x4x3xf32> {
  %0:3 = "tf.Split"(%arg0, %arg1) : (tensor<i32>, tensor<1x4x3x3xf32>) -> (tensor<1x4x3xf32>, tensor<1x4x3xf32>, tensor<1x4x3xf32>)
  return %0#0 : tensor<1x4x3xf32>

  // CHECK-LABEL: split
  // CHECK: "tfl.split"(%arg0, %arg1) {num_splits = 3 : i32} : (tensor<i32>, tensor<1x4x3x3xf32>) -> (tensor<1x4x3xf32>, tensor<1x4x3xf32>, tensor<1x4x3xf32>)
}

func @splitv(%arg0: tensor<1x4x3x3xf32>, %arg1: tensor<2xi32>, %arg2: tensor<i32>) -> tensor<1x4x2x3xf32> {
  %0:2 = "tf.SplitV"(%arg0, %arg1, %arg2) : (tensor<1x4x3x3xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<1x4x2x3xf32>, tensor<1x4x1x3xf32>)
  return %0#0 : tensor<1x4x2x3xf32>

  // CHECK-LABEL: splitv
  // CHECK: "tfl.split_v"(%arg0, %arg1, %arg2) {num_splits = 2 : i32} : (tensor<1x4x3x3xf32>, tensor<2xi32>, tensor<i32>) -> (tensor<1x4x2x3xf32>, tensor<1x4x1x3xf32>)
}

func @matmul(%arg0: tensor<40x37xf32>, %arg1: tensor<37x40xf32>) -> tensor<40x40xf32> {
  %0 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", transpose_a = false, transpose_b = false} :
(tensor<40x37xf32>, tensor<37x40xf32>) -> tensor<40x40xf32>
  return %0 : tensor<40x40xf32>
// CHECK-LABEL: matmul
// CHECK: %[[CST:.*]] = constant dense<[1, 0]> : tensor<2xi32>
// CHECK: %[[ARG:.*]] = "tfl.transpose"(%arg1, %[[CST]]) : (tensor<37x40xf32>, tensor<2xi32>) -> tensor<40x37xf32>
// CHECK: %[[CST_0:.*]] = constant unit
// CHECK: "tfl.fully_connected"(%arg0, %[[ARG]], %[[CST_0]]) {fused_activation_function = "NONE", keep_num_dims = false, weights_format = "DEFAULT"} : (tensor<40x37xf32>, tensor<40x37xf32>, none) -> tensor<40x40xf32>
}

func @matmul_transposed_a(%arg0: tensor<37x40xf32>, %arg1: tensor<37x40xf32>) -> tensor<40x40xf32> {
  %0 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", transpose_a = true, transpose_b = false} :
(tensor<37x40xf32>, tensor<37x40xf32>) -> tensor<40x40xf32>
  return %0 : tensor<40x40xf32>
// CHECK-LABEL: matmul_transposed_a
// CHECK: %[[CST_0:.*]] = constant dense<[1, 0]> : tensor<2xi32>
// CHECK: %[[ARG_0:.*]] = "tfl.transpose"(%arg0, %[[CST_0]]) : (tensor<37x40xf32>, tensor<2xi32>) -> tensor<40x37xf32>
// CHECK: %[[ARG_1:.*]] = "tfl.transpose"(%arg1, %[[CST_0]]) : (tensor<37x40xf32>, tensor<2xi32>) -> tensor<40x37xf32>
// CHECK: %[[CST_2:.*]] = constant unit
// CHECK: "tfl.fully_connected"(%[[ARG_0]], %[[ARG_1]], %[[CST_2]]) {fused_activation_function = "NONE", keep_num_dims = false, weights_format = "DEFAULT"} : (tensor<40x37xf32>, tensor<40x37xf32>, none) -> tensor<40x40xf32>
}

func @matmul_transposed_b(%arg0: tensor<40x37xf32>, %arg1: tensor<40x37xf32>) -> tensor<40x40xf32> {
  %0 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", transpose_a = false, transpose_b = true} :
(tensor<40x37xf32>, tensor<40x37xf32>) -> tensor<40x40xf32>
  return %0 : tensor<40x40xf32>
// CHECK-LABEL: matmul_transposed_b
// CHECK: "tfl.fully_connected"(%arg0, %arg1, %cst) {fused_activation_function = "NONE", keep_num_dims = false, weights_format = "DEFAULT"} : (tensor<40x37xf32>, tensor<40x37xf32>, none) -> tensor<40x40xf32>
}

func @matmul_transposed_ab(%arg0: tensor<37x40xf32>, %arg1: tensor<40x37xf32>) -> tensor<40x40xf32> {
  %0 = "tf.MatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", transpose_a = true, transpose_b = true} :
(tensor<37x40xf32>, tensor<40x37xf32>) -> tensor<40x40xf32>
  return %0 : tensor<40x40xf32>
// CHECK-LABEL: matmul_transposed_ab
// CHECK: %[[CST_0:.*]] = constant dense<[1, 0]> : tensor<2xi32>
// CHECK: %[[ARG_0:.*]] = "tfl.transpose"(%arg0, %[[CST_0]]) : (tensor<37x40xf32>, tensor<2xi32>) -> tensor<40x37xf32>
// CHECK: %[[CST_1:.*]] = constant unit
// CHECK: "tfl.fully_connected"(%[[ARG_0]], %arg1, %[[CST_1]]) {fused_activation_function = "NONE", keep_num_dims = false, weights_format = "DEFAULT"} : (tensor<40x37xf32>, tensor<40x37xf32>, none) -> tensor<40x40xf32>
}

func @concat_v2_with_3_tensors(%arg0: tensor<2x1xi32>, %arg1: tensor<2x1xi32>, %arg2: tensor<2x1xi32>) -> tensor<2x3xi32> {
  %0 = "tf.Const"() { value = dense<-1> : tensor<i32> } : () -> tensor<i32>
  %1 = "tf.ConcatV2"(%arg0, %arg1, %arg2, %0) : (tensor<2x1xi32>, tensor<2x1xi32>, tensor<2x1xi32>, tensor<i32>) -> tensor<2x3xi32>
  return %1 : tensor<2x3xi32>

// CHECK-LABEL: concat_v2_with_3_tensors
// CHECK: "tfl.concatenation"(%arg0, %arg1, %arg2) {axis = -1 : i32, fused_activation_function = "NONE"} : (tensor<2x1xi32>, tensor<2x1xi32>, tensor<2x1xi32>) -> tensor<2x3xi32>
}

func @concat_v2_i64_axis(%arg0: tensor<2x1xi32>, %arg1: tensor<2x1xi32>, %arg2: tensor<2x1xi32>) -> tensor<2x3xi32> {
  %0 = "tf.Const"() { value = dense<-1> : tensor<i64> } : () -> tensor<i64>
  %1 = "tf.ConcatV2"(%arg0, %arg1, %arg2, %0) : (tensor<2x1xi32>, tensor<2x1xi32>, tensor<2x1xi32>, tensor<i64>) -> tensor<2x3xi32>
  return %1 : tensor<2x3xi32>

// CHECK-LABEL: concat_v2_i64_axis
// CHECK: "tfl.concatenation"(%arg0, %arg1, %arg2) {axis = -1 : i32, fused_activation_function = "NONE"} : (tensor<2x1xi32>, tensor<2x1xi32>, tensor<2x1xi32>) -> tensor<2x3xi32>
}

func @concat_v2_with_bool_type(%arg0: tensor<?x1xi1>, %arg1: tensor<?x1xi1>) -> tensor<?x2xi1> {
  %0 = "tf.Const"() { value = dense<-1> : tensor<i32> } : () -> tensor<i32>
  %1 = "tf.ConcatV2"(%arg0, %arg1, %0) : (tensor<?x1xi1>, tensor<?x1xi1>, tensor<i32>) -> tensor<?x2xi1>
  return %1 : tensor<?x2xi1>

// CHECK-LABEL: concat_v2_with_bool_type
// CHECK: "tfl.concatenation"(%arg0, %arg1) {axis = -1 : i32, fused_activation_function = "NONE"} : (tensor<?x1xi1>, tensor<?x1xi1>) -> tensor<?x2xi1>
}

func @resize_with_bilinear(%arg0: tensor<1x100x100x3xf32>, %arg1: tensor<4xi32>) -> tensor<?xf32> {
  %0 = "tf.ResizeBilinear"(%arg0, %arg1) {align_corners = true} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
  // CHECK-LABEL: resize_with_bilinear
  // CHECK: "tfl.resize_bilinear"(%arg0, %arg1) {align_corners = true, half_pixel_centers = false} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
}

func @resize_with_bilinear_with_half_pixel_centers(%arg0: tensor<1x100x100x3xf32>, %arg1: tensor<4xi32>) -> tensor<?xf32> {
  %0 = "tf.ResizeBilinear"(%arg0, %arg1) {align_corners = false, half_pixel_centers = true} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
  // CHECK-LABEL: resize_with_bilinear_with_half_pixel_centers
  // CHECK: "tfl.resize_bilinear"(%arg0, %arg1) {align_corners = false, half_pixel_centers = true} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
}

func @strided_slice(%arg0: tensor<12x2x2x5xf32>, %arg1: tensor<1xi32>, %arg2: tensor<1xi32>, %arg3: tensor<1xi32>) -> tensor<1x2x2x5xf32> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
  return %0 : tensor<1x2x2x5xf32>
  // CHECK-LABEL: strided_slice
  // CHECK: "tfl.strided_slice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 0 : i32} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
}

func @strided_slice_with_constant_attributes(%arg0: tensor<10x10x10xf32>, %arg1: tensor<1xi32>, %arg2: tensor<1xi32>, %arg3: tensor<1xi32>) -> tensor<10x10xf32> {
  %cst = constant dense<-1> : tensor<1xi32>
  %cst_1 = constant dense<0> : tensor<1xi32>
  %cst_2 = constant dense<1> : tensor<1xi32>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst_1, %cst_2) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 1 : i64} : (tensor<10x10x10xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<10x10xf32>
  return %0 : tensor<10x10xf32>
  // CHECK-LABEL: strided_slice_with_constant_attributes
  // CHECK-DAG: [[BEGIN:%cst.*]] = constant dense<-1> : tensor<1xi32>
  // CHECK-DAG: [[END:%cst.*]] = constant dense<0> : tensor<1xi32>
  // CHECK-DAG: [[STRIDES:%cst.*]] = constant dense<1> : tensor<1xi32>
  // CHECK-NEXT: "tfl.strided_slice"(%arg0, [[BEGIN]], [[END]], [[STRIDES]]) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 1 : i32} : (tensor<10x10x10xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<10x10xf32>
}

func @strided_slice_with_string(%arg0: tensor<12x2x2x5x!tf_type.string>, %arg1: tensor<1xi32>, %arg2: tensor<1xi32>, %arg3: tensor<1xi32>) -> tensor<1x2x2x5x!tf_type.string> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<12x2x2x5x!tf_type.string>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5x!tf_type.string>
  return %0 : tensor<1x2x2x5x!tf_type.string>
  // CHECK-LABEL: strided_slice_with_string
  // CHECK: "tfl.strided_slice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 0 : i32} : (tensor<12x2x2x5x!tf_type.string>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5x!tf_type.string>
}

func @strided_slice_with_unranked_input_and_i64_parameters(%arg0: tensor<*xf32>, %arg1: tensor<1xi64>, %arg2: tensor<1xi64>, %arg3: tensor<1xi64>) -> tensor<*xf32> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<*xf32>, tensor<1xi64>, tensor<1xi64>, tensor<1xi64>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
  // CHECK-LABEL: strided_slice_with_unranked_input_and_i64_parameters
  // CHECK-DAG: [[BEGIN:%.*]] = "tfl.cast"(%arg1) : (tensor<1xi64>) -> tensor<1xi32>
  // CHECK-DAG: [[END:%.*]] = "tfl.cast"(%arg2) : (tensor<1xi64>) -> tensor<1xi32>
  // CHECK-DAG: [[STRIDES:%.*]] = "tfl.cast"(%arg3) : (tensor<1xi64>) -> tensor<1xi32>
  // CHECK-NEXT: "tfl.strided_slice"(%arg0, [[BEGIN]], [[END]], [[STRIDES]]) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 0 : i32} : (tensor<*xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<*xf32>
}

func @strided_slice_with_i64_parameters(%arg0: tensor<12x2x2x5xf32>, %arg1: tensor<1xi64>, %arg2: tensor<1xi64>, %arg3: tensor<1xi64>) -> tensor<1x2x2x5xf32> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<12x2x2x5xf32>, tensor<1xi64>, tensor<1xi64>, tensor<1xi64>) -> tensor<1x2x2x5xf32>
  return %0 : tensor<1x2x2x5xf32>
  // CHECK-LABEL: strided_slice_with_i64_parameters
  // CHECK-DAG: [[BEGIN:%.*]] = "tfl.cast"(%arg1) : (tensor<1xi64>) -> tensor<1xi32>
  // CHECK-DAG: [[END:%.*]] = "tfl.cast"(%arg2) : (tensor<1xi64>) -> tensor<1xi32>
  // CHECK-DAG: [[STRIDES:%.*]] = "tfl.cast"(%arg3) : (tensor<1xi64>) -> tensor<1xi32>
  // CHECK-NEXT: "tfl.strided_slice"(%arg0, [[BEGIN]], [[END]], [[STRIDES]]) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 0 : i32} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
}

func @strided_slice_with_i64_constant_attributes(%arg0: tensor<10x10x10xf32>) -> tensor<10x10xf32> {
  %cst = constant dense<-1> : tensor<1xi64>
  %cst_1 = constant dense<0> : tensor<1xi64>
  %cst_2 = constant dense<1> : tensor<1xi64>
  %0 = "tf.StridedSlice"(%arg0, %cst, %cst_1, %cst_2) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 1 : i64} : (tensor<10x10x10xf32>, tensor<1xi64>, tensor<1xi64>, tensor<1xi64>) -> tensor<10x10xf32>
  return %0 : tensor<10x10xf32>
  // CHECK-LABEL: strided_slice_with_i64_constant_attributes
  // CHECK-DAG: [[BEGIN:%cst.*]] = constant dense<-1> : tensor<1xi32>
  // CHECK-DAG: [[END:%cst.*]] = constant dense<0> : tensor<1xi32>
  // CHECK-DAG: [[STRIDES:%cst.*]] = constant dense<1> : tensor<1xi32>
  // CHECK-NEXT: "tfl.strided_slice"(%arg0, [[BEGIN]], [[END]], [[STRIDES]]) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 1 : i32} : (tensor<10x10x10xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<10x10xf32>
}

func @strided_slice_non_zero_ellipsis_mask(%arg0: tensor<12x2x2x5xf32>, %arg1: tensor<1xi32>, %arg2: tensor<1xi32>, %arg3: tensor<1xi32>) -> tensor<1x2x2x5xf32> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 1 : i64, end_mask = 0 : i64, new_axis_mask = 0 : i64, shrink_axis_mask = 0 : i64} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
  return %0 : tensor<1x2x2x5xf32>
  // CHECK-LABEL: strided_slice_non_zero_ellipsis_mask
  // CHECK:  %0 = "tfl.strided_slice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i32, ellipsis_mask = 1 : i32, end_mask = 0 : i32, new_axis_mask = 0 : i32, shrink_axis_mask = 0 : i32} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
}

func @strided_slice_non_zero_new_axis_mask(%arg0: tensor<12x2x2x5xf32>, %arg1: tensor<1xi32>, %arg2: tensor<1xi32>, %arg3: tensor<1xi32>) -> tensor<1x2x2x5xf32> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 2 : i64, shrink_axis_mask = 0 : i64} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
  return %0 : tensor<1x2x2x5xf32>
  // CHECK-LABEL: strided_slice_non_zero_new_axis_mask
  // CHECK: "tfl.strided_slice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i32, ellipsis_mask = 0 : i32, end_mask = 0 : i32, new_axis_mask = 2 : i32, shrink_axis_mask = 0 : i32} : (tensor<12x2x2x5xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1x2x2x5xf32>
}

func @strided_slice_big_dims(%arg0: tensor<5x6x7xf32>, %arg1: tensor<3xi32>, %arg2: tensor<3xi32>, %arg3: tensor<3xi32>) -> tensor<1x1x5x6x7xf32> {
  %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 7 : i64, shrink_axis_mask = 0 : i64} : (tensor<5x6x7xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<1x1x5x6x7xf32>
  return %0 : tensor<1x1x5x6x7xf32>
  // CHECK-LABEL: strided_slice_big_dims
  // CHECK: %0 = "tf.StridedSlice"(%arg0, %arg1, %arg2, %arg3) {begin_mask = 0 : i64, ellipsis_mask = 0 : i64, end_mask = 0 : i64, new_axis_mask = 7 : i64, shrink_axis_mask = 0 : i64} : (tensor<5x6x7xf32>, tensor<3xi32>, tensor<3xi32>, tensor<3xi32>) -> tensor<1x1x5x6x7xf32>
}

func @slice1Tensor(%arg0: tensor<2x3x5xf32>, %arg1: tensor<3xi32>, %arg2: tensor<3xi32>) -> tensor<?x3x5xf32> {
  %0 = "tf.Slice"(%arg0, %arg1, %arg2) : (tensor<2x3x5xf32>, tensor<3xi32>, tensor<3xi32>) -> tensor<?x3x5xf32>
  return %0 : tensor<?x3x5xf32>
  // CHECK-LABEL: slice1Tensor
  // CHECK: "tfl.slice"(%arg0, %arg1, %arg2) : (tensor<2x3x5xf32>, tensor<3xi32>, tensor<3xi32>) -> tensor<?x3x5xf32>
}

func @mirror_pad(tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32> {
^bb0(%arg0: tensor<2x1x3xf32>, %arg1: tensor<3x2xi32>):
  %0 = "tf.MirrorPad"(%arg0, %arg1) { mode = "SYMMETRIC" }: (tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32>
  return %0#0 : tensor<? x f32>

  // CHECK-LABEL: mirror_pad
  // CHECK:  "tfl.mirror_pad"(%arg0, %arg1) {mode = "SYMMETRIC"} : (tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<?xf32>
  // CHECK:  return
}

func @mirror_pad_reflect(tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32> {
^bb0(%arg0: tensor<2x1x3xf32>, %arg1: tensor<3x2xi32>):
  %0 = "tf.MirrorPad"(%arg0, %arg1) { mode = "REFLECT" }: (tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<? x f32>
  return %0#0 : tensor<? x f32>

  // CHECK-LABEL: mirror_pad_reflect
  // CHECK:  "tfl.mirror_pad"(%arg0, %arg1) {mode = "REFLECT"} : (tensor<2x1x3xf32>, tensor<3x2xi32>) -> tensor<?xf32>
  // CHECK:  return
}

func @Tanh(%arg0: tensor<1xf32>) -> tensor<1xf32> {
  %0 = "tf.Tanh"(%arg0) : (tensor<1xf32>) -> tensor<1xf32>
  return %0: tensor<1xf32>

// CHECK-LABEL: Tanh
// CHECK:  "tfl.tanh"(%arg0) : (tensor<1xf32>) -> tensor<1xf32>
}

func @cast(%arg0: tensor<1x2x2x5xi32>) -> tensor<1x2x2x5xf32> {
  %0 = "tf.Cast"(%arg0) : (tensor<1x2x2x5xi32>) -> tensor<1x2x2x5xf32>
  return %0 : tensor<1x2x2x5xf32>

  // CHECK-LABEL: cast
  // CHECK: "tfl.cast"(%arg0) : (tensor<1x2x2x5xi32>) -> tensor<1x2x2x5xf32>
}

func @castFloat32ToI16(%arg0: tensor<1x2x2x5xf32>) -> tensor<1x2x2x5xi16> {
  %0 = "tf.Cast"(%arg0) : (tensor<1x2x2x5xf32>) -> tensor<1x2x2x5xi16>
  return %0 : tensor<1x2x2x5xi16>

  // CHECK-LABEL: castFloat32ToI16
  // CHECK: "tfl.cast"(%arg0) : (tensor<1x2x2x5xf32>) -> tensor<1x2x2x5xi16>
}

func @castI16ToFloat32(%arg0: tensor<1x2x2x5xi16>) -> tensor<1x2x2x5xf32> {
  %0 = "tf.Cast"(%arg0) : (tensor<1x2x2x5xi16>) -> tensor<1x2x2x5xf32>
  return %0 : tensor<1x2x2x5xf32>

  // CHECK-LABEL: castI16ToFloat32
  // CHECK: "tfl.cast"(%arg0) : (tensor<1x2x2x5xi16>) -> tensor<1x2x2x5xf32>
}

func @castComplex(%arg0: tensor<1x2x2x5xf32>) -> tensor<1x2x2x5xcomplex<f32>> {
  %0 = "tf.Cast"(%arg0) : (tensor<1x2x2x5xf32>) -> tensor<1x2x2x5xcomplex<f32>>
  return %0 : tensor<1x2x2x5xcomplex<f32>>

  // CHECK-LABEL: castComplex
  // CHECK: "tfl.cast"(%arg0) : (tensor<1x2x2x5xf32>) -> tensor<1x2x2x5xcomplex<f32>>
}

func @unique(%arg0: tensor<5xf32>) -> (tensor<?xf32>, tensor<?xi32>) {
  %0, %1 = "tf.Unique"(%arg0) : (tensor<5xf32>) -> (tensor<?xf32>, tensor<?xi32>)
  return %0, %1 : tensor<?xf32> , tensor<?xi32>

  // CHECK-LABEL: unique
  // CHECK: "tfl.unique"(%arg0) : (tensor<5xf32>) -> (tensor<?xf32>, tensor<?xi32>)
}

func @unique64(%arg0: tensor<5xf32>) -> (tensor<?xf32>, tensor<?xi64>) {
  %0, %1 = "tf.Unique"(%arg0) : (tensor<5xf32>) -> (tensor<?xf32>, tensor<?xi64>)
  return %0, %1 : tensor<?xf32> , tensor<?xi64>

  // CHECK-LABEL: unique64
  // CHECK: "tfl.unique"(%arg0) : (tensor<5xf32>) -> (tensor<?xf32>, tensor<?xi64>)
}

func @ReverseSequence(%arg0: tensor<2x3xf32>, %arg1: tensor<2xi32>) -> tensor<2x3xf32> {
  %0 = "tf.ReverseSequence"(%arg0, %arg1) {seq_dim = 0 : i64, batch_dim = 0 : i64}: (tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x3xf32>
  return %0: tensor<2x3xf32>

// CHECK-LABEL: ReverseSequence
// CHECK:  "tfl.reverse_sequence"(%arg0, %arg1) {batch_dim = 0 : i32, seq_dim = 0 : i32} : (tensor<2x3xf32>, tensor<2xi32>) -> tensor<2x3xf32>
}

func @LRN(%arg0: tensor<2x3x4x5xf32>) -> tensor<2x3x4x5xf32> {
  %0 = "tf.LRN"(%arg0) {depth_radius = 5 :i64, bias = 1.0 :f32, alpha = 1.0 : f32, beta = 0.5 :f32} : (tensor<2x3x4x5xf32>) -> (tensor<2x3x4x5xf32>)
  return %0: tensor<2x3x4x5xf32>

  // CHECK-LABEL: LRN
  // CHECK: "tfl.local_response_normalization"(%arg0) {alpha = 1.000000e+00 : f32, beta = 5.000000e-01 : f32, bias = 1.000000e+00 : f32, radius = 5 : i32} : (tensor<2x3x4x5xf32>) -> tensor<2x3x4x5xf32>
  // CHECK: return %0 : tensor<2x3x4x5xf32>
}

func @OneHot(%arg0: tensor<3xi32>, %arg1: tensor<i32>, %arg2: tensor<f32>, %arg3: tensor<f32>) -> tensor<*xf32> {
  %0 = "tf.OneHot"(%arg0, %arg1, %arg2, %arg3) {axis = -1 : i64} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<*xf32>
  return %0: tensor<*xf32>

// CHECK-LABEL: OneHot
// CHECK: "tfl.one_hot"(%arg0, %arg1, %arg2, %arg3) {axis = -1 : i32} : (tensor<3xi32>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<*xf32>
}

func @argmax(%arg0: tensor<3xi32>, %arg1: tensor<i32>) -> tensor<i32> {
  %0 = "tf.ArgMax"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<i32>
  return %0 : tensor<i32>

// CHECK-LABEL: argmax
// CHECK:  "tfl.arg_max"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<i32>
}

func @argmax64(%arg0: tensor<3xi32>, %arg1: tensor<i32>) -> tensor<i64> {
  %0 = "tf.ArgMax"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<i64>
  return %0 : tensor<i64>

// CHECK-LABEL: argmax64
// CHECK:  "tfl.arg_max"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<i64>
}

func @space_to_depth(%arg0: tensor<1x2x2x1xf32>) -> tensor<?xf32> {
  %0 = "tf.SpaceToDepth"(%arg0) {block_size = 2: i64,  data_format = "NHWC"}: (tensor<1x2x2x1xf32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>

  // CHECK-LABEL: space_to_depth
  // CHECK: %[[ARG:.*]]: tensor<1x2x2x1xf32>
  // CHECK: "tfl.space_to_depth"(%[[ARG]]) {block_size = 2 : i32} : (tensor<1x2x2x1xf32>) -> tensor<?xf32>
}

func @round(%arg0: tensor<8x16xf32>) -> tensor<8x16xf32> {
  %0 = "tf.Round"(%arg0) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  return %0 : tensor<8x16xf32>

  // CHECK-LABEL: round
  // CHECK: %[[ARG:.*]]: tensor<8x16xf32>
  // CHECK: %[[RESULT:.*]] = "tfl.round"(%[[ARG]]) : (tensor<8x16xf32>) -> tensor<8x16xf32>
  // CHECK: return %[[RESULT]] : tensor<8x16xf32>
}

func @resize_nearest_neighbor(%arg0: tensor<1x100x100x3xf32>, %arg1: tensor<4xi32>) -> tensor<?xf32> {
  %0 = "tf.ResizeNearestNeighbor"(%arg0, %arg1) {align_corners = true} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
  // CHECK-LABEL: resize_nearest_neighbor
  // CHECK: "tfl.resize_nearest_neighbor"(%arg0, %arg1) {align_corners = true, half_pixel_centers = false} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
}

func @resize_nearest_neighbor_with_half_pixel_centers(%arg0: tensor<1x100x100x3xf32>, %arg1: tensor<4xi32>) -> tensor<?xf32> {
  %0 = "tf.ResizeNearestNeighbor"(%arg0, %arg1) {align_corners = false, half_pixel_centers = true} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
  // CHECK-LABEL: resize_nearest_neighbor_with_half_pixel_centers
  // CHECK: "tfl.resize_nearest_neighbor"(%arg0, %arg1) {align_corners = false, half_pixel_centers = true} : (tensor<1x100x100x3xf32>, tensor<4xi32>) -> tensor<?xf32>
}

func @sparse_to_dense_with_scalar_sparse_indices(%arg0: tensor<i32>, %arg1: tensor<3xi32>, %arg2: tensor<f32>, %arg3: tensor<f32>) -> tensor<?x?x?xf32> {
  %0 = "tf.SparseToDense"(%arg0, %arg1, %arg2, %arg3) {validate_indices = true}: (tensor<i32>, tensor<3xi32>, tensor<f32>, tensor<f32>) -> tensor<?x?x?xf32>
  return %0 : tensor<?x?x?xf32>
  // CHECK-LABEL: sparse_to_dense_with_scalar_sparse_indices
  // CHECK: "tfl.sparse_to_dense"(%arg0, %arg1, %arg2, %arg3) : (tensor<i32>, tensor<3xi32>, tensor<f32>, tensor<f32>) -> tensor<?x?x?xf32>
}

func @sparse_to_dense_with_vector_sparse_indices(%arg0: tensor<3xi32>, %arg1: tensor<3xi32>, %arg2: tensor<3xf32>, %arg3: tensor<f32>) -> tensor<?x?x?xf32> {
  %0 = "tf.SparseToDense"(%arg0, %arg1, %arg2, %arg3) {validate_indices = true}: (tensor<3xi32>, tensor<3xi32>, tensor<3xf32>, tensor<f32>) -> tensor<?x?x?xf32>
  return %0 : tensor<?x?x?xf32>
  // CHECK-LABEL: sparse_to_dense_with_vector_sparse_indices
  // CHECK: "tfl.sparse_to_dense"(%arg0, %arg1, %arg2, %arg3) : (tensor<3xi32>, tensor<3xi32>, tensor<3xf32>, tensor<f32>) -> tensor<?x?x?xf32>
}

func @sparse_to_dense_with_2d_sparse_indices(%arg0: tensor<3x2xi32>, %arg1: tensor<3xi32>, %arg2: tensor<2xf32>, %arg3: tensor<f32>) -> tensor<?x?x?xf32> {
  %0 = "tf.SparseToDense"(%arg0, %arg1, %arg2, %arg3) {validate_indices = true}: (tensor<3x2xi32>, tensor<3xi32>, tensor<2xf32>, tensor<f32>) -> tensor<?x?x?xf32>
  return %0 : tensor<?x?x?xf32>
  // CHECK-LABEL: sparse_to_dense_with_2d_sparse_indices
  // CHECK: "tfl.sparse_to_dense"(%arg0, %arg1, %arg2, %arg3) : (tensor<3x2xi32>, tensor<3xi32>, tensor<2xf32>, tensor<f32>) -> tensor<?x?x?xf32>
}

func @where(%arg0: tensor<3x5xi1>) -> tensor<?x2xi64> {
  %0 = "tf.Where"(%arg0) : (tensor<3x5xi1>) -> tensor<?x2xi64>
  return %0 : tensor<?x2xi64>
  // CHECK-LABEL: where
  // CHECK: "tfl.where"(%arg0) : (tensor<3x5xi1>) -> tensor<?x2xi64>
}

func @floor_mod(%arg0: tensor<5xf32>, %arg1: tensor<5xf32>) -> tensor<5xf32> {
  %0 = "tf.FloorMod"(%arg0, %arg1) : (tensor<5xf32>, tensor<5xf32>) -> tensor<5xf32>
  return %0 : tensor<5xf32>
  // CHECK-LABEL: floor_mod
  // CHECK: "tfl.floor_mod"(%arg0, %arg1) : (tensor<5xf32>, tensor<5xf32>) -> tensor<5xf32>
}

func @exp(%arg0: tensor<5xf32>) -> tensor<5xf32> {
  %0 = "tf.Exp"(%arg0) : (tensor<5xf32>) -> tensor<5xf32>
  return %0 : tensor<5xf32>
  // CHECK-LABEL: exp
  // CHECK: "tfl.exp"(%arg0) : (tensor<5xf32>) -> tensor<5xf32>
}

func @depth_to_space(%arg0: tensor<1x1x1x4xf32>) -> tensor<1x2x2x1xf32> {
  %0 = "tf.DepthToSpace"(%arg0) {block_size = 2: i64,  data_format = "NHWC"}: (tensor<1x1x1x4xf32>) -> tensor<1x2x2x1xf32>
  return %0 : tensor<1x2x2x1xf32>

  // CHECK-LABEL: depth_to_space
  // CHECK: %[[ARG:.*]]: tensor<1x1x1x4xf32>
  // CHECK: "tfl.depth_to_space"(%[[ARG]]) {block_size = 2 : i32} : (tensor<1x1x1x4xf32>) -> tensor<1x2x2x1xf32>
}

func @non_max_suppression_v4(%arg0: tensor<3x4xf32>, %arg1: tensor<3xf32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<f32>) -> tensor<2xi32> {
  %0:2 = "tf.NonMaxSuppressionV4"(%arg0, %arg1, %arg2, %arg3, %arg4) {pad_to_max_output_size = true}: (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<i32>)
  return %0#0 : tensor<2xi32>

  // CHECK-LABEL: non_max_suppression_v4
  // CHECK: "tfl.non_max_suppression_v4"(%arg0, %arg1, %arg2, %arg3, %arg4) : (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<i32>)
}

func @non_max_suppression_v4_no_pad(%arg0: tensor<3x4xf32>, %arg1: tensor<3xf32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<f32>) -> tensor<2xi32> {
  %0:2 = "tf.NonMaxSuppressionV4"(%arg0, %arg1, %arg2, %arg3, %arg4) {pad_to_max_output_size = false}: (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<i32>)
  return %0#0 : tensor<2xi32>

  // CHECK-LABEL: non_max_suppression_v4_no_pad
  // CHECK: "tfl.non_max_suppression_v4"(%arg0, %arg1, %arg2, %arg3, %arg4) : (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<i32>)
}

func @non_max_suppression_v5(%arg0: tensor<3x4xf32>, %arg1: tensor<3xf32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<f32>, %arg5: tensor<f32>) -> tensor<2xi32> {
  %0:3 = "tf.NonMaxSuppressionV5"(%arg0, %arg1, %arg2, %arg3, %arg4, %arg5) {pad_to_max_output_size = true}: (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<2xf32>, tensor<i32>)
  return %0#0 : tensor<2xi32>

  // CHECK-LABEL: non_max_suppression_v5
  // CHECK: "tfl.non_max_suppression_v5"(%arg0, %arg1, %arg2, %arg3, %arg4, %arg5) : (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<2xf32>, tensor<i32>)
}

func @non_max_suppression_v5_no_pad(%arg0: tensor<3x4xf32>, %arg1: tensor<3xf32>, %arg2: tensor<i32>, %arg3: tensor<f32>, %arg4: tensor<f32>, %arg5: tensor<f32>) -> tensor<2xi32> {
  %0:3 = "tf.NonMaxSuppressionV5"(%arg0, %arg1, %arg2, %arg3, %arg4, %arg5) {pad_to_max_output_size = false}: (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<2xf32>, tensor<i32>)
  return %0#0 : tensor<2xi32>

  // CHECK-LABEL: non_max_suppression_v5_no_pad
  // CHECK: "tfl.non_max_suppression_v5"(%arg0, %arg1, %arg2, %arg3, %arg4, %arg5) : (tensor<3x4xf32>, tensor<3xf32>, tensor<i32>, tensor<f32>, tensor<f32>, tensor<f32>) -> (tensor<2xi32>, tensor<2xf32>, tensor<i32>)
}

func @conv2d_backprop_input(%arg0: tensor<4xi32>, %arg1: tensor<3x3x1x32xf32>, %arg2: tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32> {
  %0 = "tf.Conv2DBackpropInput"(%arg0, %arg1, %arg2) {strides = [1, 2, 2, 1], padding="SAME", dilations=[1, 1, 1, 1]}: (tensor<4xi32>, tensor<3x3x1x32xf32>, tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32>
  %1 = "tf.Conv2DBackpropInput"(%arg0, %arg1, %arg2) {strides = [1, 2, 2, 1], padding="VALID", dilations=[1, 1, 1, 1]}: (tensor<4xi32>, tensor<3x3x1x32xf32>, tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32>
  %2 = "tf.Add"(%0, %1): (tensor<15x28x28x1xf32>, tensor<15x28x28x1xf32>) -> tensor<15x28x28x1xf32>
  return %2 : tensor<15x28x28x1xf32>

  // CHECK-LABEL: conv2d_backprop_input
  // CHECK: %[[CST:.*]] = constant dense<[2, 0, 1, 3]> : tensor<4xi32>
  // CHECK: %[[ARG0:.*]] = "tfl.transpose"(%arg1, %[[CST]]) : (tensor<3x3x1x32xf32>, tensor<4xi32>) -> tensor<1x3x3x32xf32>
  // CHECK: %[[CST_0:.*]] = constant unit
  // CHECK: %[[ARG1:.*]] = "tfl.transpose_conv"(%arg0, %[[ARG0]], %arg2, %[[CST_0]]) {padding = "SAME", stride_h = 2 : i32, stride_w = 2 : i32} : (tensor<4xi32>, tensor<1x3x3x32xf32>, tensor<15x14x14x32xf32>, none) -> tensor<15x28x28x1xf32>
  // CHECK: %[[ARG3:.*]] = "tfl.transpose_conv"(%arg0, %[[ARG0]], %arg2, %[[CST_0]]) {padding = "VALID", stride_h = 2 : i32, stride_w = 2 : i32} : (tensor<4xi32>, tensor<1x3x3x32xf32>, tensor<15x14x14x32xf32>, none) -> tensor<15x28x28x1xf32>
  // CHECK: %[[RESULT:.*]] = tfl.add %[[ARG1]], %[[ARG3]] {fused_activation_function = "NONE"} : tensor<15x28x28x1xf32>
  // CHECK: return %[[RESULT]] : tensor<15x28x28x1xf32>
}

func @conv2d_backprop_input_unsupported_paddings(%arg0: tensor<4xi32>, %arg1: tensor<3x3x1x32xf32>, %arg2: tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32> {
  %0 = "tf.Conv2DBackpropInput"(%arg0, %arg1, %arg2) {strides = [1, 2, 2, 1], explicit_paddings = [1, 1, 1, 1, 1, 1, 1, 1], padding="EXPLICIT", dilations=[1, 1, 1, 1]}: (tensor<4xi32>, tensor<3x3x1x32xf32>, tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32>
  return %0 : tensor<15x28x28x1xf32>

  // CHECK-LABEL: conv2d_backprop_input_unsupported_paddings
  // CHECK: tf.Conv2DBackpropInput
}

func @conv2d_backprop_unsupported_strides(%arg0: tensor<4xi32>, %arg1: tensor<3x3x1x32xf32>, %arg2: tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32> {
  %0 = "tf.Conv2DBackpropInput"(%arg0, %arg1, %arg2) {dilations = [1, 1, 1, 1], padding="SAME", strides = [2, 2, 2, 2]}: (tensor<4xi32>, tensor<3x3x1x32xf32>, tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32>
  return %0 : tensor<15x28x28x1xf32>

  // CHECK-LABEL: conv2d_backprop_unsupported_strides
  // CHECK: tf.Conv2DBackpropInput
}

func @conv2d_backprop_unsupported_data_format(%arg0: tensor<4xi32>, %arg1: tensor<3x3x1x32xf32>, %arg2: tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32> {
  %0 = "tf.Conv2DBackpropInput"(%arg0, %arg1, %arg2) {data_format = "NCHW", dilations = [1, 1, 1, 1], padding="SAME", strides = [1, 2, 2, 1]}: (tensor<4xi32>, tensor<3x3x1x32xf32>, tensor<15x14x14x32xf32>) -> tensor<15x28x28x1xf32>
  return %0 : tensor<15x28x28x1xf32>

  // CHECK-LABEL: conv2d_backprop_unsupported_data_format
  // CHECK: tf.Conv2DBackpropInput
}

func @assert_remove(%arg0: tensor<1xi32>, %arg1: tensor<1xi32>) -> tensor<1xi1> {
  %0 = "tf.LessEqual"(%arg0, %arg1) : (tensor<1xi32>, tensor<1xi32>) -> tensor<1xi1>
  "tf.Assert"(%0, %arg1) {summarize = 3} : (tensor<1xi1>, tensor<1xi32>) -> ()
  return %0 : tensor<1xi1>
  // CHECK-LABEL: assert_remove
  // CHECK: tfl.less_equal
  // CHECK-NOT: Assert
  // CHECK: return
}

func @reciprocal_f32(%arg0: tensor<8xf32>) -> tensor<8xf32> {
  %0 = "tf.Reciprocal"(%arg0) : (tensor<8xf32>) -> tensor<8xf32>
  return %0: tensor<8xf32>

// CHECK-LABEL: reciprocal_f32
// CHECK:  %cst = constant dense<1.000000e+00> : tensor<f32>
// CHECK:  tfl.div(%cst, %arg0) {fused_activation_function = "NONE"} : (tensor<f32>, tensor<8xf32>) -> tensor<8xf32>
// CHECK:  return
}

func @reciprocal_i32(%arg0: tensor<8xi32>) -> tensor<8xi32> {
  %0 = "tf.Reciprocal"(%arg0) : (tensor<8xi32>) -> tensor<8xi32>
  return %0: tensor<8xi32>

// CHECK-LABEL: reciprocal_i32
// CHECK:  %cst = constant dense<1> : tensor<i32>
// CHECK:  tfl.div(%cst, %arg0) {fused_activation_function = "NONE"} : (tensor<i32>, tensor<8xi32>) -> tensor<8xi32>
// CHECK:  return
}

func @random_uniform() -> tensor<2x5xf32> {
  %0 = "tf.Const"() { value = dense<[2, 5]> : tensor<2xi32> } : () -> tensor<2xi32>
  %1 = "tf.RandomUniform"(%0) { seed = 1, seed2 = 0} : (tensor<2xi32>) -> tensor<2x5xf32>
  return %1 : tensor<2x5xf32>

  // CHECK-LABEL: random_uniform
  // CHECK: %[[CST:.*]] = constant dense
  // CHECK: return %[[CST:.*]] : tensor<2x5xf32>
}

func @random_uniform_no_fold(%arg0: tensor<2xi32>) -> tensor<2x5xf32> {
  %1 = "tf.RandomUniform"(%arg0) { seed = 0, seed2 = 0} : (tensor<2xi32>) -> tensor<2x5xf32>
  return %1 : tensor<2x5xf32>

  // CHECK-LABEL: random_uniform_no_fold
  // CHECK: %[[RANDOM:.*]] = "tf.RandomUniform"
}

func @random_uniform_no_fold2(%arg0: tensor<2xi32>) -> tensor<*xf32> {
  %1 = "tf.RandomUniform"(%arg0) { seed = 1, seed2 = 2} : (tensor<2xi32>) -> tensor<*xf32>
  return %1 : tensor<*xf32>

  // CHECK-LABEL: random_uniform_no_fold2
  // CHECK: %[[RANDOM:.*]] = "tf.RandomUniform"
}

func @random_uniform_no_fold3() -> tensor<2x5xf64> {
  %0 = "tf.Const"() { value = dense<[2, 5]> : tensor<2xi32> } : () -> tensor<2xi32>
  %1 = "tf.RandomUniform"(%0) { seed = 1, seed2 = 0} : (tensor<2xi32>) -> tensor<2x5xf64>
  return %1 : tensor<2x5xf64>

  // CHECK-LABEL: random_uniform_no_fold3
  // CHECK: %[[RANDOM:.*]] = "tf.RandomUniform"
}

func @LstmWithoutProjection(%arg: tensor<28x1x28xf32>) -> (tensor<28x1x16xf32>) {
  %1 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<16x28xf32>} : () -> tensor<16x28xf32>
  %2 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<16x16xf32>} : () -> tensor<16x16xf32>
  %3 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<16xf32>} : () -> tensor<16xf32>
  %4 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<1x16xf32>} : () -> tensor<1x16xf32>
  %5 = "tf.Const"() {device = "", dtype = f32, value = dense<-1.000000e+00> : tensor<1xf32>} : () -> tensor<1xf32>
  %6:3 = "tf.UnidirectionalSequenceLstm"(%arg, %1, %1, %1, %1, %2, %2, %2, %2, %3, %3, %3, %3, %3, %3, %3, %5, %5, %4, %4) {_tflite_input_indices = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19], device = ""} : (tensor<28x1x28xf32>, tensor<16x28xf32>, tensor<16x28xf32>, tensor<16x28xf32>, tensor<16x28xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<1xf32>, tensor<1xf32>, tensor<1x16xf32>, tensor<1x16xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<28x1x16xf32>)
  return %6#2 : tensor<28x1x16xf32>
}

// CHECK:       func @LstmWithoutProjection([[VAL_0:%.*]]: tensor<28x1x28xf32>) -> tensor<28x1x16xf32> {
// CHECK:           [[VAL_1:%.*]] = constant dense<0.000000e+00> : tensor<16x28xf32>
// CHECK:           [[VAL_2:%.*]] = constant dense<0.000000e+00> : tensor<16x16xf32>
// CHECK:           [[VAL_3:%.*]] = constant dense<0.000000e+00> : tensor<16xf32>
// CHECK:           [[VAL_4:%.*]] = constant dense<0.000000e+00> : tensor<1x16xf32>
// CHECK:           [[VAL_5:%.*]] = constant unit
// CHECK:           [[VAL_6:%.*]] = "tfl.unidirectional_sequence_lstm"([[VAL_0]], [[VAL_1]], [[VAL_1]], [[VAL_1]], [[VAL_1]], [[VAL_2]], [[VAL_2]], [[VAL_2]], [[VAL_2]], [[VAL_3]], [[VAL_3]], [[VAL_3]], [[VAL_3]], [[VAL_3]], [[VAL_3]], [[VAL_3]], [[VAL_5]], [[VAL_5]], [[VAL_4]], [[VAL_4]], [[VAL_5]], [[VAL_5]], [[VAL_5]], [[VAL_5]]) {cell_clip = 0.000000e+00 : f32, fused_activation_function = "TANH", proj_clip = 0.000000e+00 : f32, time_major = true} : (tensor<28x1x28xf32>, tensor<16x28xf32>, tensor<16x28xf32>, tensor<16x28xf32>, tensor<16x28xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, none, none, tensor<1x16xf32>, tensor<1x16xf32>, none, none, none, none) -> tensor<28x1x16xf32>
// CHECK:           return [[VAL_6]] : tensor<28x1x16xf32>
// CHECK:         }

func @LstmWithProjection(%arg: tensor<28x1x16xf32>) -> (tensor<28x1x8xf32>) {
  %1 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<16x16xf32>} : () -> tensor<16x16xf32>
  %2 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<16x8xf32>} : () -> tensor<16x8xf32>
  %3 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<16xf32>} : () -> tensor<16xf32>
  %4 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<1x16xf32>} : () -> tensor<1x16xf32>
  %5 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<8x16xf32>} : () -> tensor<8x16xf32>
  %6 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<1x8xf32>} : () -> tensor<1x8xf32>
  %7 = "tf.Const"() {device = "", dtype = f32, value = dense<-1.000000e+00> : tensor<1xf32>} : () -> tensor<1xf32>
  %8:3 = "tf.UnidirectionalSequenceLstm"(%arg, %1, %1, %1, %1, %2, %2, %2, %2, %7, %7, %7, %3, %3, %3, %3, %5, %7, %6, %4) {_tflite_input_indices = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 13, 14, 15, 16, 18, 19], device = ""} : (tensor<28x1x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x8xf32>, tensor<16x8xf32>, tensor<16x8xf32>, tensor<16x8xf32>, tensor<1xf32>, tensor<1xf32>, tensor<1xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<8x16xf32>, tensor<1xf32>, tensor<1x8xf32>, tensor<1x16xf32>) -> (tensor<*xf32>, tensor<*xf32>, tensor<28x1x8xf32>)
  return %8#2 : tensor<28x1x8xf32>
}

// CHECK-LABEL:   func @LstmWithProjection(
// CHECK-SAME:                             [[VAL_7:%.*]]: tensor<28x1x16xf32>) -> tensor<28x1x8xf32> {
// CHECK:           [[VAL_8:%.*]] = constant dense<0.000000e+00> : tensor<16x16xf32>
// CHECK:           [[VAL_9:%.*]] = constant dense<0.000000e+00> : tensor<16x8xf32>
// CHECK:           [[VAL_10:%.*]] = constant dense<0.000000e+00> : tensor<16xf32>
// CHECK:           [[VAL_11:%.*]] = constant dense<0.000000e+00> : tensor<1x16xf32>
// CHECK:           [[VAL_12:%.*]] = constant dense<0.000000e+00> : tensor<8x16xf32>
// CHECK:           [[VAL_13:%.*]] = constant dense<0.000000e+00> : tensor<1x8xf32>
// CHECK:           [[VAL_14:%.*]] = constant unit
// CHECK:           [[VAL_15:%.*]] = "tfl.unidirectional_sequence_lstm"([[VAL_7]], [[VAL_8]], [[VAL_8]], [[VAL_8]], [[VAL_8]], [[VAL_9]], [[VAL_9]], [[VAL_9]], [[VAL_9]], [[VAL_14]], [[VAL_14]], [[VAL_14]], [[VAL_10]], [[VAL_10]], [[VAL_10]], [[VAL_10]], [[VAL_12]], [[VAL_14]], [[VAL_13]], [[VAL_11]], [[VAL_14]], [[VAL_14]], [[VAL_14]], [[VAL_14]]) {cell_clip = 0.000000e+00 : f32, fused_activation_function = "TANH", proj_clip = 0.000000e+00 : f32, time_major = true} : (tensor<28x1x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x16xf32>, tensor<16x8xf32>, tensor<16x8xf32>, tensor<16x8xf32>, tensor<16x8xf32>, none, none, none, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<16xf32>, tensor<8x16xf32>, none, tensor<1x8xf32>, tensor<1x16xf32>, none, none, none, none) -> tensor<28x1x8xf32>
// CHECK:           return [[VAL_15]] : tensor<28x1x8xf32>
// CHECK:         }

func @UnidirectionalRnn(%arg: tensor<28x1x28xf32>) -> (tensor<28x1x28xf32>) {
  %1 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<28x28xf32>} : () -> tensor<28x28xf32>
  %2 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<28xf32>} : () -> tensor<28xf32>
  %3 = "tf.Const"() {device = "", dtype = f32, value = dense<0.000000e+00>: tensor<1x28xf32>} : () -> tensor<1x28xf32>
  %4:2 = "tf.UnidirectionalSequenceRnn"(%arg, %1, %1, %2, %3) {_tflite_input_indices = [0, 1, 2, 3, 4], device = ""} : (tensor<28x1x28xf32>, tensor<28x28xf32>, tensor<28x28xf32>, tensor<28xf32>, tensor<1x28xf32>) -> (tensor<*xf32>, tensor<28x1x28xf32>)
  return %4#1 : tensor<28x1x28xf32>
}

// CHECK:       func @UnidirectionalRnn([[VAL_0:%.*]]: tensor<28x1x28xf32>) -> tensor<28x1x28xf32> {
// CHECK:           [[VAL_1:%.*]] = constant dense<0.000000e+00> : tensor<28x28xf32>
// CHECK:           [[VAL_2:%.*]] = constant dense<0.000000e+00> : tensor<28xf32>
// CHECK:           [[VAL_3:%.*]] = constant dense<0.000000e+00> : tensor<1x28xf32>
// CHECK:           [[VAL_4:%.*]] = "tfl.unidirectional_sequence_rnn"([[VAL_0]], [[VAL_1]], [[VAL_1]], [[VAL_2]], [[VAL_3]]) {fused_activation_function = "TANH", time_major = true} : (tensor<28x1x28xf32>, tensor<28x28xf32>, tensor<28x28xf32>, tensor<28xf32>, tensor<1x28xf32>) -> tensor<28x1x28xf32>
// CHECK:           return [[VAL_4]] : tensor<28x1x28xf32>
// CHECK:         }

func @broadcast_to_f32(%arg0: tensor<3xf32>, %arg1: tensor<2xi32>) -> tensor<3x3xf32> {
  %0 = "tf.BroadcastTo"(%arg0, %arg1) : (tensor<3xf32>, tensor<2xi32>) -> tensor<3x3xf32>
  return %0: tensor<3x3xf32>

// CHECK-LABEL: broadcast_to_f32
// CHECK:  [[BCT:%.*]] = "tfl.broadcast_to"(%arg0, %arg1) : (tensor<3xf32>, tensor<2xi32>) -> tensor<3x3xf32>
// CHECK:  return [[BCT]] : tensor<3x3xf32>
}

func @broadcast_to_i32(%input: tensor<3xi32>, %shape: tensor<2xi32>) -> tensor<3x3xi32> {
  %0 = "tf.BroadcastTo"(%input, %shape) : (tensor<3xi32>, tensor<2xi32>) -> tensor<3x3xi32>
  return %0: tensor<3x3xi32>

// CHECK-LABEL: broadcast_to_i32
// CHECK:  [[BCT:%.*]] = "tfl.broadcast_to"(%arg0, %arg1) : (tensor<3xi32>, tensor<2xi32>) -> tensor<3x3xi32>
// CHECK:  return [[BCT]] : tensor<3x3xi32>
}

func @matmul_batch(%arg0: tensor<10x15xf32>, %arg1: tensor<15x17xf32>) -> tensor<10x17xf32> {
  %0 = "tf.BatchMatMul"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", adj_x = false, adj_y = false} :
(tensor<10x15xf32>, tensor<15x17xf32>) -> tensor<10x17xf32>
  return %0 : tensor<10x17xf32>
// CHECK-LABEL: matmul_batch
// CHECK: "tfl.batch_matmul"(%arg0, %arg1) {adj_x = false, adj_y = false} : (tensor<10x15xf32>, tensor<15x17xf32>) -> tensor<10x17xf32>
}

func @matmul_batchv2(%arg0: tensor<2x10x15xf32>, %arg1: tensor<15x17xf32>) -> tensor<2x10x17xf32> {
  %0 = "tf.BatchMatMulV2"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", adj_x = false, adj_y = false} :
(tensor<2x10x15xf32>, tensor<15x17xf32>) -> tensor<2x10x17xf32>
  return %0 : tensor<2x10x17xf32>
// CHECK-LABEL: matmul_batchv2
// CHECK: "tfl.batch_matmul"(%arg0, %arg1) {adj_x = false, adj_y = false} : (tensor<2x10x15xf32>, tensor<15x17xf32>) -> tensor<2x10x17xf32>
}

func @matmul_batchv3(%arg0: tensor<2x10x15xf32>, %arg1: tensor<15x17xf32>) -> tensor<2x10x17xf32> {
  %0 = "tf.BatchMatMulV3"(%arg0, %arg1) {Ta = "tfdtype$DT_FLOAT", Tb = "tfdtype$DT_FLOAT",device = "/device:CPU:0", name = "MatMul", adj_x = false, adj_y = false} :
(tensor<2x10x15xf32>, tensor<15x17xf32>) -> tensor<2x10x17xf32>
  return %0 : tensor<2x10x17xf32>
// CHECK-LABEL: matmul_batchv3
// CHECK: "tfl.batch_matmul"(%arg0, %arg1) {adj_x = false, adj_y = false} : (tensor<2x10x15xf32>, tensor<15x17xf32>) -> tensor<2x10x17xf32>
}

func @matmul_batchv3_int8(%arg0: tensor<2x10x15xi8>, %arg1: tensor<15x17xi8>) -> tensor<2x10x17xi32> {
  %0 = "tf.BatchMatMulV3"(%arg0, %arg1) {Ta = "tfdtype$DT_INT8", Tb = "tfdtype$DT_INT8", Tout = "tfdtype$DT_INT32", device = "/device:CPU:0", name = "MatMul", adj_x = false, adj_y = false} :
(tensor<2x10x15xi8>, tensor<15x17xi8>) -> tensor<2x10x17xi32>
  return %0 : tensor<2x10x17xi32>
// CHECK-LABEL: matmul_batchv3_int8
// CHECK: "tfl.batch_matmul"(%arg0, %arg1) {adj_x = false, adj_y = false} : (tensor<2x10x15xi8>, tensor<15x17xi8>) -> tensor<2x10x17xi32>
}

func @matmul_batchv2_unknown_dim(%arg0: tensor<?x10x15xf32>, %arg1: tensor<15x17xf32>) -> tensor<?x10x17xf32> {
  %0 = "tf.BatchMatMulV2"(%arg0, %arg1) {T = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", adj_x = false, adj_y = false} :
(tensor<?x10x15xf32>, tensor<15x17xf32>) -> tensor<?x10x17xf32>
  return %0 : tensor<?x10x17xf32>
// CHECK-LABEL: matmul_batchv2_unknown_dim
// CHECK: "tfl.batch_matmul"(%arg0, %arg1) {adj_x = false, adj_y = false} : (tensor<?x10x15xf32>, tensor<15x17xf32>) -> tensor<?x10x17xf32>
}

func @matmul_batchv3_unknown_dim(%arg0: tensor<?x10x15xf32>, %arg1: tensor<15x17xf32>) -> tensor<?x10x17xf32> {
  %0 = "tf.BatchMatMulV3"(%arg0, %arg1) {Ta = "tfdtype$DT_FLOAT", Tb = "tfdtype$DT_FLOAT", device = "/device:CPU:0", name = "MatMul", adj_x = false, adj_y = false} :
(tensor<?x10x15xf32>, tensor<15x17xf32>) -> tensor<?x10x17xf32>
  return %0 : tensor<?x10x17xf32>
// CHECK-LABEL: matmul_batchv3_unknown_dim
// CHECK: "tfl.batch_matmul"(%arg0, %arg1) {adj_x = false, adj_y = false} : (tensor<?x10x15xf32>, tensor<15x17xf32>) -> tensor<?x10x17xf32>
}

// -----

func @select_v2_with_6d_broadcasting(%arg0: tensor<1x1x1x1x3x1xi1>, %arg1 : tensor<1x1x1x1x1x4xf32>, %arg2 : tensor<1x1x1x2x1x1xf32>) -> tensor<1x1x1x2x3x4xf32> {
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2): (tensor<1x1x1x1x3x1xi1>, tensor<1x1x1x1x1x4xf32>, tensor<1x1x1x2x1x1xf32>) -> tensor<1x1x1x2x3x4xf32>
  return %0 : tensor<1x1x1x2x3x4xf32>
// CHECK-LABEL: select_v2_with_6d_broadcasting
// CHECK: [[CST:%.*]] = constant dense<[1, 1, 1, 2, 3, 4]> : tensor<6xi64>
// CHECK: [[BCT:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
// CHECK: [[BCT_0:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
// CHECK: [[BCT_1:%.*]] = "tfl.broadcast_to"(%arg2, [[CST]])
// CHECK: "tfl.select"([[BCT]], [[BCT_0]], [[BCT_1]])
}

// -----

func @maximum_with_6d_broadcasting(%arg0: tensor<1x1x1x1x8x16xf32>, %arg1: tensor<8x16xf32>) -> tensor<1x1x1x1x8x16xf32> {
  %0 = "tf.Maximum"(%arg0, %arg1) : (tensor<1x1x1x1x8x16xf32>, tensor<8x16xf32>) -> tensor<1x1x1x1x8x16xf32>
  return %0 : tensor<1x1x1x1x8x16xf32>

// CHECK-LABEL: maximum_with_6d_broadcasting
// CHECK: [[CST:%.*]] = constant dense<[1, 1, 1, 1, 8, 16]> : tensor<6xi64>
// CHECK: [[BCT:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
// CHECK:  "tfl.maximum"(%arg0, [[BCT]])
}

// -----

func @add_with_int32_5d_inputs(%arg0: tensor<1x1x1x3x1xi32>, %arg1 : tensor<1x1x1x1x4xi32>) -> tensor<1x1x1x3x4xi32> {
  %0 = "tf.Add"(%arg0, %arg1): (tensor<1x1x1x3x1xi32>, tensor<1x1x1x1x4xi32>) -> tensor<1x1x1x3x4xi32>
  return %0 : tensor<1x1x1x3x4xi32>
// CHECK-LABEL: add_with_int32_5d_inputs
// CHECK: [[CST:%.*]] = constant dense<[1, 1, 1, 3, 4]> : tensor<5xi64>
// CHECK: [[BCT:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
// CHECK: [[BCT_0:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
// CHECK:  tfl.add [[BCT]], [[BCT_0]]
}

// CHECK-LABEL: testAddWithBroadcastToOps
func @testAddWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.add [[BCAST]], [[BCAST_1]] {fused_activation_function = "NONE"} : tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Add"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testSubWithBroadcastToOps
func @testSubWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.sub [[BCAST]], [[BCAST_1]] {fused_activation_function = "NONE"} : tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Sub"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testMulWithBroadcastToOps
func @testMulWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.mul [[BCAST]], [[BCAST_1]] {fused_activation_function = "NONE"} : tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Mul"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testDivWithBroadcastToOps
func @testDivWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.div [[BCAST]], [[BCAST_1]] {fused_activation_function = "NONE"} : tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Div"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testFloorDivWithBroadcastToOps
func @testFloorDivWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.floor_div [[BCAST]], [[BCAST_1]] : tensor<1x2x3x4x5x6xi32>
  %0 = "tf.FloorDiv"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testFloorModWithBroadcastToOps
func @testFloorModWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: "tfl.floor_mod"([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi32>
  %0 = "tf.FloorMod"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testPowWithBroadcastToOps
func @testPowWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.pow [[BCAST]], [[BCAST_1]] : tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Pow"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testMaximumWithBroadcastToOps
func @testMaximumWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: "tfl.maximum"([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Maximum"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testMinimumWithBroadcastToOps
func @testMinimumWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: "tfl.minimum"([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi32>
  %0 = "tf.Minimum"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testSelectV2WithBroadcastToOps
func @testSelectV2WithBroadcastToOps(%arg0: tensor<1x2x1x4x1x6xi1>, %arg1: tensor<1x2x3x4x1x1xi32>, %arg2: tensor<1x2x1x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: [[BCAST_2:%.*]] = "tfl.broadcast_to"(%arg2, [[CST]])
  // CHECK: "tfl.select"([[BCAST]], [[BCAST_1]], [[BCAST_2]])
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<1x2x1x4x1x6xi1>, tensor<1x2x3x4x1x1xi32>, tensor<1x2x1x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi32>
  return %0 : tensor<1x2x3x4x5x6xi32>
}

// CHECK-LABEL: testLessEqualWithBroadcastToOps
func @testLessEqualWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.less_equal([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi1>
  %0 = "tf.LessEqual"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1>
  return %0 : tensor<1x2x3x4x5x6xi1>
}

// CHECK-LABEL: testGreaterEqualWithBroadcastToOps
func @testGreaterEqualWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.greater_equal([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi1>
  %0 = "tf.GreaterEqual"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1>
  return %0 : tensor<1x2x3x4x5x6xi1>
}

// CHECK-LABEL: testEqualWithBroadcastToOps
func @testEqualWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: "tfl.equal"([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi1>
  %0 = "tf.Equal"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1>
  return %0 : tensor<1x2x3x4x5x6xi1>
}

// CHECK-LABEL: testNotEqualWithBroadcastToOps
func @testNotEqualWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.not_equal([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi1>
  %0 = "tf.NotEqual"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1>
  return %0 : tensor<1x2x3x4x5x6xi1>
}

// CHECK-LABEL: testLessWithBroadcastToOps
func @testLessWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.less([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi1>
  %0 = "tf.Less"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1>
  return %0 : tensor<1x2x3x4x5x6xi1>
}

// CHECK-LABEL: testGreaterWithBroadcastToOps
func @testGreaterWithBroadcastToOps(%arg0: tensor<1x2x1x4x5x6xi32>, %arg1: tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1> {
  // CHECK: [[CST:%.*]] = constant dense<[1, 2, 3, 4, 5, 6]> : tensor<6xi64>
  // CHECK: [[BCAST:%.*]] = "tfl.broadcast_to"(%arg0, [[CST]])
  // CHECK: [[BCAST_1:%.*]] = "tfl.broadcast_to"(%arg1, [[CST]])
  // CHECK: tfl.greater([[BCAST]], [[BCAST_1]]) : (tensor<1x2x3x4x5x6xi32>, tensor<1x2x3x4x5x6xi32>) -> tensor<1x2x3x4x5x6xi1>
  %0 = "tf.Greater"(%arg0, %arg1) : (tensor<1x2x1x4x5x6xi32>, tensor<1x2x3x4x5x1xi32>) -> tensor<1x2x3x4x5x6xi1>
  return %0 : tensor<1x2x3x4x5x6xi1>
}

func @tranpose_int32_perm(%arg0: tensor<2x3xf32>) -> tensor<3x2xf32> {
  %cst = "tf.Const"() { value = dense<[1, 0]> : tensor<2xi32> } : () -> tensor<2xi32>
  %0 = "tf.Transpose"(%arg0, %cst): (tensor<2x3xf32>, tensor<2xi32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
  // CHECK-LABEL: tranpose_int32_perm
  // CHECK: "tfl.transpose"
}

func @tranpose_int64_perm(%arg0: tensor<2x3xf32>) -> tensor<3x2xf32> {
  %cst = "tf.Const"() { value = dense<[1, 0]> : tensor<2xi64> } : () -> tensor<2xi64>
  %0 = "tf.Transpose"(%arg0, %cst): (tensor<2x3xf32>, tensor<2xi64>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
  // CHECK-LABEL: tranpose_int64_perm
  // CHECK: "tfl.transpose"
}

func @tranpose_arg32(%arg0: tensor<2x3xf32>, %arg1: tensor<2xi32>) -> tensor<3x2xf32> {
  %0 = "tf.Transpose"(%arg0, %arg1): (tensor<2x3xf32>, tensor<2xi32>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
  // CHECK-LABEL: tranpose_arg32
  // CHECK: "tfl.transpose"
}

func @tranpose_arg64(%arg0: tensor<2x3xf32>, %arg1: tensor<2xi64>) -> tensor<3x2xf32> {
  %0 = "tf.Transpose"(%arg0, %arg1): (tensor<2x3xf32>, tensor<2xi64>) -> tensor<3x2xf32>
  return %0 : tensor<3x2xf32>
  // CHECK-LABEL: tranpose_arg64
  // CHECK: "tfl.transpose"
}

func @cumsum(%arg0: tensor<3x3xf32>, %arg1: tensor<i32>) -> tensor<3x3xf32> {
  %0 = "tf.Cumsum"(%arg0, %arg1) {exclusive = false, reverse = false} : (tensor<3x3xf32>, tensor<i32>) -> tensor<3x3xf32>
  return %0 : tensor<3x3xf32>
  // CHECK-LABEL: cumsum
  // CHECK: "tfl.cumsum"(%arg0, %arg1) {exclusive = false, reverse = false} : (tensor<3x3xf32>, tensor<i32>) -> tensor<3x3xf32>
}

func @cumsum_i64(%arg0: tensor<3x3xf32>, %arg1: tensor<i64>) -> tensor<3x3xf32> {
  %0 = "tf.Cumsum"(%arg0, %arg1) {exclusive = false, reverse = false} : (tensor<3x3xf32>, tensor<i64>) -> tensor<3x3xf32>
  return %0 : tensor<3x3xf32>
  // CHECK-LABEL: cumsum_i64
  // CHECK: "tfl.cast"
  // CHECK: "tfl.cumsum"
}

func @segmentsum(%arg0: tensor<3x3xf32>, %arg1: tensor<i32>) -> tensor<*xf32> {
  %0 = "tf.SegmentSum"(%arg0, %arg1) : (tensor<3x3xf32>, tensor<i32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
  // CHECK-LABEL: segmentsum
  // CHECK: "tfl.segment_sum"(%arg0, %arg1) : (tensor<3x3xf32>, tensor<i32>) -> tensor<*xf32>
}

func @segmentsum_i64(%arg0: tensor<3x3xf32>, %arg1: tensor<i64>) -> tensor<*xf32> {
  %0 = "tf.SegmentSum"(%arg0, %arg1) : (tensor<3x3xf32>, tensor<i64>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
  // CHECK-LABEL: segmentsum_i64
  // CHECK: "tfl.cast"
  // CHECK: "tfl.segment_sum"
}

func @rfft2d(%arg0: tensor<10x20x10x30xf32>, %arg1: tensor<2xi32>) -> tensor<10x20x10x30xcomplex<f32>> {
  %0 = "tf.RFFT2D"(%arg0, %arg1) : (tensor<10x20x10x30xf32>, tensor<2xi32>) -> tensor<10x20x10x30xcomplex<f32>>
  return %0 : tensor<10x20x10x30xcomplex<f32>>
  // CHECK-LABEL: rfft2d
  // CHECK: "tfl.rfft2d"(%arg0, %arg1) : (tensor<10x20x10x30xf32>, tensor<2xi32>) -> tensor<10x20x10x30xcomplex<f32>>
}

func @rfft2d_invalid(%arg0: tensor<10x20x10x30xf64>, %arg1: tensor<2xi32>) -> tensor<10x20x10x30xcomplex<f64>> {
  %0 = "tf.RFFT2D"(%arg0, %arg1) : (tensor<10x20x10x30xf64>, tensor<2xi32>) -> tensor<10x20x10x30xcomplex<f64>>
  return %0 : tensor<10x20x10x30xcomplex<f64>>
  // CHECK-LABEL: rfft2d_invalid
  // CHECK-NOT: "tfl.rfft2d"
}


func @conv3d_valid(%arg0: tensor<?x?x?x?x?xf32>,%arg1:  tensor<?x?x?x?x?xf32>) -> tensor<?x?x?x?x?xf32> {
  %0 = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1, 1]} : (tensor<?x?x?x?x?xf32>, tensor<?x?x?x?x?xf32>) -> tensor<?x?x?x?x?xf32>
  return %0: tensor<?x?x?x?x?xf32>

  // CHECK-LABEL: conv3d_valid
  // CHECK:  %cst = constant unit
  // CHECK:  [[BCT:%.*]] = "tfl.conv_3d"(%arg0, %arg1, %cst) {dilation_d_factor = 1 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "SAME", stride_d = 1 : i32, stride_h = 1 : i32, stride_w = 1 : i32} : (tensor<?x?x?x?x?xf32>, tensor<?x?x?x?x?xf32>, none) -> tensor<?x?x?x?x?xf32>
  // CHECK:  return [[BCT]] : tensor<?x?x?x?x?xf32>
}

func @conv3d_invalid_strides(%arg0: tensor<?x?x?x?x?xf32>,%arg1:  tensor<?x?x?x?x?xf32>) -> tensor<?x?x?x?x?xf32> {
  %0 = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [2, 1, 1, 1, 1]} : (tensor<?x?x?x?x?xf32>, tensor<?x?x?x?x?xf32>) -> tensor<?x?x?x?x?xf32>
  return %0: tensor<?x?x?x?x?xf32>
  // CHECK-LABEL: conv3d_invalid_strides
  // CHECK:  [[BCT:%.*]] = "tf.Conv3D"(%arg0, %arg1) {padding = "SAME", strides = [2, 1, 1, 1, 1]} : (tensor<?x?x?x?x?xf32>, tensor<?x?x?x?x?xf32>) -> tensor<?x?x?x?x?xf32>
  // CHECK:  return [[BCT]] : tensor<?x?x?x?x?xf32>
}

func @complex_abs(%arg0: tensor<1 x complex<f32>>) -> tensor<1xf32> {
  %0 = "tf.ComplexAbs"(%arg0) : (tensor<1 x complex<f32>>) -> tensor<1xf32>
  return %0: tensor<1xf32>

// CHECK-LABEL: complex_abs
// CHECK:  "tfl.complex_abs"(%arg0) : (tensor<1xcomplex<f32>>) -> tensor<1xf32>
// CHECK:  return
}

func @real(%arg0: tensor<1 x complex<f64>>) -> tensor<1xf64> {
  %0 = "tf.Real"(%arg0) : (tensor<1 x complex<f64>>) -> tensor<1xf64>
  return %0: tensor<1xf64>

// CHECK-LABEL: real
// CHECK:  "tfl.real"(%arg0) : (tensor<1xcomplex<f64>>) -> tensor<1xf64>
// CHECK:  return
}

func @imag(%arg0: tensor<1 x complex<f64>>) -> tensor<1xf64> {
  %0 = "tf.Imag"(%arg0) : (tensor<1 x complex<f64>>) -> tensor<1xf64>
  return %0: tensor<1xf64>

// CHECK-LABEL: imag
// CHECK:  "tfl.imag"(%arg0) : (tensor<1xcomplex<f64>>) -> tensor<1xf64>
// CHECK:  return
}

func @all(%arg0: tensor<2x2xi1>, %arg1: tensor<i32>) -> tensor<i1> {
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i32>) -> tensor<i1>
  return %0 : tensor<i1>

// CHECK-LABEL:all
// CHECK:  "tfl.reduce_all"(%arg0, %arg1) {keep_dims = false} : (tensor<2x2xi1>, tensor<i32>) -> tensor<i1>
}

func @all_i64axes(%arg0: tensor<8x16x16xi1>, %arg1: tensor<2xi64>) -> tensor<?xi1> {
  %0 = "tf.All"(%arg0, %arg1) {keep_dims = false} : (tensor<8x16x16xi1>, tensor<2xi64>) -> tensor<?xi1>
  return %0 : tensor<?xi1>

  // CHECK-LABEL: all_i64axes
  // CHECK: %[[V0:.*]] = "tfl.cast"(%arg1) : (tensor<2xi64>) -> tensor<2xi32>
  // CHECK: "tfl.reduce_all"(%arg0, %[[V0]]) {keep_dims = false} : (tensor<8x16x16xi1>, tensor<2xi32>) -> tensor<?xi1>
}

func @quantize_dequantize_v4(%arg0 : tensor<?x?xf32>) -> tensor<?x?xf32> {
  %cst = constant dense<0.0> : tensor<f32>
  %cst_0 = constant dense<255.0> : tensor<f32>
  %0 = "tf.QuantizeAndDequantizeV4"(%arg0, %cst, %cst_0) : (tensor<?x?xf32>, tensor<f32>, tensor<f32>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>

// CHECK-LABEL: quantize_dequantize_v4
// CHECK:  %[[QUANT:.*]] = "tfl.quantize"(%arg0) {qtype = tensor<?x?x!quant.uniform<u8:f32, 1.000000e+00>>} : (tensor<?x?xf32>) -> tensor<?x?x!quant.uniform<u8:f32, 1.000000e+00>>
// CHECK:  %[[DEQUANT:.*]] = "tfl.dequantize"(%[[QUANT]]) : (tensor<?x?x!quant.uniform<u8:f32, 1.000000e+00>>) -> tensor<?x?xf32>
// CHECK:  return %[[DEQUANT]]
}

func @conv3d_transpose(%arg0: tensor<2x5x6x8x2xf32>, %arg1: tensor<1x2x2x3x2xf32>, %arg2: tensor<5xi64>) -> tensor<?x?x?x?x?xf32> {
  %0 = "tf.Conv3DBackpropInputV2"(%arg2, %arg1, %arg0) {data_format = "NDHWC", dilations = [1, 1, 1, 1, 1], padding = "VALID", strides = [1, 2, 2, 2, 1]} : (tensor<5xi64>, tensor<1x2x2x3x2xf32>, tensor<2x5x6x8x2xf32>) -> tensor<?x?x?x?x?xf32>
  return %0 : tensor<?x?x?x?x?xf32>
  // CHECK-LABEL: conv3d_transpose
  // CHECK: %[[CST:.*]] = constant unit
  // CHECK: %[[OUT_SHAPE:.*]] = "tfl.cast"(%arg2) : (tensor<5xi64>) -> tensor<5xi32>
  // CHECK: %[[RESULT:.*]] = "tfl.conv_3d_transpose"(%[[OUT_SHAPE]], %arg1, %arg0, %[[CST]]) {dilation_d_factor = 1 : i32, dilation_h_factor = 1 : i32, dilation_w_factor = 1 : i32, fused_activation_function = "NONE", padding = "VALID", stride_d = 2 : i32, stride_h = 2 : i32, stride_w = 2 : i32} : (tensor<5xi32>, tensor<1x2x2x3x2xf32>, tensor<2x5x6x8x2xf32>, none) -> tensor<?x?x?x?x?xf32>
  // CHECK: return %[[RESULT]] : tensor<?x?x?x?x?xf32>
}

func @conv3d_transpose_unmatched_channels(%arg0: tensor<2x5x6x8x3xf32>, %arg1: tensor<1x2x2x3x2xf32>, %arg2: tensor<5xi64>) -> tensor<?x?x?x?x?xf32> {
  %0 = "tf.Conv3DBackpropInputV2"(%arg2, %arg1, %arg0) {data_format = "NDHWC", dilations = [1, 1, 1, 1, 1], padding = "VALID", strides = [1, 2, 2, 2, 1]} : (tensor<5xi64>, tensor<1x2x2x3x2xf32>, tensor<2x5x6x8x3xf32>) -> tensor<?x?x?x?x?xf32>
  return %0 : tensor<?x?x?x?x?xf32>
  // CHECK-LABEL: conv3d_transpose_unmatched_channels
  // CHECK: "tf.Conv3DBackpropInputV2"
}

func @conv3d_transpose_unsupported_strides(%arg0: tensor<2x5x6x8x2xf32>, %arg1: tensor<1x2x2x3x2xf32>, %arg2: tensor<5xi64>) -> tensor<?x?x?x?x?xf32> {
  %0 = "tf.Conv3DBackpropInputV2"(%arg2, %arg1, %arg0) {data_format = "NDHWC", dilations = [1, 1, 1, 1, 1], padding = "VALID", strides = [2, 2, 2, 2, 1]} : (tensor<5xi64>, tensor<1x2x2x3x2xf32>, tensor<2x5x6x8x2xf32>) -> tensor<?x?x?x?x?xf32>
  return %0 : tensor<?x?x?x?x?xf32>
  // CHECK-LABEL: conv3d_transpose_unsupported_strides
  // CHECK: "tf.Conv3DBackpropInputV2"
}

func @mul_i64(%arg0: tensor<14xi64>, %arg1: tensor<14xi64>) -> tensor<14xi64> {
  %0 = "tf.Mul"(%arg0, %arg1) : (tensor<14xi64>, tensor<14xi64>) -> tensor<14xi64>
  return %0: tensor<14xi64>

// CHECK-LABEL: mul_i64
// CHECK:  tfl.mul %arg0, %arg1 {fused_activation_function = "NONE"} : tensor<14xi64>
// CHECK:  return
}

func @broadcast_args(%arg0: tensor<3xi32>, %arg1: tensor<1xi32>) -> tensor<3xi32> {
  %0 = "tf.BroadcastArgs"(%arg0, %arg1) : (tensor<3xi32>, tensor<1xi32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>

// CHECK-LABEL:broadcast_args
// CHECK:  "tfl.broadcast_args"(%arg0, %arg1) : (tensor<3xi32>, tensor<1xi32>) -> tensor<3xi32>
}

func @broadcast_args_i64(%arg0: tensor<3xi64>, %arg1: tensor<1xi64>) -> tensor<3xi64> {
  %0 = "tf.BroadcastArgs"(%arg0, %arg1) : (tensor<3xi64>, tensor<1xi64>) -> tensor<3xi64>
  return %0 : tensor<3xi64>

// CHECK-LABEL:broadcast_args_i64
// CHECK:  "tfl.broadcast_args"(%arg0, %arg1) : (tensor<3xi64>, tensor<1xi64>) -> tensor<3xi64>
}

func @mul_with_high_dims_dynamic_shape(%arg0: tensor<8x7x6x5x4x3x2x1xf32>, %arg1: tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x4x3x2x1xf32> {
  %0 = "tf.Mul"(%arg0, %arg1): (tensor<8x7x6x5x4x3x2x1xf32>, tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x4x3x2x1xf32>
  return %0 : tensor<8x7x6x5x4x3x2x1xf32>

  // CHECK-LABEL:mul_with_high_dims_dynamic_shape
  // CHECK: %[[CST:.*]] = constant dense<[8, 7, 6, 5, 4, 3, 2, 1]> : tensor<8xi64>
  // CHECK: %[[SHAPE:.*]] = "tfl.shape"(%arg1) : (tensor<?x3x2x1xf32>) -> tensor<4xi64>
  // CHECK: %[[BROADCAST_ARGS:.*]] = "tfl.broadcast_args"(%[[CST]], %[[SHAPE]]) : (tensor<8xi64>, tensor<4xi64>) -> tensor<8xi64>
  // CHECK: %[[BROADCAST_TO:.*]] = "tfl.broadcast_to"(%arg1, %[[BROADCAST_ARGS]]) : (tensor<?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x4x3x2x1xf32>
  // CHECK: %[[MUL:.*]] = tfl.mul %arg0, %[[BROADCAST_TO]] {fused_activation_function = "NONE"} : tensor<8x7x6x5x4x3x2x1xf32>
  // CHECK: return %[[MUL]] : tensor<8x7x6x5x4x3x2x1xf32>
}

func @mul_with_high_dims_dynamic_shape_both_sides(%arg0: tensor<8x7x6x5x?x3x2x1xf32>, %arg1: tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xf32> {
  %0 = "tf.Mul"(%arg0, %arg1): (tensor<8x7x6x5x?x3x2x1xf32>, tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xf32>
  return %0 : tensor<8x7x6x5x?x3x2x1xf32>

  // CHECK-LABEL:mul_with_high_dims_dynamic_shape_both_sides
  // CHECK: %[[SHAPE:.*]] = "tfl.shape"(%arg0) : (tensor<8x7x6x5x?x3x2x1xf32>) -> tensor<8xi64>
  // CHECK: %[[SHAPE_1:.*]] = "tfl.shape"(%arg1) : (tensor<?x3x2x1xf32>) -> tensor<4xi64>
  // CHECK: %[[BROADCAST_ARGS:.*]] = "tfl.broadcast_args"(%[[SHAPE]], %[[SHAPE_1]]) : (tensor<8xi64>, tensor<4xi64>) -> tensor<8xi64>
  // CHECK: %[[BROADCAST_TO:.*]] = "tfl.broadcast_to"(%arg0, %[[BROADCAST_ARGS]]) : (tensor<8x7x6x5x?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: %[[BROADCAST_TO_1:.*]] = "tfl.broadcast_to"(%arg1, %[[BROADCAST_ARGS]]) : (tensor<?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: %[[MUL:.*]] = tfl.mul %[[BROADCAST_TO]], %[[BROADCAST_TO_1]] {fused_activation_function = "NONE"} : tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: return %[[MUL]] : tensor<8x7x6x5x?x3x2x1xf32>
}

func @select_v2_with_high_dims_dynamic_shape_both_sides(%arg0: tensor<8x7x6x5x?x3x2x1xf32>, %arg1: tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xf32> {
  %0 = "tf.Less"(%arg0, %arg1) : (tensor<8x7x6x5x?x3x2x1xf32>, tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xi1>
  %1 = "tf.SelectV2"(%0, %arg0, %arg1) : (tensor<8x7x6x5x?x3x2x1xi1>, tensor<8x7x6x5x?x3x2x1xf32>, tensor<?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xf32>
  return %1 : tensor<8x7x6x5x?x3x2x1xf32>

  // CHECK-LABEL:select_v2_with_high_dims_dynamic_shape_both_sides
  // CHECK: %[[SHAPE_0:.*]] = "tfl.shape"(%arg0) : (tensor<8x7x6x5x?x3x2x1xf32>) -> tensor<8xi64>
  // CHECK: %[[SHAPE_1:.*]] = "tfl.shape"(%arg1) : (tensor<?x3x2x1xf32>) -> tensor<4xi64>
  // CHECK: %[[BROADCAST_ARGS_0:.*]] = "tfl.broadcast_args"(%[[SHAPE_0]], %[[SHAPE_1]]) : (tensor<8xi64>, tensor<4xi64>) -> tensor<8xi64>
  // CHECK: %[[BROADCAST_TO_0:.*]] = "tfl.broadcast_to"(%arg0, %[[BROADCAST_ARGS_0]]) : (tensor<8x7x6x5x?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: %[[BROADCAST_TO_1:.*]] = "tfl.broadcast_to"(%arg1, %[[BROADCAST_ARGS_0]]) : (tensor<?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: %[[LESS:.*]] = tfl.less(%[[BROADCAST_TO_0]], %[[BROADCAST_TO_1]]) : (tensor<8x7x6x5x?x3x2x1xf32>, tensor<8x7x6x5x?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xi1>
  // CHECK: %[[SHAPE_2:.*]] = "tfl.shape"(%[[LESS]]) : (tensor<8x7x6x5x?x3x2x1xi1>) -> tensor<8xi64>
  // CHECK: %[[BROADCAST_ARGS_1:.*]] = "tfl.broadcast_args"(%[[BROADCAST_ARGS_0]], %[[SHAPE_2]]) : (tensor<8xi64>, tensor<8xi64>) -> tensor<8xi64>
  // CHECK: %[[BROADCAST_TO_2:.*]] = "tfl.broadcast_to"(%[[LESS]], %[[BROADCAST_ARGS_1]]) : (tensor<8x7x6x5x?x3x2x1xi1>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xi1>
  // CHECK: %[[BROADCAST_TO_3:.*]] = "tfl.broadcast_to"(%arg0, %[[BROADCAST_ARGS_1]]) : (tensor<8x7x6x5x?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: %[[BROADCAST_TO_4:.*]] = "tfl.broadcast_to"(%arg1, %[[BROADCAST_ARGS_1]]) : (tensor<?x3x2x1xf32>, tensor<8xi64>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: %[[SELECT_V2:.*]] = "tfl.select_v2"(%[[BROADCAST_TO_2]], %[[BROADCAST_TO_3]], %[[BROADCAST_TO_4]]) : (tensor<8x7x6x5x?x3x2x1xi1>, tensor<8x7x6x5x?x3x2x1xf32>, tensor<8x7x6x5x?x3x2x1xf32>) -> tensor<8x7x6x5x?x3x2x1xf32>
  // CHECK: return %[[SELECT_V2]] : tensor<8x7x6x5x?x3x2x1xf32>
}

