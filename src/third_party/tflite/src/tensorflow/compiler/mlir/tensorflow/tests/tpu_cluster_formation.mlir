// RUN: tf-opt %s -split-input-file -verify-diagnostics -tf-tpu-cluster-formation | FileCheck %s


// Test ops in cluster only have `_tpu_replicate` and `device` attributes
// removed when moved to a `tf_device.cluster`.
// CHECK-LABEL: func @cluster_ops_removed_attrs
func @cluster_ops_removed_attrs() {
  %0 = "tf.opA"() {_tpu_replicate = "replicate", device = "device", name = "name"} : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  return
}

// CHECK:      "tf.opA"
// CHECK-SAME: name = "name"
// CHECK-NOT:  _tpu_replicate = "replicate"
// CHECK-NOT:  device = "device"
// CHECK:      tf_device.return


// Test TPUReplicateMetadata ops `name` and `num_replicas` attributes are not
// copied over to `tf_device.cluster`.
// CHECK-LABEL: func @removed_metadata_attrs
func @removed_metadata_attrs() {
  %0 = "tf.opA"() {_tpu_replicate = "replicate"} : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", name = "name", num_replicas = 1, topology = "topology"} : () -> ()
  return
}

// CHECK-NOT:  name = "name"
// CHECK-NOT:  num_replicas = 1


// Test TPUReplicateMetadata op is removed when forming clusters.
// CHECK-LABEL: func @metadata_op_removed
func @metadata_op_removed() {
  %0 = "tf.opA"() {_tpu_replicate = "replicate"} : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  return
}

// CHECK-NOT:  "tf.TPUReplicateMetadata"


// Test ops in an island with the same `_tpu_replicate` attribute are merged
// under a `tf_device.cluster`.
// CHECK-LABEL: func @simple_island
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @simple_island(%arg0 : tensor<i1>) -> tensor<i1> {
  %0 = tf_executor.graph {
    %1:2 = tf_executor.island {
      "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
      %3 = "tf.opA"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      %4 = "tf.opB"() : () -> tensor<i1>
      %5 = "tf.opC"(%3) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      tf_executor.yield %5 : tensor<i1>
    }
    tf_executor.fetch %1#0 : tensor<i1>
  }
  return %0 : tensor<i1>
}

// CHECK:          "tf.opB"
// CHECK:          %[[CLUSTER:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:       %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:       %[[OP_C:[0-9]*]] = "tf.opC"(%[[OP_A]])
// CHECK-NEXT:       tf_device.return %[[OP_C]]
// CHECK-NEXT:     _tpu_replicate = "replicate"
// CHECK-SAME:     device = "device"
// CHECK-SAME:     topology = "topology"
// CHECK:          tf_executor.yield %[[CLUSTER]]


// Test ops in an island with the same `_tpu_replicate` attribute are merged
// under a `tf_device.cluster`, even when the associated TPUReplicateMetadata op
// is in a different island.
// CHECK-LABEL: func @simple_island_separate_metadata
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @simple_island_separate_metadata(%arg0 : tensor<i1>) -> tensor<i1> {
  %0 = tf_executor.graph {
    %1 = tf_executor.island {
      "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
    }
    %2:2 = tf_executor.island {
      %3 = "tf.opA"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      %4 = "tf.opB"() : () -> tensor<i1>
      %5 = "tf.opC"(%3) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      tf_executor.yield %5 : tensor<i1>
    }
    tf_executor.fetch %2#0 : tensor<i1>
  }
  return %0 : tensor<i1>
}

// CHECK:          "tf.opB"
// CHECK:          %[[CLUSTER:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:       %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:       %[[OP_C:[0-9]*]] = "tf.opC"(%[[OP_A]])
// CHECK-NEXT:       tf_device.return %[[OP_C]]
// CHECK-NEXT:     _tpu_replicate = "replicate"
// CHECK-SAME:     device = "device"
// CHECK-SAME:     topology = "topology"
// CHECK:          tf_executor.yield %[[CLUSTER]]


