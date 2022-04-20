// Copyright 2022 The TensorFlow Runtime Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// RUN: tf-quant-opt %s -split-input-file -quant-lift-quantizable-spots-as-functions -quant-quantize -verify-each=false | FileCheck %s

func.func private @conv(%input: tensor<1x3x4x3xf32> {tf._user_specified_name = "input_tensor"}) -> tensor<*xf32> attributes {tf._construction_context = "kEagerRuntime", tf._input_shapes = [#tf_type.shape<1x3x4x3>]} {
  %weight = arith.constant opaque<"elided_large_const", "0xDEADBEEF"> : tensor<2x3x3x2xf32>
  %bias = arith.constant dense<[7.11401462, 7.05456924]> : tensor<2xf32>

  %q_input= "quant.qcast"(%input) : (tensor<1x3x4x3xf32>) -> tensor<1x3x4x3x!quant.uniform<i8:f32, 0.58810077742034317:-128>>
  %dq_input= "quant.dcast"(%q_input) : (tensor<1x3x4x3x!quant.uniform<i8:f32, 0.58810077742034317:-128>>) -> tensor<1x3x4x3xf32>
  %q_weight = "quant.qcast"(%weight) : (tensor<2x3x3x2xf32>) -> tensor<2x3x3x2x!quant.uniform<i8:f32, 0.074855112561992565:-1>>
  %dq_weight = "quant.dcast"(%q_weight) : (tensor<2x3x3x2x!quant.uniform<i8:f32, 0.074855112561992565:-1>>) -> tensor<2x3x3x2xf32>
  %q_bias = "quant.qcast"(%bias) : (tensor<2xf32>) -> tensor<2x!quant.uniform<i32:f32, 0.044022349891595126>>
  %dq_bias = "quant.dcast"(%q_bias) : (tensor<2x!quant.uniform<i32:f32, 0.044022349891595126>>) -> tensor<2xf32>

  %res = "tf._FusedConv2D"(%dq_input, %dq_weight, %dq_bias) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], epsilon = 0.000000e+00 : f32, explicit_paddings = [], fused_ops = ["BiasAdd", "Relu6"], leakyrelu_alpha = 2.000000e-01 : f32, padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>, tensor<2xf32>) -> tensor<*xf32>

  %q_res = "quant.qcast"(%res) : (tensor<*xf32>) -> tensor<*x!quant.uniform<i8:f32, 0.023529411764705882:-128>>
  %dq_res = "quant.dcast"(%q_res) : (tensor<*x!quant.uniform<i8:f32, 0.023529411764705882:-128>>) -> tensor<*xf32>

  func.return %dq_res : tensor<*xf32>
}

// CHECK: [[bias:%.+]] = "arith.constant"() {value = dense<[7.11401462, 7.05456924]> : tensor<2xf32>} : () -> tensor<2xf32>
// CHECK-NEXT: [[weight:%.+]] = "arith.constant"() {value = opaque<"elided_large_const", "0xDEADBEEF"> : tensor<2x3x3x2xf32>} : () -> tensor<2x3x3x2x!quant.uniform<i8:f32, 0.074855112561992565:-1>>
// CHECK-NEXT: [[q_input:%.+]] = "quant.qcast"(%arg0) : (tensor<1x3x4x3xf32>) -> tensor<1x3x4x3x!quant.uniform<i8:f32, 0.58810077742034317:-128>>
// CHECK-NEXT: [[q_bias:%.+]] = "quant.qcast"([[bias]]) : (tensor<2xf32>) -> tensor<2x!quant.uniform<i32:f32, 0.044022349891595126>>
// CHECK-NEXT: [[conv:%.+]] = "tf.PartitionedCall"([[q_input]], [[weight]], [[q_bias]]) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @[[fused_fn:fused_conv2d_with_bias_and_relu6_fn.*]]} : (tensor<1x3x4x3x!quant.uniform<i8:f32, 0.58810077742034317:-128>>, tensor<2x3x3x2x!quant.uniform<i8:f32, 0.074855112561992565:-1>>, tensor<2x!quant.uniform<i32:f32, 0.044022349891595126>>) -> tensor<*x!quant.uniform<i8:f32, 0.023529411764705882:-128>>
// CHECK-NEXT: [[res:%.+]] = "quant.dcast"([[conv]]) : (tensor<*x!quant.uniform<i8:f32, 0.023529411764705882:-128>>) -> tensor<*xf32>
// CHECK-NEXT: "func.return"(%5) : (tensor<*xf32>) -> ()


// -----

// CHECK-LABEL: same_scale_test
func.func @same_scale_test(%arg0: tensor<*xf32>) -> tensor<*xf32> {
  %cst = arith.constant dense<[-1, 144]> : tensor<2xi32>
  %0 = "quant.qcast"(%arg0) : (tensor<*xf32>) -> tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>
  %1 = "quant.dcast"(%0) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>) -> tensor<*xf32>
  %2 = "tf.MaxPool"(%1) {data_format = "NHWC", device = "", explicit_paddings = [], ksize = [1, 2, 2, 1], padding = "VALID", strides = [1, 2, 2, 1]} : (tensor<*xf32>) -> tensor<*xf32>
  %3 = "quant.qcast"(%2) {volatile} : (tensor<*xf32>) -> tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>
  %4 = "quant.dcast"(%3) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>) -> tensor<*xf32>
  %5 = "tf.Reshape"(%4, %cst) {device = ""} : (tensor<*xf32>, tensor<2xi32>) -> tensor<*xf32>
  %6 = "quant.qcast"(%5) {volatile} : (tensor<*xf32>) -> tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>
  %7 = "quant.dcast"(%6) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>) -> tensor<*xf32>
  func.return %7 : tensor<*xf32>
}

// CHECK: %[[q:.*]] = "quant.qcast"(%arg0)
// CHECK: %[[sc1:.*]] = "quant.scast"(%[[q]]) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>)
// CHECK: %[[maxpool_i8:.*]] = "tf.MaxPool"(%[[sc1]])
// CHECK-SAME: (tensor<*xi8>) -> tensor<*xi8>
// CHECK: %[[reshape_i8:.*]] = "tf.Reshape"(%[[maxpool_i8]]
// CHECK-SAME: (tensor<*xi8>, tensor<2xi32>) -> tensor<*xi8>
// CHECK: %[[sc2:.*]] = "quant.scast"(%[[reshape_i8]])
// CHECK: %[[dq:.*]] = "quant.dcast"(%[[sc2]]) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-10>>)
// CHECK: return %[[dq]]
