// RUN: tf-tfrt-opt -tf-jitrt-tile-fill %s | FileCheck %s

func @fill(%tensor : tensor<64xf32>, %value : f32) -> tensor<64xf32> {
  %0 = linalg.fill(%value, %tensor) : f32, tensor<64xf32> -> tensor<64xf32>
  return %0 : tensor<64xf32>
}
// CHECK-LABEL: func @fill(
// CHECK-SAME:      %[[TNSR:.*]]: tensor<64xf32>, %[[VAL:.*]]: f32)
// CHECK-DAG:     %[[STEP:.*]] = arith.constant 8 : index
// CHECK-DAG:     %[[C64:.*]] = arith.constant 64 : index
// CHECK-DAG:     %[[C0:.*]] = arith.constant 0 : index
// CHECK:         gml_st.loop (%[[I:.*]]) = (%[[C0]]) to (%[[C64]])
// CHECK-SAME:        step (%[[STEP]])
// CHECK-SAME:        ins (%[[VAL_:.*]] = %[[VAL]]: f32)
// CHECK-SAME:        outs (%[[OUT_:.*]] = %[[TNSR]]: tensor<64xf32>)
// CHECK:           %[[SLICE_:.*]] = tensor.extract_slice %[[OUT_]][%[[I]]] [%[[STEP]]] [1]
// CHECK:           %[[FILLED_:.*]] = linalg.fill(%[[VAL_]], %[[SLICE_]])
// CHECK:           gml_st.yield %[[FILLED_:.*]]
