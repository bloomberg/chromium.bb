// RUN: flatbuffer_translate -mlir-to-tflite-flatbuffer %s -o - | flatbuffer_to_string - | FileCheck %s

func @main(tensor<4xf32>) -> tensor<4xf32> {
^bb0(%arg0: tensor<4xf32>):
  // CHECK:      {
  // CHECK-NEXT:   version: 3,
  // CHECK-NEXT:   operator_codes: [ {
  // CHECK-NEXT:     builtin_code: SQUARED_DIFFERENCE,
  // CHECK-NEXT:     version: 1
  // CHECK-NEXT:   }, {
  // CHECK-NEXT:     builtin_code: MUL,
  // CHECK-NEXT:     version: 1
  // CHECK-NEXT:   }, {
  // CHECK-NEXT:     builtin_code: DIV,
  // CHECK-NEXT:     version: 1
  // CHECK-NEXT:   }, {
  // CHECK-NEXT:     builtin_code: EXP,
  // CHECK-NEXT:     version: 1
  // CHECK-NEXT:   }, {
  // CHECK-NEXT:     builtin_code: NEG,
  // CHECK-NEXT:     version: 1
  // CHECK-NEXT:   } ],
  // CHECK-NEXT:   subgraphs: [ {
  // CHECK-NEXT:     tensors: [ {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 1,
  // CHECK-NEXT:       name: "arg0",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 2,
  // CHECK-NEXT:       name: "Const",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 3,
  // CHECK-NEXT:       name: "squared_difference",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 4,
  // CHECK-NEXT:       name: "mul",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 5,
  // CHECK-NEXT:       name: "div",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 6,
  // CHECK-NEXT:       name: "exp",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       shape: [ 4 ],
  // CHECK-NEXT:       buffer: 7,
  // CHECK-NEXT:       name: "neg",
  // CHECK-NEXT:       quantization: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     } ],
  // CHECK-NEXT:     inputs: [ 0 ],
  // CHECK-NEXT:     outputs: [ 6 ],
  // CHECK-NEXT:     operators: [ {
  // CHECK-NEXT:       inputs: [ 0, 1 ],
  // CHECK-NEXT:       outputs: [ 2 ]
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       opcode_index: 1,
  // CHECK-NEXT:       inputs: [ 0, 2 ],
  // CHECK-NEXT:       outputs: [ 3 ],
  // CHECK-NEXT:       builtin_options_type: MulOptions,
  // CHECK-NEXT:       builtin_options: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       opcode_index: 2,
  // CHECK-NEXT:       inputs: [ 3, 2 ],
  // CHECK-NEXT:       outputs: [ 4 ],
  // CHECK-NEXT:       builtin_options_type: DivOptions,
  // CHECK-NEXT:       builtin_options: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       opcode_index: 3,
  // CHECK-NEXT:       inputs: [ 4 ],
  // CHECK-NEXT:       outputs: [ 5 ],
  // CHECK-NEXT:       builtin_options_type: ExpOptions,
  // CHECK-NEXT:       builtin_options: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     }, {
  // CHECK-NEXT:       opcode_index: 4,
  // CHECK-NEXT:       inputs: [ 5 ],
  // CHECK-NEXT:       outputs: [ 6 ],
  // CHECK-NEXT:       builtin_options_type: NegOptions,
  // CHECK-NEXT:       builtin_options: {
  // CHECK-EMPTY:
  // CHECK-NEXT:       }
  // CHECK-NEXT:     } ]
  // CHECK-NEXT:    name: "main"
  // CHECK-NEXT:   } ],
  // CHECK-NEXT:   description: "MLIR Converted.",
  // CHECK-NEXT:   buffers: [ {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-NEXT:     data: [ 0, 0, 128, 63, 0, 0, 128, 63, 0, 0, 128, 63, 0, 0, 128, 63 ]
  // CHECK-NEXT:   }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-EMPTY:
  // CHECK-NEXT:   }, {
  // CHECK-NEXT:     data: [ 49, 46, 49, 51, 46, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]
  // CHECK-NEXT:   } ],
  // CHECK-NEXT:   metadata: [ {
  // CHECK-NEXT:   name: "min_runtime_version",
  // CHECK-NEXT:   buffer: 8
  // CHECK-NEXT:   } ]
  // CHECK-NEXT: }

  %0 = "tfl.pseudo_const" () {value = dense<1.0> : tensor<4xf32>} : () -> tensor<4xf32> loc("Const")
  %1 = "tfl.squared_difference"(%arg0, %0) {fused_activation_function = "NONE"} : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32> loc("squared_difference")
  %2 = "tfl.mul"(%arg0, %1) {fused_activation_function = "NONE"} : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32> loc("mul")
  %3 = "tfl.div"(%2, %1) {fused_activation_function = "NONE"} : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32> loc("div")
  %4 = "tfl.exp"(%3) : (tensor<4xf32>) -> tensor<4xf32> loc("exp")
  %5 = "tfl.neg"(%4) : (tensor<4xf32>) -> tensor<4xf32> loc("neg")
  return %5 : tensor<4xf32>
}
