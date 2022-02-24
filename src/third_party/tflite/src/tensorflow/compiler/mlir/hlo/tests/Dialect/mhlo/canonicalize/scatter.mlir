// RUN: mlir-hlo-opt %s -split-input-file -pass-pipeline='builtin.func(canonicalize)' | FileCheck %s

// Folding this case would explode the IR
func @scatter_fold_explosion() ->  tensor<512x1x6400x6400xf32> {
  %base = mhlo.constant dense<0.000000e+00> : tensor<512x1x6400x6400xf32>
  %index = mhlo.constant dense<1> : tensor<1xi32>
  %update = mhlo.constant dense<1.000000e+00> : tensor<511x1x6400x6400xf32>
  // CHECK: mhlo.scatter
  %scatter = "mhlo.scatter"(%base, %index, %update) ({
    ^bb0(%arg5: tensor<f32>, %arg6: tensor<f32>):
      "mhlo.return"(%arg6) : (tensor<f32>) -> ()
  }) {indices_are_sorted = true, scatter_dimension_numbers = #mhlo.scatter<update_window_dims = [0, 1, 2, 3], scatter_dims_to_operand_dims = [3]>, unique_indices = true} : (tensor<512x1x6400x6400xf32>, tensor<1xi32>, tensor<511x1x6400x6400xf32>) -> tensor<512x1x6400x6400xf32>

  return %scatter :  tensor<512x1x6400x6400xf32>
}

// -----

// Verify that a full overwrite of the "base" with a scatter is not folded
// if the type mismatch.
// TODO(mhlo): this would be nice to handle: the update could be broadcasted
// to the type of the base here.
func @scatter_full_overwrite_type_mismatch(%base : tensor<1x1x1xf64>) ->  tensor<1x1x1xf64> {
  %0 = mhlo.constant dense<0.28209479177387814> : tensor<1xf64>
  %1 = mhlo.constant dense<0> : tensor<2xi32>
  %scatter = "mhlo.scatter"(%base, %1, %0) ({
  ^bb0(%arg11: tensor<f64>, %arg12: tensor<f64>):
    "mhlo.return"(%arg12) : (tensor<f64>) -> ()
  }) {indices_are_sorted = true, scatter_dimension_numbers = #mhlo.scatter<update_window_dims = [0], inserted_window_dims = [0, 1], scatter_dims_to_operand_dims = [0, 1]>, unique_indices = true} : (tensor<1x1x1xf64>, tensor<2xi32>, tensor<1xf64>) -> tensor<1x1x1xf64>

  // CHECK: %[[SCATTER:.*]] = "mhlo.scatter
  // CHECK: return %[[SCATTER]]
  return %scatter :  tensor<1x1x1xf64>
}

// -----

// Verify that a full overwrite of the "base" with a scatter is correctly folded
// even if the tensor is huge.
func @scatter_full_overwrite() ->  tensor<512x1x6400x6400xf32> {
  %base = mhlo.constant dense<0.000000e+00> : tensor<512x1x6400x6400xf32>
  %index = mhlo.constant dense<0> : tensor<1xi32>
  %update = mhlo.constant dense<1.000000e+00> : tensor<512x1x6400x6400xf32>
  %scatter = "mhlo.scatter"(%base, %index, %update) ({
    ^bb0(%arg5: tensor<f32>, %arg6: tensor<f32>):
      "mhlo.return"(%arg6) : (tensor<f32>) -> ()
  }) {indices_are_sorted = true, scatter_dimension_numbers = #mhlo.scatter<update_window_dims = [0, 1, 2, 3], scatter_dims_to_operand_dims = [3]>, unique_indices = true} : (tensor<512x1x6400x6400xf32>, tensor<1xi32>, tensor<512x1x6400x6400xf32>) -> tensor<512x1x6400x6400xf32>

  // CHECK: %[[FOLD:.*]] = mhlo.constant dense<1.000000e+00> : tensor<512x1x6400x6400xf32>
  // CHECK: return %[[FOLD]]
  return %scatter :  tensor<512x1x6400x6400xf32>
}

// -----

// Verify that a full overwrite of the "base" with a scatter is correctly folded
// even if the base and update are not constant values.
func @scatter_full_overwrite_non_const(
        %base : tensor<512x1x6400x6400xf32>,
        %update : tensor<512x1x6400x6400xf32>) ->  tensor<512x1x6400x6400xf32> {
  %index = mhlo.constant dense<0> : tensor<1xi32>
  %scatter = "mhlo.scatter"(%base, %index, %update) ({
    ^bb0(%arg5: tensor<f32>, %arg6: tensor<f32>):
      "mhlo.return"(%arg6) : (tensor<f32>) -> ()
  }) {indices_are_sorted = true, scatter_dimension_numbers = #mhlo.scatter<update_window_dims = [0, 1, 2, 3], scatter_dims_to_operand_dims = [3]>, unique_indices = true} : (tensor<512x1x6400x6400xf32>, tensor<1xi32>, tensor<512x1x6400x6400xf32>) -> tensor<512x1x6400x6400xf32>

  // CHECK: return %arg1
  return %scatter :  tensor<512x1x6400x6400xf32>
}

// -----

// Verify that a full overwrite of the "base" with a scatter is not folded when
// there is a non-identity computation.
func public @scatter_non_identity(%arg0: tensor<12xbf16>, %arg1: tensor<12xbf16>) -> tensor<12xbf16> {
  %0 = mhlo.constant dense<0> : tensor<1xi32>
  %1 = "mhlo.scatter"(%arg0, %0, %arg1) ({
  ^bb0(%arg2: tensor<bf16>, %arg3: tensor<bf16>):
    %2 = mhlo.add %arg2, %arg3 : tensor<bf16>
    "mhlo.return"(%2) : (tensor<bf16>) -> ()
  }) {indices_are_sorted = true, scatter_dimension_numbers = #mhlo.scatter<update_window_dims = [0], scatter_dims_to_operand_dims = [0]>, unique_indices = true} : (tensor<12xbf16>, tensor<1xi32>, tensor<12xbf16>) -> tensor<12xbf16>
  // CHECK: %[[SCATTER:.*]] = "mhlo.scatter
  // CHECK: return %[[SCATTER]]
  return %1 : tensor<12xbf16>
}
