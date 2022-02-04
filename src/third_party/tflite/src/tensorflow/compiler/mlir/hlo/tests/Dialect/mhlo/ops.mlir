// RUN: mlir-hlo-opt %s -verify-diagnostics -split-input-file -allow-unregistered-dialect | FileCheck %s

// Tests for types, ops with custom constraints, verifiers, printer or parser
// methods.

// CHECK-LABEL: func private @token_type() -> !mhlo.token
func private @token_type() -> !mhlo.token

// -----

// expected-error@+1 {{unknown mhlo type: foobar}}
func private @invalid_type() -> !mhlo.foobar

// -----

// CHECK-LABEL: func @reduce_scatter
func @reduce_scatter(%data: tensor<4x16xf32>) -> tensor<4x4xf32> {
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 1 : i64} : (tensor<4x16xf32>) -> tensor<4x4xf32>
  return %0 : tensor<4x4xf32>
}

// -----

func @invalid_reduce_scatter(%data: tensor<4x16xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{operand scatter dimension has size 16, expected to be a multiple of result scatter dimension size 5}}
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 1 : i64} : (tensor<4x16xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @invalid_reduce_scatter(%data: tensor<4x0xf32>) -> tensor<4x4xf32> {
  // expected-error@+1 {{operand scatter dimension cannot be zero}}
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 1 : i64} : (tensor<4x0xf32>) -> tensor<4x4xf32>
  return %0 : tensor<4x4xf32>
}

// -----

func @invalid_reduce_scatter(%data: tensor<4x16xf32>) -> tensor<4x0xf32> {
  // expected-error@+1 {{result scatter dimension cannot be zero}}
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 1 : i64} : (tensor<4x16xf32>) -> tensor<4x0xf32>
  return %0 : tensor<4x0xf32>
}

// -----

func @invalid_reduce_scatter(%data: tensor<4x16xf32>) -> tensor<4xf32> {
  // expected-error@+1 {{operand and result should have same rank}}
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 1 : i64} : (tensor<4x16xf32>) -> tensor<4xf32>
  return %0 : tensor<4xf32>
}

// -----

func @invalid_reduce_scatter(%data: tensor<4x16xf32>) -> tensor<4x4xf32> {
  // expected-error@+1 {{scatter dim should be less than operand/result rank}}
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 4 : i64} : (tensor<4x16xf32>) -> tensor<4x4xf32>
  return %0 : tensor<4x4xf32>
}

// -----

