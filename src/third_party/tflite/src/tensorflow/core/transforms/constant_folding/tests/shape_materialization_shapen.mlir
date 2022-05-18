// RUN: tfg-transforms-opt -constant-folding %s | FileCheck %s

module {
  tfg.graph #tf_type.version<producer = 1010, min_consumer = 0> {
    %VariableV2, %ctl = VariableV2 name("v1") {container = "", dtype = f32, shape = #tf_type.shape<3x?>, shared_name = ""} : () -> (tensor<3x?x!tf_type.f32ref>)
    %VariableV2_0, %ctl_1 = VariableV2 name("v2") {container = "", dtype = f32, shape = #tf_type.shape<*>, shared_name = ""} : () -> (tensor<*x!tf_type.f32ref>)
    %VariableV2_2, %ctl_3 = VariableV2 name("v3") {container = "", dtype = f32, shape = #tf_type.shape<4x6>, shared_name = ""} : () -> (tensor<4x6x!tf_type.f32ref>)
    // CHECK: %[[CONST:.*]], %[[CTRL1:.*]] = {{.*}}-matshapes-2
    // CHECK: %[[CONST:.*]], %[[CTRL2:.*]] = {{.*}}-matshapes-2
    %ShapeN:3, %ctl_4 = ShapeN(%VariableV2, %VariableV2_0, %VariableV2_2) name("s") {N = 3 : i64, T = f32, out_type = i32} : (tensor<3x?x!tf_type.f32ref>, tensor<*x!tf_type.f32ref>, tensor<4x6x!tf_type.f32ref>) -> (tensor<*xi32>, tensor<*xi32>, tensor<*xi32>)
    %Identity, %ctl_5 = Identity(%ShapeN#0) name("i1a") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
    %Identity_6, %ctl_7 = Identity(%ShapeN#0) name("i1b") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
    %Identity_8, %ctl_9 = Identity(%ShapeN#1) name("i2a") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
    %Identity_10, %ctl_11 = Identity(%ShapeN#1) name("i2b") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
    %Identity_12, %ctl_13 = Identity(%ShapeN#1) name("i2c") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
    // CHECK: Const [%[[CTRL2]]] name("i3a")
    %Identity_14, %ctl_15 = Identity(%ShapeN#2) name("i3a") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
    // CHECK: Const [%[[CTRL1]]] name("i3b")
    %Identity_16, %ctl_17 = Identity(%ShapeN#2) name("i3b") {T = i32} : (tensor<*xi32>) -> (tensor<*xi32>)
  }
}
