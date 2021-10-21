// RUN: tf-tfrt-opt %s -allow-unregistered-dialect -split-input-file -tf-cpurt-peel-tiled-loops \
// RUN:   | FileCheck %s
// RUN: tf-tfrt-opt %s -allow-unregistered-dialect -split-input-file -tf-cpurt-peel-tiled-loops \
// RUN:   | FileCheck %s --check-prefixes=COUNT

#map0 = affine_map<(d0) -> (8, -d0 + 102401)>
#map1 = affine_map<(d0)[s0] -> (d0 + s0)>
builtin.module attributes {tf.versions = {producer = 0 : i32}}  {
  builtin.func @tanh_1d(%arg0: memref<102401xf32>) -> memref<102401xf32> {
    %c102401 = constant 102401 : index
    %c8 = constant 8 : index
    %cst = constant 0.000000e+00 : f32
    %c0 = constant 0 : index
    %0 = memref.alloc() : memref<102401xf32>
    linalg.tiled_loop (%arg1) = (%c0) to (%c102401) step (%c8)
        ins (%arg2 = %arg0: memref<102401xf32>)
        outs (%arg3 = %0: memref<102401xf32>)
    {
      %1 = affine.min #map0(%arg1)
      %2 = memref.subview %arg2[%arg1] [%1] [1]
          : memref<102401xf32> to memref<?xf32, #map1>
      %3 = memref.subview %arg3[%arg1] [%1] [1]
          : memref<102401xf32> to memref<?xf32, #map1>
      %4 = vector.transfer_read %2[%c0], %cst
          : memref<?xf32, #map1>, vector<8xf32>
      %5 = math.tanh %4 : vector<8xf32>
      vector.transfer_write %5, %3[%c0] : vector<8xf32>, memref<?xf32, #map1>
      linalg.copy(%3, %3) : memref<?xf32, #map1>, memref<?xf32, #map1>
      linalg.yield
    }
    return %0 : memref<102401xf32>
  }
}

// CHECK: #[[MAP0:.*]] = affine_map<(d0)[s0] -> (d0 + s0)>
// CHECK: #[[MAP1:.*]] = affine_map<(d0) -> (-d0 + 102401)>
// CHECK:   func @tanh_1d(
// CHECK-SAME:                  %[[IN:.*]]: memref<102401xf32>)
// CHECK-SAME:                  -> memref<102401xf32> {
// CHECK-DAG:           %[[C102401:.*]] = constant 102401 : index
// CHECK-DAG:           %[[CF0:.*]] = constant 0.000000e+00 : f32
// CHECK-DAG:           %[[C0:.*]] = constant 0 : index
// CHECK-DAG:           %[[C102400:.*]] = constant 102400 : index
// CHECK-DAG:           %[[C8:.*]] = constant 8 : index
// CHECK:           %[[OUT:.*]] = memref.alloc() : memref<102401xf32>
// CHECK:           linalg.tiled_loop (%[[LOOP_IDX:.*]]) =
// CHECK-SAME:          (%[[C0]]) to (%[[C102400]]) step (%[[C8]])
// CHECK-SAME:          ins (%[[LOOP_IN:.*]] = %[[IN]]: memref<102401xf32>)
// CHECK-SAME:          outs (%[[LOOP_OUT:.*]] = %[[OUT]]: memref<102401xf32>)
// CHECK-SAME:      {
// CHECK:             %[[LOOP_VIEW_IN:.*]] = memref.subview
// CHECK-SAME:            %[[LOOP_IN]]{{\[}}%[[LOOP_IDX]]] {{\[}}%[[C8]]] [1]
// CHECK-SAME:            : memref<102401xf32> to memref<?xf32, #[[MAP0]]>
// CHECK:             %[[LOOP_VIEW_OUT:.*]] = memref.subview
// CHECK-SAME:            %[[LOOP_OUT]]{{\[}}%[[LOOP_IDX]]]
// CHECK-SAME:            {{\[}}%[[C8]]] [1]
// CHECK-SAME:            : memref<102401xf32> to memref<?xf32, #[[MAP0]]>
// CHECK:             %[[LOOP_IN_VEC:.*]] = vector.transfer_read
// CHECK-SAME:            %[[LOOP_VIEW_IN]]{{\[}}%[[C0]]], %[[CF0]]
// CHECK-SAME:            : memref<?xf32, #[[MAP0]]>, vector<8xf32>
// CHECK:             %[[LOOP_OUT_VEC:.*]] = math.tanh %[[LOOP_IN_VEC]]
// CHECK-SAME:            : vector<8xf32>
// CHECK:             vector.transfer_write %[[LOOP_OUT_VEC]],
// CHECK-SAME:            %[[LOOP_VIEW_OUT]]{{\[}}%[[C0]]]
// CHECK-SAME:            : vector<8xf32>, memref<?xf32, #[[MAP0]]>
// CHECK:             linalg.yield
// CHECK:           }
// CHECK:           linalg.tiled_loop (%[[PEELED_IDX:.*]]) =
// CHECK-SAME:          (%[[C102400]]) to (%[[C102401]]) step (%[[C8]])
// CHECK-SAME:          ins (%[[PEELED_IN:.*]] = %[[IN]]: memref<102401xf32>)
// CHECK-SAME:          outs (%[[PEELED_OUT:.*]] = %[[OUT]]: memref<102401xf32>)
// CHECK-SAME:      {
// CHECK:             %[[PEELED_SIZE:.*]] = affine.apply #[[MAP1]]
// CHECK-SAME:            (%[[PEELED_IDX]])
// CHECK:             %[[PEELED_VIEW_IN:.*]] = memref.subview
// CHECK-SAME:            %[[PEELED_IN]]{{\[}}%[[PEELED_IDX]]]
// CHECK-SAME:            {{\[}}%[[PEELED_SIZE]]] [1]
// CHECK-SAME:            : memref<102401xf32> to memref<?xf32, #[[MAP0]]>
// CHECK:             %[[PEELED_VIEW_OUT:.*]] = memref.subview
// CHECK-SAME:            %[[PEELED_OUT]]{{\[}}%[[PEELED_IDX]]]
// CHECK-SAME:            {{\[}}%[[PEELED_SIZE]]] [1]
// CHECK-SAME:            : memref<102401xf32> to memref<?xf32, #[[MAP0]]>
// CHECK:             %[[PEELED_IN_VEC:.*]] = vector.transfer_read
// CHECK-SAME:            %[[PEELED_VIEW_IN]]{{\[}}%[[C0]]], %[[CF0]]
// CHECK-SAME:            : memref<?xf32, #[[MAP0]]>, vector<8xf32>
// CHECK:             %[[PEELED_RESULT:.*]] = math.tanh %[[PEELED_IN_VEC]]
// CHECK-SAME:            : vector<8xf32>
// CHECK:             vector.transfer_write %[[PEELED_RESULT]],
// CHECK-SAME:            %[[PEELED_VIEW_OUT]]{{\[}}%[[C0]]]
// CHECK-SAME:            vector<8xf32>, memref<?xf32, #[[MAP0]]>
// CHECK:             linalg.yield
// CHECK:           }
// CHECK:           return %[[OUT]] : memref<102401xf32>
// CHECK:         }

