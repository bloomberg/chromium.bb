// RUN: xla-opt -lhlo-legalize-to-affine %s -o - | FileCheck %s

// Smoke test.
// CHECK-LABEL: func @min_op
func @min_op(%lhs: memref<4x3x2x1xf32>, %rhs: memref<4x3x2x1xf32>,
             %result: memref<4x3x2x1xf32>) -> () {
  // CHECK-NEXT: affine.for %[[I:.*]] = 0 to 4 {
  // CHECK-NEXT:   affine.for %[[J:.*]] = 0 to 3 {
  // CHECK-NEXT:     affine.for %[[K:.*]] = 0 to 2 {
  // CHECK-NEXT:       affine.for %[[L:.*]] = 0 to 1 {
  // CHECK-NEXT:         %[[LHS:.*]] = affine.load %{{.*}}[%[[I]], %[[J]], %[[K]], %[[L]]] : memref<4x3x2x1xf32>
  // CHECK-NEXT:         %[[RHS:.*]] = affine.load %{{.*}}[%[[I]], %[[J]], %[[K]], %[[L]]] : memref<4x3x2x1xf32>
  // CHECK-NEXT:         %[[MIN_PREDICATE:.*]] = cmpf "olt", %[[LHS]], %[[RHS]] : f32
  // CHECK-NEXT:         %[[MIN:.*]] = select %[[MIN_PREDICATE]], %[[LHS]], %[[RHS]] : f32
  // CHECK-NEXT:         affine.store %[[MIN]], %{{.*}}[%[[I]], %[[J]], %[[K]], %[[L]]] : memref<4x3x2x1xf32>
  // CHECK:      return
  "xla_lhlo.minimum"(%lhs, %rhs, %result) {name = "min.1"} :
      (memref<4x3x2x1xf32>, memref<4x3x2x1xf32>, memref<4x3x2x1xf32>) -> ()
  return
}

