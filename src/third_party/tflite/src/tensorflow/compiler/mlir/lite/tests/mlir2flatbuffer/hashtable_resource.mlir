// RUN: flatbuffer_translate -mlir-to-tflite-flatbuffer %s -emit-custom-ops -emit-builtin-tflite-ops=false -o - | flatbuffer_to_string - | FileCheck %s

// CHECK: {
// CHECK:  version: 3,
// CHECK:  operator_codes: [ {
// CHECK:    deprecated_builtin_code: 32,
// CHECK:    custom_code: "HashTableV2",
// CHECK:    builtin_code: CUSTOM
// CHECK: } ],
// CHECK: subgraphs: [ {
// CHECK:   tensors: [ {
// CHECK:     shape: [  ],
// CHECK:     type: RESOURCE,
// CHECK:     buffer: 1,
// CHECK:     name: "tf.HashTableV2",
// CHECK:     quantization: {
// CHECK-EMPTY
// CHECK:     }
// CHECK:   } ],
// CHECK:   inputs: [  ],
// CHECK:   outputs: [ 0 ],
// CHECK:   operators: [ {
// CHECK:     inputs: [  ],
// CHECK:     outputs: [ 0 ],
// CHECK:     custom_options:
// CHECK:   name: "main"
// CHECK: } ],
// CHECK: description: "MLIR Converted.",
// CHECK: buffers: [ {
// CHECK-EMPTY
// CHECK: }, {
// CHECK-EMPTY
// CHECK: } ]
// CHECK: }

func.func @main() -> tensor<*x!tf_type.resource> {
  %0 = "tf.HashTableV2"() {container = "" , shared_name= "table", use_node_name_sharing = false, key_dtype = i32, value_dtype = i32 } : () -> tensor<*x!tf_type.resource>
  func.return %0 : tensor<*x!tf_type.resource>
}