// Test ops in multiple islands with the same `_tpu_replicate` attribute are
// merged under `tf_device.cluster` ops only within their respective island.
// CHECK-LABEL: func @multiple_islands_separate_metadata
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @multiple_islands_separate_metadata(%arg0 : tensor<i1>) -> (tensor<i1>, tensor<i1>) {
  %0:2 = tf_executor.graph {
    %1 = tf_executor.island {
      "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
    }
    %2:2 = tf_executor.island {
      %3 = "tf.opA"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      %4 = "tf.opB"() : () -> tensor<i1>
      %5 = "tf.opC"(%3) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      tf_executor.yield %5 : tensor<i1>
    }
    %6:2 = tf_executor.island {
      %7 = "tf.opD"(%2#0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      %8 = "tf.opE"() : () -> tensor<i1>
      %9 = "tf.opF"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
      tf_executor.yield %9 : tensor<i1>
    }
    tf_executor.fetch %2#0, %6#0 : tensor<i1>, tensor<i1>
  }
  return %0#0, %0#1 : tensor<i1>, tensor<i1>
}

// CHECK:        %[[ISLAND_1:.*]], %[[ISLAND_1_control:.*]] = tf_executor.island {
// CHECK:          "tf.opB"
// CHECK:          %[[CLUSTER_0:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:       %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:       %[[OP_C:[0-9]*]] = "tf.opC"(%[[OP_A]])
// CHECK-NEXT:       tf_device.return %[[OP_C]]
// CHECK-NEXT:     _tpu_replicate = "replicate"
// CHECK-SAME:     device = "device"
// CHECK-SAME:     topology = "topology"
// CHECK:          tf_executor.yield %[[CLUSTER_0]]
// CHECK:        tf_executor.island {
// CHECK:          "tf.opE"
// CHECK:          %[[CLUSTER_1:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:       %[[OP_D:[0-9]*]] = "tf.opD"(%[[ISLAND_1]])
// CHECK-NEXT:       %[[OP_F:[0-9]*]] = "tf.opF"(%[[ARG_0]])
// CHECK-NEXT:       tf_device.return %[[OP_F]]
// CHECK-NEXT:     _tpu_replicate = "replicate"
// CHECK-SAME:     device = "device"
// CHECK-SAME:     topology = "topology"
// CHECK:          tf_executor.yield %[[CLUSTER_1]]


// Test ops in a function body with the same `_tpu_replicate` attribute are
// merged under a `tf_device.cluster` op.
// CHECK-LABEL: func @ops_in_func_body
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @ops_in_func_body(%arg0 : tensor<i1>) -> (tensor<i1>, tensor<i1>, tensor<i1>) {
  %0 = "tf.opA"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  %1 = "tf.opB"() : () -> tensor<i1>
  %2 = "tf.opC"(%0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  %3 = "tf.opD"(%2) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  %4 = "tf.opE"() : () -> tensor<i1>
  %5 = "tf.opF"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  return %2, %3, %5 : tensor<i1>, tensor<i1>, tensor<i1>
}

// CHECK:      "tf.opB"
// CHECK:      "tf.opE"
// CHECK:      %[[CLUSTER:[0-9]*]]:3 = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:   %[[OP_C:[0-9]*]] = "tf.opC"(%[[OP_A]])
// CHECK-NEXT:   %[[OP_D:[0-9]*]] = "tf.opD"(%[[OP_C]])
// CHECK-NEXT:   %[[OP_F:[0-9]*]] = "tf.opF"(%[[ARG_0]])
// CHECK-NEXT:   tf_device.return %[[OP_C]], %[[OP_D]], %[[OP_F]]
// CHECK-NEXT: _tpu_replicate = "replicate"
// CHECK-SAME: device = "device"
// CHECK-SAME: topology = "topology"
// CHECK:      return %[[CLUSTER]]#0, %[[CLUSTER]]#1, %[[CLUSTER]]#2


// Test a nested user of an op in a cluster has its operand be updated to
// `tf_device.cluster` result.
// CHECK-LABEL: func @nested_cluster_op_user
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @nested_cluster_op_user(%arg0 : tensor<i1>) -> (tensor<i1>) {
  %0 = "tf.opA"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  %1 = tf_executor.graph {
    tf_executor.fetch %0 : tensor<i1>
  }
  %2 = "tf.opB"(%0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  return %2 : tensor<i1>
}

// CHECK:      %[[CLUSTER:[0-9]*]]:2 = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:   %[[OP_B:[0-9]*]] = "tf.opB"(%[[OP_A]])
// CHECK-NEXT:   tf_device.return %[[OP_A]], %[[OP_B]]
// CHECK-NEXT: _tpu_replicate = "replicate"
// CHECK-SAME: device = "device"
// CHECK-SAME: topology = "topology"
// CHECK:      tf_executor.graph {
// CHECK-NEXT:   tf_executor.fetch %[[CLUSTER]]#0
// CHECK:      return %[[CLUSTER]]#1


// Test nested op of a cluster with an operand from an op of the same cluster
// retains its original operand.
// CHECK-LABEL: func @nested_cluster_op
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @nested_cluster_op(%arg0 : tensor<i1>) -> (tensor<i1>) {
  %0 = "tf.opA"(%arg0) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  %1 = "tf.opB"() ( {
    "tf.opC"(%0) : (tensor<i1>) -> tensor<i1>
  }) {_tpu_replicate = "replicate"} : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  return %1 : tensor<i1>
}

// CHECK:      %[[CLUSTER:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:   %[[OP_B:[0-9]*]] = "tf.opB"() ( {
// CHECK-NEXT:     "tf.opC"(%[[OP_A]])
// CHECK:        tf_device.return %[[OP_B]]
// CHECK-NEXT: _tpu_replicate = "replicate"
// CHECK-SAME: device = "device"
// CHECK-SAME: topology = "topology"
// CHECK:      return %[[CLUSTER]]


// Test multiple clusters interleaved.
// CHECK-LABEL: func @interleaved_clusters
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @interleaved_clusters(%arg0 : tensor<i1>) -> (tensor<i1>, tensor<i1>) {
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate_1", device = "device_1", num_replicas = 1, topology = "topology_1"} : () -> ()
  %0 = "tf.opA"(%arg0) {_tpu_replicate = "replicate_0"} : (tensor<i1>) -> tensor<i1>
  %1 = "tf.opB"(%arg0) {_tpu_replicate = "replicate_1"} : (tensor<i1>) -> tensor<i1>
  %2 = "tf.opC"(%0) {_tpu_replicate = "replicate_0"} : (tensor<i1>) -> tensor<i1>
  %3 = "tf.opD"(%1) {_tpu_replicate = "replicate_1"} : (tensor<i1>) -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate_0", device = "device_0", num_replicas = 1, topology = "topology_0"} : () -> ()
  return %2, %3 : tensor<i1>, tensor<i1>
}

// CHECK:      %[[CLUSTER_0:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:   %[[OP_C:[0-9]*]] = "tf.opC"(%[[OP_A]])
// CHECK-NEXT:   tf_device.return %[[OP_C]]
// CHECK-NEXT: _tpu_replicate = "replicate_0"
// CHECK-SAME: device = "device_0"
// CHECK-SAME: topology = "topology_0"
// CHECK:      %[[CLUSTER_1:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_B:[0-9]*]] = "tf.opB"(%[[ARG_0]])
// CHECK-NEXT:   %[[OP_D:[0-9]*]] = "tf.opD"(%[[OP_B]])
// CHECK-NEXT:   tf_device.return %[[OP_D]]
// CHECK-NEXT: _tpu_replicate = "replicate_1"
// CHECK-SAME: device = "device_1"
// CHECK-SAME: topology = "topology_1"
// CHECK:      return %[[CLUSTER_0]], %[[CLUSTER_1]]


// Test operands and results of ops of a cluster that are interleaved between
// other ops of the same cluster are moved before and after the cluster
// properly.
// CHECK-LABEL: func @interleaved_cluster_operands_results
func @interleaved_cluster_operands_results() {
  %0 = "tf.opA"() {_tpu_replicate = "replicate"} : () -> tensor<i1>
  %1 = "tf.opB"(%0) : (tensor<i1>) -> tensor<i1>
  %2 = "tf.opC"() : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  %3 = "tf.opD"(%1) : (tensor<i1>) -> tensor<i1>
  %4 = "tf.opE"(%2) : (tensor<i1>) -> tensor<i1>
  %5 = "tf.opF"(%4) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  return
}

// CHECK:      %[[OP_C:[0-9]*]] = "tf.opC"
// CHECK:      %[[OP_E:[0-9]*]] = "tf.opE"(%[[OP_C]])
// CHECK:      %[[CLUSTER:[0-9]*]] = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_A:[0-9]*]] = "tf.opA"
// CHECK-NEXT:   "tf.opF"(%[[OP_E]])
// CHECK-NEXT:   tf_device.return %[[OP_A]]
// CHECK-NEXT: _tpu_replicate = "replicate"
// CHECK-SAME: device = "device"
// CHECK-SAME: topology = "topology"
// CHECK:      %[[OP_B:[0-9]*]] = "tf.opB"(%[[CLUSTER]])
// CHECK:      "tf.opD"(%[[OP_B]])


// Test one replica cluster results in removing of TPUReplicatedInput and
// TPUReplicatedOutput nodes and operands are forwarded to results.
// CHECK-LABEL: func @one_replica
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>)
func @one_replica(%arg0: tensor<i1>) -> tensor<i1> {
  %ri = "tf.TPUReplicatedInput"(%arg0) : (tensor<i1>) -> tensor<i1>
  %0 = "tf.opA"(%ri) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  %1 = "tf.opB"(%0) : (tensor<i1>) -> tensor<i1>
  %2 = "tf.opC"() : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 1, topology = "topology"} : () -> ()
  %3 = "tf.opD"(%1) : (tensor<i1>) -> tensor<i1>
  %4 = "tf.opE"(%2) : (tensor<i1>) -> tensor<i1>
  %5 = "tf.opF"(%4) {_tpu_replicate = "replicate"} : (tensor<i1>) -> tensor<i1>
  %ro = "tf.TPUReplicatedOutput"(%5) : (tensor<i1>) -> tensor<i1>
  return %ro : tensor<i1>
}

// CHECK:      %[[OP_C:[0-9]*]] = "tf.opC"
// CHECK:      %[[OP_E:[0-9]*]] = "tf.opE"(%[[OP_C]])
// CHECK:      %[[CLUSTER:[0-9]*]]:2 = "tf_device.cluster"() ( {
// CHECK-NEXT:   %[[OP_A:[0-9]*]] = "tf.opA"(%[[ARG_0]])
// CHECK-NEXT:   %[[OP_F:[0-9]*]] = "tf.opF"(%[[OP_E]])
// CHECK-NEXT:   tf_device.return %[[OP_A]], %[[OP_F]]
// CHECK-NEXT: _tpu_replicate = "replicate"
// CHECK-SAME: device = "device"
// CHECK-SAME: topology = "topology"
// CHECK:      %[[OP_B:[0-9]*]] = "tf.opB"(%[[CLUSTER]]#0)
// CHECK:      "tf.opD"(%[[OP_B]])
// CHECK:      return %[[CLUSTER]]#1
// CHECK-NOT:  "tf.TPUReplicatedInput"
// CHECK-NOT:  "tf.TPUReplicatedOutput"


// Test replication with replicated operands and replicated results. The cluster
// will be wrapped in a `tf_device.cluster` first and then by a replicate.
// TPUReplicatedInput and TPUReplicatedOutput nodes will be replaced by the
// replicate operands and results.
// CHECK-LABEL: func @replication
// CHECK-SAME: (%[[ARG_0:[a-z0-9]*]]: tensor<i1>, %[[ARG_1:[a-z0-9]*]]: tensor<i32>, %[[ARG_2:[a-z0-9]*]]: tensor<f32>)
func @replication(%arg0: tensor<i1>, %arg1: tensor<i32>, %arg2: tensor<f32>) -> (tensor<i32>, tensor<f32>) {
  %0 = "tf.opA"() : () -> tensor<i1>
  %ri_0 = "tf.TPUReplicatedInput"(%arg0, %0) : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %1 = "tf.opB"() : () -> tensor<i32>
  %ri_1 = "tf.TPUReplicatedInput"(%1, %arg1) : (tensor<i32>, tensor<i32>) -> tensor<i32>
  %2 = "tf.opC"() : () -> tensor<f32>
  %3 = "tf.opD"(%ri_0, %ri_1, %arg2, %2) {_tpu_replicate = "replicate"} : (tensor<i1>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<i32>
  %ro_0:2 = "tf.TPUReplicatedOutput"(%3) : (tensor<i32>) -> (tensor<i32>, tensor<i32>)
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  %7 = "tf.opE"(%3, %ri_0, %ri_1, %arg2, %2) {_tpu_replicate = "replicate"} : (tensor<i32>, tensor<i1>, tensor<i32>, tensor<f32>, tensor<f32>) -> tensor<f32>
  %ro_1:2 = "tf.TPUReplicatedOutput"(%7) : (tensor<f32>) -> (tensor<f32>, tensor<f32>)
  return %ro_0#0, %ro_1#1 : tensor<i32>, tensor<f32>
}

// CHECK:      %[[OP_A:[0-9]*]] = "tf.opA"
// CHECK:      %[[OP_B:[0-9]*]] = "tf.opB"
// CHECK:      %[[OP_C:[0-9]*]] = "tf.opC"
// CHECK:      %[[REPLICATE:[0-9]*]]:4 = tf_device.replicate
// CHECK-DAG:  [%[[ARG_0]], %[[OP_A]]] as %[[RI_0:[a-z0-9]*]]: tensor<i1>
// CHECK-DAG:  [%[[OP_B]], %[[ARG_1]]] as %[[RI_1:[a-z0-9]*]]: tensor<i32>
// CHECK-SAME: n = 2 : i32
// CHECK-NEXT:   %[[CLUSTER:[0-9]*]]:2 = "tf_device.cluster"() ( {
// CHECK:          %[[OP_D:[0-9]*]] = "tf.opD"(%[[RI_0]], %[[RI_1]], %[[ARG_2]], %[[OP_C]])
// CHECK:          %[[OP_E:[0-9]*]] = "tf.opE"(%[[OP_D]], %[[RI_0]], %[[RI_1]], %[[ARG_2]], %[[OP_C]])
// CHECK:          tf_device.return %[[OP_D]], %[[OP_E]]
// CHECK-NEXT:   _tpu_replicate = "replicate"
// CHECK-SAME:   device = "device"
// CHECK-SAME:   topology = "topology"
// CHECK:        tf_device.return %[[CLUSTER]]#0, %[[CLUSTER]]#1
// CHECK:      return %[[REPLICATE]]#0, %[[REPLICATE]]#3


// Test TPUReplicatedInput ops are sorted by their `index` attribute.
// Non-negative `index` should precede `index` of -1, and ordering of ops with
// `index` of -1 does not matter.
// CHECK-LABEL: func @sort_replicated_input
// CHECK-SAME: (%[[ARG_0:.*]]: tensor<i1>, %[[ARG_1:.*]]: tensor<i1>, %[[ARG_2:.*]]: tensor<i1>, %[[ARG_3:.*]]: tensor<i1>, %[[ARG_4:.*]]: tensor<i1>, %[[ARG_5:.*]]: tensor<i1>)
func @sort_replicated_input(%arg0: tensor<i1>, %arg1: tensor<i1>, %arg2: tensor<i1>, %arg3: tensor<i1>, %arg4: tensor<i1>, %arg5: tensor<i1>) {
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg0) {index = -1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %1 = "tf.TPUReplicatedInput"(%arg1, %arg1) {index = 2 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %2 = "tf.TPUReplicatedInput"(%arg2, %arg2) {index = 0 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %3 = "tf.TPUReplicatedInput"(%arg3, %arg3) {index = -1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %4 = "tf.TPUReplicatedInput"(%arg4, %arg4) {index = 1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %5 = "tf.TPUReplicatedInput"(%arg5, %arg5) {index = -1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  "tf.opA"(%0, %1, %2, %3, %4, %5) {_tpu_replicate = "replicate", device = "device"} : (tensor<i1>, tensor<i1>, tensor<i1>, tensor<i1>, tensor<i1>, tensor<i1>) -> ()
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}

// CHECK:      tf_device.replicate
// CHECK-SAME: [%[[ARG_2]], %[[ARG_2]]] as %{{[a-z0-9]*}}
// CHECK-SAME: [%[[ARG_4]], %[[ARG_4]]] as %{{[a-z0-9]*}}
// CHECK-SAME: [%[[ARG_1]], %[[ARG_1]]] as %{{[a-z0-9]*}}
// CHECK-DAG:  [%[[ARG_0]], %[[ARG_0]]] as %{{[a-z0-9]*}}
// CHECK-DAG:  [%[[ARG_3]], %[[ARG_3]]] as %{{[a-z0-9]*}}
// CHECK-DAG:  [%[[ARG_5]], %[[ARG_5]]] as %{{[a-z0-9]*}}


// -----


// Test cluster with missing `num_replicas` attribute.
func @missing_num_replicas() {
  %0 = "tf.opA"() {_tpu_replicate = "replicate", device = "device", name = "name"} : () -> tensor<i1>
  // expected-error@+1 {{'tf.TPUReplicateMetadata' op requires attribute 'num_replicas'}}
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", topology = "topology"} : () -> ()
  return
}


// -----


// Test cluster with bad `num_replicas` attribute.
func @bad_num_replicas() {
  // expected-error@+1 {{requires 'num_replicas' int attribute to be at least 1}}
  %0 = "tf.opA"() {_tpu_replicate = "replicate", device = "device", name = "name"} : () -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 0, topology = "topology"} : () -> ()
  return
}

// -----

// Test that functions without TPUReplicateMetadata op are skipped without
// error
// CHECK-LABEL: func @missing_metadata_op
func @missing_metadata_op() {
  // expected-warning@+1 {{TPUReplicateMetadata for associated '_tpu_replicate' attribute 'replicate' is missing}}
  %0 = "tf.opA"() {_tpu_replicate = "replicate"} : () -> tensor<i1>
  return
}

// -----


// Test cluster with TPUReplicatedInput where the number of operands does not
// match associated `num_replicas` attribute.
func @mismatched_replicated_input(%arg0: tensor<i1>) {
  // expected-error@+1 {{'tf.TPUReplicatedInput' op requires 2 operands}}
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg0, %arg0) : (tensor<i1>, tensor<i1>, tensor<i1>) -> tensor<i1>
  %1 = "tf.opA"(%0) {_tpu_replicate = "replicate", device = "device", name = "name"} : (tensor<i1>) -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}


// -----


// Test cluster with TPUReplicatedOutput where the number of results does not
// match associated `num_replicas` attribute.
func @mismatched_replicated_output() {
  %0 = "tf.opA"() {_tpu_replicate = "replicate", device = "device", name = "name"} : () -> tensor<i1>
  // expected-error@+1 {{'tf.TPUReplicatedOutput' op requires 2 results}}
  %1:3 = "tf.TPUReplicatedOutput"(%0) : (tensor<i1>) -> (tensor<i1>, tensor<i1>, tensor<i1>)
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}


// -----


// Test cluster that should be replicated where its outputs do not lead to a
// TPUReplicatedOutput.
func @missing_replicated_output() {
  // expected-error@+1 {{requires output of tf_device.cluster to lead to a 'tf.TPUReplicatedOutput' op}}
  %0 = "tf.opA"() {_tpu_replicate = "replicate", device = "device", name = "name"} : () -> tensor<i1>
  %1 = "tf.opB"(%0) : (tensor<i1>) -> tensor<i1>
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}


// -----


// Test unused TPUReplicatedInput that has more than one operand.
func @leftover_replicated_input(%arg0: tensor<i1>) {
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg0) : (tensor<i1>, tensor<i1>) -> tensor<i1>
  return
}


// -----


// Test unused TPUReplicatedOutput that has more than one result.
func @leftover_replicated_output(%arg0: tensor<i1>) {
  %0:2 = "tf.TPUReplicatedOutput"(%arg0) : (tensor<i1>) -> (tensor<i1>, tensor<i1>)
  return
}


// -----


// Test bad TPUReplicatedInput positive `index` attribute.
func @bad_positive_index_input(%arg0: tensor<i1>) {
  // expected-error@+1 {{'tf.TPUReplicatedInput' index is not in range [-1, 1), got 1}}
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg0) {index = 1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  "tf.opA"(%0) {_tpu_replicate = "replicate", device = "device", name = "name"} : (tensor<i1>) -> ()
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}


// -----


// Test bad TPUReplicatedInput negative `index` attribute.
func @bad_negative_index_input(%arg0: tensor<i1>) {
  // expected-error@+1 {{'tf.TPUReplicatedInput' index is not in range [-1, 1), got -2}}
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg0) {index = -2 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  "tf.opA"(%0) {_tpu_replicate = "replicate", device = "device", name = "name"} : (tensor<i1>) -> ()
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}


// -----


// Test TPUReplicatedInput with conflicting `index` attribute. This will result
// in gaps in the TPUReplicatedInput ordering.
func @input_index_gaps(%arg0: tensor<i1>) {
  // expected-error@+1 {{failed to sort 'tf.TPUReplicatedInput' ops, gap(s) found in indices}}
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg0) {index = 1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  %1 = "tf.TPUReplicatedInput"(%arg0, %arg0) {index = 1 : i64} : (tensor<i1>, tensor<i1>) -> tensor<i1>
  "tf.opA"(%0, %1) {_tpu_replicate = "replicate", device = "device", name = "name"} : (tensor<i1>, tensor<i1>) -> ()
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}


// -----


// Test that the `is_mirrored_variable` attribute is preserved in the
// tf_device.replicate op.
// CHECK-LABEL: func @mirrored_variables
// CHECK-SAME: (%[[ARG_0:.*]]: tensor<!tf.resource<tensor<32xf32>>>, %[[ARG_1:.*]]: tensor<!tf.resource<tensor<32xf32>>>, %[[ARG_2:.*]]: tensor<!tf.resource<tensor<32xf32>>>, %[[ARG_3:.*]]: tensor<!tf.resource<tensor<32xf32>>>)
func @mirrored_variables(%arg0: tensor<!tf.resource<tensor<32xf32>>>, %arg1: tensor<!tf.resource<tensor<32xf32>>>, %arg2: tensor<!tf.resource<tensor<32xf32>>>, %arg3: tensor<!tf.resource<tensor<32xf32>>>) {
  %0 = "tf.TPUReplicatedInput"(%arg0, %arg1) {index = 0 : i64} : (tensor<!tf.resource<tensor<32xf32>>>, tensor<!tf.resource<tensor<32xf32>>>) -> tensor<!tf.resource<tensor<32xf32>>>
  %1 = "tf.TPUReplicatedInput"(%arg2, %arg3) {index = 1 : i64, is_mirrored_variable = true} : (tensor<!tf.resource<tensor<32xf32>>>, tensor<!tf.resource<tensor<32xf32>>>) -> tensor<!tf.resource<tensor<32xf32>>>
  "tf.opA"(%0, %1) {_tpu_replicate = "replicate", device = "device"} : (tensor<!tf.resource<tensor<32xf32>>>, tensor<!tf.resource<tensor<32xf32>>>) -> ()
  "tf.TPUReplicateMetadata"() {_tpu_replicate = "replicate", device = "device", num_replicas = 2, topology = "topology"} : () -> ()
  return
}

// CHECK:      tf_device.replicate
// CHECK-SAME: [%[[ARG_0]], %[[ARG_1]]] as %{{[a-z0-9]*}}
// CHECK-SAME: _mirrored_variable_indices = [1]
