// RUN: tf-opt -tf-rewrite-tpu-embedding-ops %s | FileCheck %s

// CHECK-LABEL: func @recv_tpu_embedding_activations
func @recv_tpu_embedding_activations() -> (tensor<512x256xf32>) {
  // CHECK: %[[DATA:.*]] = "tf._RecvTPUEmbeddingDeduplicationData"() {config = {{.*}}} : () -> tensor<!tf.variant>
  // CHECK: %[[RESULT:.*]] = "tf._RecvTPUEmbeddingActivations"(%[[DATA]]) {config = {{.*}}} : (tensor<!tf.variant>) -> tensor<512x256xf32>
  // CHECK: return %[[RESULT]]
  // CHECK-NOT: tf.RecvTPUEmbeddingActivations
  // CHECK-NOT: tf.SendTPUEmbeddingGradients

  %0 = "tf.RecvTPUEmbeddingActivations"() {config = "\0A%\0A\0Dwatches_table\10\F5\03\18\80\02 \01*\0C\1A\00j\05\0D\00\00\80?\88\01\01\10\02\18\80\04 \01(\02"} : () -> tensor<512x256xf32>
  return %0 : tensor<512x256xf32>
}

// CHECK-LABEL: func @send_tpu_embedding_gradients
func @send_tpu_embedding_gradients(%arg0: tensor<512x256xf32>) -> () {
  // CHECK: %[[DATA:.*]] = "tf._RecvTPUEmbeddingDeduplicationData"() {config = {{.*}}} : () -> tensor<!tf.variant>
  // CHECK: "tf._SendTPUEmbeddingGradients"(%arg0, %[[DATA]]) {config = {{.*}}, operand_segment_sizes = dense<[1, 0, 1]> : vector<3xi32>} : (tensor<512x256xf32>, tensor<!tf.variant>) -> ()
  // CHECK-NOT: tf.SendTPUEmbeddingGradients
  // CHECK-NOT: tf.RecvTPUEmbeddingActivations

  "tf.SendTPUEmbeddingGradients"(%arg0) {config = "\0A%\0A\0Dwatches_table\10\F5\03\18\80\02 \01*\0C\1A\00j\05\0D\00\00\80?\88\01\01\10\02\18\80\04 \01(\02", operand_segment_sizes = dense<[1, 0]> : vector<2xi32>} : (tensor<512x256xf32>) -> ()
  return
}

// CHECK-LABEL: func @recv_send_ops
func @recv_send_ops() -> () {
  // CHECK: %[[DATA:.*]] = "tf._RecvTPUEmbeddingDeduplicationData"()
  // CHECK: %[[ACTIVATIONS:.*]] = "tf._RecvTPUEmbeddingActivations"(%[[DATA]])
  // CHECK: "tf._SendTPUEmbeddingGradients"(%[[ACTIVATIONS]], %[[DATA]])

  %0 = "tf.RecvTPUEmbeddingActivations"() {config = "\0A%\0A\0Dwatches_table\10\F5\03\18\80\02 \01*\0C\1A\00j\05\0D\00\00\80?\88\01\01\10\02\18\80\04 \01(\02"} : () -> tensor<512x256xf32>
  "tf.SendTPUEmbeddingGradients"(%0) {config = "\0A%\0A\0Dwatches_table\10\F5\03\18\80\02 \01*\0C\1A\00j\05\0D\00\00\80?\88\01\01\10\02\18\80\04 \01(\02", operand_segment_sizes = dense<[1, 0]> : vector<2xi32>} : (tensor<512x256xf32>) -> ()
  return
}

// CHECK-LABEL: func @no_embedding_ops
func @no_embedding_ops(%arg0: tensor<2x2xf32>) -> (tensor<2x2xf32>) {
  // CHECK: tf.Add
  %0 = "tf.Add"(%arg0, %arg0) : (tensor<2x2xf32>, tensor<2x2xf32>) -> tensor<2x2xf32>
  return %0 : tensor<2x2xf32>
}
