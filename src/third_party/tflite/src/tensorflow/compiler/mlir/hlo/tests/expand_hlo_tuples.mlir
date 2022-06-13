// RUN: mlir-hlo-opt %s -split-input-file -expand-hlo-tuples='entry-function=main' | FileCheck %s
// Check if the `expand-hlo-tuples` pass adds the right variable to return_op and function return type.

func @main(%arg0: tensor<1x1xf32>, %arg1: tensor<1x8x8x16xf32>) -> tuple<tensor<1024xf32>, tensor<1xf32>> {
  %1 = "mhlo.reshape"(%arg0) : (tensor<1x1xf32>) -> tensor<1xf32>
  %2 = "mhlo.reshape"(%arg1) : (tensor<1x8x8x16xf32>) -> tensor<1024xf32>
  %3 = "mhlo.tuple"(%2, %1) {name = "tuple.374"} : (tensor<1024xf32>, tensor<1xf32>) -> tuple<tensor<1024xf32>, tensor<1xf32>>
  return %3 : tuple<tensor<1024xf32>, tensor<1xf32>>
  // CHECK: %[[RES0:.*]] = "mhlo.reshape"(%arg0) : (tensor<1x1xf32>) -> tensor<1xf32>
  // CHECK: %[[RES1:.*]] = "mhlo.reshape"(%arg1) : (tensor<1x8x8x16xf32>) -> tensor<1024xf32>
  // CHECK: return %[[RES1]], %[[RES0]] : tensor<1024xf32>, tensor<1xf32>
}

// -----
func @main(%arg0: tensor<1x224x224x3xf16>, %arg1: tensor<f32>) -> tensor<1x224x224x3xf16> {
  return %arg0 : tensor<1x224x224x3xf16>
}

// -----

func @main(%arg0: tuple<tensor<1024xf32>, tensor<1xf32>>) -> tuple<tensor<1024xf32>, tensor<1xf32>> {
  return %arg0 : tuple<tensor<1024xf32>, tensor<1xf32>>
}

// CHECK:   func @main(%[[VAL_0:.*]]: tensor<1024xf32>, %[[VAL_1:.*]]: tensor<1xf32>) -> (tensor<1024xf32>, tensor<1xf32>) {
// CHECK:           %[[VAL_2:.*]] = "mhlo.tuple"(%[[VAL_0]], %[[VAL_1]]) : (tensor<1024xf32>, tensor<1xf32>) -> tuple<tensor<1024xf32>, tensor<1xf32>>
// CHECK:           return %[[VAL_0]], %[[VAL_1]] : tensor<1024xf32>, tensor<1xf32>
// CHECK:         }
