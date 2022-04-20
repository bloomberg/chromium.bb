// RUN: tf-opt %s -split-input-file -verify-diagnostics -tf-guarantee-all-funcs-one-use | FileCheck %s

// -----
// Basic test.
// CHECK-LABEL: func @f
func.func @f() {
  // CHECK: call @g() : () -> ()
  // CHECK: call @[[NEWG:.+]]() : () -> ()
  func.call @g() : () -> ()
  func.call @g() : () -> ()
  func.return
}

// CHECK: func @g()
// CHECK: func private @[[NEWG]]()
func.func @g() {
  func.return
}

// -----
// Transitive callees.
// CHECK-LABEL: func @f
// 2 copies of @g
// CHECK-DAG: func @g{{.*}}
// CHECK-DAG: func private @g{{.*}}
// 4 copies of @h
// CHECK-DAG: func @h{{.*}}
// CHECK-DAG: func private @h{{.*}}
// CHECK-DAG: func private @h{{.*}}
// CHECK-DAG: func private @h{{.*}}
func.func @f() {
  func.call @g() : () -> ()
  func.call @g() : () -> ()
  func.return
}

func.func @g() {
  func.call @h() : () -> ()
  func.call @h() : () -> ()
  func.return
}

func.func @h() {
  func.return
}

// -----
// Handle error case of infinite recursion.
// expected-error @+1 {{reached cloning limit}}
func.func private @f() {
  func.call @f() : () -> ()
  func.call @f() : () -> ()
  func.return
}
