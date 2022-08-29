// RUN: tf-opt -tf-shape-inference -inline='default-pipeline=''' %s | FileCheck %s --dump-input=always
// RUN: tf-opt -tf-standard-pipeline=enable-inliner %s | FileCheck %s --dump-input=always

// Tests function with argument has no resource subtype but caller operand has a
// resource subtype, and after shape inference, function argument is refined and
// no `tf.Cast` ops are generated.

module attributes {tf.versions = {bad_consumers = [], min_consumer = 12 : i32, producer = 384 : i32}} {
  // CHECK-LABEL: func @main
  func.func @main() -> tensor<f32> {
    // CHECK-NEXT: %[[VAR:.*]] = "tf.VarHandleOp"
    // CHECK-NEXT: %[[READ_VAR:.*]] = "tf.ReadVariableOp"(%[[VAR]])
    // CHECK-NEXT: return %[[READ_VAR]]
    // CHECK-NOT: "tf.Cast"
    %0 = "tf.VarHandleOp"() {_class = ["loc:@Variable"], allowed_devices = [], container = "", device = "", shared_name = "Variable"} : () -> tensor<!tf_type.resource<tensor<f32>>>
    %1 = "tf.StatefulPartitionedCall"(%0) {config = "", config_proto = "", executor_type = "", f = @callee} : (tensor<!tf_type.resource<tensor<f32>>>) -> tensor<f32>
    func.return %1 : tensor<f32>
  }

  // CHECK-NOT: func private @callee
  func.func private @callee(%arg0: tensor<!tf_type.resource>) -> tensor<*xf32> attributes {tf.signature.is_stateful} {
    %0 = "tf.ReadVariableOp"(%arg0) {device = ""} : (tensor<!tf_type.resource>) -> tensor<*xf32>
    func.return %0 : tensor<*xf32>
  }
}
