// RUN: xla-opt %s -lhlo-legalize-to-linalg -split-input-file | FileCheck %s

// CHECK: #map0 = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @element_wise
func @element_wise(%lhs: memref<2x2xf32>, %rhs: memref<2x2xf32>,
                   %result: memref<2x2xf32>) {
  "xla_lhlo.add"(%lhs, %rhs, %result)
      : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: f32, %[[RHS_IN:.*]]: f32, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT:   %[[RESULT:.*]] = addf %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @element_wise_with_dynamic_shape
func @element_wise_with_dynamic_shape(%lhs: memref<?x?xf32>,
                                       %rhs: memref<?x?xf32>,
                                       %result: memref<?x?xf32>) {
  "xla_lhlo.add"(%lhs, %rhs, %result)
      : (memref<?x?xf32>, memref<?x?xf32>, memref<?x?xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: f32, %[[RHS_IN:.*]]: f32, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT:   %[[RESULT:.*]] = addf %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @element_wise_scalar
func @element_wise_scalar(%lhs: memref<f32>, %rhs: memref<f32>,
                          %result: memref<f32>) {
  "xla_lhlo.add"(%lhs, %rhs, %result)
      : (memref<f32>, memref<f32>, memref<f32>) -> ()
  return
}
// CHECK: %[[LHS:.*]] = load
// CHECK: %[[RHS:.*]] = load
// CHECK: %[[RES:.*]] = addf %[[LHS]], %[[RHS]]
// CHECK: store %[[RES]]
// CHECK-NEXT: return

// -----

// CHECK-LABEL: func @minf
func @minf(%lhs: memref<2x2xf32>, %rhs: memref<2x2xf32>,
           %result: memref<2x2xf32>) {
  "xla_lhlo.minimum"(%lhs, %rhs, %result)
      : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: f32, %[[RHS_IN:.*]]: f32, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT:   %[[CMP:.*]] = cmpf "olt", %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   %[[RESULT:.*]] = select %[[CMP]], %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @maxi
func @maxi(%lhs: memref<2x2xi32>, %rhs: memref<2x2xi32>,
           %result: memref<2x2xi32>) {
  "xla_lhlo.maximum"(%lhs, %rhs, %result)
      : (memref<2x2xi32>, memref<2x2xi32>, memref<2x2xi32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: i32, %[[RHS_IN:.*]]: i32, %[[RESULT_OUT:.*]]: i32):
// CHECK-NEXT:   %[[CMP:.*]] = cmpi "sgt", %[[LHS_IN]], %[[RHS_IN]] : i32
// CHECK-NEXT:   %[[RESULT:.*]] = select %[[CMP]], %[[LHS_IN]], %[[RHS_IN]] : i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i32

// -----

// CHECK-LABEL: func @and
func @and(%lhs: memref<2x2xi32>, %rhs: memref<2x2xi32>,
          %result: memref<2x2xi32>) {
  "xla_lhlo.and"(%lhs, %rhs, %result)
      : (memref<2x2xi32>, memref<2x2xi32>, memref<2x2xi32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: i32, %[[RHS_IN:.*]]: i32, %[[RESULT_OUT:.*]]: i32):
// CHECK-NEXT:   %[[RESULT:.*]] = and %[[LHS_IN]], %[[RHS_IN]] : i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i32

// -----

// CHECK-LABEL: func @exp
func @exp(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.exponential"(%input, %result)
      : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = exp %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @log
func @log(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.log"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = log %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @copy
func @copy(%in: memref<2x4x8xf32>, %out: memref<2x4x8xf32>) {
  "xla_lhlo.copy"(%in, %out) : (memref<2x4x8xf32>, memref<2x4x8xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   linalg.yield %[[OPERAND_IN]] : f32

// -----

// CHECK-LABEL: func @float_cmp
func @float_cmp(%lhs: memref<2x2xf32>, %rhs: memref<2x2xf32>,
                %result: memref<2x2xi1>) {
  "xla_lhlo.compare"(%lhs, %rhs, %result) {comparison_direction = "EQ"}
      : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xi1>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: f32, %[[RHS_IN:.*]]: f32, %[[RESULT_OUT:.*]]: i1):
// CHECK-NEXT:   %[[RESULT:.*]] = cmpf "oeq", %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i1

// -----

// CHECK-LABEL: func @int_cmp
func @int_cmp(%lhs: memref<2x2xi32>, %rhs: memref<2x2xi32>,
          %result: memref<2x2xi1>) {
  "xla_lhlo.compare"(%lhs, %rhs, %result) {comparison_direction = "LT"}
      : (memref<2x2xi32>, memref<2x2xi32>, memref<2x2xi1>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: i32, %[[RHS_IN:.*]]: i32, %[[RESULT_OUT:.*]]: i1):
// CHECK-NEXT:   %[[RESULT:.*]] = cmpi "slt", %[[LHS_IN]], %[[RHS_IN]] : i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i1

// -----

// CHECK-LABEL: func @select
func @select(%pred: memref<2x2xi1>, %lhs: memref<2x2xf32>,
             %rhs: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.select"(%pred, %lhs, %rhs, %result)
    : (memref<2x2xi1>, memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[PRED_IN:.*]]: i1, %[[LHS_IN:.*]]: f32, %[[RHS_IN:.*]]: f32, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT:   %[[RESULT:.*]] = select %[[PRED_IN]], %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK: #[[RESULT_MAP:.*]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @iota
func @iota(%out: memref<7x10xf32>) {
  "xla_lhlo.iota"(%out) {iota_dimension = 1 : i64} : (memref<7x10xf32>) -> ()
  return
}
// CHECK: linalg.indexed_generic
// CHECK-SAME: indexing_maps = [#[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[D0:.*]]: index, %[[D1:.*]]: index, %[[RESULT:.*]]: f32):
// CHECK-NEXT:   %[[INT_CAST:.*]] = index_cast %[[D1]] : index to i32
// CHECK-NEXT:   %[[FLOAT_CAST:.*]] = sitofp %[[INT_CAST]] : i32 to f32
// CHECK-NEXT:   linalg.yield %[[FLOAT_CAST]] : f32

// -----

// CHECK-DAG: #[[OPERAND_MAP:.+]] = affine_map<(d0, d1, d2) -> ()>
// CHECK-DAG: #[[RESULT_MAP:.+]] = affine_map<(d0, d1, d2) -> (d0, d1, d2)>
// CHECK-LABEL: func @broadcast_scalar
func @broadcast_scalar(%operand: memref<f32>, %result: memref<4x2x1xf32>) {
  "xla_lhlo.broadcast"(%operand, %result) {
    broadcast_sizes = dense<[4, 2, 1]> : tensor<3xi64>
  } : (memref<f32>, memref<4x2x1xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-SAME: indexing_maps = [#[[OPERAND_MAP]], #[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[OPERAND:.+]]: f32, %{{.+}}: f32):
// CHECK-NEXT:   linalg.yield %[[OPERAND]] : f32

// -----

// CHECK-DAG: #[[OPERAND_MAP:.+]] = affine_map<(d0, d1, d2, d3, d4, d5) -> (d3, d4, d5)>
// CHECK-DAG: #[[RESULT_MAP:.+]] = affine_map<(d0, d1, d2, d3, d4, d5) -> (d0, d1, d2, d3, d4, d5)>
// CHECK-LABEL: func @broadcast
func @broadcast(%operand: memref<4x?x16xf32>,
                %result: memref<4x2x1x4x?x16xf32>) {
  "xla_lhlo.broadcast"(%operand, %result) {
    broadcast_sizes = dense<[4, 2, 1]> : tensor<3xi64>
  } : (memref<4x?x16xf32>, memref<4x2x1x4x?x16xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-SAME: indexing_maps = [#[[OPERAND_MAP]], #[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[OPERAND:.+]]: f32, %{{.+}}: f32):
// CHECK-NEXT:   linalg.yield %[[OPERAND]] : f32

// -----

// CHECK-DAG: #[[OPERAND_MAP:.*]] = affine_map<(d0, d1, d2, d3, d4) -> (d4, d0, d2)>
// CHECK-DAG: #[[RESULT_MAP:.*]] = affine_map<(d0, d1, d2, d3, d4) -> (d0, d1, d2, d3, d4)>
// CHECK-LABEL: func @dynamic_broadcast_in_dim
func @dynamic_broadcast_in_dim(%operand: memref<?x?x?xf32>,
                               %result: memref<?x?x?x?x?xf32>) {
  "xla_lhlo.broadcast_in_dim"(%operand, %result) {
    broadcast_dimensions = dense<[4,0,2]> : tensor<3xi64>
  } : (memref<?x?x?xf32>, memref<?x?x?x?x?xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-SAME: indexing_maps = [#[[OPERAND_MAP]], #[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[OPERAND:.*]]: f32, %[[RESULT:.*]]: f32):
// CHECK-NEXT:   linalg.yield %[[OPERAND]] : f32

// -----

// CHECK-DAG: #[[OPERAND_MAP:.*]] = affine_map<(d0, d1) -> (d0)>
// CHECK-DAG: #[[RESULT_MAP:.*]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @static_broadcast_in_dim_no_expansion
func @static_broadcast_in_dim_no_expansion(%operand: memref<5xf32>,
                                           %result: memref<5x10xf32>) {
  "xla_lhlo.broadcast_in_dim"(%operand, %result) {
    broadcast_dimensions = dense<[0]> : tensor<1xi64>
  } : (memref<5xf32>, memref<5x10xf32>) -> ()
  return
}
// CHECK-NOT: linalg.reshape
// CHECK: linalg.generic {{{.*}}indexing_maps = [#[[OPERAND_MAP]], #[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[OPERAND:.*]]: f32, %[[RESULT:.*]]: f32):
// CHECK-NEXT:   linalg.yield %[[OPERAND]] : f32

// -----

// CHECK-DAG: #[[REASSOCIATION:.*]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-DAG: #[[OPERAND_MAP:.*]] = affine_map<(d0, d1, d2) -> (d0)>
// CHECK-DAG: #[[RESULT_MAP:.*]] = affine_map<(d0, d1, d2) -> (d0, d1, d2)>
// CHECK-LABEL: func @static_broadcast_in_dim_expansion
func @static_broadcast_in_dim_expansion(%operand: memref<1x5xf32>,
                                        %result: memref<5x10x100xf32>) {
  "xla_lhlo.broadcast_in_dim"(%operand, %result) {
    broadcast_dimensions = dense<[2, 0]> : tensor<2xi64>
  } : (memref<1x5xf32>, memref<5x10x100xf32>) -> ()
  return
}
// CHECK: %[[RESHAPED_ARG:.*]] = linalg.reshape %{{.*}}#[[REASSOCIATION]]]
// CHECK-SAME:                   memref<1x5xf32> into memref<5xf32>
// CHECK: linalg.generic {{{.*}}indexing_maps =
// CHECK-SAME:       [#[[OPERAND_MAP]], #[[RESULT_MAP]]]{{.*}} %[[RESHAPED_ARG]]
// CHECK-NEXT: ^bb0(%[[OPERAND:.*]]: f32, %[[RESULT:.*]]: f32):
// CHECK-NEXT:   linalg.yield %[[OPERAND]] : f32

// -----

// CHECK-DAG: #[[RESULT_MAP_0:.*]] = affine_map<(d0, d1) -> ()>
// CHECK-DAG: #[[RESULT_MAP:.*]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @static_broadcast_in_dim_scalar
func @static_broadcast_in_dim_scalar(%operand: memref<f32>,
                                     %result: memref<5x10xf32>) {
  "xla_lhlo.broadcast_in_dim"(%operand, %result) {
    broadcast_dimensions = dense<[]> : tensor<0xi64>
  } : (memref<f32>, memref<5x10xf32>) -> ()
  return
}
// CHECK-NOT: linalg.reshape
// CHECK: linalg.generic {{{.*}}indexing_maps = [#[[RESULT_MAP_0]], #[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[CONST:.*]]: f32, %[[RESULT:.*]]: f32):
// CHECK-NEXT:   linalg.yield %[[CONST]] : f32

// -----

// CHECK-DAG: #[[OPERAND_MAP:.+]] = affine_map<(d0, d1) -> (d0)>
// CHECK-DAG: #[[RESULT_MAP:.+]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @static_broadcast_in_dim_with_one_to_one
func @static_broadcast_in_dim_with_one_to_one(%operand: memref<1xf32>,
                                              %result: memref<1x5xf32>) {
  "xla_lhlo.broadcast_in_dim"(%operand, %result) {
    broadcast_dimensions = dense<[0]> : tensor<1xi64>
  } : (memref<1xf32>, memref<1x5xf32>) -> ()
  return
}
// CHECK-NOT: linalg.reshape
// CHECK: linalg.generic {{{.*}}indexing_maps = [#[[OPERAND_MAP]], #[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%[[OPERAND:.+]]: f32, %{{.+}}: f32):
// CHECK-NEXT:   linalg.yield %[[OPERAND]] : f32

// -----

// CHECK-DAG: #[[RESULT_MAP:.+]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @static_broadcast_in_dim_with_one_to_many
func @static_broadcast_in_dim_with_one_to_many(%operand: memref<1xf32>,
                                               %result: memref<5x5xf32>) {
  "xla_lhlo.broadcast_in_dim"(%operand, %result) {
    broadcast_dimensions = dense<[1]> : tensor<1xi64>
  } : (memref<1xf32>, memref<5x5xf32>) -> ()
  return
}
// CHECK-NOT: linalg.reshape
// CHECK: %[[C0:.*]] = constant 0 : index
// CHECK: %[[VALUE:.*]] = load %{{.*}}[[C0]]
// CHECK: linalg.generic {{{.*}}indexing_maps = [#[[RESULT_MAP]]]
// CHECK-NEXT: ^bb0(%{{.+}}: f32):
// CHECK-NEXT:   linalg.yield %[[VALUE]] : f32

// -----

// CHECK-LABEL: func @constant
func @constant(%value: memref<i32>) {
  "xla_lhlo.constant"(%value) {
    value = dense<10> : tensor<i32>
  } : (memref<i32>) -> ()
  return
}
// CHECK: %[[CONSTANT:.*]] = constant 10 : i32
// CHECK: store %[[CONSTANT]], %{{.*}}[] : memref<i32>

// -----

// CHECK-LABEL: func @absf
func @absf(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.abs"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = absf %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @absi
func @absi(%input: memref<2x2xi32>,
          %result: memref<2x2xi32>) {
  "xla_lhlo.abs"(%input, %result) : (memref<2x2xi32>, memref<2x2xi32>) -> ()
  return
}

// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: i32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[L0:.*]] = constant 0 : i32
// CHECK-NEXT:   %[[L1:.*]] = cmpi "sge", %[[OPERAND_IN]], %[[L0]] : i32
// CHECK-NEXT:   %[[L2:.*]] = subi %[[L0]], %[[OPERAND_IN]] : i32
// CHECK-NEXT:   %[[RESULT:.*]] = select %[[L1]], %[[OPERAND_IN]], %[[L2]] : i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i32

// -----

// CHECK-LABEL: func @ceil
func @ceil(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.ceil"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = ceilf %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @convert_i32_to_f32
func @convert_i32_to_f32(%input: memref<2x2xi32>, %result: memref<2x2xf32>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xi32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: i32, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT:   %[[RESULT:.*]] = sitofp %[[OPERAND_IN]] : i32 to f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @convert_i16_to_i32
func @convert_i16_to_i32(%input: memref<2x2xi16>,
          %result: memref<2x2xi32>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xi16>, memref<2x2xi32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: i16, %[[RESULT_OUT:.*]]: i32):
// CHECK-NEXT:   %[[RESULT:.*]] = zexti %[[OPERAND_IN]] : i16 to i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i32

// -----

// CHECK-LABEL: func @convert_i32_to_i16
func @convert_i32_to_i16(%input: memref<2x2xi32>, %result: memref<2x2xi16>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xi32>, memref<2x2xi16>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: i32, %[[RESULT_OUT:.*]]: i16):
// CHECK-NEXT:   %[[RESULT:.*]] = trunci %[[OPERAND_IN]] : i32 to i16
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i16

// -----

// CHECK-LABEL: func @convert_f32_to_f64
func @convert_f32_to_f64(%input: memref<2x2xf32>, %result: memref<2x2xf64>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xf32>, memref<2x2xf64>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]: f64):
// CHECK-NEXT:   %[[RESULT:.*]] = fpext %[[OPERAND_IN]] : f32 to f64
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f64

// -----

// CHECK-LABEL: func @convert_f64_to_f32
func @convert_f64_to_f32(%input: memref<2x2xf64>, %result: memref<2x2xf32>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xf64>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f64, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT:   %[[RESULT:.*]] = fptrunc %[[OPERAND_IN]] : f64 to f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @convert_i32_to_i32
func @convert_i32_to_i32(%input: memref<2x2xi32>, %result: memref<2x2xi32>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xi32>, memref<2x2xi32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: i32, %[[RESULT_OUT:.*]]: i32):
// CHECK-NEXT: linalg.yield %[[OPERAND_IN]] : i32

// -----

// CHECK-LABEL: func @convert_f32_to_f32
func @convert_f32_to_f32(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.convert"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]: f32):
// CHECK-NEXT: linalg.yield %[[OPERAND_IN]] : f32

// -----

// CHECK-LABEL: func @convert_f32_to_i32
func @convert_f32_to_i32(%input: memref<2x2xf32>, %result: memref<2x2xi32>) {
  "xla_lhlo.convert"(%input, %result)
      : (memref<2x2xf32>, memref<2x2xi32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]: i32):
// CHECK-NEXT:   %[[RESULT:.*]] = fptosi %[[OPERAND_IN]] : f32 to i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i32

// -----

// CHECK-LABEL: func @cos
func @cos(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.cosine"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = cos %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @sin
func @sin(%input: memref<2x2xf32>,
          %result: memref<2x2xf32>) {
  "xla_lhlo.sine"(%input, %result)
      : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = sin %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @negf
func @negf(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.negate"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = negf %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @negi
func @negi(%input: memref<2x2xi32>, %result: memref<2x2xi32>) {
  "xla_lhlo.negate"(%input, %result) : (memref<2x2xi32>, memref<2x2xi32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: i32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[L0:.*]] = constant 0 : i32
// CHECK-NEXT:   %[[RESULT:.*]] = subi %[[L0]], %[[OPERAND_IN]] : i32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : i32

// -----

// CHECK-LABEL: func @rem
func @remainder(%lhs: memref<2x2xf32>, %rhs: memref<2x2xf32>,
                %result: memref<2x2xf32>) {
  "xla_lhlo.remainder"(%lhs, %rhs, %result)
      : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[LHS_IN:.*]]: f32, %[[RHS_IN:.*]]: f32, %[[RESULT:.*]]: f32):
// CHECK-NEXT:   %[[RESULT:.*]] = remf %[[LHS_IN]], %[[RHS_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @rsqrt
func @rsqrt(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.rsqrt"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = rsqrt %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @sign
func @sign(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.sign"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[CST:.*]] = constant 1.000000e+00 : f32
// CHECK-NEXT:   %[[RESULT:.*]] = copysign %[[CST]], %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @sqrt
func @sqrt(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.sqrt"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = sqrt %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @tanh
func @tanh(%input: memref<2x2xf32>, %result: memref<2x2xf32>) {
  "xla_lhlo.tanh"(%input, %result) : (memref<2x2xf32>, memref<2x2xf32>) -> ()
  return
}
// CHECK: linalg.generic
// CHECK-NEXT: ^bb0(%[[OPERAND_IN:.*]]: f32, %[[RESULT_OUT:.*]]):
// CHECK-NEXT:   %[[RESULT:.*]] = tanh %[[OPERAND_IN]] : f32
// CHECK-NEXT:   linalg.yield %[[RESULT]] : f32

// -----

// CHECK-LABEL: func @complex
func @complex(%real: memref<2x2xf32>,
              %imag: memref<2x2xf32>,
              %cplx: memref<2x2xcomplex<f32>>) {
  "xla_lhlo.complex"(%real, %imag, %cplx)
      : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xcomplex<f32>>) -> ()
  return
}
// CHECK:      linalg.generic
// CHECK-NEXT: ^bb0(%[[RE:.*]]: f32, %[[IM:.*]]: f32, %[[CP:.*]]: complex<f32>):
// CHECK-NEXT:   %[[RESULT:.*]] = create_complex %[[RE]], %[[IM]] : complex<f32>
// CHECK-NEXT:   linalg.yield %[[RESULT]] : complex<f32>

// -----

// CHECK-LABEL: func @real
func @real(%cplx: memref<2x2xcomplex<f32>>,
           %real: memref<2x2xf32>) {
  "xla_lhlo.real"(%cplx, %real)
      : (memref<2x2xcomplex<f32>>, memref<2x2xf32>) -> ()
  return
}
// CHECK:      linalg.generic
// CHECK-NEXT: ^bb0(%[[CPLX_IN:.*]]: complex<f32>, %[[REAL_OUT:.*]]: f32):
// CHECK-NEXT:   %[[REAL:.*]] = re %[[CPLX_IN:.*]] : complex<f32>
// CHECK-NEXT:   linalg.yield %[[REAL]] : f32

// -----

// CHECK-LABEL: func @imag
func @imag(%cplx: memref<2x2xcomplex<f32>>,
           %imag: memref<2x2xf32>) {
  "xla_lhlo.imag"(%cplx, %imag)
      : (memref<2x2xcomplex<f32>>, memref<2x2xf32>) -> ()
  return
}
// CHECK:      linalg.generic
// CHECK-NEXT: ^bb0(%[[CPLX_IN:.*]]: complex<f32>, %[[IMAG_OUT:.*]]: f32):
// CHECK-NEXT:   %[[IMAG:.*]] = im %[[CPLX_IN:.*]] : complex<f32>
// CHECK-NEXT:   linalg.yield %[[IMAG]] : f32

// -----

// CHECK: func @slice(%[[IN:.*]]: memref<?x?xf32>, %[[OUT:.*]]: memref<?x?xf32>)
func @slice(%operand: memref<?x?xf32>, %result: memref<?x?xf32>) {
  "xla_lhlo.slice"(%operand, %result) {
    start_indices = dense<[0,1]> : tensor<2xi64>,
    limit_indices = dense<[2,3]> : tensor<2xi64>,
    strides = dense<[1,1]> : tensor<2xi64>
  } : (memref<?x?xf32>, memref<?x?xf32>) -> ()
  return
}
// CHECK: %[[L0:.*]] = constant 0 : index
// CHECK: %[[L2:.*]] = constant 2 : index
// CHECK: %[[L1:.*]] = constant 1 : index
// CHECK: %[[LHS:.*]] = linalg.range %[[L0]] : %[[L2]] : %[[L1]]
// CHECK: %[[R0:.*]] = constant 1 : index
// CHECK: %[[R2:.*]] = constant 3 : index
// CHECK: %[[R1:.*]] = constant 1 : index
// CHECK: %[[RHS:.*]] = linalg.range %[[R0]] : %[[R2]] : %[[R1]]
// CHECK: %[[RESULT:.*]] = linalg.slice %[[IN]][%[[LHS]], %[[RHS]]]
// CHECK: linalg.copy(%[[RESULT]], %[[OUT]])

// -----

// CHECK-DAG: #[[MAP1:.*]] = affine_map<(d0, d1, d2) -> (d0, d1)>
// CHECK-DAG: #[[MAP2:.*]] = affine_map<(d0, d1, d2) -> (d2)>
// CHECK-LABEL: func @reshape_3D_2D
func @reshape_3D_2D(%arg0: memref<12x1x42xi32>, %arg1 : memref<12x42xi32>) {
  "xla_lhlo.reshape"(%arg0, %arg1)
    : (memref<12x1x42xi32>, memref<12x42xi32>) -> ()
  return
}
// CHECK: linalg.reshape %{{.*}} [#[[MAP1]], #[[MAP2]]]
// CHECK-NEXT: linalg.copy

// -----

// CHECK-DAG: #[[MAP1:.*]] = affine_map<(d0, d1, d2, d3) -> (d0)>
// CHECK-DAG: #[[MAP2:.*]] = affine_map<(d0, d1, d2, d3) -> (d1, d2, d3)>
// CHECK-LABEL: func @reshape_4D_2D
func @reshape_4D_2D(%arg0: memref<12x42x1x1xi32>, %arg1 : memref<12x42xi32>) {
  "xla_lhlo.reshape"(%arg0, %arg1)
    : (memref<12x42x1x1xi32>, memref<12x42xi32>) -> ()
  return
}
// CHECK: linalg.reshape %{{.*}} [#[[MAP1]], #[[MAP2]]]
// CHECK-NEXT: linalg.copy

// -----

// CHECK-DAG: #[[MAP1:.*]] = affine_map<(d0, d1, d2, d3) -> (d0, d1)>
// CHECK-DAG: #[[MAP2:.*]] = affine_map<(d0, d1, d2, d3) -> (d2, d3)>
// CHECK-LABEL: func @reshape_2D_4D
func @reshape_2D_4D(%arg0: memref<12x42xi32>, %arg1 : memref<12x1x42x1xi32>) {
  "xla_lhlo.reshape"(%arg0, %arg1)
    : (memref<12x42xi32>, memref<12x1x42x1xi32>) -> ()
  return
}
// CHECK: linalg.reshape %{{.*}} [#[[MAP1]], #[[MAP2]]]
// CHECK-NEXT: linalg.copy

// -----

// CHECK-DAG: #[[OPERAND_MAP:.*]] = affine_map<(d0, d1) -> (d0, -d1 + 2)>
// CHECK-DAG: #[[RESULT_MAP:.*]] = affine_map<(d0, d1) -> (d0, d1)>
// CHECK-LABEL: func @reverse
func @reverse(%arg0: memref<2x3xf32>, %arg1: memref<2x3xf32>) {
  "xla_lhlo.reverse"(%arg0, %arg1) {
    dimensions = dense<1> : tensor<1xi64>
  } : (memref<2x3xf32>, memref<2x3xf32>) -> ()
  return
}
// CHECK: linalg.generic {{{.*}}indexing_maps = [#[[OPERAND_MAP]], #[[RESULT_MAP]]]


// -----

func @conv(%input: memref<3x5x5x3xf32>, %filter: memref<2x2x3x4xf32>, %output: memref<3x5x5x4xf32>) {
  %c0 = constant 0 : index
  %0 = alloc() : memref<3x5x5x4xf32>
  // CHECK: linalg.conv(%{{.+}}, %{{.+}}, %{{.+}})
  // CHECK-SAME: dilations = [1, 2]
  // CHECK-SAME: padding = dense<{{\[\[}}0, 1], [0, 1]]> : tensor<2x2xi64>
  // CHECK-SAME: strides = [2, 1]}
  // With all atributes explicitly specified.
  "xla_lhlo.convolution"(%filter, %input, %0) {batch_group_count = 1 : i64, dimension_numbers = {input_batch_dimension = 0 : i64, input_feature_dimension = 3 : i64, input_spatial_dimensions = dense<[1, 2]> : tensor<2xi64>, kernel_input_feature_dimension = 2 : i64, kernel_output_feature_dimension = 3 : i64, kernel_spatial_dimensions = dense<[0, 1]> : tensor<2xi64>, output_batch_dimension = 0 : i64, output_feature_dimension = 3 : i64, output_spatial_dimensions = dense<[1, 2]> : tensor<2xi64>}, feature_group_count = 1 : i64, padding = dense<[[0, 1], [0, 1]]> : tensor<2x2xi64>, rhs_dilation = dense<[1, 2]> : tensor<2xi64>, window_strides = dense<[2, 1]> : tensor<2xi64>} : (memref<2x2x3x4xf32>, memref<3x5x5x3xf32>, memref<3x5x5x4xf32>) -> ()

  // Dilation left unspecified, sets default dilation since linalg expects it.
  // CHECK: linalg.conv(%{{.+}}, %{{.+}}, %{{.+}})
  // CHECK-SAME: dilations = [1, 1]
  // Padding is not set if it's zero.
  // CHECK-NOT: padding
  "xla_lhlo.convolution"(%filter, %input, %0) {batch_group_count = 1 : i64, dimension_numbers = {input_batch_dimension = 0 : i64, input_feature_dimension = 3 : i64, input_spatial_dimensions = dense<[1, 2]> : tensor<2xi64>, kernel_input_feature_dimension = 2 : i64, kernel_output_feature_dimension = 3 : i64, kernel_spatial_dimensions = dense<[0, 1]> : tensor<2xi64>, output_batch_dimension = 0 : i64, output_feature_dimension = 3 : i64, output_spatial_dimensions = dense<[1, 2]> : tensor<2xi64>}, feature_group_count = 1 : i64, window_strides = dense<[2, 1]> : tensor<2xi64>} : (memref<2x2x3x4xf32>, memref<3x5x5x3xf32>, memref<3x5x5x4xf32>) -> ()

  "xla_lhlo.copy"(%0, %output) : (memref<3x5x5x4xf32>, memref<3x5x5x4xf32>) -> ()
  "xla_lhlo.terminator"() : () -> ()
}