func @invalid_reduce_scatter(%data: tensor<4x16xf32>) -> tensor<3x4xf32> {
  // expected-error@+1 {{non scatter dimensions should be same for operand (4) and result (3)}}
  %0 = "mhlo.reduce_scatter"(%data) ( {
    // reduction computation
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
      scatter_dimension = 1 : i64} : (tensor<4x16xf32>) -> tensor<3x4xf32>
  return %0 : tensor<3x4xf32>
}

// -----

// CHECK-LABEL: func @alltoall
func @alltoall(%data: tensor<4x16xf32>) -> tensor<16x4xf32> {
  %0 = "mhlo.all_to_all"(%data) {
    split_dimension = 1 : i64,
    concat_dimension = 0 : i64,
    split_count = 4 : i64,
    replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>
  } : (tensor<4x16xf32>) -> tensor<16x4xf32>
  return %0 : tensor<16x4xf32>
}

// -----

// CHECK-LABEL: func @alltoall_unranked_input
func @alltoall_unranked_input(%data: tensor<*xf32>) -> tensor<*xf32> {
  %0 = "mhlo.all_to_all"(%data) {
    split_dimension = 1 : i64,
    concat_dimension = 0 : i64,
    split_count = 5 : i64,
    replica_groups = dense<[[0, 1, 2, 3, 4]]> : tensor<1x5xi64>
  } : (tensor<*xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @alltoall_invalid_split_dim_size(%data: tensor<4x16xf32>) -> tensor<16x4xf32> {
// expected-error@+1 {{split dimension has size 16, expected to be a multiple of split_count 5}}
  %0 = "mhlo.all_to_all"(%data) {
    split_dimension = 1 : i64,
    concat_dimension = 0 : i64,
    split_count = 5 : i64,
    replica_groups = dense<[[0, 1, 2, 3, 4]]> : tensor<1x5xi64>
  } : (tensor<4x16xf32>) -> tensor<16x4xf32>
  return %0 : tensor<16x4xf32>
}

// -----

func @allgather_incompatible_types(%arg0: tensor<128x32xf32>) -> tensor<128x100xf32> {
  // expected-error@+1 {{result gather dimension has size 100, expected to be a multiple of operand gather dimension size 32}}
  %0 = "mhlo.all_gather"(%arg0) {
    all_gather_dim = 1 : i64,
    channel_handle = {handle = 1 : i64, type = 0 : i64},
    replica_groups = dense<[[0, 2, 4, 6], [1, 3, 5, 7]]> : tensor<2x4xi64>
  } : (tensor<128x32xf32>) -> tensor<128x100xf32>
  return %0 : tensor<128x100xf32>
}

// -----

func @allgather_gather_along_zero_dimension(%arg0: tensor<128x0x32xf32>) -> tensor<128x100xf32> {
  // expected-error@+1 {{operand gather dimension cannot be zero}}
  %0 = "mhlo.all_gather"(%arg0) {
    all_gather_dim = 1 : i64,
    channel_handle = {handle = 1 : i64, type = 0 : i64},
    replica_groups = dense<[[0, 2, 4, 6], [1, 3, 5, 7]]> : tensor<2x4xi64>
  } : (tensor<128x0x32xf32>) -> tensor<128x100xf32>
  return %0 : tensor<128x100xf32>
}

// -----

// CHECK-LABEL: func @allgather_dynamic_gather_dim
func @allgather_dynamic_gather_dim(%arg0: tensor<128x32xf32>) -> tensor<128x?xf32> {
  %0 = "mhlo.all_gather"(%arg0) {
    all_gather_dim = 1 : i64,
    channel_handle = {handle = 1 : i64, type = 0 : i64},
    replica_groups = dense<[[0, 2, 4, 6], [1, 3, 5, 7]]> : tensor<2x4xi64>
  } : (tensor<128x32xf32>) -> tensor<128x?xf32>
  return %0 : tensor<128x?xf32>
}

// -----

// CHECK-LABEL: func @broadcast
func @broadcast(%arg0: tensor<3xi32>) -> tensor<1x2x3xi32> {
  %0 = "mhlo.broadcast"(%arg0) {broadcast_sizes = dense<[1, 2]> : tensor<2xi64>} : (tensor<3xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

func @broadcast_bad_sizes_rank(%arg0: tensor<3xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{broadcast_sizes has rank 2 instead of rank 1}}
  %0 = "mhlo.broadcast"(%arg0) {broadcast_sizes = dense<[[1, 2]]> : tensor<1x2xi64>} : (tensor<3xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

func @broadcast_bad_result_rank(%arg0: tensor<3xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{result rank (3) does not match operand rank (1) plus size of broadcast_sizes (1)}}
  %0 = "mhlo.broadcast"(%arg0) {broadcast_sizes = dense<[2]> : tensor<1xi64>} : (tensor<3xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

func @broadcast_bad_first_part_result_shape(%arg0: tensor<3xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{result has shape [1, 3] instead of [2, 3]}}
  %0 = "mhlo.broadcast"(%arg0) {broadcast_sizes = dense<[2]> : tensor<1xi64>} : (tensor<3xi32>) -> tensor<1x3xi32>
  return %0 : tensor<1x3xi32>
}

// -----

func @broadcast_bad_second_part_result_shape(%arg0: tensor<3xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{result has shape [2, 1] instead of [2, 3]}}
  %0 = "mhlo.broadcast"(%arg0) {broadcast_sizes = dense<[2]> : tensor<1xi64>} : (tensor<3xi32>) -> tensor<2x1xi32>
  return %0 : tensor<2x1xi32>
}

// -----

// CHECK-LABEL: func @broadcast_in_dim
func @broadcast_in_dim(%arg0: tensor<1x2xi32>) -> tensor<1x2x2xi32> {
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[1, 2]> : tensor<2xi64>} : (tensor<1x2xi32>) -> tensor<1x2x2xi32>
  return %0 : tensor<1x2x2xi32>
}

// -----

// CHECK-LABEL: func @broadcast_in_dim_zero_rank
func @broadcast_in_dim_zero_rank(%arg0: tensor<i32>) -> tensor<1x2x3xi32> {
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[]> : tensor<0xi64>} : (tensor<i32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

// CHECK-LABEL: func @dynamic_broadcast_in_dim
func @dynamic_broadcast_in_dim(%arg0: tensor<?x?xi32>, %shape: tensor<3xi64>) -> tensor<?x?x?xi32> {
  %0 = "mhlo.dynamic_broadcast_in_dim"(%arg0, %shape) {broadcast_dimensions = dense<[1, 2]> : tensor<2xi64>} : (tensor<?x?xi32>, tensor<3xi64>) -> tensor<?x?x?xi32>
  return %0 : tensor<?x?x?xi32>
}

// -----

// CHECK-LABEL: func @dynamic_broadcast_in_dim_unknown_dim
func @dynamic_broadcast_in_dim_unknown_dim(%arg0: tensor<32xf32>, %shape: tensor<3xi64>) -> tensor<?x?x?xf32> {
  %0 = "mhlo.dynamic_broadcast_in_dim"(%arg0, %shape) {broadcast_dimensions = dense<[2]> : tensor<1xi64>} : (tensor<32xf32>, tensor<3xi64>) -> tensor<?x?x?xf32>
  return %0 : tensor<?x?x?xf32>
}

// -----

// CHECK-LABEL: func @dynamic_broadcast_in_dim_ok_dim
func @dynamic_broadcast_in_dim_ok_dim(%arg0: tensor<1xf32>, %shape: tensor<3xi64>) -> tensor<7x8x9xf32> {
  %0 = "mhlo.dynamic_broadcast_in_dim"(%arg0, %shape) {broadcast_dimensions = dense<[2]> : tensor<1xi64>} : (tensor<1xf32>, tensor<3xi64>) -> tensor<7x8x9xf32>
  return %0 : tensor<7x8x9xf32>
}

// -----

func @dynamic_broadcast_in_dim_shape_mismatch(%arg0: tensor<32xf32>, %shape: tensor<3xi64>) -> tensor<7x8x9xf32> {
  // expected-error@+1 {{size of operand dimension 0 (32) is not compatible with size of result dimension 2 (9)}}
  %0 = "mhlo.dynamic_broadcast_in_dim"(%arg0, %shape) {broadcast_dimensions = dense<[2]> : tensor<1xi64>} : (tensor<32xf32>, tensor<3xi64>) -> tensor<7x8x9xf32>
  return %0 : tensor<7x8x9xf32>
}

// -----

func @broadcast_in_dim_bad_dimension_rank(%arg0: tensor<1x2xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{broadcast_dimensions has rank 2 instead of rank 1}}
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[[1,1],[1,1]]> : tensor<2x2xi64>} : (tensor<1x2xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

func @broadcast_in_dim_bad_dimension_size(%arg0: tensor<1x2xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{broadcast_dimensions size (1) does not match operand rank (2)}}
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[1]> : tensor<1xi64>} : (tensor<1x2xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

func @broadcast_in_dim_bad_rank_decrease(%arg0: tensor<1x2x3xi32>) -> tensor<3xi32> {
  // expected-error@+1 {{result rank (1) is less than operand rank (3)}}
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[0,1,2]> : tensor<3xi64>} : (tensor<1x2x3xi32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @broadcast_in_dim_dimension_values_too_large(%arg0: tensor<1x2xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{broadcast_dimensions contains invalid value 9 for result with rank 3}}
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[9, 2]> : tensor<2xi64>} : (tensor<1x2xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

func @broadcast_in_dim_bad_shape_mismatch(%arg0: tensor<3xi32>) -> tensor<1x2x3xi32> {
  // expected-error@+1 {{size of operand dimension 0 (3) is not equal to 1 or size of result dimension 1 (2)}}
  %0 = "mhlo.broadcast_in_dim"(%arg0) {broadcast_dimensions = dense<[1]> : tensor<1xi64>} : (tensor<3xi32>) -> tensor<1x2x3xi32>
  return %0 : tensor<1x2x3xi32>
}

// -----

// Regression test for b/180052624, where this was improperly marked as an
// invalid mhlo.broadcast_in_dim op.
// CHECK-LABEL: func @broadcast_in_dim_dynamic_shaped_operand
func @broadcast_in_dim_dynamic_shaped_operand(%arg0 : tensor<?xf32>) -> tensor<2xf32> {
  %0 = "mhlo.broadcast_in_dim"(%arg0) {
    broadcast_dimensions = dense<0> : tensor<1xi64>
  } : (tensor<?xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

// Regression test for b/180052624, where this crashed verification given the
// unranked operand.
// CHECK-LABEL: func @broadcast_in_dim_unranked_operand
func @broadcast_in_dim_unranked_operand(%arg0 : tensor<*xf32>) -> tensor<2xf32> {
  %0 = "mhlo.broadcast_in_dim"(%arg0) {
    broadcast_dimensions = dense<0> : tensor<1xi64>
  } : (tensor<*xf32>) -> tensor<2xf32>
  return %0 : tensor<2xf32>
}

// -----

// CHECK-LABEL: @if_nested_different_return_types(
func @if_nested_different_return_types(%pred : tensor<i1>, %branch_operand : tensor<f32>) {
  %0 = "mhlo.if"(%pred) ({
      "mhlo.return"(%branch_operand) : (tensor<f32>) -> ()
  }, {
      %2 = "mhlo.if"(%pred) ({
          "mhlo.return"(%pred) : (tensor<i1>) -> ()
      }, {
          "mhlo.return"(%pred) : (tensor<i1>) -> ()
      }) : (tensor<i1>) -> tensor<i1>
      "mhlo.return"(%branch_operand) : (tensor<f32>) -> ()
  }) : (tensor<i1>) -> tensor<f32>
  return
}

// -----

func @if_mismatch_return_type(%pred : tensor<i1>, %branch_operand : tensor<f32>, %wrong_type : tensor<3xf32>) {
  // @expected-error@+1 {{true_branch returned types ('tensor<3xf32>') do not match op result types ('tensor<f32>')}}
  %0 = "mhlo.if"(%pred) ({
      "mhlo.return"(%wrong_type) : (tensor<3xf32>) -> ()
    }, {
      "mhlo.return"(%branch_operand) : (tensor<f32>) -> ()
    }) : (tensor<i1>) -> tensor<f32>
  return
}

// -----

func @if_mismatch_num_return_types(%pred : tensor<i1>, %branch_operand : tensor<f32>) {
  // @expected-error@+1 {{true_branch returned types ('tensor<f32>', 'tensor<f32>') do not match op result types ('tensor<f32>')}}
  %0 = "mhlo.if"(%pred) ({
      "mhlo.return"(%branch_operand, %branch_operand) : (tensor<f32>, tensor<f32>) -> ()
    }, {
      "mhlo.return"(%branch_operand) : (tensor<f32>) -> ()
    }) : (tensor<i1>) -> tensor<f32>
  return
}

// -----

// CHECK-LABEL: @case_nested_different_return_types(
func @case_nested_different_return_types(%index : tensor<i32>, %branch_operand : tensor<f32>) {
  %0 = "mhlo.case"(%index) ({
      "mhlo.return"(%branch_operand) : (tensor<f32>) -> ()
  }, {
      %2 = "mhlo.case"(%index) ({
          "mhlo.return"(%index) : (tensor<i32>) -> ()
      }) : (tensor<i32>) -> tensor<i32>
      "mhlo.return"(%branch_operand) : (tensor<f32>) -> ()
  }) : (tensor<i32>) -> tensor<f32>
  return
}

// -----

func @case_mismatch_num_results(%index: tensor<i32>, %operand_1: tensor<f32>, %operand_2: tensor<f32>, %operand_3: tensor<f32>) -> tensor<f32> {
  // expected-error@+1 {{branch 1 returned types ('tensor<f32>', 'tensor<f32>') do not match op result types ('tensor<f32>')}}
  %0 = "mhlo.case"(%index) ( {
      %1 = "mhlo.negate"(%operand_1) : (tensor<f32>) -> tensor<f32>
      "mhlo.return"(%1) : (tensor<f32>) -> ()
    },  {
      %1 = "mhlo.copy"(%operand_2) : (tensor<f32>) -> tensor<f32>
      "mhlo.return"(%1, %operand_2) : (tensor<f32>, tensor<f32>) -> ()
    },  {
      %1 = "mhlo.floor"(%operand_3) : (tensor<f32>) -> tensor<f32>
      "mhlo.return"(%1) : (tensor<f32>) -> ()
    }
  ) : (tensor<i32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

func @case_mismatch_return_type(%index: tensor<i32>, %operand_1: tensor<f32>, %operand_2: tensor<f32>, %operand_3: tensor<f32>) -> tensor<f32> {
  // expected-error@+1 {{branch 1 returned types ('tensor<i32>') do not match op result types ('tensor<f32>')}}
  %0 = "mhlo.case"(%index) ( {
      %1 = "mhlo.negate"(%operand_1) : (tensor<f32>) -> tensor<f32>
      "mhlo.return"(%1) : (tensor<f32>) -> ()
    },  {
      %1 = mhlo.constant dense<2> : tensor<i32>
      "mhlo.return"(%1) : (tensor<i32>) -> ()
    },  {
      %1 = "mhlo.floor"(%operand_3) : (tensor<f32>) -> tensor<f32>
      "mhlo.return"(%1) : (tensor<f32>) -> ()
    }
  ) : (tensor<i32>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// CHECK-LABEL: func @comp_eq
func @comp_eq(%arg0: tensor<3xi32>, %arg1: tensor<3xi32>) -> tensor<3xi1> {
  %0 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "EQ"} : (tensor<3xi32>, tensor<3xi32>) -> tensor<3xi1>
  return %0 : tensor<3xi1>
}

// -----

func @comp_bad_direction(%arg0: tensor<3xi32>, %arg1: tensor<3xi32>) -> tensor<3xi1> {
  // expected-error@+1 {{'comparison_direction' failed to satisfy constraint}}
  %0 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "FOOBAR"} : (tensor<3xi32>, tensor<3xi32>) -> tensor<3xi1>
  return %0 : tensor<3xi1>
}

// -----

func @collective_permute_duplicate_sources(%arg0: tensor<128x32xf32>) -> tensor<128x32xf32> {
  // expected-error@+1 {{duplicate sources not allowed}}
  %0 = "mhlo.collective_permute"(%arg0) {
    source_target_pairs = dense<[[0, 1], [0, 2], [2, 3]]> : tensor<3x2xi64>
  } : (tensor<128x32xf32>) -> tensor<128x32xf32>
  return %0 : tensor<128x32xf32>
}

// -----

func @collective_permute_duplicate_targets(%arg0: tensor<128x32xf32>) -> tensor<128x32xf32> {
  // expected-error@+1 {{duplicate targets not allowed}}
  %0 = "mhlo.collective_permute"(%arg0) {
    source_target_pairs = dense<[[0, 1], [1, 2], [2, 1]]> : tensor<3x2xi64>
  } : (tensor<128x32xf32>) -> tensor<128x32xf32>
  return %0 : tensor<128x32xf32>
}

// -----

func @collective_permute_duplicate_sources(%arg0: tensor<128x32xf32>) -> tensor<128x32xf32> {
  // expected-error@+1 {{expect source_target_pairs attribute to be of rank 2, but got rank 1}}
  %0 = "mhlo.collective_permute"(%arg0) {
    source_target_pairs = dense<[0, 1]> : tensor<2xi64>
  } : (tensor<128x32xf32>) -> tensor<128x32xf32>
  return %0 : tensor<128x32xf32>
}

// -----

func @collective_permute_duplicate_sources(%arg0: tensor<128x32xf32>) -> tensor<128x32xf32> {
  // expected-error@+1 {{expect source_target_pairs attribute of shape (N, 2), but got (2, 3)}}
  %0 = "mhlo.collective_permute"(%arg0) {
    source_target_pairs = dense<[[0, 1, 2], [3, 4, 5]]> : tensor<2x3xi64>
  } : (tensor<128x32xf32>) -> tensor<128x32xf32>
  return %0 : tensor<128x32xf32>
}

// -----

func @concat_0D(%arg0: tensor<i32>, %arg1: tensor<i32>)  -> tensor<2xi32> {
  // expected-error@+1 {{rank-0 values cannot be concatenated}}
  %0 = "mhlo.concatenate"(%arg0, %arg1) { dimension = 0 : i64 } : (tensor<i32>, tensor<i32>) -> tensor<2xi32>
  return %0 : tensor<2xi32>
}

// -----

// CHECK-LABEL: @concat_1D
func @concat_1D(%arg0: tensor<1xi32>, %arg1: tensor<2xi32>)  -> tensor<3xi32> {
  %0 = "mhlo.concatenate"(%arg0, %arg1) { dimension = 0 : i64 } : (tensor<1xi32>, tensor<2xi32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

// CHECK-LABEL: @concat_1D
// Verifies that an error is not thrown if the inferred type is compatible with
// the result type.
func @concat_1D(%arg0: tensor<1xi32>, %arg1: tensor<*xi32>)  -> tensor<3xi32> {
  %0 = "mhlo.concatenate"(%arg0, %arg1) { dimension = 0 : i64 } : (tensor<1xi32>, tensor<*xi32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

func @concat_1D_type_error(%arg0: tensor<1xi32>, %arg1: tensor<2xf32>)  -> tensor<3xi32> {
  // expected-error@+1 {{'mhlo.concatenate' op requires the same element type for all operands and results}}
  %0 = "mhlo.concatenate"(%arg0, %arg1) { dimension = 0 : i64 } : (tensor<1xi32>, tensor<2xf32>) -> tensor<3xi32>
  return %0 : tensor<3xi32>
}

// -----

// CHECK-LABEL: @concat_1D_unranked
func @concat_1D_unranked(%arg0: tensor<1xi32>, %arg1: tensor<*xi32>)  -> tensor<*xi32> {
  %0 = "mhlo.concatenate"(%arg0, %arg1) { dimension = 0 : i64 } : (tensor<1xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %0 : tensor<*xi32>
}

// -----

func @concat_1D_error(%arg0: tensor<1xi32>, %arg1: tensor<2xi32>)  -> tensor<4xi32> {
  // expected-error@+1 {{op inferred type(s) 'tensor<3xi32>' are incompatible with return type(s) of operation 'tensor<4xi32>'}}
  %0 = "mhlo.concatenate"(%arg0, %arg1) { dimension = 0 : i64 } : (tensor<1xi32>, tensor<2xi32>) -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----

// CHECK-LABEL: func @clamp
func @clamp(%arg0: tensor<1xi32>) -> tensor<1xi32> {
  %0 = "mhlo.clamp"(%arg0, %arg0, %arg0) : (tensor<1xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1xi32>
  return %0: tensor<1xi32>
}

// -----

// CHECK-LABEL: func @clamp_scalar
func @clamp_scalar(%arg0: tensor<1xi32>, %arg1: tensor<i32>) -> tensor<1xi32> {
  %0 = "mhlo.clamp"(%arg1, %arg0, %arg1) : (tensor<i32>, tensor<1xi32>, tensor<i32>) -> tensor<1xi32>
  return %0: tensor<1xi32>
}

// -----

func @clamp_invalid_clamp_element_type(%arg0: tensor<1xi32>, %arg1: tensor<1xf32>) -> tensor<1xi32> {
  // expected-error@+1 {{'mhlo.clamp' op requires the same element type for all operands and results}}
  %0 = "mhlo.clamp"(%arg1, %arg0, %arg0) : (tensor<1xf32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1xi32>
  return %0: tensor<1xi32>
}

// -----

func @clamp_invalid_clamp_shape(%arg0: tensor<1xi32>, %arg1: tensor<2xi32>) -> tensor<1xi32> {
  // expected-error@+1 {{min shape [2] is not scalar and does not match operand shape [1]}}
  %0 = "mhlo.clamp"(%arg1, %arg0, %arg0) : (tensor<2xi32>, tensor<1xi32>, tensor<1xi32>) -> tensor<1xi32>
  return %0: tensor<1xi32>
}

// -----

// CHECK-LABEL: func @dot_vector
func @dot_vector(%arg0: tensor<1x2xi32>, %arg1: tensor<2x1xi32>) -> tensor<i32> {
  %0 = "mhlo.dot"(%arg0, %arg1) : (tensor<1x2xi32>, tensor<2x1xi32>) -> tensor<i32>
  return %0: tensor<i32>
}

// -----

// CHECK-LABEL: func @dot_matrix
func @dot_matrix(%arg0: tensor<2x2xi32>, %arg1: tensor<2x2xi32>) -> tensor<2x2xi32> {
  %0 = "mhlo.dot"(%arg0, %arg1) : (tensor<2x2xi32>, tensor<2x2xi32>) -> tensor<2x2xi32>
  return %0: tensor<2x2xi32>
}

// -----

// CHECK-LABEL: func @dot_precision_config
func @dot_precision_config(%arg0: tensor<2x2xi32>, %arg1: tensor<2x2xi32>) -> tensor<2x2xi32> {
  %0 = "mhlo.dot"(%arg0, %arg1) {precision_config = ["HIGH", "HIGHEST"]} : (tensor<2x2xi32>, tensor<2x2xi32>) -> tensor<2x2xi32>
  return %0: tensor<2x2xi32>
}

// -----

func @dot_bad_precision_config(%arg0: tensor<2x2xi32>, %arg1: tensor<2x2xi32>) -> tensor<2x2xi32> {
  // expected-error@+1 {{'precision_config' failed to satisfy constraint}}
  %0 = "mhlo.dot"(%arg0, %arg1) {precision_config = ["FOO", "HIGHEST"]} : (tensor<2x2xi32>, tensor<2x2xi32>) -> tensor<2x2xi32>
  return %0: tensor<2x2xi32>
}

// -----

// CHECK-LABEL: func @imag_fp_input
func @imag_fp_input(%arg0: tensor<*xf32>) -> tensor<*xf32> {
  %0 = "mhlo.imag"(%arg0) : (tensor<*xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @infeed_invalid_number_of_results(%token: !mhlo.token) -> tuple<tuple<tensor<i32>>, !mhlo.token, tensor<i32>> {
  // expected-error@+1 {{result is expected to be a tuple of size 2, but got 3}}
  %0 = "mhlo.infeed"(%token) {infeed_config = "foobar", layout = [[[0]], unit, [0]]} : (!mhlo.token) -> tuple<tuple<tensor<i32>>, !mhlo.token, tensor<i32>>
  return %0 : tuple<tuple<tensor<i32>>, !mhlo.token, tensor<i32>>
}

// -----

func @infeed_non_token_second_result(%token: !mhlo.token) -> tuple<tuple<tensor<i32>>, tensor<i32>> {
  // expected-error@+1 {{second element of result tuple is expected to be of token type, but got 'tensor<i32>'}}
  %0 = "mhlo.infeed"(%token) {infeed_config = "foobar", layout = [[[0]], [0]]} : (!mhlo.token) -> tuple<tuple<tensor<i32>>, tensor<i32>>
  return %0 : tuple<tuple<tensor<i32>>, tensor<i32>>
}

// -----

func @iota_scalar() -> tensor<i32> {
  // expected-error@+1 {{does not support scalars}}
  %0 = "mhlo.iota"() {iota_dimension = 0 : i64} : () -> tensor<i32>
  return %0 : tensor<i32>
}

// -----

func @iota_invalid_iota_dimension() -> tensor<4xi32> {
  // expected-error@+1 {{iota dimension cannot go beyond the output rank or be negative}}
  %0 = "mhlo.iota"() {iota_dimension = 1 : i64} : () -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----

func @map_mismatched_args(%arg0: tensor<4xf32>, %arg1: tensor<4xf32>) -> tensor<4xf32> {
  // expected-error@+1 {{expects number of operands to match the arity of map computation, but got: 2 and 1}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg: tensor<f32>):
    %1 = mhlo.add %arg, %arg {name = "add"} : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {dimensions = dense<0> : tensor<1xi64>} : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
  return %0 : tensor<4xf32>
}

// -----

func @map_non_scalar_computation_operand(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{computation arguments must be 0-rank tensor, but got: arg #1 of type 'tensor<5xf32>'}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<5xf32>):
    %1 = mhlo.constant dense<2.0> : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {dimensions = dense<[0, 1]> : tensor<2xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @map_mismatch_operand_and_computation_args(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{element type of operands and computation arguments must match, but got: 'f32' and 'i32'}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<i32>, %arg3: tensor<i32>):
    %1 = mhlo.constant dense<2.0> : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {dimensions = dense<[0, 1]> : tensor<2xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @map_invalid_number_of_computation_output(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{computation must return single output, but got: 0}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.constant dense<2.0> : tensor<f32>
    "mhlo.return"() : () -> ()
  }) {dimensions = dense<[0, 1]> : tensor<2xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @main_non_scalar_computation_output(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{computation must return 0-rank tensor, but got: 'tensor<5xf32>'}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.constant dense<2.0> : tensor<5xf32>
    "mhlo.return"(%1) : (tensor<5xf32>) -> ()
  }) {dimensions = dense<[0, 1]> : tensor<2xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @mismatch_computation_output_type(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{element type of result and computation output must match, but got: 'f32' and 'i32'}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.constant dense<2> : tensor<i32>
    "mhlo.return"(%1) : (tensor<i32>) -> ()
  }) {dimensions = dense<[0, 1]> : tensor<2xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @map_invalid_dimension_numbers(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{requires monotonically increasing dimension numbers, but got: dense<[1, 0]> : tensor<2xi64>}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 {name = "add"} : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {dimensions = dense<[1, 0]> : tensor<2xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

func @map_mismatch_arguments_and_dimensions(%arg0: tensor<4x5xf32>, %arg1: tensor<4x5xf32>) -> tensor<4x5xf32> {
  // expected-error@+1 {{applied to a subset of dimensions currently not supported: operand dimensions = 2, requested map dimensions size = 3}}
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 {name = "add"} : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {dimensions = dense<[0, 1, 2]> : tensor<3xi64>} : (tensor<4x5xf32>, tensor<4x5xf32>) -> tensor<4x5xf32>
  return %0 : tensor<4x5xf32>
}

// -----

// CHECK-LABEL: func @map_unranked
func @map_unranked(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>) -> tensor<*xf32> {
  %0 = "mhlo.map"(%arg0, %arg1) ( {
    ^bb0(%arg2: tensor<f32>, %arg3: tensor<f32>):
    %1 = mhlo.add %arg2, %arg3 {name = "add"} : tensor<f32>
    "mhlo.return"(%1) : (tensor<f32>) -> ()
  }) {dimensions = dense<0> : tensor<1xi64>} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

// CHECK-LABEL: func @real_fp_input
func @real_fp_input(%arg0: tensor<*xf32>) -> tensor<*xf32> {
  %0 = "mhlo.real"(%arg0) : (tensor<*xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @recv_invalid_number_of_results(%token: !mhlo.token) -> tuple<tensor<3x4xi32>, tensor<i32>, !mhlo.token> {
  // expected-error@+1 {{result is expected to be a tuple of size 2, but got 3}}
  %0 = "mhlo.recv"(%token) {
    channel_handle = {
      handle = 5 : i64,
      type = 3 : i64  // Host to device channel
    },
    is_host_transfer = true
  } : (!mhlo.token) -> tuple<tensor<3x4xi32>, tensor<i32>, !mhlo.token>
  return %0 : tuple<tensor<3x4xi32>, tensor<i32>, !mhlo.token>
}

// -----

func @recv_non_token_second_result(%token: !mhlo.token) -> tuple<tensor<3x4xi32>, tensor<i32>> {
  // expected-error@+1 {{second element of result tuple is expected to be of token type, but got 'tensor<i32>'}}
  %0 = "mhlo.recv"(%token) {
    channel_handle = {
      handle = 5 : i64,
      type = 3 : i64  // Host to device channel
    },
    is_host_transfer = true
  } : (!mhlo.token) -> tuple<tensor<3x4xi32>, tensor<i32>>
  return %0 : tuple<tensor<3x4xi32>, tensor<i32>>
}

// -----

// CHECK-LABEL: func @replica_id
func @replica_id() -> tensor<ui32> {
  %0 = "mhlo.replica_id"() : () -> tensor<ui32>
  return %0 : tensor<ui32>
}

// -----

func @rng_uniform_invalid_type(%mu: tensor<complex<f32>>, %sigma: tensor<f32>) -> tensor<2x3x5xf32> {
  %shape = mhlo.constant dense<[2, 3, 5]> : tensor<3xi64>
  // expected-error@+1 {{but got 'tensor<complex<f32>>'}}
  %0 = "mhlo.rng_uniform"(%mu, %sigma, %shape) : (tensor<complex<f32>>, tensor<f32>, tensor<3xi64>) -> tensor<2x3x5xf32>
  return %0 : tensor<2x3x5xf32>
}

// -----

// CHECK-LABEL: func @select
func @select(%arg0: tensor<2x3xi1>, %arg1: tensor<2x3xi32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<2x3xi1>, tensor<2x3xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

// CHECK-LABEL: func @select_scalar_pred
func @select_scalar_pred(%arg0: tensor<i1>, %arg1: tensor<2x3xi32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<2x3xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

// CHECK-LABEL: func @select_cast_compatible_types
func @select_cast_compatible_types(%arg0: tensor<i1>, %arg1: tensor<*xi32>, %arg2: tensor<2x3xi32>) -> tensor<*xi32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<*xi32>, tensor<2x3xi32>) -> tensor<*xi32>
  return %0 : tensor<*xi32>
}

// -----

// CHECK-LABEL: func @select_cast_compatible_types
func @select_cast_compatible_types(%arg0: tensor<i1>, %arg1: tensor<2x3xi32>, %arg2: tensor<*xi32>) -> tensor<*xi32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<2x3xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %0 : tensor<*xi32>
}

// -----

// CHECK-LABEL: func @select_cast_compatible_types
func @select_cast_compatible_types(%arg0: tensor<i1>, %arg1: tensor<2x?xi32>, %arg2: tensor<?x3xi32>) -> tensor<?x?xi32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<2x?xi32>, tensor<?x3xi32>) -> tensor<?x?xi32>
  return %0 : tensor<?x?xi32>
}

// -----

// CHECK-LABEL: func @select_cast_compatible_types
func @select_cast_compatible_types(%arg0: tensor<i1>, %arg1: tensor<?x3xi32>, %arg2: tensor<2x?xi32>) -> tensor<?x?xi32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<?x3xi32>, tensor<2x?xi32>) -> tensor<?x?xi32>
  return %0 : tensor<?x?xi32>
}

// -----

// CHECK-LABEL: func @select_scalar_x_y
func @select_scalar_x_y(%arg0: tensor<i1>, %arg1: tensor<i32>, %arg2: tensor<i32>) -> tensor<i32> {
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i1>, tensor<i32>, tensor<i32>) -> tensor<i32>
  return %0 : tensor<i32>
}

// -----

func @select_bad_pred_type(%arg0: tensor<i32>, %arg1: tensor<2x3xi32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  // expected-error@+1 {{must be tensor of pred (AKA boolean or 1-bit integer) values}}
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<i32>, tensor<2x3xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

func @select_bad_pred_shape(%arg0: tensor<3xi1>, %arg1: tensor<2x3xi32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  // expected-error@+1 {{requires the same type for all operands and results}}
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<2x3xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

func @select_bad_shape_mismatch(%arg0: tensor<3xi1>, %arg1: tensor<2x4xi32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  // expected-error@+1 {{op inferred type(s) 'tensor<2x4xi32>' are incompatible with return type(s) of operation 'tensor<2x3xi32>'}}
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<2x4xi32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

func @select_bad_shape_mismatch(%arg0: tensor<3xi1>, %arg1: tensor<2x3xi32>, %arg2: tensor<2xi32>) -> tensor<2x3xi32> {
  // expected-error@+1 {{op requires the same type for all operands and results}}
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<2x3xi32>, tensor<2xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

func @select_bad_element_type_mismatch(%arg0: tensor<3xi1>, %arg1: tensor<2x3xf32>, %arg2: tensor<2x3xi32>) -> tensor<2x3xi32> {
  // expected-error@+1 {{requires the same type for all operands and results}}
  %0 = "mhlo.select"(%arg0, %arg1, %arg2) : (tensor<3xi1>, tensor<2x3xf32>, tensor<2x3xi32>) -> tensor<2x3xi32>
  return %0 : tensor<2x3xi32>
}

// -----

// CHECK-LABEL: func @slice
func @slice(%arg0: tensor<3x4xi32>) -> tensor<1x2xi32> {
  %0 = "mhlo.slice"(%arg0) {start_indices = dense<[1, 0]> : tensor<2xi64>, limit_indices = dense<[2, 4]> : tensor<2xi64>, strides = dense<[1, 2]> : tensor<2xi64>} : (tensor<3x4xi32>) -> tensor<1x2xi32>
  return %0 : tensor<1x2xi32>
}

// -----

func @slice_indices_mismatch(%arg0: tensor<3x4xi32>) -> tensor<1x4xi32> {
  // expected-error@+1 {{failed to verify that all of {start_indices, limit_indices, strides} have same type}}
  %0 = "mhlo.slice"(%arg0) {start_indices = dense<[1, 2]> : tensor<2xi64>, limit_indices = dense<[2, 4, 1]> : tensor<3xi64>, strides = dense<[1, 2]> : tensor<2xi64>} : (tensor<3x4xi32>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

func @slice_operand_result_mismatch(%arg0: tensor<3x4xi32>) -> tensor<1x4xf32> {
  // expected-error@+1 {{requires the same element type for all operands and results}}
  %0 = "mhlo.slice"(%arg0) {start_indices = dense<[1, 0]> : tensor<2xi64>, limit_indices = dense<[2, 4]> : tensor<2xi64>, strides = dense<[1, 2]> : tensor<2xi64>} : (tensor<3x4xi32>) -> tensor<1x4xf32>
  return %0 : tensor<1x4xf32>
}

// -----

func @slice_indices_not_rank_1(%arg0: tensor<3x4xi32>) -> tensor<1x2xi32> {
  // expected-error@+1 {{start_indices has rank 2 instead of required rank 1}}
  %0 = "mhlo.slice"(%arg0) {
    start_indices = dense<[[1, 0]]> : tensor<1x2xi64>,
    limit_indices = dense<[[2, 4]]> : tensor<1x2xi64>,
    strides = dense<[[1, 2]]> : tensor<1x2xi64>
  } : (tensor<3x4xi32>) -> tensor<1x2xi32>
  return %0 : tensor<1x2xi32>
}

// -----

func @slice_indices_wrong_size(%arg0: tensor<3x4xi32>) -> tensor<1x2xi32> {
  // expected-error@+1 {{the number of elements in start_indices (3) does not match the rank of the operand (2)}}
  %0 = "mhlo.slice"(%arg0) {
    start_indices = dense<[1, 0, 0]> : tensor<3xi64>,
    limit_indices = dense<[2, 4, 0]> : tensor<3xi64>,
    strides = dense<[1, 2, 0]> : tensor<3xi64>
  } : (tensor<3x4xi32>) -> tensor<1x2xi32>
  return %0 : tensor<1x2xi32>
}

// -----

// CHECK-LABEL: func @dynamic_slice
func @dynamic_slice(%arg0: tensor<3x4xi32>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<1x4xi32> {
  %0 = "mhlo.dynamic-slice"(%arg0, %arg1, %arg2) {slice_sizes = dense<[1, 4]> : tensor<2xi64>} : (tensor<3x4xi32>, tensor<i64>, tensor<i64>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

func @dynamic_slice_mismatch_indices(%arg0: tensor<3x4xi32>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<1x4xi32> {
  // expected-error@+1 {{has mismatched number of slice sizes (1) and number of start indices (2)}}
  %0 = "mhlo.dynamic-slice"(%arg0, %arg1, %arg2) {slice_sizes = dense<[4]> : tensor<1xi64>} : (tensor<3x4xi32>, tensor<i64>, tensor<i64>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

// CHECK-LABEL: @dynamic_slice_different_indice_element_type
func @dynamic_slice_different_indice_element_type(%arg0: tensor<3x4xi32>, %arg1: tensor<i32>) -> tensor<1x4xi32> {
  %0 = "mhlo.dynamic-slice"(%arg0, %arg1) {slice_sizes = dense<[4]> : tensor<1xi64>} : (tensor<3x4xi32>, tensor<i32>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

func @dynamic_slice_mismatch_element_types(%arg0: tensor<3x4xi32>, %arg1: tensor<i64>, %arg2: tensor<i64>) -> tensor<1x4xf32> {
  // expected-error@+1 {{failed to verify that all of {operand, result} have same element type}}
  %0 = "mhlo.dynamic-slice"(%arg0, %arg1, %arg2) {slice_sizes = dense<[1, 4]> : tensor<2xi64>} : (tensor<3x4xi32>, tensor<i64>, tensor<i64>) -> tensor<1x4xf32>
  return %0 : tensor<1x4xf32>
}

// -----

func @dynamic_slice_invalid_start(%arg0: tensor<3x4xi32>, %arg1: tensor<2xi64>) -> tensor<1x4xi32> {
  // expected-error@+1 {{operand #1 must be 0D tensor of 8/16/32/64-bit signless integer or 8/16/32/64-bit unsigned integer values, but got 'tensor<2xi64>'}}
  %0 = "mhlo.dynamic-slice"(%arg0, %arg1) {slice_sizes = dense<[1, 4]> : tensor<2xi64>} : (tensor<3x4xi32>, tensor<2xi64>) -> tensor<1x4xi32>
  return %0 : tensor<1x4xi32>
}

// -----

// CHECK-LABEL: @dynamic_update_slice
func @dynamic_update_slice(%input: tensor<3x4xi64>, %update: tensor<2xi64>, %start1: tensor<i64>, %start2: tensor<i64>) -> tensor<3x4xi64> {
  %0 = "mhlo.dynamic-update-slice"(%input, %update, %start1, %start2) : (tensor<3x4xi64>, tensor<2xi64>, tensor<i64>, tensor<i64>) -> tensor<3x4xi64>
  return %0 : tensor<3x4xi64>
}

// -----

func @dynamic_update_slice_invalid_start(%input: tensor<3x4xi64>, %update: tensor<2xi64>, %start: tensor<2xi64>) -> tensor<3x4xi64> {
  // expected-error@+1 {{operand #2 must be 0D tensor of 8/16/32/64-bit signless integer or 8/16/32/64-bit unsigned integer values, but got 'tensor<2xi64>'}}
  %0 = "mhlo.dynamic-update-slice"(%input, %update, %start) : (tensor<3x4xi64>, tensor<2xi64>, tensor<2xi64>) -> tensor<3x4xi64>
  return %0 : tensor<3x4xi64>
}

// -----

func @dynamic_update_slice_mismatched_start(%input: tensor<11x3x4xi32>, %update: tensor<1x3x4xi32>, %start1: tensor<i32>, %start2: tensor<i64>, %start3: tensor<i64>) -> tensor<11x3x4xi32> {
  // expected-error@+1 {{start indices must have same element type (encountered mismatch: 'i32' vs 'i64')}}
  %0 = "mhlo.dynamic-update-slice"(%input, %update, %start1, %start2, %start3) : (tensor<11x3x4xi32>, tensor<1x3x4xi32>, tensor<i32>, tensor<i64>, tensor<i64>) -> tensor<11x3x4xi32>
  return %0 : tensor<11x3x4xi32>
}

// -----

// CHECK-LABEL: func @transpose
func @transpose(%arg0: tensor<1x2x3x4xi32>) -> tensor<2x1x4x3xi32> {
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[1, 0, 3, 2]> : tensor<4xi64>} : (tensor<1x2x3x4xi32>) -> tensor<2x1x4x3xi32>
  return %0: tensor<2x1x4x3xi32>
}

// -----

func @transpose_ranked(%arg0: tensor<?x?x?x?xi32>) ->  tensor<?x?x?x?xi32> {
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[1, 0, 3, 2]> : tensor<4xi64>} : (tensor<?x?x?x?xi32>) -> tensor<?x?x?x?xi32>
  return %0: tensor<?x?x?x?xi32>
}

// -----

func @transpose_unranked(%arg0: tensor<*xi32>) ->  tensor<*xi32> {
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[1, 0, 3, 2]> : tensor<4xi64>} : (tensor<*xi32>) -> tensor<*xi32>
  return %0: tensor<*xi32>
}

// -----

func @transpose_bad_permutations_rank(%arg0: tensor<1x2x3x4xi32>) ->  tensor<2x1x4x3xi32> {
  // expected-error@+1 {{permutation has rank 2 instead of rank 1}}
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[[1]]> : tensor<1x1xi64>} : (tensor<1x2x3x4xi32>) -> tensor<2x1x4x3xi32>
  return %0: tensor<2x1x4x3xi32>
}

// -----

func @transpose_bad_permutations_size(%arg0: tensor<1x2x3x4xi32>) ->  tensor<2x1x4x3xi32> {
  // expected-error@+1 {{TransposeOp operand rank 4 does not match permutation size 1}}
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[1]> : tensor<1xi64>} : (tensor<1x2x3x4xi32>) -> tensor<2x1x4x3xi32>
  return %0: tensor<2x1x4x3xi32>
}

// -----

func @transpose_operand_result_rank_mismatch(%arg0: tensor<1x2x3x4xi32>) ->  tensor<2xi32> {
  // expected-error@+1 {{op inferred type(s) 'tensor<2x1x4x3xi32>' are incompatible with return type(s) of operation 'tensor<2xi32>'}}
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[1, 0, 3, 2]> : tensor<4xi64>} : (tensor<1x2x3x4xi32>) -> tensor<2xi32>
  return %0: tensor<2xi32>
}

// -----

func @transpose_operand_result_permutation_mismatch(%arg0: tensor<1x?x3x?xi32>) ->  tensor<?x2x?x?xi32> {
  // expected-error@+1 {{op inferred type(s) 'tensor<?x1x?x3xi32>' are incompatible with return type(s) of operation 'tensor<?x2x?x?xi32>}}
  %0 = "mhlo.transpose"(%arg0) {permutation = dense<[1, 0, 3, 2]> : tensor<4xi64>} : (tensor<1x?x3x?xi32>) -> tensor<?x2x?x?xi32>
  return %0: tensor<?x2x?x?xi32>
}

// -----

func @triangular_solve_unranked(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>) -> tensor<*xf32> {
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
  return %0 : tensor<*xf32>
}

// -----

func @triangular_solve_rank_less_than_2(%arg0: tensor<4xf32>, %arg1: tensor<4x3xf32>) -> tensor<4x3xf32> {
  // expected-error@+1 {{operand 'a' must have rank >= 2, but got 'tensor<4xf32>'}}
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<4xf32>, tensor<4x3xf32>) -> tensor<4x3xf32>
  return %0 : tensor<4x3xf32>
}

// -----

func @triangular_solve_unequal_minor_dims_a(%arg0: tensor<4x3xf32>, %arg1: tensor<4x3xf32>) -> tensor<4x3xf32> {
  // expected-error@+1 {{two minor dimensions of operand 'a' must have equal size, but got 'tensor<4x3xf32>'}}
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<4x3xf32>, tensor<4x3xf32>) -> tensor<4x3xf32>
  return %0 : tensor<4x3xf32>
}

// -----

func @triangular_solve_unequal_rank(%arg0: tensor<10x4x4xf32>, %arg1: tensor<4x3xf32>) -> tensor<4x3xf32> {
  // expected-error@+1 {{operands must have equal rank, but got 'tensor<10x4x4xf32>' and 'tensor<4x3xf32>'}}
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<10x4x4xf32>, tensor<4x3xf32>) -> tensor<4x3xf32>
  return %0 : tensor<4x3xf32>
}

// -----

func @triangular_solve_mismatch_shared_dim(%arg0: tensor<4x4xf32>, %arg1: tensor<3x4xf32>) -> tensor<3x4xf32> {
  // expected-error@+1 {{shared dimension of operands 'a' and 'b' does not match, but got 'tensor<4x4xf32>' and 'tensor<3x4xf32>'}}
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<4x4xf32>, tensor<3x4xf32>) -> tensor<3x4xf32>
  return %0 : tensor<3x4xf32>
}

// -----

func @triangular_solve_mismatch_leading_dims(%arg0: tensor<10x5x4x4xf32>, %arg1: tensor<10x6x4x3xf32>) -> tensor<10x6x4x3xf32> {
  // expected-error@+1 {{leading batch dimensions of the operands must be same, but got 'tensor<10x5x4x4xf32>' and 'tensor<10x6x4x3xf32>'}}
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<10x5x4x4xf32>, tensor<10x6x4x3xf32>) -> tensor<10x6x4x3xf32>
  return %0 : tensor<10x6x4x3xf32>
}

// -----

func @triangular_solve_mismatch_result_and_b_type(%arg0: tensor<4x4xf32>, %arg1: tensor<4x3xf32>) -> tensor<4x4xf32> {
  // expected-error@+1 {{result and operand 'b' must have same shape, but got 'tensor<4x4xf32>' and 'tensor<4x3xf32>'}}
  %0 = "mhlo.triangular_solve"(%arg0, %arg1) {left_side = true, lower = true, transpose_a = "NO_TRANSPOSE", unit_diagonal = true} : (tensor<4x4xf32>, tensor<4x3xf32>) -> tensor<4x4xf32>
  return %0 : tensor<4x4xf32>
}

// -----

// CHECK-LABEL: func @tuple
func @tuple(%arg0: tensor<1xi32>, %arg1: tensor<1x2xf32>) -> tuple<tensor<1xi32>, tensor<1x2xf32>> {
  %0 = "mhlo.tuple"(%arg0, %arg1) : (tensor<1xi32>, tensor<1x2xf32>) -> tuple<tensor<1xi32>, tensor<1x2xf32>>
  return %0: tuple<tensor<1xi32>, tensor<1x2xf32>>
}

// -----

func @tuple_token(%arg0: tensor<f32>, %arg1: !mhlo.token) -> tuple<tensor<f32>, !mhlo.token> {
  %0 = "mhlo.tuple"(%arg0, %arg1) : (tensor<f32>, !mhlo.token) -> tuple<tensor<f32>, !mhlo.token>
  return %0 : tuple<tensor<f32>, !mhlo.token>
}

// -----

func @tuple_arg_size_mismatch(%arg0: tensor<f32>, %arg1: tensor<f32>) -> tuple<tensor<f32>, tensor<f32>, tensor<f32>> {
  // expected-error@+1 {{number of operands to tuple expected to match number of types}}
  %0 = "mhlo.tuple"(%arg0, %arg1) : (tensor<f32>, tensor<f32>) -> tuple<tensor<f32>, tensor<f32>, tensor<f32>>
  return %0 : tuple<tensor<f32>, tensor<f32>, tensor<f32>>
}

// -----

func @tuple_type_mismatch(%arg0: tensor<f32>, %arg1: tensor<f32>) -> tuple<tensor<f32>, tensor<i32>> {
  // expected-error@+1 {{op has return type mismatch at 1th value}}
  %0 = "mhlo.tuple"(%arg0, %arg1) : (tensor<f32>, tensor<f32>) -> tuple<tensor<f32>, tensor<i32>>
  return %0 : tuple<tensor<f32>, tensor<i32>>
}

// -----

func @get_tuple_element(%arg0: tuple<tensor<f32>, tensor<i32>>) -> tensor<f32> {
  %0 = "mhlo.get_tuple_element"(%arg0) {index = 0 : i32} : (tuple<tensor<f32>, tensor<i32>>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

func @get_tuple_element_token(%arg0: tuple<tensor<f32>, !mhlo.token>) -> !mhlo.token {
  %0 = "mhlo.get_tuple_element"(%arg0) {index = 1 : i32} : (tuple<tensor<f32>, !mhlo.token>) -> !mhlo.token
  return %0 : !mhlo.token
}

// -----

func @get_tuple_element_bad_type(%arg0: tuple<tensor<f32>, tensor<i32>>) -> tensor<i32> {
  // expected-error@+1 {{has return type tensor<i32>, but expected tensor<f32>}}
  %0 = "mhlo.get_tuple_element"(%arg0) {index = 0 : i32} : (tuple<tensor<f32>, tensor<i32>>) -> tensor<i32>
  return %0 : tensor<i32>
}

// -----

func @get_tuple_element_index_out_of_bounds(%arg0: tuple<tensor<f32>, tensor<i32>>) -> tensor<f32> {
  // expected-error@+1 {{index 2 is out of bounds of operand with size 2}}
  %0 = "mhlo.get_tuple_element"(%arg0) {index = 2 : i32} : (tuple<tensor<f32>, tensor<i32>>) -> tensor<f32>
  return %0 : tensor<f32>
}

// -----

// CHECK-LABEL: func @and_i32_type
func @and_i32_type(%arg0: tensor<4xi32>, %arg1: tensor<4xi32>) -> tensor<4xi32> {
  %0 = "mhlo.and"(%arg0, %arg1) : (tensor<4xi32>, tensor<4xi32>) -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----
// CHECK-LABEL: func @or_i1_type
func @or_i1_type(%arg0: tensor<4xi1>, %arg1: tensor<4xi1>) -> tensor<4xi1> {
  %0 = "mhlo.or"(%arg0, %arg1) : (tensor<4xi1>, tensor<4xi1>) -> tensor<4xi1>
  return %0 : tensor<4xi1>
}

// -----

func @or_invalid_f32_type(%arg0: tensor<4xf32>, %arg1: tensor<4xf32>) -> tensor<4xf32> {
  // expected-error@+1 {{but got 'tensor<4xf32>'}}
  %0 = "mhlo.or"(%arg0, %arg1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
  return %0 : tensor<4xf32>
}

// -----

func @floor_invalid_i32_type(%arg0: tensor<4xi32>) -> tensor<4xi32> {
  // expected-error@+1 {{must be tensor of floating-point values, but got 'tensor<4xi32>'}}
  %0 = "mhlo.floor"(%arg0) : (tensor<4xi32>) -> tensor<4xi32>
  return %0 : tensor<4xi32>
}

// -----

// Verifiers HLO constant op custom printing and parsing.
// CHECK-LABEL: func @constants
func @constants() -> () {
  // CHECK: mhlo.constant dense<0> : tensor<i32>
  %0 = "mhlo.constant"() {value = dense<0> : tensor<i32>} : () -> (tensor<i32>)

  // CHECK: mhlo.constant {extra_attr = 3 : i32} dense<0> : tensor<i32>
  %1 = "mhlo.constant"() {extra_attr = 3 : i32, value = dense<0> : tensor<i32>} : () -> (tensor<i32>)
  return
}

// -----

func @constant_invalid() -> () {
  // expected-error@+1 {{op failed to verify that all of {value, output} have same type}}
  %0 = "mhlo.constant"() {value = dense<0> : tensor<i32>} : () -> (tensor<3xi32>)
  return
}

// -----

func @constant_invalid() -> () {
  // expected-error@+1 {{op result #0 must be statically shaped tensor}}
  %0 = "mhlo.constant"() {value = dense<1> : tensor<i32>} : () -> tensor<?xi32>
  return
}

// -----

func @constant_invalid() -> () {
  // expected-error@+1 {{elements literal type must have static shape}}
  %0 = "mhlo.constant"() {value = dense<1> : tensor<?xi32>} : () -> tensor<?xi32>
  return
}

// -----

func @sort(%input0: tensor<16x16xf32>, %input1: tensor<16x16xi32>) {
  // CHECK: mhlo.sort
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<i32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : (tensor<16x16xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_no_operands() {
  // expected-error @+1 {{expected named operation to have atleast 1 result}}
  %0:0 = "mhlo.sort"() ( {
  ^bb0(%arg1: tensor<f32>, %arg2: tensor<f32>, %arg3: tensor<i32>, %arg4: tensor<i32>):
    %7 = "mhlo.compare"(%arg1, %arg2) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : () -> ()
  return
}

// -----

func @sort_unknown_rank(%input0: tensor<*xf32>, %input1: tensor<16x16xi32>) {
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<i32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : (tensor<*xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_unknown_rank(%input0: tensor<*xf32>, %input1: tensor<16x16xi32>) {
  // expected-error @+1 {{comparator block argument #0 should be of type 'tensor<f32>' but got 'tensor<i32>'}}
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<i32>, %arg1: tensor<i32>, %arg2: tensor<i32>, %arg3: tensor<i32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : (tensor<*xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_different_dims(%input0: tensor<16x8xf32>, %input1: tensor<16x16xi32>) {
  // expected-error @+1 {{op requires the same shape for all operands and results}}
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<i32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : (tensor<16x8xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_dim_out_of_range(%input0: tensor<16x16xf32>, %input1: tensor<16x16xi32>) {
  // expected-error @+1 {{dimension attribute value must be in range [-2, 2), but found 10}}
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<i32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 10 : i64, is_stable = true} : (tensor<16x16xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_dim_out_of_range(%input0: tensor<16x16xf32>, %input1: tensor<16x16xi32>) {
  // expected-error @+1 {{dimension attribute value must be in range [-2, 2), but found -3}}
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<i32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = -3 : i64, is_stable = true} : (tensor<16x16xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_wrong_block_arg_count(%input0: tensor<16x16xf32>, %input1: tensor<16x16xi32>) {
  // expected-error @+1 {{op comparator block should have 4 arguments}}
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : (tensor<16x16xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

func @sort_wrong_block_arg_type(%input0: tensor<16x16xf32>, %input1: tensor<16x16xi32>) {
  // expected-error @+1 {{op comparator block argument #3 should be of type 'tensor<i32>' but got 'tensor<f32>'}}
  %0:2 = "mhlo.sort"(%input0, %input1) ( {
  ^bb0(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<i32>, %arg3: tensor<f32>):
    %7 = "mhlo.compare"(%arg0, %arg1) {comparison_direction = "GT"} : (tensor<f32>, tensor<f32>) -> tensor<i1>
    "mhlo.return"(%7) : (tensor<i1>) -> ()
  }) {dimension = 1 : i64, is_stable = true} : (tensor<16x16xf32>, tensor<16x16xi32>) -> (tensor<16x16xf32>, tensor<16x16xi32>)
  return
}

// -----

// CHECK: func @dequantize
func @dequantize(%arg: tensor<16x16xi32>) -> tensor<16x64xbf16> {
  %0 = "mhlo.dequantize"(%arg) {min_range = -0.1 : f32, max_range = 0.1 : f32, mode = "MIN_COMBINED", transpose_output = false} : (tensor<16x16xi32>) -> tensor<16x64xbf16>
  return %0 : tensor<16x64xbf16>
}

// -----

func @dequantize_wrong_shape(%arg: tensor<16x16xi32>) -> tensor<16x64xbf16> {
  // expected-error @+1 {{mismatched dimensions.}}
  %0 = "mhlo.dequantize"(%arg) {min_range = -0.1 : f32, max_range = 0.1 : f32, mode = "MIN_COMBINED", transpose_output = true} : (tensor<16x16xi32>) -> tensor<16x64xbf16>
  return %0 : tensor<16x64xbf16>
}

// -----

func @dequantize_wrong_size(%arg: tensor<16x16xi32>) -> tensor<16x16xbf16> {
  // expected-error @+1 {{last dimension of output should be 4x of the input.}}
  %0 = "mhlo.dequantize"(%arg) {min_range = -0.1 : f32, max_range = 0.1 : f32, mode = "MIN_COMBINED", transpose_output = false} : (tensor<16x16xi32>) -> tensor<16x16xbf16>
  return %0 : tensor<16x16xbf16>
}

// -----

func @dequantize_wrong_mode(%arg: tensor<16x16xi32>) -> tensor<16x64xbf16> {
  // expected-error @+1 {{Dequantization mode. Only MIN_COMBINED is supported.}}
  %0 = "mhlo.dequantize"(%arg) {min_range = -0.1 : f32, max_range = 0.1 : f32, mode = "hello", transpose_output = false} : (tensor<16x16xi32>) -> tensor<16x64xbf16>
  return %0 : tensor<16x64xbf16>
}

// -----

func @reshape_invalid_shapes(%operand: tensor<2x4xf32>) -> tensor<3x3xf32> {
  // expected-error @+1 {{number of output elements (9) doesn't match expected number of elements (8)}}
  %0 = "mhlo.reshape"(%operand) : (tensor<2x4xf32>) -> tensor<3x3xf32>
  return %0 : tensor<3x3xf32>
}

// -----

func @dot_general(%arg0: tensor<?x?x?xf32>, %arg1: tensor<?x?x?xf32>) {
  // expected-error @+1 {{lhs and rhs should have the same number of batching dimensions}}
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [],
      lhs_contracting_dimensions = [2],
      rhs_contracting_dimensions = [1]
    >
  } : (tensor<?x?x?xf32>, tensor<?x?x?xf32>) -> tensor<?x?x?xf32>
  return
}

// -----

func @dot_general(%arg0: tensor<?x?x?xf32>, %arg1: tensor<?x?x?xf32>) {
  // expected-error @+1 {{lhs and rhs should have the same number of contracting dimensions}}
  %0 = "mhlo.dot_general"(%arg0, %arg1) {
    dot_dimension_numbers = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [0],
      lhs_contracting_dimensions = [],
      rhs_contracting_dimensions = [1]
    >
  } : (tensor<?x?x?xf32>, tensor<?x?x?xf32>) -> tensor<?x?x?xf32>
  return
}

// -----

func @compatible_shapes(%arg0: tensor<?xf32>, %shape: tensor<2xindex>) -> tensor<?x?xf32> {
  %0 = "mhlo.dynamic_reshape"(%arg0, %shape) : (tensor<?xf32>, tensor<2xindex>) -> tensor<?x?xf32>
  return %0 : tensor<?x?xf32>
}

// -----

func @incompatible_shapes(%arg0: tensor<?xf32>, %shape: tensor<2xindex>) -> tensor<?xf32> {
  // expected-error @+1 {{output should have a rank equal to the number of elements in output_shape}}
  %0 = "mhlo.dynamic_reshape"(%arg0, %shape) : (tensor<?xf32>, tensor<2xindex>) -> tensor<?xf32>
  return %0 : tensor<?xf32>
}

// -----

func @cbrt(%arg: tensor<2x4xf32>) -> tensor<2x4xf32> {
  %0 = "mhlo.cbrt"(%arg) : (tensor<2x4xf32>) -> tensor<2x4xf32>
  return %0 : tensor<2x4xf32>
}

// -----

func @bitcast(%arg: tensor<2x4xf32>) -> tensor<2x4xf32> {
  %0 = "mhlo.bitcast"(%arg) : (tensor<2x4xf32>) -> tensor<2x4xf32>
  return %0 : tensor<2x4xf32>
}

// -----

func @bitcast(%arg: tensor<2x4xf32>) -> tensor<2x4xf32> {
  %0 = "mhlo.reduce_precision"(%arg) {exponent_bits=2 : i32, mantissa_bits=3 : i32} : (tensor<2x4xf32>) -> tensor<2x4xf32>
  return %0 : tensor<2x4xf32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<*xi32>, %start_indices : tensor<*xi32>) -> tensor<*xi32> {
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  // expected-error@+1 {{index_vector_dim 4 is out of bounds for start indices with rank 3}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 4,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  // expected-error@+1 {{offset_dims size (2) plus collapse_slice_dims size (2) is not equal to operand rank (3)}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [1, 2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  // expected-error@+1 {{start_index_map size (1) is not equal to size of index dimension (2) of start_indices (2)}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  // expected-error@+1 {{slice_sizes.rank != 1}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[[1, 1, 8]]> : tensor<1x3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  // expected-error@+1 {{slice_sizes size (2) not equal to (implied) operand rank (3)}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 8]> : tensor<2xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<*xi32>, %start_indices : tensor<*xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{slice_sizes size (6) not equal to (implied) operand rank (3)}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8, 1, 2, 3]> : tensor<6xi64>
  } : (tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<3xi32> {
  // expected-error@+1 {{inferred type(s) 'tensor<1x5x8xi32>' are incompatible with return type(s) of operation 'tensor<3xi32>'}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<3xi32>
  return %res : tensor<3xi32>
}

// -----

func @gather(%operand : tensor<*xi32>, %start_indices : tensor<?x?x?xi32>) -> tensor<3xi32> {
  // expected-error@+1 {{inferred type(s) 'tensor<8x?x7x1x6x1x?xi32>' are incompatible with return type(s) of operation 'tensor<3xi32>'}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1, 3],
      index_vector_dim = 2,
      offset_dims = [0, 2, 3, 4, 5],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8, 1, 7, 1, 6, 1]> : tensor<8xi64>
  } : (tensor<*xi32>, tensor<?x?x?xi32>) -> tensor<3xi32>
  return %res : tensor<3xi32>
}

// -----

func @gather(%operand : tensor<*xi32>, %start_indices : tensor<*xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{slice_sizes collapsed dimension 2 != 1 (8)}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 2],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>) -> tensor<1x5x8xi32> {
  // expected-error@+1 {{collapsed dimension 17 is greater than slice_sizes.size (3)}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 17],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @gather(%operand : tensor<?x?x2xi32>, %start_indices : tensor<*xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{slice size (8) is larger than operand dimension (2) at index 2}}
  %res = "mhlo.gather"(%operand, %start_indices) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false,
    slice_sizes = dense<[1, 1, 8]> : tensor<3xi64>
  } : (tensor<?x?x2xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>, %slice_sizes : tensor<3xi32>) -> tensor<1x5x8xi32> {
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>, tensor<3xi32>) -> tensor<1x5x8xi32>
  return %res : tensor<1x5x8xi32>
}

// -----

func @dynamic_gather(%operand : tensor<*xi32>, %start_indices : tensor<*xi32>, %slice_sizes : tensor<*xi32>) -> tensor<*xi32> {
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<*xi32>, tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<*xi32>, %slice_sizes : tensor<*xi32>) -> tensor<*xi32> {
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<2x4x9xi32>, tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}


// -----

func @dynamic_gather(%operand : tensor<*xi32>, %start_indices : tensor<?x?x?xi32>, %slice_sizes : tensor<*xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{index_vector_dim 4 is out of bounds for start indices with rank 3}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 4,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<*xi32>, tensor<?x?x?xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<?x?x?xi32>, %start_indices : tensor<*xi32>, %slice_sizes : tensor<*xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{offset_dims size (2) plus collapse_slice_dims size (2) is not equal to operand rank (3)}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [1, 2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<?x?x?xi32>, tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<*xi32>, %start_indices : tensor<?x?x2xi32>, %slice_sizes : tensor<*xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{start_index_map size (1) is not equal to size of index dimension (2) of start_indices (2)}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0]
    >,
    indices_are_sorted = false
  } : (tensor<*xi32>, tensor<?x?x2xi32>, tensor<*xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<*xi32>, %start_indices : tensor<*xi32>, %slice_sizes : tensor<?x?xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{slice_sizes.rank != 1}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<*xi32>, tensor<*xi32>, tensor<?x?xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<?x?x?xi32>, %start_indices : tensor<*xi32>, %slice_sizes : tensor<2xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{slice_sizes size (2) not equal to (implied) operand rank (3)}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<?x?x?xi32>, tensor<*xi32>, tensor<2xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<*xi32>, %start_indices : tensor<*xi32>, %slice_sizes : tensor<3xi32>) -> tensor<*xi32> {
  // expected-error@+1 {{collapsed dimension 17 is greater than slice_sizes.size (3)}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 17],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<*xi32>, tensor<*xi32>, tensor<3xi32>) -> tensor<*xi32>
  return %res : tensor<*xi32>
}

// -----

func @dynamic_gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>, %slice_sizes : tensor<3xi32>) -> tensor<3xi32> {
  // expected-error@+1 {{inferred type(s) 'tensor<1x5x?xi32>' are incompatible with return type(s) of operation 'tensor<3xi32>'}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>, tensor<3xi32>) -> tensor<3xi32>
  return %res : tensor<3xi32>
}

// -----

func @dynamic_gather(%operand : tensor<2x4x9xi32>, %start_indices : tensor<1x5x2xi32>, %slice_sizes : tensor<*xi32>) -> tensor<3xi32> {
  // expected-error@+1 {{inferred type(s) 'tensor<1x5x?xi32>' are incompatible with return type(s) of operation 'tensor<3xi32>'}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<2x4x9xi32>, tensor<1x5x2xi32>, tensor<*xi32>) -> tensor<3xi32>
  return %res : tensor<3xi32>
}

// -----

func @dynamic_gather(%operand : tensor<?x?x?xi32>, %start_indices : tensor<?x?x?xi32>, %slice_sizes : tensor<3xi32>) -> tensor<3xi32> {
  // expected-error@+1 {{inferred type(s) 'tensor<?x?x?xi32>' are incompatible with return type(s) of operation 'tensor<3xi32>'}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<?x?x?xi32>, tensor<?x?x?xi32>, tensor<3xi32>) -> tensor<3xi32>
  return %res : tensor<3xi32>
}

// -----

func @dynamic_gather(%operand : tensor<*xi32>, %start_indices : tensor<?x?x?xi32>, %slice_sizes : tensor<?xi32>) -> tensor<?xi32> {
  // expected-error@+1 {{inferred type(s) 'tensor<?x?x?xi32>' are incompatible with return type(s) of operation 'tensor<?xi32>'}}
  %res = "mhlo.dynamic_gather"(%operand, %start_indices, %slice_sizes) {
    dimension_numbers = #mhlo.gather<
      collapsed_slice_dims = [0, 1],
      index_vector_dim = 2,
      offset_dims = [2],
      start_index_map = [0, 1]
    >,
    indices_are_sorted = false
  } : (tensor<*xi32>, tensor<?x?x?xi32>, tensor<?xi32>) -> tensor<?xi32>
  return %res : tensor<?xi32>
}

// -----

func @get_dimension_size(%I: tensor<1x128x512xf32>) -> tensor<i32> {
  // expected-error@+1 {{requires dimension attribute in range [0, 3); found (3)}}
  %size = "mhlo.get_dimension_size"(%I) {dimension = 3 : i64} : (tensor<1x128x512xf32>) -> tensor<i32>
  return %size : tensor<i32>
}

// -----

func @get_dimension_size(%I: tensor<1x128x512xf32>) -> tensor<i32> {
  %size = "mhlo.get_dimension_size"(%I) {dimension = 2 : i64} : (tensor<1x128x512xf32>) -> tensor<i32>
  return %size : tensor<i32>
}

// -----

func @set_dimension_size(%I: tensor<1x128x512xf32>) -> tensor<1x128x512xf32> {
  %dim = mhlo.constant dense<512> : tensor<1xi32>

  // expected-error@+1 {{size operand should be of rank-0}}
  %result = "mhlo.set_dimension_size"(%I, %dim) {dimension = 2 : i64} : (tensor<1x128x512xf32>, tensor<1xi32>) -> tensor<1x128x512xf32>
  return %result : tensor<1x128x512xf32>
}

// -----

func @set_dimension_size(%I: tensor<1x128x512xf32>) -> tensor<1x128x512xf32> {
  %dim = mhlo.constant dense<512> : tensor<i32>

  // expected-error@+1 {{requires dimension attribute in range [0, 3); found (3)}}
  %result = "mhlo.set_dimension_size"(%I, %dim) {dimension = 3 : i64} : (tensor<1x128x512xf32>, tensor<i32>) -> tensor<1x128x512xf32>
  return %result : tensor<1x128x512xf32>
}

// -----

// CHECK: func @custom_call_multiple_inputs_outputs
func @custom_call_multiple_inputs_outputs(%x: tensor<2xf32>, %token: !mhlo.token) -> tensor<2xf32> {
  %0:3 = "mhlo.custom_call"(%x, %token) {backend_config="", call_target_name = "foo", has_side_effect = false} : (tensor<2xf32>, !mhlo.token) -> (tensor<2xf32>, tensor<2xf32>, !mhlo.token)
  %1 = "mhlo.add"(%0#0, %0#1) : (tensor<2xf32>, tensor<2xf32>) -> tensor<2xf32>
  return %1 : tensor<2xf32>
}

// -----

// CHECK: func @custom_call_multiple_inputs_outputs_with_layout
func @custom_call_multiple_inputs_outputs_with_layout(%x: tensor<2xf32>, %token: !mhlo.token) -> tensor<f32> {
  %0:3 = "mhlo.custom_call"(%x, %token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>],
    result_layouts = [dense<> : tensor<0xindex>, dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>]
  } : (tensor<2xf32>, !mhlo.token) -> (tensor<f32>, tensor<2xf32>, !mhlo.token)
  return %0#0 : tensor<f32>
}

// -----

// CHECK: func @custom_call_tuple_output_with_layout
func @custom_call_tuple_output_with_layout(%x: tensor<2xf32>, %token: !mhlo.token) -> tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token> {
  %0 = "mhlo.custom_call"(%x, %token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>],
    result_layouts = [dense<[0]> : tensor<1xindex>, dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>]
  } : (tensor<2xf32>, !mhlo.token) -> tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token>
  return %0 : tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token>
}

// -----

func @custom_call_only_operand_layout_constraints(%x: tensor<2xf32>, %token: !mhlo.token) -> tensor<2xf32> {
  // expected-error@+1 {{Layout attributes should be specified for either both operands and results or none}}
  %0:3 = "mhlo.custom_call"(%x, %token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>]
  } : (tensor<2xf32>, !mhlo.token) -> (tensor<2xf32>, tensor<2xf32>, !mhlo.token)
  return %0#0 : tensor<2xf32>
}

// -----

func @custom_call_layout_mismatch_num_operands(%x: tensor<2xf32>, %token: !mhlo.token) -> tensor<2xf32> {
  // expected-error@+1 {{Number of operands must match the number of operand layouts, 2 != 1}}
  %0:3 = "mhlo.custom_call"(%x, %token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0]> : tensor<1xindex>],
    result_layouts = [dense<[0]> : tensor<1xindex>, dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>]
  } : (tensor<2xf32>, !mhlo.token) -> (tensor<2xf32>, tensor<2xf32>, !mhlo.token)
  return %0#0 : tensor<2xf32>
}

// -----

func @custom_call_layout_mismatch_num_results() -> tensor<2xf32> {
  // expected-error@+1 {{Number of results must match the number of result layouts, 3 != 2}}
  %0:3 = "mhlo.custom_call"() {
    call_target_name = "foo",
    operand_layouts = [],
    result_layouts = [dense<[0]> : tensor<1xindex>, dense<[0]> : tensor<1xindex>]
  } : () -> (tensor<2xf32>, tensor<2xf32>, !mhlo.token)
  return %0#0 : tensor<2xf32>
}

// -----

func @custom_call_layout_mismatch_num_results_tuple(%x: tensor<2xf32>, %token: !mhlo.token) -> tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token> {
  // expected-error@+1 {{Number of results must match the number of result layouts, 3 != 2}}
  %0 = "mhlo.custom_call"(%x, %token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>],
    result_layouts = [dense<[0]> : tensor<1xindex>, dense<[0]> : tensor<1xindex>]
  } : (tensor<2xf32>, !mhlo.token) -> tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token>
  return %0 : tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token>
}

// -----

func @custom_call_tuple_operand_input(%x: tuple<tensor<2xf32>>, %token: !mhlo.token) -> tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token> {
  // expected-error@+1 {{Tuple types are not fully supported with layout constraints yet}}
  %0 = "mhlo.custom_call"(%x, %token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>],
    result_layouts = [dense<[0]> : tensor<1xindex>, dense<[0]> : tensor<1xindex>, dense<> : tensor<0xindex>]
  } : (tuple<tensor<2xf32>>, !mhlo.token) -> tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token>
  return %0 : tuple<tensor<2xf32>, tensor<2xf32>, !mhlo.token>
}

// -----

func @custom_call_token_with_layout(%token: !mhlo.token) {
  // expected-error@+1 {{Only tensor types can have non-empty layout: operand #0 of type '!mhlo.token' has layout dense<[0, 1]> : tensor<2xindex>}}
  "mhlo.custom_call"(%token) {
    call_target_name = "foo",
    operand_layouts = [dense<[0, 1]> : tensor<2xindex>],
    result_layouts = []
  } : (!mhlo.token) -> ()
  return
}

// -----

func @custom_call_mismatch_tensor_and_layout_rank(%arg: tensor<2x3xf32>) {
  // expected-error@+1 {{incorrect layout dense<[0, 1, 2]> : tensor<3xindex> for type 'tensor<2x3xf32>', layout must be a permutation of [0, 2)}}
  "mhlo.custom_call"(%arg) {
    call_target_name = "foo",
    operand_layouts = [dense<[0, 1, 2]> : tensor<3xindex>],
    result_layouts = []
  } : (tensor<2x3xf32>) -> ()
  return
}

// -----

func @custom_call_mismatch_tensor_and_layout_permutation(%arg: tensor<1x2x3xf32>) {
  // expected-error@+1 {{incorrect layout dense<[0, 1, 3]> : tensor<3xindex> for type 'tensor<1x2x3xf32>', layout must be a permutation of [0, 3)}}
  "mhlo.custom_call"(%arg) {
    call_target_name = "foo",
    operand_layouts = [dense<[0, 1, 3]> : tensor<3xindex>],
    result_layouts = []
  } : (tensor<1x2x3xf32>) -> ()
  return
}

// -----

// CHECK: func @reduce_window
func @reduce_window(%arg0: tensor<4x2xf32>, %arg1: tensor<4x2xi32>, %init0: tensor<f32>, %init1: tensor<i32>) -> (tensor<2x2xf32>, tensor<2x2xi32>) {
  %0:2 = "mhlo.reduce_window"(%arg0, %arg1, %init0, %init1) ({
         ^bb0(%a0: tensor<f32>, %a1: tensor<i32>, %b0: tensor<f32>, %b1: tensor<i32>):  // no predecessors
              %2 = mhlo.add %a0, %b0 : tensor<f32>
              %3 = mhlo.add %a1, %b1 : tensor<i32>
              %4 = "mhlo.tuple"(%2, %3) : (tensor<f32>, tensor<i32>) -> tuple<tensor<f32>, tensor<i32>>
              "mhlo.return"(%4) : (tuple<tensor<f32>, tensor<i32>>) -> ()
            })
         { padding = dense<[[2, 2], [0, 0]]> : tensor<2x2xi64>,
           window_dimensions = dense<[5, 1]> : tensor<2xi64>,
           window_strides = dense<[3, 1]> : tensor<2xi64> } : (tensor<4x2xf32>, tensor<4x2xi32>, tensor<f32>, tensor<i32>) -> (tensor<2x2xf32>, tensor<2x2xi32>)
  return %0#0, %0#1 : tensor<2x2xf32>, tensor<2x2xi32>
}

// -----

func @reduce_window_invalid(%arg0: tensor<4x2xf32>, %arg1: tensor<4x3xi32>, %init0: tensor<f32>, %init1: tensor<i32>) -> (tensor<2x2xf32>, tensor<2x2xi32>) {
  // expected-error @+1 {{requires same shape for all inputs}}
  %0:2 = "mhlo.reduce_window"(%arg0, %arg1, %init0, %init1) ({
         ^bb0(%a0: tensor<f32>, %a1: tensor<i32>, %b0: tensor<f32>, %b1: tensor<i32>):  // no predecessors
              %2 = mhlo.add %a0, %b0 : tensor<f32>
              %3 = mhlo.add %a1, %b1 : tensor<i32>
              %4 = "mhlo.tuple"(%2, %3) : (tensor<f32>, tensor<i32>) -> tuple<tensor<f32>, tensor<i32>>
              "mhlo.return"(%4) : (tuple<tensor<f32>, tensor<i32>>) -> ()
            })
         { padding = dense<[[2, 2], [0, 0]]> : tensor<2x2xi64>,
           window_dimensions = dense<[5, 1]> : tensor<2xi64>,
           window_strides = dense<[3, 1]> : tensor<2xi64> } : (tensor<4x2xf32>, tensor<4x3xi32>, tensor<f32>, tensor<i32>) -> (tensor<2x2xf32>, tensor<2x2xi32>)
  return %0#0, %0#1 : tensor<2x2xf32>, tensor<2x2xi32>
}

// -----

func @rng_normal_invalid(%arg0: tensor<f32>, %arg1: tensor<f32>) {
  %cst = "mhlo.constant"() {value = dense<7> : tensor<1xi64>} : () -> tensor<1xi64>
  // expected-error @+1 {{tensor<7xf32>}}
  %0 = "mhlo.rng_normal"(%arg0, %arg1, %cst) : (tensor<f32>, tensor<f32>, tensor<1xi64>) -> tensor<12xf32>
  return
}

// -----

func @rng_uniform_invalid(%arg0: tensor<f32>, %arg1: tensor<f32>, %arg2: tensor<7xi64>) {
  // expected-error @+1 {{tensor<?x?x?x?x?x?x?xf32>}}
  %0 = "mhlo.rng_uniform"(%arg0, %arg1, %arg2) : (tensor<f32>, tensor<f32>, tensor<7xi64>) -> tensor<?xf32>
  return
}

// -----
// CHECK: func @conv2d_generic
// CHECK: mhlo.convolution
// CHECK-SAME: dim_numbers = [b, 0, 1, ?, f]x[0, 1, ?, i, o]->[?, b, 0, 1, f]
// CHECK-SAME{LITERAL}: window = {stride = [1, 1], pad = [[1, 1], [1, 1]], lhs_dilate = [1, 1], rhs_dilate = [1, 1]}
func @conv2d_generic(%arg0: tensor<1x8x8x32x207xf32>, %arg1: tensor<3x3x32x207x16xf32>) -> tensor<32x1x8x8x16xf32> {
  %0 = "mhlo.convolution"(%arg0, %arg1) {batch_group_count = 1 : i64,
    dimension_numbers = #mhlo.conv<raw
      input_batch_dimension = 0,
      input_feature_dimension = 4,
      input_spatial_dimensions = [1, 2],
      kernel_input_feature_dimension = 3,
      kernel_output_feature_dimension = 4,
      kernel_spatial_dimensions = [0, 1],
      output_batch_dimension = 1,
      output_feature_dimension = 4,
      output_spatial_dimensions = [2, 3]
    >, feature_group_count = 1 : i64, lhs_dilation = dense<1> : tensor<2xi64>, padding = dense<1> : tensor<2x2xi64>, precision_config = ["DEFAULT", "DEFAULT"], rhs_dilation = dense<1> : tensor<2xi64>, window_strides = dense<1> : tensor<2xi64>} :
       (tensor<1x8x8x32x207xf32>, tensor<3x3x32x207x16xf32>) -> tensor<32x1x8x8x16xf32>
  return %0 : tensor<32x1x8x8x16xf32>
}

// CHECK: func @conv2d
// CHECK: mhlo.convolution
// CHECK-SAME: dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f]
// CHECK-SAME{LITERAL}: window = {stride = [1, 1], pad = [[1, 1], [1, 1]], lhs_dilate = [1, 1], rhs_dilate = [1, 1]}
func @conv2d(%arg0: tensor<1x8x8x207xf32>, %arg1: tensor<3x3x207x16xf32>) -> tensor<1x8x8x16xf32> {
  %0 = mhlo.convolution(%arg0, %arg1)
         dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f],
         window = {stride = [1, 1], pad = [[1, 1], [1, 1]], lhs_dilate = [1, 1], rhs_dilate = [1, 1]}
         {batch_group_count = 1 : i64, feature_group_count = 1 : i64, precision_config = ["DEFAULT", "DEFAULT"]} :
       (tensor<1x8x8x207xf32>, tensor<3x3x207x16xf32>) -> tensor<1x8x8x16xf32>
  return %0 : tensor<1x8x8x16xf32>
}

// CHECK: func @conv_empty_spatial_dimensions
// CHECK: mhlo.convolution
// CHECK-SAME: dim_numbers = [b, f]x[i, o]->[b, f]
// CHECK-SAME{LITERAL}: window = {stride = [], pad = [], lhs_dilate = [], rhs_dilate = [], reverse = []}
func @conv_empty_spatial_dimensions(%arg0: tensor<3x2xf16>, %arg1: tensor<2x2xf16>) -> tuple<tensor<3x2xf16>> {
  %0 = mhlo.convolution(%arg0, %arg1)
         dim_numbers = [b, f]x[i, o]->[b, f],
         window = {stride = [], pad = [], lhs_dilate = [], rhs_dilate = [], reverse = []}
         {batch_group_count = 1 : i64, feature_group_count = 1 : i64, precision_config = ["DEFAULT", "DEFAULT"]}
       : (tensor<3x2xf16>, tensor<2x2xf16>) -> tensor<3x2xf16>
  %1 = "mhlo.tuple"(%0) : (tensor<3x2xf16>) -> tuple<tensor<3x2xf16>>
  return %1 : tuple<tensor<3x2xf16>>
}
// -----

func @conv2d(%arg0: tensor<1x8x8x207xf32>, %arg1: tensor<3x3x207x16xf32>) -> tensor<1x8x8x16xf32> {
  // expected-error @+3 {{'mhlo.convolution' Expected array with 2 elements, got 3 elements instead}}
  %0 = mhlo.convolution(%arg0, %arg1)
         dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f],
         window = {stride = [1, 1], pad = [[1, 1, 1], [1, 1, 1]], lhs_dilate = [1, 1], rhs_dilate = [1, 1]}
         {batch_group_count = 1 : i64, feature_group_count = 1 : i64, precision_config = ["DEFAULT", "DEFAULT"]} :
       (tensor<1x8x8x207xf32>, tensor<3x3x207x16xf32>) -> tensor<1x8x8x16xf32>
  return %0 : tensor<1x8x8x16xf32>
}

// -----

// CHECK: module
// CHECK-SAME: mhlo.conv = #mhlo.conv<[b, 0, 1, f]x[0, 1, i, o]->[b, 1, 0, f]>
module attributes { mhlo.conv = #mhlo.conv<raw
      input_batch_dimension = 0,
      input_feature_dimension = 3,
      input_spatial_dimensions = [1, 2],
      kernel_input_feature_dimension = 2,
      kernel_output_feature_dimension = 3,
      kernel_spatial_dimensions = [0, 1],
      output_batch_dimension = 0,
      output_feature_dimension = 3,
      output_spatial_dimensions = [2, 1]>} {}

// -----

// CHECK-LABEL: func @convolution
// CHECK: mhlo.convolution
// CHECK-SAME: dim_numbers = [b, 1, 0, f]x[0, 1, i, o]->[b, 0, 1, f]
// CHECK-SAME{LITERAL}: window = {stride = [2, 1], pad = [[0, 1], [0, 1]], rhs_dilate = [1, 2]}
func @convolution(%arg0: tensor<2x2x3x4xf32>, %arg1: tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32> {
  %0 = mhlo.convolution(%arg0, %arg1)
     dim_numbers = [b, 1, 0, f]x[0, 1, i, o]->[b, 0, 1, f],
     window = {stride = [2, 1], pad = [[0, 1], [0, 1]], rhs_dilate = [1, 2]}
     { batch_group_count = 1 : i64, feature_group_count = 1 : i64}
  : (tensor<2x2x3x4xf32>, tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32>
  return %0 : tensor<3x5x5x4xf32>
}

// -----

// CHECK: module
// CHECK: mhlo.conv = #mhlo.conv<[b, 1, 0, f]x[0, 1, i, o]->[b, 0, 1, f]>
module attributes {
  mhlo.conv = #mhlo.conv<[b, 1, 0, f]x[0, 1, i, o]->[b, 0, 1, f]>
} {}

// -----

// CHECK: module
// CHECK: mhlo.conv = #mhlo.conv<[b, 1, 0, ?, f]x[?, 0, 1, i, o]->[b, ?, 0, 1, f]>
module attributes {
  mhlo.conv = #mhlo.conv<[b, 1, 0, ?, f]x[?, 0, 1, i, o]->[b, ?, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{Unexpected dimension c, expecting b, f}}
  mhlo.conv = #mhlo.conv<[c, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{Unexpected dimension b, expecting i, o}}
  mhlo.conv = #mhlo.conv<[b, 0, 1, f]x[0, 1, b, o]->[b, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{Unexpected dimension i, expecting o}}
  mhlo.conv = #mhlo.conv<[b, 0, 1, f]x[0, 1, i, i]->[b, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{Expected dimensions f not specified}}
  mhlo.conv = #mhlo.conv<[b, 0, 1]x[0, 1, i, o]->[b, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{Unexpected keyword b}}
  mhlo.conv = #mhlo.conv<[b, 0, 1, f]x[0, 1, i, o, b]->[b, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{expected '['}}
  mhlo.conv = #mhlo.conv<{b, 0, 1, f}x[0, 1, i, o]->[b, 0, 1, f]>
} {}

// -----

module attributes {
  // expected-error@+1{{Expected spatial dimensions 0 not specified}}
  mhlo.conv = #mhlo.conv<[b, f, 1]x[o, 0, 1, i]->[f, b, 0, 1]>
} {}

// -----

module attributes {
  // expected-error@+1{{Duplicate entries for spatial dimension 1}}
  mhlo.conv = #mhlo.conv<[b, f, 1, 0, 1]x[o, 0, 1, i]->[f, b, 0, 1]>
} {}

// -----

module attributes {
  // expected-error@+1{{Unexpected dimension -2}}
  mhlo.conv = #mhlo.conv<[b, f, 1, -2]x[o, 0, 1, i]->[f, b, 0, 1]>
} {}

// -----

func @convolution(%arg0: tensor<2x2x3x4xf32>, %arg1: tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32> {
  // expected-error@+3{{Expected array with 2 elements, got 3 elements instead}}
  %0 = mhlo.convolution(%arg0, %arg1)
     dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f],
     window = {stride = [2, 1], pad = [[0, 1, 2], [0, 1]], rhs_dilate = [1, 2]}
     { batch_group_count = 1 : i64, feature_group_count = 1 : i64}
  : (tensor<2x2x3x4xf32>, tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32>
  return %0 : tensor<3x5x5x4xf32>
}

// -----

func @convolution(%arg0: tensor<2x2x3x4xf32>, %arg1: tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32> {
  // expected-error@+3{{Unexpected keyword stide}}
  %0 = mhlo.convolution(%arg0, %arg1)
     dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f],
     window = {stide = [2, 1], pad = [[0, 1], [0, 1]], rhs_dilate = [1, 2]}
     { batch_group_count = 1 : i64, feature_group_count = 1 : i64}
  : (tensor<2x2x3x4xf32>, tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32>
  return %0 : tensor<3x5x5x4xf32>
}
// -----

func @convolution(%arg0: tensor<2x2x3x4xf32>, %arg1: tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32> {
  // expected-error@+3{{expected integer value}}
  %0 = mhlo.convolution(%arg0, %arg1)
     dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f],
     window = {stride = [2, b], pad = [[0, 1], [0, 1]], rhs_dilate = [1, 2]}
     { batch_group_count = 1 : i64, feature_group_count = 1 : i64}
  : (tensor<2x2x3x4xf32>, tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32>
  return %0 : tensor<3x5x5x4xf32>
}
// -----

func @convolution(%arg0: tensor<2x2x3x4xf32>, %arg1: tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32> {
  // expected-error@+3{{Unexpected keyword stride}}
  %0 = mhlo.convolution(%arg0, %arg1)
     dim_numbers = [b, 0, 1, f]x[0, 1, i, o]->[b, 0, 1, f],
     window = {stride = [2, 1], pad = [[0, 1], [0, 1]], rhs_dilate = [1, 2], stride=[2,1]}
     { batch_group_count = 1 : i64, feature_group_count = 1 : i64}
  : (tensor<2x2x3x4xf32>, tensor<3x5x5x3xf32>) -> tensor<3x5x5x4xf32>
  return %0 : tensor<3x5x5x4xf32>
}

// -----

// CHECK: func @lt_loop
func @lt_loop(%arg0: tensor<4xf32>, %arg1: tensor<f32>, %arg2: tensor<f32>, %arg3: tensor<4xf32>, %arg4: tensor<f32>, %arg5: tensor<f32>, %arg6: tensor<f32>, %arg7: tensor<f32>, %arg8: tensor<i32>) -> (tensor<i32>, tensor<i32>, tensor<i32>) {
  %cst = arith.constant dense<-1> : tensor<i32>
  %cst_0 = arith.constant dense<1> : tensor<i32>
  %cst_1 = arith.constant dense<0> : tensor<i32>
  %cst_2 = arith.constant dense<1000> : tensor<i32>
  %1:3 = "mhlo.while"(%cst_1, %cst, %cst_2) ( {
  ^bb0(%arg9: tensor<i32>, %arg10: tensor<i32>, %arg11: tensor<i32>):  // no predecessors
    %4 = "mhlo.compare"(%arg9, %arg11) {comparison_direction = "LT"} : (tensor<i32>, tensor<i32>) -> tensor<i1>
    "mhlo.return"(%4) : (tensor<i1>) -> ()
  },  {
  ^bb0(%arg9: tensor<i32>, %arg10: tensor<i32>, %arg11: tensor<i32>):  // no predecessors
    %3 = mhlo.add %arg9, %cst_0 : tensor<i32>
    "mhlo.return"(%3, %arg10, %arg11) : (tensor<i32>, tensor<i32>, tensor<i32>) -> ()
  }) : (tensor<i32>, tensor<i32>, tensor<i32>) -> (tensor<i32>, tensor<i32>, tensor<i32>)
  return %1#0, %1#2, %1#2: tensor<i32>, tensor<i32>, tensor<i32>
}

// -----

// CHECK-LABEL: while_with_different_types
func @while_with_different_types(%arg0: tensor<3xf32>) -> tensor<3xf32> {
  %cst_0 = arith.constant dense<0> : tensor<1xi32>
  %cst_1 = arith.constant dense<[100, 100]> : tensor<2xi32>
  %cst_2 = arith.constant dense<1.00> : tensor<1xf32>
  %1:4 = "mhlo.while"(%cst_0, %cst_1, %cst_2, %arg0) ( {
  ^bb0(%arg1: tensor<1xi32>, %arg2: tensor<2xi32>, %arg3: tensor<1xf32>, %arg4: tensor<3xf32>):  // no predecessors
    %2 = arith.constant dense<0> : tensor<i32>
    %3 = "mhlo.slice"(%arg2) {limit_indices = dense<[1]> : tensor<1xi64>, start_indices = dense<[0]> : tensor<1xi64>, strides = dense<1> : tensor<1xi64>} : (tensor<2xi32>) -> tensor<1xi32>
    %4 = "mhlo.compare"(%arg1, %3) {comparison_direction = "LT"} : (tensor<1xi32>, tensor<1xi32>) -> tensor<1xi1>
    "mhlo.return"(%4) : (tensor<1xi1>) -> ()
  },  {
  ^bb0(%arg1: tensor<1xi32>, %arg2: tensor<2xi32>, %arg3: tensor<1xf32>, %arg4: tensor<3xf32>):  // no predecessors
    %3 = "mhlo.broadcast_in_dim"(%arg3) {broadcast_dimensions = dense<0> : tensor<1xi64>} : (tensor<1xf32>) -> tensor<3xf32>
    %4 = mhlo.add %3, %arg4 : tensor<3xf32>
    "mhlo.return"(%arg1, %arg2, %arg3, %4) : (tensor<1xi32>, tensor<2xi32>, tensor<1xf32>, tensor<3xf32>) -> ()
  }) : (tensor<1xi32>, tensor<2xi32>, tensor<1xf32>, tensor<3xf32>) -> (tensor<1xi32>, tensor<2xi32>, tensor<1xf32>, tensor<3xf32>)
  return %1#3: tensor<3xf32>
}

// -----

func @while_with_invalid_types(%arg0: tensor<3xf32>) -> tensor<3xf32> {
  %cst_0 = arith.constant dense<0> : tensor<1xi32>
  %cst_1 = arith.constant dense<[100, 100]> : tensor<2xi32>
  %cst_2 = arith.constant dense<1.00> : tensor<1xf32>
  // expected-error @+1 {{'mhlo.while' op requires the same type for operand and result at index 2}}
  %1:4 = "mhlo.while"(%cst_0, %cst_1, %cst_2, %arg0) ( {
  ^bb0(%arg1: tensor<1xi32>, %arg2: tensor<2xi32>, %arg3: tensor<1xf32>, %arg4: tensor<3xf32>):  // no predecessors
    %2 = arith.constant dense<0> : tensor<i32>
    %3 = "mhlo.slice"(%arg2) {limit_indices = dense<[1]> : tensor<1xi64>, start_indices = dense<[0]> : tensor<1xi64>, strides = dense<1> : tensor<1xi64>} : (tensor<2xi32>) -> tensor<1xi32>
    %4 = "mhlo.compare"(%arg1, %3) {comparison_direction = "LT"} : (tensor<1xi32>, tensor<1xi32>) -> tensor<1xi1>
    "mhlo.return"(%4) : (tensor<1xi1>) -> ()
  },  {
  ^bb0(%arg1: tensor<1xi32>, %arg2: tensor<2xi32>, %arg3: tensor<1xf32>, %arg4: tensor<3xf32>):  // no predecessors
    %3 = "mhlo.broadcast_in_dim"(%arg3) {broadcast_dimensions = dense<0> : tensor<1xi64>} : (tensor<1xf32>) -> tensor<3xf32>
    %4 = mhlo.add %3, %arg4 : tensor<3xf32>
    "mhlo.return"(%arg1, %arg2, %arg3, %4) : (tensor<1xi32>, tensor<2xi32>, tensor<1xf32>, tensor<3xf32>) -> ()
  }) : (tensor<1xi32>, tensor<2xi32>, tensor<1xf32>, tensor<3xf32>) -> (tensor<1xi32>, tensor<2xi32>, tensor<3xf32>, tensor<1xf32>)
  return %1#2: tensor<3xf32>
}

// -----

func @while_with_invalid_tuples(%arg0: tensor<3xf32>) -> tensor<3xf32> {
  %cst_0 = arith.constant dense<0> : tensor<1xi32>
  %cst_1 = arith.constant dense<[100, 100]> : tensor<2xi32>
  %cst_2 = arith.constant dense<1.00> : tensor<1xf32>
  %0 = "mhlo.tuple"(%arg0, %cst_2) : (tensor<3xf32>, tensor<1xf32>) -> tuple<tensor<3xf32>, tensor<1xf32>>
  %1 = "mhlo.tuple"(%cst_1, %0) : (tensor<2xi32>, tuple<tensor<3xf32>, tensor<1xf32>>) -> tuple<tensor<2xi32>, tuple<tensor<3xf32>, tensor<1xf32>>>
  // expected-error @+1 {{'mhlo.while' op requires the same type for operand and result at index 1}}
  %2:2 = "mhlo.while"(%cst_0, %1) ( {
  ^bb0(%arg1: tensor<1xi32>, %arg2: tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>):  // no predecessors
    %t0 = "mhlo.get_tuple_element"(%arg2) {index = 0 : i32} : (tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>) -> tensor<2xi32>
    %3 = arith.constant dense<0> : tensor<i32>
    %4 = "mhlo.slice"(%t0) {limit_indices = dense<[1]> : tensor<1xi64>, start_indices = dense<[0]> : tensor<1xi64>, strides = dense<1> : tensor<1xi64>} : (tensor<2xi32>) -> tensor<1xi32>
    %5 = "mhlo.compare"(%arg1, %4) {comparison_direction = "LT"} : (tensor<1xi32>, tensor<1xi32>) -> tensor<1xi1>
    "mhlo.return"(%5) : (tensor<1xi1>) -> ()
  },  {
  ^bb0(%arg1: tensor<1xi32>, %arg2: tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>):  // no predecessors
    %t0 = "mhlo.get_tuple_element"(%arg2) {index = 0 : i32} : (tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>) -> tensor<2xi32>
    %t1_2 = "mhlo.get_tuple_element"(%arg2) {index = 1 : i32} : (tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>) -> tuple<tensor<1xf32>, tensor<3xf32>>
    %t1 = "mhlo.get_tuple_element"(%t1_2) {index = 0 : i32} : (tuple<tensor<1xf32>, tensor<3xf32>>) -> tensor<1xf32>
    %t2 = "mhlo.get_tuple_element"(%t1_2) {index = 1 : i32} : (tuple<tensor<1xf32>, tensor<3xf32>>) -> tensor<3xf32>
    %3 = "mhlo.broadcast_in_dim"(%t1) {broadcast_dimensions = dense<0> : tensor<1xi64>} : (tensor<1xf32>) -> tensor<3xf32>
    %4 = mhlo.add %3, %t2 : tensor<3xf32>
    %5 = "mhlo.tuple"(%t1, %4) : (tensor<1xf32>, tensor<3xf32>) -> tuple<tensor<1xf32>, tensor<3xf32>>
    %6 = "mhlo.tuple"(%t0, %5) : (tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>) -> tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>
    "mhlo.return"(%arg1, %6) : (tensor<1xi32>, tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>) -> ()
  }) : (tensor<1xi32>, tuple<tensor<2xi32>, tuple<tensor<3xf32>, tensor<1xf32>>>) -> (tensor<1xi32>, tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>)
  %3 = "mhlo.get_tuple_element"(%2#1) {index = 1 : i32} : (tuple<tensor<2xi32>, tuple<tensor<1xf32>, tensor<3xf32>>>) -> tuple<tensor<1xf32>, tensor<3xf32>>
  %4 = "mhlo.get_tuple_element"(%3) {index = 1 : i32} : (tuple<tensor<1xf32>, tensor<3xf32>>) -> tensor<3xf32>
  return %4: tensor<3xf32>
}

// -----

// Test custom attribute printing/parsing.
// We really just need one op as holder, use module: this is the simplest top-level.

// CHECK: module
// CHECK-SAME: mhlo.scatter = #mhlo.scatter<>
module attributes{mhlo.scatter = #mhlo.scatter<>} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.scatter = #mhlo.scatter<update_window_dims = [1], inserted_window_dims = [0, 1], scatter_dims_to_operand_dims = [0, 2], index_vector_dim = 1>
module attributes{
 mhlo.scatter = #mhlo.scatter<
  index_vector_dim = 1,
  scatter_dims_to_operand_dims = [0, 2],
  inserted_window_dims = [0, 1],
  update_window_dims = [1]
 >} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.scatter = #mhlo.scatter<update_window_dims = [1], inserted_window_dims = [0, 1]>
module attributes{
 mhlo.scatter = #mhlo.scatter<
  inserted_window_dims = [0, 1],
  update_window_dims = [1]
 >} {}

// -----

module attributes{
 mhlo.scatter = #mhlo.scatter<
  index_vector_dim = 1,
  // expected-error@+2 {{duplicated `index_vector_dim` entry}}
  // expected-error@+1 {{failed parsing scatter dimension numbers}}
  index_vector_dim = 1,
 >} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.gather = #mhlo.gather<>
module attributes{mhlo.gather = #mhlo.gather<>} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.gather = #mhlo.gather<offset_dims = [1], collapsed_slice_dims = [0], start_index_map = [0], index_vector_dim = 1>
module attributes{
 mhlo.gather = #mhlo.gather<
   collapsed_slice_dims = [0],
   index_vector_dim = 1,
   offset_dims = [1],
   start_index_map = [0],
 >} {}

// -----

module attributes{
 mhlo.gather = #mhlo.gather<
   collapsed_slice_dims = [0],
   // expected-error @+2 {{failed parsing gather dimension numbers}}
   // expected-error @+1 {{duplicated `collapsed_slice_dims` entry}}
   collapsed_slice_dims = [0],
 >} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.dot = #mhlo.dot<
// CHECK-SAME:       lhs_batching_dimensions = [0],
// CHECK-SAME:       rhs_batching_dimensions = [1],
// CHECK-SAME:       lhs_contracting_dimensions = [2],
// CHECK-SAME:       rhs_contracting_dimensions = [3]
// CHECK-SAME:     >
module attributes {
  mhlo.dot = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [1],
      lhs_contracting_dimensions = [2],
      rhs_contracting_dimensions = [3]
  >} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.dot = #mhlo.dot<
// CHECK-SAME:       lhs_batching_dimensions = [0],
// CHECK-SAME:       rhs_batching_dimensions = [1],
// CHECK-SAME:       lhs_contracting_dimensions = [2],
// CHECK-SAME:       rhs_contracting_dimensions = [3]
// CHECK-SAME:     >
module attributes {
  mhlo.dot = #mhlo.dot<
      lhs_batching_dimensions = [0],
      rhs_batching_dimensions = [1],
      lhs_contracting_dimensions = [2],
      rhs_contracting_dimensions = [3],
  >} {}

// -----

// CHECK: module
// CHECK-SAME: mhlo.dot = #mhlo.dot<
// CHECK-SAME:       lhs_batching_dimensions = [0],
// CHECK-SAME:       rhs_batching_dimensions = [1],
// CHECK-SAME:       lhs_contracting_dimensions = [2],
// CHECK-SAME:       rhs_contracting_dimensions = [3]
// CHECK-SAME:     >
module attributes {
  mhlo.dot = #mhlo.dot<
      rhs_batching_dimensions = [1],
      rhs_contracting_dimensions = [3],
      lhs_contracting_dimensions = [2],
      lhs_batching_dimensions = [0],
  >} {}

// -----

module attributes {
  mhlo.dot = #mhlo.dot<
      rhs_batching_dimensions = [1],
      // expected-error@+2 {{duplicated `rhs_batching_dimensions` entry}}
      // expected-error@+1 {{failed parsing dot dimension numbers}}
      rhs_batching_dimensions = [3],
      lhs_contracting_dimensions = [2],
      lhs_batching_dimensions = [0],
  >} {}

// -----

module attributes {
  // expected-error@+4 {{expected '>'}}
  // expected-error@+3 {{failed parsing dot dimension numbers}}
  mhlo.dot = #mhlo.dot<
      rhs_batching_dimensions = [1]
      rhs_contracting_dimensions = [3]
      lhs_contracting_dimensions = [2]
      lhs_batching_dimensions = [0]
  >} {}


// -----

module attributes {
  // expected-error@+2 {{expected one of: `lhs_batching_dimensions`, `rhs_batching_dimensions`, `lhs_contracting_dimensions`, `rhs_contracting_dimensions`}}
  // expected-error@+1 {{failed parsing dot dimension numbers}}
  mhlo.dot = #mhlo.dot<foo = [0]>
} {}

// -----

module attributes {
  mhlo.dot = #mhlo.dot<
      rhs_batching_dimensions = [1],
      rhs_contracting_dimensions = [3],
      lhs_contracting_dimensions = [2],
      lhs_batching_dimensions = [0],
      // expected-error@+2 {{expected one of: `lhs_batching_dimensions`, `rhs_batching_dimensions`, `lhs_contracting_dimensions`, `rhs_contracting_dimensions`}}
      // expected-error@+1 {{failed parsing dot dimension numbers}}
      foo = [0]
  >} {}

// -----

// CHECK-LABEL: func @test_alias_attribute
// CHECK-SAME:  mhlo.result_alias = #mhlo.result_alias<
// CHECK-SAME:       tuple_indices = [1, 1],
// CHECK-SAME:       result_index = [2, 0, 1],
// CHECK-SAME:       must_alias>
func @test_alias_attribute (%arg0: tuple<i32, tuple<i32, tensor<3xf32>>> {mhlo.result_alias = #mhlo.result_alias<
      tuple_indices = [1, 1],
      result_index = [2, 0, 1],
      must_alias>}
    ) -> (i32, i32, tuple<tuple<i32, tensor<3xf32>>>) {
  %0:3 = "Test.Op"() : () -> (i32, i32, tuple<tuple<i32, tensor<3xf32>>>)
  return %0#0, %0#1, %0#2 : i32, i32, tuple<tuple<i32, tensor<3xf32>>>
}

// -----

// CHECK-LABEL: func @test_alias_dynamic_dimension
// CHECK-SAME:  mhlo.result_alias = #mhlo.result_alias<result_index = [2]>
func @test_alias_dynamic_dimension (%arg0: tensor<?xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [2]>}
    ) -> (i32, i32, tensor<2xf32>) {
  %0:3 = "Test.Op"() : () -> (i32, i32, tensor<2xf32>)
  return %0#0, %0#1, %0#2 : i32, i32, tensor<2xf32>
}

// -----

// CHECK-LABEL: func @test_may_alias_no_tuple
// CHECK-SAME:  mhlo.result_alias = #mhlo.result_alias<result_index = [2]>
func @test_may_alias_no_tuple (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [2]>}
    ) -> (i32, i32, tensor<2xf32>) {
  %0:3 = "Test.Op"() : () -> (i32, i32, tensor<2xf32>)
  return %0#0, %0#1, %0#2 : i32, i32, tensor<2xf32>
}

// -----

// CHECK-LABEL: func @test_may_alias_arg_tuple
// CHECK-SAME:  mhlo.result_alias = #mhlo.result_alias<tuple_indices = [2, 0], result_index = [2]>
func @test_may_alias_arg_tuple (%arg0: tuple<i32, i32, tuple<tensor<2xf32>, i32>> {mhlo.result_alias = #mhlo.result_alias<tuple_indices = [2, 0], result_index = [2]>}
    ) -> (i32, i32, tensor<2xf32>) {
  %0:3 = "Test.Op"() : () -> (i32, i32, tensor<2xf32>)
  return %0#0, %0#1, %0#2 : i32, i32, tensor<2xf32>
}

// -----

// CHECK-LABEL: func @test_may_alias_result_tuple
// CHECK-SAME:  mhlo.result_alias = #mhlo.result_alias<result_index = [2, 1, 2]>
func @test_may_alias_result_tuple (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [2, 1, 2]>}
    ) -> (i32, i32, tuple<i32, tuple<i32, i32, tensor<2xf32>>>, i32) {
  %0:4 = "Test.Op"() : () -> (i32, i32, tuple<i32, tuple<i32, i32, tensor<2xf32>>>, i32)
  return %0#0, %0#1, %0#2, %0#3 : i32, i32, tuple<i32, tuple<i32, i32, tensor<2xf32>>>, i32
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" can only be used on function-like operations}}
module attributes {mhlo.result_alias = #mhlo.result_alias<result_index = [2, 3]>} {}

// -----

// expected-error @+2 {{expected at least 1 element(s), found 0}}
// expected-error@+1 {{failed parsing argument-result alias attribute}}
func @error_empty_result_index (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = []>}
    ) -> (tensor<2xf32>) {
  return %arg0 : tensor<2xf32>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" expects all argument and result indices to be >= 0}}
func @error_negative_arg_tuple_index (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<tuple_indices = [0, -1], result_index = [0]>}
    ) -> (tensor<2xf32>) {
  return %arg0 : tensor<2xf32>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" expects all argument and result indices to be >= 0}}
func @error_negative_result_index (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [-1]>}
    ) -> (tensor<2xf32>) {
  return %arg0 : tensor<2xf32>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" expects all argument and result indices to be >= 0}}
func @error_negative_result_tuple_index (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [0, -1]>}
    ) -> (tensor<2xf32>) {
  return %arg0 : tensor<2xf32>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" result index is out of range, must be <1}}
func @error_result_index_out_of_range (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [1]>}
    ) -> (tensor<2xf32>) {
  return %arg0 : tensor<2xf32>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" argument tuple indices are invalid}}
func @error_invalid_argument_tuple_indices (%arg0: tuple<i32, tensor<2xf32>> {mhlo.result_alias = #mhlo.result_alias<tuple_indices = [2], result_index = [0]>}
    ) -> (tensor<2xf32>) {
  %0 = "Test.Op"() : () -> (tensor<2xf32>)
  return %0 : tensor<2xf32>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" aliases do not have compatible types, 'tensor<2xf32>' vs. 'tensor<1xf32>'}}
func @error_incompatible_alias_shapes (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [0, 1]>}
    ) -> (tuple<i32, tensor<1xf32>>) {
  %0 = "Test.Op"() : () -> (tuple<i32, tensor<1xf32>>)
  return %0 : tuple<i32, tensor<1xf32>>
}

// -----

// expected-error@+1 {{attribute "mhlo.result_alias" aliases do not have compatible types, 'tensor<2xf32>' vs. 'tensor<2xi32>'}}
func @error_incompatible_alias_element_types (%arg0: tensor<2xf32> {mhlo.result_alias = #mhlo.result_alias<result_index = [0, 1]>}
    ) -> (tuple<i32, tensor<2xi32>>) {
  %0 = "Test.Op"() : () -> (tuple<i32, tensor<2xi32>>)
  return %0 : tuple<i32, tensor<2xi32>>
}
