// RUN: mlir-hlo-opt --mhlo-test-infer-shaped-type-methods --allow-unregistered-dialect --split-input-file --verify-diagnostics %s | FileCheck %s

// CHECK-LABEL: @select
// CHECK-SAME: (%{{.*}}: tensor<i1>, %[[SHAPED_ARG:.*]]: tensor<2x?xf32>, %{{.*}}: tensor<2x?xf32>
func.func @select(%pred : tensor<i1>, %a : tensor<2x?xf32>, %b : tensor<2x?xf32>)
    -> tensor<2xindex> {
  // CHECK: %[[SHAPE:.*]] = shape.shape_of %[[SHAPED_ARG]] : tensor<2x?xf32> -> tensor<2xindex>
  // CHECK: return %[[SHAPE]] : tensor<2xindex>
  %0 = "mhlo.select"(%pred, %a, %b)
      : (tensor<i1>, tensor<2x?xf32>, tensor<2x?xf32>) -> tensor<2x?xf32>
  %1 = "mhlo_test.reify_return_type_shapes"(%0)
      : (tensor<2x?xf32>) -> tensor<2xindex>
  func.return %1 : tensor<2xindex>
}

// -----

// CHECK-LABEL: @compare
// CHECK-SAME: (%[[A:.*]]: tensor<2x?xf32>,
func.func @compare(%a : tensor<2x?xf32>, %b : tensor<2x?xf32>) -> tensor<2xindex> {
  // CHECK: %[[SHAPE:.*]] = shape.shape_of %[[A]] : tensor<2x?xf32> -> tensor<2xindex>
  // CHECK: return %[[SHAPE]] : tensor<2xindex>
  %0 = "mhlo.compare"(%a, %b) {comparison_direction = #mhlo<"comparison_direction NE">}
      : (tensor<2x?xf32>, tensor<2x?xf32>) -> tensor<2x?xi1>
  %1 = "mhlo_test.reify_return_type_shapes"(%0)
      : (tensor<2x?xi1>) -> tensor<2xindex>
  func.return %1 : tensor<2xindex>
}

// -----

// CHECK-LABEL: @select
func.func @select(%pred : tensor<i1>, %a : tensor<2x2xf32>, %b : tensor<2x2xf32>)
    -> tensor<2x2xindex> {
  %0 = "mhlo.select"(%pred, %a, %b)
      : (tensor<i1>, tensor<2x2xf32>, tensor<2x2xf32>) -> tensor<2x2xf32>
  %1 = "mhlo_test.get_return_type_components"(%0)
      : (tensor<2x2xf32>) -> tensor<2x2xindex>
// CHECK: %1 = "mhlo_test.return_type_components"(%0) {dims0 = [2, 2], element_type0 = f32} : (tensor<2x2xf32>) -> tensor<2x2xindex>
  func.return %1 : tensor<2x2xindex>
}

// -----

// CHECK-LABEL: @compare
func.func @compare(%a : tensor<2x2xf32>, %b : tensor<2x2xf32>) -> tensor<2x2xindex> {
  %0 = "mhlo.compare"(%a, %b) {comparison_direction = #mhlo<"comparison_direction NE">}
      : (tensor<2x2xf32>, tensor<2x2xf32>) -> tensor<2x2xi1>
  %1 = "mhlo_test.get_return_type_components"(%0)
      : (tensor<2x2xi1>) -> tensor<2x2xindex>
// CHECK: %1 = "mhlo_test.return_type_components"(%0) {dims0 = [2, 2], element_type0 = i1} : (tensor<2x2xi1>) -> tensor<2x2xindex>
  func.return %1 : tensor<2x2xindex>
}

// -----

// CHECK-LABEL: @broadcast
func.func @broadcast(%a : tensor<3xi32>) -> tensor<1x2x3xindex> {
  %0 = "mhlo.broadcast"(%a) {broadcast_sizes = dense<[1, 2]> : tensor<2xi64>}
      : (tensor<3xi32>) -> tensor<1x2x3xi32>
  %1 = "mhlo_test.get_return_type_components"(%0)
      : (tensor<1x2x3xi32>) -> tensor<1x2x3xindex>
// CHECK: %1 = "mhlo_test.return_type_components"(%0) {dims0 = [1, 2, 3], element_type0 = i32} : (tensor<1x2x3xi32>) -> tensor<1x2x3xindex>
  func.return %1 : tensor<1x2x3xindex>
}

// -----

func.func @broadcast(%a : tensor<3xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{Broadcast with negative dimension size -2}}
  %0 = "mhlo.broadcast"(%a) {broadcast_sizes = dense<[1, -2]> : tensor<2xi64>}
      : (tensor<3xi32>) -> tensor<1x2x3xi32>
  func.return %0 : tensor<1x2x3xi32>
}

// -----

// CHECK-LABEL: @dynamic_slice
func.func @dynamic_slice(%arg0: tensor<3x4xi32>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<1x4xindex> {
  %0 = "mhlo.dynamic-slice"(%arg0, %arg1, %arg2) {slice_sizes = dense<[1, 4]> : tensor<2xi64>} : (tensor<3x4xi32>, tensor<i64>, tensor<i64>) -> tensor<1x4xi32>
  %1 = "mhlo_test.get_return_type_components"(%0)
      : (tensor<1x4xi32>) -> tensor<1x4xindex>
  func.return %1 : tensor<1x4xindex>
}

// -----

// CHECK-LABEL: @pad
func.func @pad(%arg0: tensor<1x2x3xf16>, %arg1: tensor<f16>) -> tensor<2x4x7xf16> {
  %0 = "mhlo.pad"(%arg0, %arg1) {
    edge_padding_high = dense<[1, 1, 0]> : tensor<3xi64>,
    edge_padding_low = dense<[0, 1, 2]> : tensor<3xi64>,
    interior_padding = dense<[0, 0, 1]> : tensor<3xi64>
  } : (tensor<1x2x3xf16>, tensor<f16>) -> tensor<2x4x7xf16>
  %1 = "mhlo_test.get_return_type_components"(%0)
      : (tensor<2x4x7xf16>) -> tensor<2x4x7xindex>
// CHECK: %1 = "mhlo_test.return_type_components"(%0) {dims0 = [2, 4, 7], element_type0 = f16} : (tensor<2x4x7xf16>) -> tensor<2x4x7xindex>
  func.return %0 : tensor<2x4x7xf16>
}