// Add tests.
// CHECK-LABEL: func @float_add_op
func @float_add_op(%lhs: memref<7xf32>, %rhs: memref<7xf32>,
                   %result: memref<7xf32>) -> () {
  // CHECK: addf %{{.*}}, %{{.*}} : f32
  "xla_lhlo.add"(%lhs, %rhs, %result) {name = "add.1"}
      : (memref<7xf32>, memref<7xf32>, memref<7xf32>) -> ()
  return
}
// CHECK-LABEL: func @int_add_op
func @int_add_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: addi %{{.*}}, %{{.*}} : i32
  "xla_lhlo.add"(%lhs, %rhs, %result) {name = "add.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// And test.
// CHECK-LABEL: func @int_and_op
func @int_and_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: and %{{.*}}, %{{.*}} : i32
  "xla_lhlo.and"(%lhs, %rhs, %result) {name = "and.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// Div tests.
// CHECK-LABEL: func @float_div_op
func @float_div_op(%lhs: memref<7xf32>, %rhs: memref<7xf32>,
                   %result: memref<7xf32>) -> () {
  // CHECK: divf %{{.*}}, %{{.*}} : f32
  "xla_lhlo.divide"(%lhs, %rhs, %result) {name = "div.1"}
      : (memref<7xf32>, memref<7xf32>, memref<7xf32>) -> ()
  return
}
// CHECK-LABEL: func @int_div_op
func @int_div_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: divi_signed %{{.*}}, %{{.*}} : i32
  "xla_lhlo.divide"(%lhs, %rhs, %result) {name = "div.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// Max tests.
// CHECK-LABEL: func @float_max_op
func @float_max_op(%lhs: memref<7xf32>, %rhs: memref<7xf32>,
                   %result: memref<7xf32>) -> () {
  // CHECK: %[[CHECK:.*]] = cmpf "ogt", %[[ONE:.*]], %[[TWO:.*]] : f32
  // CHECK: select %[[CHECK]], %[[ONE]], %[[TWO]] : f32
  "xla_lhlo.maximum"(%lhs, %rhs, %result) {name = "max.1"}
      : (memref<7xf32>, memref<7xf32>, memref<7xf32>) -> ()
  return
}

// CHECK-LABEL: func @int_max_op
func @int_max_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: %[[CHECK:.*]] = cmpi "sgt", %[[ONE:.*]], %[[TWO:.*]] : i32
  // CHECK: select %[[CHECK]], %[[ONE]], %[[TWO]] : i32
  "xla_lhlo.maximum"(%lhs, %rhs, %result) {name = "max.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// Min tests.
// CHECK-LABEL: func @float_min_op
func @float_min_op(%lhs: memref<7xf32>, %rhs: memref<7xf32>,
                   %result: memref<7xf32>) -> () {
  // CHECK: %[[CHECK:.*]] = cmpf "olt", %[[ONE:.*]], %[[TWO:.*]] : f32
  // CHECK: select %[[CHECK]], %[[ONE]], %[[TWO]] : f32
  "xla_lhlo.minimum"(%lhs, %rhs, %result) {name = "min.1"}
      : (memref<7xf32>, memref<7xf32>, memref<7xf32>) -> ()
  return
}

// CHECK-LABEL: func @int_min_op
func @int_min_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: %[[CHECK:.*]] = cmpi "slt", %[[ONE:.*]], %[[TWO:.*]] : i32
  // CHECK: select %[[CHECK]], %[[ONE]], %[[TWO]] : i32
  "xla_lhlo.minimum"(%lhs, %rhs, %result) {name = "min.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// Mul tests.
// CHECK-LABEL: func @float_mul_op
func @float_mul_op(%lhs: memref<7xf32>, %rhs: memref<7xf32>,
                   %result: memref<7xf32>) -> () {
  // CHECK: mulf %{{.*}}, %{{.*}} : f32
  "xla_lhlo.multiply"(%lhs, %rhs, %result) {name = "mul.1"}
      : (memref<7xf32>, memref<7xf32>, memref<7xf32>) -> ()
  return
}

// CHECK-LABEL: func @int_mul_op
func @int_mul_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: muli %{{.*}}, %{{.*}} : i32
  "xla_lhlo.multiply"(%lhs, %rhs, %result) {name = "mul.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// Sub tests.
// CHECK-LABEL: func @float_sub_op
func @float_sub_op(%lhs: memref<7xf32>, %rhs: memref<7xf32>,
                   %result: memref<7xf32>) -> () {
  // CHECK: subf %{{.*}}, %{{.*}} : f32
  "xla_lhlo.subtract"(%lhs, %rhs, %result) {name = "sub.1"}
      : (memref<7xf32>, memref<7xf32>, memref<7xf32>) -> ()
  return
}
// CHECK-LABEL: func @int_sub_op
func @int_sub_op(%lhs: memref<7xi32>, %rhs: memref<7xi32>,
                 %result: memref<7xi32>) -> () {
  // CHECK: subi %{{.*}}, %{{.*}} : i32
  "xla_lhlo.subtract"(%lhs, %rhs, %result) {name = "sub.1"}
      : (memref<7xi32>, memref<7xi32>, memref<7xi32>) -> ()
  return
}

// Dot tests.
// CHECK-LABEL: func @float_dot_op
func @float_dot_op(%lhs: memref<7x3xf32>, %rhs:
                  memref<3x4xf32>, %result: memref<7x4xf32> ) -> () {
    // CHECK-NEXT: affine.for %[[I:.*]] = 0 to 7 {
    // CHECK-NEXT:  affine.for %[[J:.*]] = 0 to 4 {
    // CHECK-NEXT:    affine.for %[[K:.*]] = 0 to 3 {
    // CHECK-NEXT:      %[[LHS:.*]] = affine.load %{{.*}}[%[[I]], %[[K]]] : memref<7x3xf32>
    // CHECK-NEXT:      %[[RHS:.*]] = affine.load %{{.*}}[%[[K]], %[[J]]] : memref<3x4xf32>
    // CHECK-NEXT:      %[[RESULT:.*]] = affine.load %{{.*}}[%[[I]], %[[J]]] : memref<7x4xf32>
    // CHECK-NEXT:      %[[MULT:.*]] = mulf %[[LHS]], %[[RHS]] : f32
    // CHECK-NEXT:      %[[ADD:.*]] =  addf %[[MULT]], %[[RESULT]] : f32
    // CHECK-NEXT:      affine.store %[[ADD]], %{{.*}}[%[[I]], %[[J]]] : memref<7x4xf32>
    // CHECK: return
  "xla_lhlo.dot"(%lhs, %rhs, %result) :
    (memref<7x3xf32>, memref<3x4xf32>, memref<7x4xf32>) -> ()
  return
}
// CHECK-LABEL: func @int_dot_op
func @int_dot_op(%lhs: memref<7x3xi32>, %rhs:
                  memref<3x4xi32>, %result: memref<7x4xi32> ) -> () {
    // CHECK-NEXT: affine.for %[[I:.*]] = 0 to 7 {
    // CHECK-NEXT:  affine.for %[[J:.*]] = 0 to 4 {
    // CHECK-NEXT:    affine.for %[[K:.*]] = 0 to 3 {
    // CHECK-NEXT:      %[[LHS:.*]] = affine.load %{{.*}}[%[[I]], %[[K]]] : memref<7x3xi32>
    // CHECK-NEXT:      %[[RHS:.*]] = affine.load %{{.*}}[%[[K]], %[[J]]] : memref<3x4xi32>
    // CHECK-NEXT:      %[[RESULT:.*]] = affine.load %{{.*}}[%[[I]], %[[J]]] : memref<7x4xi32>
    // CHECK-NEXT:      %[[MULT:.*]] = muli %[[LHS]], %[[RHS]] : i32
    // CHECK-NEXT:      %[[ADD:.*]] =  addi %[[MULT]], %[[RESULT]] : i32
    // CHECK-NEXT:      affine.store %[[ADD]], %{{.*}}[%[[I]], %[[J]]] : memref<7x4xi32>
    // CHECK: return
  "xla_lhlo.dot"(%lhs, %rhs, %result) :
    (memref<7x3xi32>, memref<3x4xi32>, memref<7x4xi32>) -> ()
  return
}