// -----

builtin.module attributes {tf.versions = {producer = 0 : i32}}  {
  builtin.func @tanh_3d(%d0: index, %d1: index, %d2: index) {
    %c0 = constant 0 : index
    %c1 = constant 1 : index
    %c8 = constant 8 : index
    linalg.tiled_loop (%arg1 ,%arg2, %arg3) = (%c0, %c0, %c0) to (%d0, %d1, %d2)
        step (%c8, %c1, %c8)
        ins ()
        outs ()
    {
      "prevent.dce"() : () -> ()
      linalg.yield
    }
    return
  }
}

// CHECK-LABEL:func @tanh_3d(
// CHECK-SAME:                  %[[D0:[a-z0-9]+]]: index,
// CHECK-SAME:                  %[[D1:[a-z0-9]+]]: index,
// CHECK-SAME:                  %[[D2:[a-z0-9]+]]: index) {
// CHECK-DAG:       %[[C0:.*]] = constant 0 : index
// CHECK-DAG:       %[[C1:.*]] = constant 1 : index
// CHECK-DAG:       %[[C8:.*]] = constant 8 : index

// CHECK-DAG:       %[[SPLIT0:.*]] = affine.apply{{.*}}%[[D0]]
// CHECK-DAG:       %[[SPLIT2:.*]] = affine.apply{{.*}}%[[D2]]

// CHECK-DAG:       linalg.tiled_loop{{.*}}(%[[C0]], %[[C0]], %[[C0]]) to (%[[SPLIT0]], %arg1, %[[SPLIT2]]) step  (%[[C8]], %[[C1]], %[[C8]])
// CHECK-DAG:       linalg.tiled_loop{{.*}}(%[[SPLIT0]], %[[C0]], %[[C0]]) to (%arg0, %arg1, %[[SPLIT2]]) step  (%[[C8]], %[[C1]], %[[C8]])
// CHECK-DAG:       linalg.tiled_loop{{.*}}(%[[C0]], %[[C0]], %[[SPLIT2]]) to (%arg0, %arg1, %arg2) step  (%[[C8]], %[[C1]], %[[C8]])

// COUNT-LABEL:func @tanh_3d(
// COUNT-COUNT-3:    linalg.tiled_loop
// COUNT-NOT:        linalg.tiled_loop
