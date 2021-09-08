// RUN: tf-opt -xla-legalize-tf-with-tf2xla=device-type=XLA_CPU_JIT %s | FileCheck %s

// INVALID_DEVICE: xla-opt -xla-legalize-tf-with-tf2xla=device-type=INVALID_DEVICE %s | FileCheck %s

module attributes {tf.versions = {bad_consumers = [], min_consumer = 0 : i32, producer = 268 : i32}} {

// CHECK-LABEL: abs
// expected-error@+1 {{unsupported device}}
func @abs(%arg0: tensor<2xf32>) -> tensor<2xf32> {
  // CHECK: %[[RESULT:.*]] = "xla_hlo.abs"(%arg0) : (tensor<2xf32>) -> tensor<2xf32>
  %0 = "tf.Abs"(%arg0) : (tensor<2xf32>) -> tensor<2xf32>

  // return %[[RESULT]]
  return %0 : tensor<2xf32>
}

// CHECK-LABEL: unknown_op
func @unknown_op(%arg0: tensor<2xf32>) -> tensor<2xf32> {
  // CHECK: tf.CustomTestOp
  // expected-remark@+1 {{constant 20}}
  %0 = "tf.CustomTestOp"(%arg0) : (tensor<2xf32>) -> tensor<2xf32>

  return %0 : tensor<2xf32>
}

// CHECK-LABEL: not_whitelisted_op
func @not_whitelisted_op(%arg0: tensor<3xi32>, %arg1: tensor<i32>, %arg2: tensor<i32>) -> tensor<?x?x?xf32> {
  // CHECK: tf.TensorListReserve
  %0 = "tf.TensorListReserve"(%arg0, %arg1) : (tensor<3xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x?x?xf32>>>
  // CHECK: tf.TensorListGetItem
  %1 = "tf.TensorListGetItem"(%0, %arg2, %arg0) : (tensor<!tf.variant<tensor<?x?x?xf32>>>, tensor<i32>, tensor<3xi32>) -> tensor<?x?x?xf32>
  return %1 : tensor<?x?x?xf32>
}

// CHECK-LABEL: unranked_operand
func @unranked_operand(%arg0: tensor<*xf32>) -> tensor<*xf32> {
  // CHECK: tf.Abs
  // expected-remark@+1 {{lowering requires static shaped tensor operands}}
  %0 = "tf.Abs"(%arg0) : (tensor<*xf32>) -> tensor<*xf32>

  return %0 : tensor<*xf32>
}

// CHECK-LABEL: dynamic_operand
func @dynamic_operand(%arg0: tensor<?xf32>) -> tensor<?xf32> {
  // CHECK: tf.Abs
  // expected-remark@+1 {{lowering requires static shaped tensor operands}}
  %0 = "tf.Abs"(%arg0) : (tensor<?xf32>) -> tensor<?xf32>

  return %0 : tensor<?xf32>
}

// CHECK-LABEL: tuple_type
func @tuple_type(%arg0: tuple<tensor<f32>, tensor<i32>>) -> tensor<f32> {
  // Verifies that the pass can handle operands of non-tensor type like tuple
  // from non TensorFlow ops.
  %0 = "xla_hlo.get_tuple_element"(%arg0) {index = 0 : i32} : (tuple<tensor<f32>, tensor<i32>>) -> tensor<f32>
  return %0 : tensor<f32>
}

// CHECK-LABEL: unsupported_dtype
func @unsupported_dtype(%arg0: tensor<2x!tf.variant>) -> tensor<2x!tf.variant> {
  // CHECK: tf.AddN
  // expected-remark@+1 {{unsupported type: tensor<2x!tf.variant>}}
  %0 = "tf.AddN"(%arg0, %arg0) : (tensor<2x!tf.variant>, tensor<2x!tf.variant>) -> tensor<2x!tf.variant>

  return %0 : tensor<2x!tf.variant>
}

// CHECK-LABEL: multiple_dialect_ops
func @multiple_dialect_ops(%arg0: tensor<2xf32>) -> tensor<2xf32> {
  // CHECK: xla_hlo.negate
  %0 = "xla_hlo.negate"(%arg0) : (tensor<2xf32>) -> tensor<2xf32>
  // CHECK: xla_hlo.abs
  %1 = "tf.Abs"(%0) : (tensor<2xf32>) -> tensor<2xf32>

  return %1 : tensor<2xf32>
}

// CHECK-LABEL: binary_op
func @binary_op(%arg0: tensor<2xf32>, %arg1: tensor<2xf32>) -> tensor<2xf32> {
  // CHECK: xla_hlo.atan2 %arg0, %arg1 : tensor<2xf32>
  %0 = "tf.Atan2"(%arg0, %arg1) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// CHECK-LABEL: binary_op_broadcast
func @binary_op_broadcast(%arg0: tensor<4x1xf32>, %arg1: tensor<4x1x4xf32>) -> tensor<4x4x4xf32> {
  // CHECK: %[[BROADCAST0:.*]] = "xla_hlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[1, 2]> : tensor<2xi64>} : (tensor<4x1xf32>) -> tensor<4x4x1xf32>
  // CHECK: %[[RESHAPE0:.*]] = "xla_hlo.reshape"(%[[BROADCAST0]]) : (tensor<4x4x1xf32>) -> tensor<4x4xf32>
  // CHECK: %[[UPDATED_ARG0:.*]] = "xla_hlo.broadcast_in_dim"(%[[RESHAPE0]]) {broadcast_dimensions = dense<[0, 1]> : tensor<2xi64>} : (tensor<4x4xf32>) -> tensor<4x4x4xf32>

  // CHECK: %[[RESHAPE1:.*]] = "xla_hlo.reshape"(%arg1) : (tensor<4x1x4xf32>) -> tensor<4x4xf32>
  // CHECK: %[[UPDATED_ARG1:.*]] = "xla_hlo.broadcast_in_dim"(%[[RESHAPE1]]) {broadcast_dimensions = dense<[0, 2]> : tensor<2xi64>} : (tensor<4x4xf32>) -> tensor<4x4x4xf32>

  // CHECK: %[[RESULT:.*]] = xla_hlo.atan2 %[[UPDATED_ARG0]], %[[UPDATED_ARG1]] : tensor<4x4x4xf32>
  // CHECK: return %[[RESULT]] : tensor<4x4x4xf32>

  %0 = "tf.Atan2"(%arg0, %arg1) : (tensor<4x1xf32>, tensor<4x1x4xf32>) -> tensor<4x4x4xf32>
  return %0: tensor<4x4x4xf32>
}

// CHECK-LABEL: func @ternary_op
func @ternary_op(%arg0: tensor<2xi1>, %arg1: tensor<2xi32>, %arg2: tensor<2xi32>) -> tensor<2xi32> {
  // CHECK: "xla_hlo.select"(%arg0, %arg1, %arg2)
  %0 = "tf.SelectV2"(%arg0, %arg1, %arg2) : (tensor<2xi1>, tensor<2xi32>, tensor<2xi32>) -> tensor<2xi32>
  return %0: tensor<2xi32>
}

// CHECK-LABEL: func @convert
func @convert(%arg0: tensor<2xi32>) -> tensor<2xf32> {
  // CHECK: "xla_hlo.convert"(%arg0) : (tensor<2xi32>) -> tensor<2xf32>
  %0 = "tf.Cast"(%arg0) {Truncate = false} : (tensor<2xi32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// CHECK-LABEL: func @constant
func @constant(%arg0: tensor<2xf32>) -> tensor<2xf32> {
  // CHECK: %[[SCALAR_ONE:.*]] = xla_hlo.constant dense<1.000000e+00> : tensor<f32>
  // CHECK: %[[ONE:.*]] = "xla_hlo.broadcast_in_dim"(%[[SCALAR_ONE]]) {broadcast_dimensions = dense<[]> : tensor<0xi64>} : (tensor<f32>) -> tensor<2xf32>
  // CHECK: %[[RESULT:.*]] = xla_hlo.divide %[[ONE]], %arg0 : tensor<2xf32>
  // CHECK: return %[[RESULT]]

  %0 = "tf.Inv"(%arg0) : (tensor<2xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// CHECK-LABEL: func @greater
func @greater(%arg0: tensor<2xi32>) -> tensor<2xi1> {
  // CHECK-NEXT:  "xla_hlo.compare"(%arg0, %arg0) {comparison_direction = "GT"}
  %0 = "tf.Greater"(%arg0, %arg0) : (tensor<2xi32>, tensor<2xi32>) -> tensor<2xi1>
  return %0: tensor<2xi1>
}

// CHECK-LABEL: func @const_inputs
// CHECK-SAME: (%[[ARG0:.*]]: tensor<2x2xf64>, %[[ARG1:.*]]: tensor<f64>,
func @const_inputs(%arg0: tensor<2x2xf64>, %arg1: tensor<f64>, %arg2: tensor<2xi32>, %arg3: tensor<2xi32>, %arg4: tensor<2xi32>) -> tensor<6x5xf64> {

  // CHECK: "xla_hlo.pad"(%[[ARG0]], %[[ARG1]])
  // CHECK-SAME-DAG: edge_padding_high = dense<[1, 2]> : tensor<2xi64>
  // CHECK-SAME-DAG: edge_padding_low = dense<[2, 1]> : tensor<2xi64>
  // CHECK-SAME-DAG: interior_padding = dense<[1, 0]> : tensor<2xi64>

  %0 = xla_hlo.constant dense<[2, 1]> : tensor<2xi32>
  %1 = xla_hlo.constant dense<[1, 2]> : tensor<2xi32>
  %2 = xla_hlo.constant dense<[1, 0]> : tensor<2xi32>
  %3 = "tf.XlaPad"(%arg0, %arg1, %0, %1, %2) : (tensor<2x2xf64>, tensor<f64>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<6x5xf64>
  return %3 : tensor<6x5xf64>
}

func @non_const_inputs(%arg0: tensor<2x2xf64>, %arg1: tensor<f64>, %arg2: tensor<2xi32>, %arg3: tensor<2xi32>, %arg4: tensor<2xi32>) -> tensor<6x5xf64> {
  // expected-remark@+1 {{lowering requires operand #2 to be a constant}}
  %0 = "tf.XlaPad"(%arg0, %arg1, %arg2, %arg3, %arg4) : (tensor<2x2xf64>, tensor<f64>, tensor<2xi32>, tensor<2xi32>, tensor<2xi32>) -> tensor<6x5xf64>
  return %0 : tensor<6x5xf64>
}

// CHECK-LABEL: dynamic_result_type
func @dynamic_result_type(%arg0: tensor<2xf32>) -> tensor<*xf32> {
  // CHECK: %[[RESULT:.*]] = "xla_hlo.abs"(%arg0) : (tensor<2xf32>) -> tensor<2xf32>
  // CHECK: tensor_cast %0 : tensor<2xf32> to tensor<*xf32>
  %0 = "tf.Abs"(%arg0) : (tensor<2xf32>) -> tensor<*xf32>

  // return %[[RESULT]]
  return %0 : tensor<*xf32>
}

func @truncated_normal() -> tensor<2x2xf32> {
  // CHECK-NOT: tf.TruncatedNormal
  %0 = xla_hlo.constant dense<[2, 2]> : tensor<2xi32>
  %1 = "tf.TruncatedNormal"(%0) {T = i32, device = "", dtype = f32, seed = 0 : i64, seed2 = 1950157571 : i64} : (tensor<2xi32>) -> tensor<2x2xf32>
  return %1 : tensor<2x2xf32>
}

// CHECK-LABEL: dynamic_update_slice
// CHECK-SAME: (%[[ARG0:.*]]: tensor<3x4xi32>, %[[ARG1:.*]]: tensor<2x2xi32>, %[[ARG2:.*]]: tensor<2xi32>
func @dynamic_update_slice(%arg0: tensor<3x4xi32>, %arg1: tensor<2x2xi32>, %arg2: tensor<2xi32>) -> tensor<3x4xi32> {

  // CHECK: %[[SLICE0:.*]] = "xla_hlo.slice"(%[[ARG2]])
  // CHECK-DAG-SAME: start_indices = dense<0> : tensor<1xi64>
  // CHECK-DAG-SAME: limit_indices = dense<1> : tensor<1xi64>
  // CHECK-DAG-SAME: strides = dense<1> : tensor<1xi64>
  // CHECK-SAME: (tensor<2xi32>) -> tensor<1xi32>
  // CHECK: %[[DIM0:.*]] = "xla_hlo.reshape"(%[[SLICE0]]) : (tensor<1xi32>) -> tensor<i32>

  // CHECK: %[[SLICE1:.*]] = "xla_hlo.slice"(%[[ARG2]])
  // CHECK-DAG-SAME: start_indices = dense<1> : tensor<1xi64>
  // CHECK-DAG-SAME: limit_indices = dense<2> : tensor<1xi64>
  // CHECK-DAG-SAME: strides = dense<1> : tensor<1xi64>
  // CHECK-SAME: (tensor<2xi32>) -> tensor<1xi32>
  // CHECK: %[[DIM1:.*]] = "xla_hlo.reshape"(%[[SLICE1]]) : (tensor<1xi32>) -> tensor<i32>

  // CHECK: "xla_hlo.dynamic-update-slice"(%[[ARG0]], %[[ARG1]], %[[DIM0]], %[[DIM1]])

  %0 = "tf.XlaDynamicUpdateSlice"(%arg0, %arg1, %arg2) : (tensor<3x4xi32>, tensor<2x2xi32>, tensor<2xi32>) -> tensor<3x4xi32>
  return %0: tensor<3x4xi32>
}

// CHECK-LABEL: @sparse_to_dense
// CHECK-SAME: (%[[ARG0:.*]]: tensor<3x2xi32>, %[[ARG1:.*]]: tensor<3xf32>, %[[ARG2:.*]]: tensor<f32>)
func @sparse_to_dense(%arg0: tensor<3x2xi32>, %arg1: tensor<3xf32>, %arg2: tensor<f32>) -> tensor<3x3xf32> {

// CHECK:      %[[CST:.*]] = xla_hlo.constant dense<3> : tensor<2xi32>
// CHECK:      %[[DEFAULT:.*]] = "xla_hlo.broadcast_in_dim"(%[[ARG2]]) {broadcast_dimensions = dense<[]> : tensor<0xi64>} : (tensor<f32>) -> tensor<3x3xf32>

// CHECK:      %[[RESULT:.*]] = "xla_hlo.scatter"(%[[DEFAULT]], %[[ARG0]], %[[ARG1]]) ( {
// CHECK:      ^bb0(%[[ARG3:.*]]: tensor<f32>, %[[ARG4:.*]]: tensor<f32>):  // no predecessors
// CHECK:        "xla_hlo.return"(%[[ARG4]]) : (tensor<f32>) -> ()
// CHECK:      })
// CHECK-SAME: indices_are_sorted = false
// CHECK-SAME: scatter_dimension_numbers
// CHECK-SAME:   index_vector_dim = 1 : i64
// CHECK-SAME:   inserted_window_dims = dense<[0, 1]> : tensor<2xi64>
// CHECK-SAME:   scatter_dims_to_operand_dims = dense<[0, 1]> : tensor<2xi64>
// CHECK-SAME:   update_window_dims = dense<[]> : tensor<0xi64>
// CHECK-SAME: unique_indices = false
// CHECK-SAME: (tensor<3x3xf32>, tensor<3x2xi32>, tensor<3xf32>) -> tensor<3x3xf32>

// return %[[RESULT]] : tensor<3x3xf32>

  %cst = xla_hlo.constant dense<3> : tensor<2xi32>
  %0 = "tf.SparseToDense"(%arg0, %cst, %arg1, %arg2) {validate_indices = true}: (tensor<3x2xi32>, tensor<2xi32>, tensor<3xf32>, tensor<f32>) -> tensor<3x3xf32>
  return %0 : tensor<3x3xf32>
}

// CHECK-LABEL: fft
func @fft(%arg0: tensor<3x5x8xcomplex<f32>>) -> tensor<3x5x8xcomplex<f32>> {
  // CHECK: "xla_hlo.fft"(%arg0)
  %0 = "tf.FFT"(%arg0) : (tensor<3x5x8xcomplex<f32>>) -> tensor<3x5x8xcomplex<f32>>
  return %0 : tensor<3x5x8xcomplex<f32>>
}

// CHECK-LABEL: reverse_sequence
func @reverse_sequence(%arg0: tensor<4x2x3x1x1xi32>, %arg1: tensor<3xi32>) -> tensor<4x2x3x1x1xi32> {
  // CHECK-NOT: tf.ReverseSequence
  %0 = "tf.ReverseSequence"(%arg0, %arg1) {batch_dim = 2 : i64, seq_dim = 0 : i64}: (tensor<4x2x3x1x1xi32>, tensor<3xi32>) -> tensor<4x2x3x1x1xi32>
  return %0 : tensor<4x2x3x1x1xi32>
}

// CHECK-LABEL: mirror_pad
func @mirror_pad(%arg0: tensor<2x3xcomplex<f64>>) -> tensor<4x7xcomplex<f64>> {
  %0 = xla_hlo.constant dense<[[1, 1], [2, 2]]> : tensor<2x2xi32>
  // CHECK-NOT: tf.MirrorPad
  %1 = "tf.MirrorPad"(%arg0, %0) {mode = "SYMMETRIC"} : (tensor<2x3xcomplex<f64>>, tensor<2x2xi32>) -> tensor<4x7xcomplex<f64>>
  return %1 : tensor<4x7xcomplex<f64>>
}

// CHECK-LABEL: bucketize
func @bucketize(%arg0: tensor<2x5xf32>) -> tensor<2x5xi32> {
  // CHECK-NOT: tf.Bucketize
  %0 = "tf.Bucketize"(%arg0) {boundaries = [0.000000e+00 : f32, 3.000000e+00 : f32, 8.000000e+00 : f32, 1.100000e+01 : f32]} : (tensor<2x5xf32>) -> tensor<2x5xi32>
  return %0 : tensor<2x5xi32>
}

// CHECK-LABEL: arg_min
func @arg_min(%arg0: tensor<6xf64>) -> tensor<i32> {
  // CHECK-NOT: ArgMin
  %0 = xla_hlo.constant dense<0> : tensor<i32>
  %1 = "tf.ArgMin"(%arg0, %0) : (tensor<6xf64>, tensor<i32>) -> tensor<i32>
  return %1 : tensor<i32>
}

// TODO(hinsu): Add a test with a valid TF op for which tf2xla kernel is
// available but doesn't support this instance.
}
