// RUN: tf-opt %s -split-input-file -verify-diagnostics -tf-resource-op-lifting | FileCheck %s

// Tests that resource load operations are hoisted.

// CHECK-LABEL: func @only_resource_load
func @only_resource_load() -> tensor<*xi32> {

  // CHECK: %[[RES_HANDLE:[0-9]*]] = "tf.VarHandleOp"
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource>

  // CHECK: %[[RES_READ_VAL:[0-9]*]] = "tf.ReadVariableOp"(%[[RES_HANDLE]]) {dtype = i32}
  // CHECK: "tf_device.cluster"
  // CHECK: %[[COMPUTE_RES:[0-9]*]] = "tf.SomeComputation"(%[[RES_READ_VAL]])
  // CHECK: tf_device.return %[[COMPUTE_RES]]
  // CHECK: {cluster_attr = "cluster_attr"}
  // CHECK-SAME: () -> tensor<*xi32>

  %1 = "tf_device.cluster"() ( {
    %2 = "tf.ReadVariableOp"(%0) {dtype = i32} : (tensor<*x!tf.resource>) -> tensor<*xi32>
    %3 = "tf.SomeComputation"(%2) : (tensor<*xi32>) -> (tensor<*xi32>)
    tf_device.return %3 : tensor<*xi32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<*xi32>

  return %1 : tensor<*xi32>
}

// -----

// Tests that resource store operations are hoisted.

// CHECK-LABEL: func @only_resource_store
func @only_resource_store() -> tensor<*xi32> {

  // CHECK: %[[RES_HANDLE:[0-9]*]] = "tf.VarHandleOp"
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource>

  // CHECK: %[[CLUSTER_RES:[0-9]*]]:2 = "tf_device.cluster"
  // CHECK: %[[COMPUTE_RES:[0-9]*]] = "tf.SomeComputation"()
  // CHECK: tf_device.return %[[COMPUTE_RES]], %[[COMPUTE_RES]]
  // CHECK: {cluster_attr = "cluster_attr"}
  // CHECK-SAME: () -> (tensor<*xi32>, tensor<*xi32>)
  // CHECK: "tf.AssignVariableOp"(%[[RES_HANDLE]], %[[CLUSTER_RES]]#1) {dtype = i32}

  %1 = "tf_device.cluster"() ( {
    %2 = "tf.SomeComputation"() : () -> (tensor<*xi32>)
    "tf.AssignVariableOp"(%0, %2) {dtype = i32} : (tensor<*x!tf.resource>, tensor<*xi32>) -> ()
    tf_device.return %2 : tensor<*xi32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<*xi32>

  // CHECK: return %[[CLUSTER_RES]]#0
  return %1 : tensor<*xi32>
}

// -----

// Tests that a resource ops with both load and store are hoisted.

// CHECK-LABEL: func @same_resource_load_and_store
func @same_resource_load_and_store() -> tensor<*xi32> {

  // CHECK: %[[RES_HANDLE:[0-9]*]] = "tf.VarHandleOp"
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource>

  // CHECK: %[[RES_READ_VAL:[0-9]*]] = "tf.ReadVariableOp"(%[[RES_HANDLE]]) {dtype = i32}
  // CHECK: %[[CLUSTER_RES:[0-9]*]]:2 = "tf_device.cluster"
  // CHECK: %[[COMPUTE_RES:[0-9]*]] = "tf.SomeComputation"(%[[RES_READ_VAL]])
  // CHECK: tf_device.return %[[COMPUTE_RES]], %[[COMPUTE_RES]]
  // CHECK: {cluster_attr = "cluster_attr"}
  // CHECK-SAME: () -> (tensor<*xi32>, tensor<*xi32>)
  // CHECK: "tf.AssignVariableOp"(%[[RES_HANDLE]], %[[CLUSTER_RES]]#1) {dtype = i32}

  %1 = "tf_device.cluster"() ( {
    %2 = "tf.ReadVariableOp"(%0) {dtype = i32} : (tensor<*x!tf.resource>) -> tensor<*xi32>
    %3 = "tf.SomeComputation"(%2) : (tensor<*xi32>) -> (tensor<*xi32>)
    "tf.AssignVariableOp"(%0, %3) {dtype = i32} : (tensor<*x!tf.resource>, tensor<*xi32>) -> ()
    tf_device.return %3 : tensor<*xi32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<*xi32>

  // CHECK: return %[[CLUSTER_RES]]#0
  return %1 : tensor<*xi32>
}

// -----

// Tests that internal resource operations are not hoisted.

// CHECK-LABEL: func @internal_resource
func @internal_resource() -> tensor<*xi32> {

  // CHECK: %[[CLUSTER_RES:[0-9]*]] = "tf_device.cluster"
  %0 = "tf_device.cluster"() ( {

    // CHECK: %[[RES_HANDLE:[0-9]*]] = "tf.VarHandleOp"
    %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource>

    // CHECK: %[[RES_READ_VAL:[0-9]*]] = "tf.ReadVariableOp"(%[[RES_HANDLE]])
    %2 = "tf.ReadVariableOp"(%1) {dtype = i32} : (tensor<*x!tf.resource>) -> tensor<*xi32>

    // CHECK: %[[COMPUTE_RES:[0-9]*]] = "tf.SomeComputation"(%[[RES_READ_VAL]])
    %3 = "tf.SomeComputation"(%2) : (tensor<*xi32>) -> (tensor<*xi32>)

    // CHECK: "tf.AssignVariableOp"(%[[RES_HANDLE]], %[[COMPUTE_RES]])
    "tf.AssignVariableOp"(%1, %3) {dtype = i32} : (tensor<*x!tf.resource>, tensor<*xi32>) -> ()

    // CHECK: tf_device.return %[[COMPUTE_RES]]
    tf_device.return %3 : tensor<*xi32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<*xi32>

  // CHECK: return %[[CLUSTER_RES]]
  return %0 : tensor<*xi32>
}

// -----

// Tests that pass fails when there are remaining resource operationss that can
// not be lifted.

func @lifting_failure() -> tensor<*xi32> {

  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource>

  // expected-error @+1 {{has remaining resource inputs that can not be lifted}}
  %1 = "tf_device.cluster"() ( {
    %2 = "tf.ReadVariableOp"(%0) {dtype = i32} : (tensor<*x!tf.resource>) -> tensor<*xi32>
		%3 = "tf.SomeResourceOp"(%0, %2) : (tensor<*x!tf.resource>, tensor<*xi32>) -> tensor<*xi32>
    "tf.AssignVariableOp"(%0, %3) {dtype = i32} : (tensor<*x!tf.resource>, tensor<*xi32>) -> ()
    tf_device.return %3 : tensor<*xi32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<*xi32>

  return %1 : tensor<*xi32>
}

// -----

// Tests that pass lifts resource reads/writes from a loop, and removed unused
// resources.

// CHECK-LABEL: func @cluster_with_loop
func @cluster_with_loop() -> () {
  // CHECK: %[[COUNT:.*]] = "tf.Const"() {value = dense<10> : tensor<i32>}
  %0 = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
  // CHECK: %[[VH:.*]] = "tf.VarHandleOp"()
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  %unused = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[VH]])
  // CHECK: %[[CLUSTER:.*]] = "tf_device.cluster"()
  "tf_device.cluster"() ( {
    // CHECK: %[[WHILE:.*]]:2 = "tf.While"(%[[COUNT]], %[[READ]])
    %2:3 = "tf.While"(%0, %1, %unused)
               {body = @while_body, cond = @while_cond, device = "", is_stateless = false,
                output_shapes = [#tf.shape<>, #tf.shape<>]}
         : (tensor<i32>, tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
         -> (tensor<i32>, tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK: tf_device.return %[[WHILE]]#1 : tensor<f32>
    tf_device.return
  // CHECK: {cluster_attr = "cluster_attr"} : () -> tensor<f32>
  }) {cluster_attr = "cluster_attr"} : () -> ()
  // CHECK: "tf.AssignVariableOp"(%[[VH]], %[[CLUSTER]])
  // CHECK: return
  return
}
// CHECK: func @while_body(%[[BARG0:.*]]: tensor<i32>, %[[BARG1:.*]]: tensor<f32>)
func @while_body(%arg0: tensor<i32>, %arg1: tensor<*x!tf.resource<tensor<f32>>>, %arg2: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<i32>, tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>) {
  %read0 = "tf.ReadVariableOp"(%arg1) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  // CHECK-NEXT: %[[ADD0:.*]] = "tf.AddV2"(%[[BARG1]], %[[BARG1]])
  %add0 = "tf.AddV2"(%read0, %read0) : (tensor<f32>, tensor<f32>) -> tensor<f32>
  "tf.AssignVariableOp"(%arg1, %add0) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  %read1 = "tf.ReadVariableOp"(%arg1) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  // CHECK-NEXT: %[[ADD1:.*]] = "tf.AddV2"(%[[ADD0]], %[[ADD0]])
  %add1 = "tf.AddV2"(%read1, %read1) : (tensor<f32>, tensor<f32>) -> tensor<f32>
  "tf.AssignVariableOp"(%arg1, %add1) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  // CHECK-NEXT: %[[DELTA:.*]] = "tf.Const"() {value = dense<-1> : tensor<i32>}
  %constant = "tf.Const"() {value = dense<-1> : tensor<i32>} : () -> tensor<i32>
  // CHECK-NEXT: %[[ADD2:.*]] = "tf.AddV2"(%[[BARG0]], %[[DELTA]])
  %add2 = "tf.AddV2"(%arg0, %constant) : (tensor<i32>, tensor<i32>) -> tensor<i32>
  // CHECK-NEXT: return %[[ADD2]], %[[ADD1]]
  %id = "tf.Identity"(%arg2) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
  return %add2, %arg1, %id : tensor<i32>, tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @while_cond(%[[CARG0:.*]]: tensor<i32>, %[[CARG1:.*]]: tensor<f32>)
func @while_cond(%arg0: tensor<i32>, %arg1: tensor<*x!tf.resource<tensor<f32>>>, %arg2: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<i32> {
  // CHECK-NEXT: return %[[CARG0]]
  return %arg0 : tensor<i32>
}

// -----

// Tests that pass lifts resource reads from loop condition.

// CHECK-LABEL: func @cluster_with_loop
func @cluster_with_loop() -> () {
  // CHECK: %[[VH:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[VH]])
  // CHECK: %[[CLUSTER:.*]] = "tf_device.cluster"()
  "tf_device.cluster"() ( {
    // CHECK: %[[WHILE:.*]] = "tf.While"(%[[READ]])
    %1 = "tf.While"(%0) {
      body = @while_body, cond = @while_cond, device = "", is_stateless = false,
      output_shapes = [#tf.shape<>]}
         : (tensor<*x!tf.resource<tensor<f32>>>)
         -> (tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK: tf_device.return %[[WHILE]] : tensor<f32>
    tf_device.return
  // CHECK: {cluster_attr = "cluster_attr"} : () -> tensor<f32>
  }) {cluster_attr = "cluster_attr"} : () -> ()
  // CHECK: "tf.AssignVariableOp"(%[[VH]], %[[CLUSTER]])
  // CHECK: return
  return
}
// CHECK: func @while_body(%[[BARG0:.*]]: tensor<f32>)
func @while_body(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: %[[CONST:.*]] = "tf.Const"()
  %constant = "tf.Const"() {value = dense<0.0> : tensor<f32>} : () -> tensor<f32>
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  // CHECK-NEXT: return %[[CONST]]
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @while_cond(%arg0: tensor<f32>)
func @while_cond(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %id = "tf.Identity"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
  %read = "tf.ReadVariableOp"(%id) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  // CHECK-NEXT: return %[[CARG0]]
  return %read : tensor<f32>
}

// -----

// Tests that pass lifts read-only resource reads from loop, but does not add
// assign after the loop.

// CHECK-LABEL: func @cluster_with_loop
func @cluster_with_loop() -> () {
  // CHECK: %[[VH:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[VH]])
  // CHECK: "tf_device.cluster"()
  "tf_device.cluster"() ( {
    // CHECK: %[[WHILE:.*]] = "tf.While"(%[[READ]])
    %1 = "tf.While"(%0) {
      body = @while_body, cond = @while_cond, device = "", is_stateless = false,
      output_shapes = [#tf.shape<>]}
         : (tensor<*x!tf.resource<tensor<f32>>>)
         -> (tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK: tf_device.return
    tf_device.return
  // CHECK: {cluster_attr = "cluster_attr"} : () -> ()
  }) {cluster_attr = "cluster_attr"} : () -> ()
  // CHECK-NOT: "tf.AssignVariableOp"
  // CHECK: return
  return
}
// CHECK: func @while_body(%[[BARG0:.*]]: tensor<f32>)
func @while_body(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: return %[[BARG0]]
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @while_cond(%[[CARG0:.*]]: tensor<f32>)
func @while_cond(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %read = "tf.ReadVariableOp"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  // CHECK-NEXT: return %[[CARG0]]
  return %read : tensor<f32>
}

// -----

// Tests that pass lifts resource reads from nested loops.

// CHECK-LABEL: func @cluster_with_nested_loop
func @cluster_with_nested_loop() -> () {
  // CHECK: %[[VH:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[VH_UNUSED:.*]] = "tf.VarHandleOp"()
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[VH]])
  // CHECK: %[[CLUSTER:.*]] = "tf_device.cluster"()
  "tf_device.cluster"() ( {
    // CHECK: %[[WHILE:.*]] = "tf.While"(%[[READ]])
    %2:2 = "tf.While"(%0, %1) {
      body = @while_body, cond = @while_cond, device = "", is_stateless = false,
      output_shapes = [#tf.shape<>, #tf.shape<>]}
         : (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
         -> (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK: tf_device.return %[[WHILE]] : tensor<f32>
    tf_device.return
  // CHECK: {cluster_attr = "cluster_attr"} : () -> tensor<f32>
  }) {cluster_attr = "cluster_attr"} : () -> ()
  // CHECK: "tf.AssignVariableOp"(%[[VH]], %[[CLUSTER]])
  // CHECK: return
  return
}
// CHECK: func @while_body(%[[BARG0:.*]]: tensor<f32>)
func @while_body(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK: %[[WHILE:.*]] = "tf.While"(%[[BARG0]])
  %0:2 = "tf.While"(%arg0, %arg1) {
    body = @while_body1, cond = @while_cond1, device = "", is_stateless = false,
    output_shapes = [#tf.shape<>, #tf.shape<>]}
       : (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
       -> (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
  // CHECK-NEXT: return %[[WHILE]]
  return %0#0, %0#1 : tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @while_cond(%arg0: tensor<f32>)
func @while_cond(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %id = "tf.Identity"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
  %read = "tf.ReadVariableOp"(%id) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  // CHECK-NEXT: return %[[CARG0]]
  return %read : tensor<f32>
}
// CHECK: func @while_body1(%[[BARG0:.*]]: tensor<f32>)
func @while_body1(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: %[[CONST:.*]] = "tf.Const"()
  %constant = "tf.Const"() {value = dense<0.0> : tensor<f32>} : () -> tensor<f32>
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  // CHECK-NEXT: return %[[CONST]]
  return %arg0, %arg1 : tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @while_cond1(%arg0: tensor<f32>)
func @while_cond1(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %id = "tf.Identity"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
  %read = "tf.ReadVariableOp"(%id) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  // CHECK-NEXT: return %[[CARG0]]
  return %read : tensor<f32>
}

// -----

// Tests that pass reports error on non-aliasing while input/output resources.

func @cluster_with_loop() -> () {
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  "tf_device.cluster"() ( {
    %1 = "tf.While"(%0) {
      body = @while_body, cond = @while_cond, device = "", is_stateless = false,
      output_shapes = [#tf.shape<>]}
         : (tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>)
    tf_device.return
  }) {cluster_attr = "cluster_attr"} : () -> ()
  return
}
func @while_body(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>) {
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // expected-error @+1 {{resource used in while loop is only supported when the resource input and output alias each other in the loop body}}
  return %0 : tensor<*x!tf.resource<tensor<f32>>>
}
func @while_cond(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %read = "tf.ReadVariableOp"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  return %read : tensor<f32>
}

// -----

// Tests that pass reports error on unsupported ops in loop body.

func @cluster_with_loop() -> () {
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  "tf_device.cluster"() ( {
    %1 = "tf.While"(%0) {
      body = @while_body, cond = @while_cond, device = "", is_stateless = false,
      output_shapes = [#tf.shape<>]}
         : (tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>)
    tf_device.return
  }) {cluster_attr = "cluster_attr"} : () -> ()
  return
}
func @while_body(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // expected-error @+1 {{found unsupported operations on resource.}}
  "tf._UnknownOp"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> ()
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
func @while_cond(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %read = "tf.ReadVariableOp"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  return %read : tensor<f32>
}

// -----

// Tests that pass reports error on unsupported ops in loop cond.

func @cluster_with_loop() -> () {
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  "tf_device.cluster"() ( {
    %1 = "tf.While"(%0) {
      body = @while_body, cond = @while_cond, device = "", is_stateless = false,
      output_shapes = [#tf.shape<>]}
         : (tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>)
    tf_device.return
  }) {cluster_attr = "cluster_attr"} : () -> ()
  return
}
func @while_body(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource<tensor<f32>>>) {
  %constant = "tf.Const"() {value = dense<0.0> : tensor<f32>} : () -> tensor<f32>
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
func @while_cond(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32> {
  %read = "tf.ReadVariableOp"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  %constant = "tf.Const"() {value = dense<0.0> : tensor<f32>} : () -> tensor<f32>
  // expected-error @+1 {{found resource write in loop condition.}}
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  return %read : tensor<f32>
}

// -----

// CHECK: func @cluster_with_case(%[[ARG0:.*]]: tensor<i32>) -> tensor<4xf32>
func @cluster_with_case(%arg0: tensor<i32>) -> tensor<4xf32> {
  // CHECK: %[[VH0:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<4xf32>>>
  // CHECK: %[[VH1:.*]] = "tf.VarHandleOp"()
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<4xf32>>>
  // CHECK-DAG: %[[READ0:.*]] = "tf.ReadVariableOp"(%[[VH0]])
  // CHECK-DAG: %[[READ1:.*]] = "tf.ReadVariableOp"(%[[VH1]])
  // CHECK: %[[CLUSTER:.*]]:2 = "tf_device.cluster"()
  %2 = "tf_device.cluster"() ( {
    // CHECK: %[[CASE:.*]]:2 = "tf.Case"(%[[ARG0]], %[[READ0]], %[[READ1]])
    %3:2 = "tf.Case"(%arg0, %0, %1) {branches = [@branch_0, @branch_1, @branch_2]}
      : (tensor<i32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<*x!tf.resource<tensor<4xf32>>>)
      -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>)
    // CHECK-NEXT: %[[ADD:.*]] = "tf.AddV2"(%[[CASE]]#1, %[[CASE]]#0)
    %4 = "tf.ReadVariableOp"(%3#0) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
    %5 = "tf.AddV2"(%4, %3#1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
    // CHECK-NEXT: tf_device.return %[[ADD]], %[[CASE]]#1
    tf_device.return %5 : tensor<4xf32>
  // CHECK: {cluster_attr = "cluster_attr"} : () -> (tensor<4xf32>, tensor<4xf32>)
  }) {cluster_attr = "cluster_attr"} : () -> tensor<4xf32>
  // CHECK: "tf.AssignVariableOp"(%[[VH0]], %[[CLUSTER]]#1)
  // CHECK: return %[[CLUSTER]]#0
  return %2 : tensor<4xf32>
}
// CHECK: func @branch_0(%[[TARG0:.*]]: tensor<4xf32>, %[[TARG1:.*]]: tensor<4xf32>)
func @branch_0(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) {
  // CHECK-NEXT: %[[CONST:.*]] = "tf.Const"()
  %constant = "tf.Const"() {value = dense<0.0> : tensor<4xf32>} : () -> tensor<4xf32>
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) -> ()
  // CHECK-NEXT: return %[[CONST]], %[[CONST]]
  return %arg0, %constant : tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>
}
// CHECK: func @branch_1(%[[EARG0:.*]]: tensor<4xf32>, %[[EARG1:.*]]: tensor<4xf32>)
func @branch_1(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) {
  %id = "tf.Identity"(%arg1) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<*x!tf.resource<tensor<4xf32>>>
  %read = "tf.ReadVariableOp"(%id) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
  "tf.AssignVariableOp"(%arg0, %read) : (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) -> ()
  // CHECK-NEXT: return %[[EARG1]], %[[EARG1]]
  return %arg0, %read : tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>
}
// CHECK: func @branch_2(%[[EARG0:.*]]: tensor<4xf32>, %[[EARG1:.*]]: tensor<4xf32>)
func @branch_2(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) {
  %id = "tf.Identity"(%arg1) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<*x!tf.resource<tensor<4xf32>>>
  %read = "tf.ReadVariableOp"(%id) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
  "tf.AssignVariableOp"(%arg0, %read) : (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) -> ()
  // CHECK-NEXT: return %[[EARG1]], %[[EARG1]]
  return %arg0, %read : tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>
}

// -----

// Tests that pass lifts resource reads from if branches.

// CHECK: func @cluster_with_if(%[[ARG0:.*]]: tensor<i1>) -> tensor<4xf32>
func @cluster_with_if(%arg0: tensor<i1>) -> tensor<4xf32> {
  // CHECK: %[[VH0:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<4xf32>>>
  // CHECK: %[[VH1:.*]] = "tf.VarHandleOp"()
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<4xf32>>>
  // CHECK-DAG: %[[READ0:.*]] = "tf.ReadVariableOp"(%[[VH0]])
  // CHECK-DAG: %[[READ1:.*]] = "tf.ReadVariableOp"(%[[VH1]])
  // CHECK: %[[CLUSTER:.*]]:2 = "tf_device.cluster"()
  %2 = "tf_device.cluster"() ( {
    // CHECK: %[[IF:.*]]:2 = "tf.If"(%[[ARG0]], %[[READ0]], %[[READ1]])
    %3:2 = "tf.If"(%arg0, %0, %1) {then_branch = @if_then, else_branch = @if_else,
        is_stateless = false}
      : (tensor<i1>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<*x!tf.resource<tensor<4xf32>>>)
      -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>)
    // CHECK-NEXT: %[[ADD:.*]] = "tf.AddV2"(%[[IF]]#1, %[[IF]]#0)
    %4 = "tf.ReadVariableOp"(%3#0) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
    %5 = "tf.AddV2"(%4, %3#1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
    // CHECK-NEXT: tf_device.return %[[ADD]], %[[IF]]#1
    tf_device.return %5 : tensor<4xf32>
  // CHECK: {cluster_attr = "cluster_attr"} : () -> (tensor<4xf32>, tensor<4xf32>)
  }) {cluster_attr = "cluster_attr"} : () -> tensor<4xf32>
  // CHECK: "tf.AssignVariableOp"(%[[VH0]], %[[CLUSTER]]#1)
  // CHECK: return %[[CLUSTER]]#0
  return %2 : tensor<4xf32>
}
// CHECK: func @if_then(%[[TARG0:.*]]: tensor<4xf32>, %[[TARG1:.*]]: tensor<4xf32>)
func @if_then(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) {
  // CHECK-NEXT: %[[CONST:.*]] = "tf.Const"()
  %constant = "tf.Const"() {value = dense<0.0> : tensor<4xf32>} : () -> tensor<4xf32>
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) -> ()
  // CHECK-NEXT: return %[[CONST]], %[[CONST]]
  return %arg0, %constant : tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>
}
// CHECK: func @if_else(%[[EARG0:.*]]: tensor<4xf32>, %[[EARG1:.*]]: tensor<4xf32>)
func @if_else(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) {
  %id = "tf.Identity"(%arg1) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<*x!tf.resource<tensor<4xf32>>>
  %read = "tf.ReadVariableOp"(%id) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
  "tf.AssignVariableOp"(%arg0, %read) : (tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>) -> ()
  // CHECK-NEXT: return %[[EARG1]], %[[EARG1]]
  return %arg0, %read : tensor<*x!tf.resource<tensor<4xf32>>>, tensor<4xf32>
}

// -----

// Tests that pass lifts resource reads from nested if ops.

// CHECK: func @cluster_with_nested_if(%[[ARG0:.*]]: tensor<i1>) -> tensor<f32>
func @cluster_with_nested_if(%arg0: tensor<i1>) -> tensor<f32> {
  // CHECK: %[[VH0:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[VH1:.*]] = "tf.VarHandleOp"()
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK-DAG: %[[READ0:.*]] = "tf.ReadVariableOp"(%[[VH0]])
  // CHECK: %[[CLUSTER:.*]]:2 = "tf_device.cluster"()
  %2 = "tf_device.cluster"() ( {
    // CHECK: %[[IF:.*]] = "tf.If"(%[[ARG0]], %[[READ0]])
    %3 = "tf.If"(%arg0, %0, %1) {then_branch = @if_then, else_branch = @if_else,
        is_stateless = false}
      : (tensor<i1>, tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
      -> (tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK-NEXT: %[[ADD:.*]] = "tf.AddV2"(%[[IF]], %[[IF]])
    %4 = "tf.ReadVariableOp"(%3) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
    %5 = "tf.AddV2"(%4, %4) : (tensor<f32>, tensor<f32>) -> tensor<f32>
    // CHECK-NEXT: tf_device.return %[[ADD]], %[[IF]]
    tf_device.return %5 : tensor<f32>
  // CHECK: {cluster_attr = "cluster_attr"} : () -> (tensor<f32>, tensor<f32>)
  }) {cluster_attr = "cluster_attr"} : () -> tensor<f32>
  // CHECK: "tf.AssignVariableOp"(%[[VH0]], %[[CLUSTER]]#1)
  // CHECK: return %[[CLUSTER]]#0
  return %2 : tensor<f32>
}
// CHECK: func @if_then(%[[TARG0:.*]]: tensor<f32>)
func @if_then(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: %[[IIF:.*]] = "tf.If"(%[[TARG0]], %[[TARG0]])
  %read = "tf.ReadVariableOp"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  %3 = "tf.If"(%read, %arg0) {then_branch = @inner_if_then, else_branch = @inner_if_else,
      is_stateless = false}
    : (tensor<f32>, tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>)
  // CHECK-NEXT: return %[[IIF]]
  return %3 : tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @if_else(%[[EARG0:.*]]: tensor<f32>)
func @if_else(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: return %[[EARG0]]
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @inner_if_then(%[[ITARG0:.*]]: tensor<f32>)
func @inner_if_then(%arg0: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: %[[CONST:.*]] = "tf.Const"()
  %constant = "tf.Const"() {value = dense<0.0> : tensor<f32>} : () -> tensor<f32>
  "tf.AssignVariableOp"(%arg0, %constant) : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  // CHECK-NEXT: return %[[CONST]]
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @inner_if_else(%[[IEARG0:.*]]: tensor<f32>)
func @inner_if_else(%arg0: tensor<*x!tf.resource<tensor<f32>>>)
    -> (tensor<*x!tf.resource<tensor<f32>>>) {
  // CHECK-NEXT: return %[[IEARG0]]
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}

// -----

// Tests that the pass reports error for ambiguous resource aliasing.

func @cluster_with_if(%arg0: tensor<i1>) -> tensor<4xf32> {
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<4xf32>>>
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<4xf32>>>
  %2 = "tf_device.cluster"() ( {
    // expected-error @+1 {{unsupported output: resource does not alias a single input}}
    %3 = "tf.If"(%arg0, %0, %1) {then_branch = @if_then, else_branch = @if_else,
        is_stateless = false}
      : (tensor<i1>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<*x!tf.resource<tensor<4xf32>>>)
      -> (tensor<*x!tf.resource<tensor<4xf32>>>)
    %4 = "tf.ReadVariableOp"(%3) : (tensor<*x!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
    tf_device.return %4 : tensor<4xf32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<4xf32>
  return %2 : tensor<4xf32>
}
func @if_then(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>) {
  return %arg0 : tensor<*x!tf.resource<tensor<4xf32>>>
}
func @if_else(%arg0: tensor<*x!tf.resource<tensor<4xf32>>>, %arg1: tensor<*x!tf.resource<tensor<4xf32>>>)
    -> (tensor<*x!tf.resource<tensor<4xf32>>>) {
  return %arg1 : tensor<*x!tf.resource<tensor<4xf32>>>
}

// -----

// Tests that the pass lifts resources on two partitioned call ops sharing the
// same callee. The lifting should clone the callee then modify the clone.

// CHECK-LABEL: @cluster_with_partitioned_call
func @cluster_with_partitioned_call() -> tensor<f32> {
  // CHECK: %[[VH:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[CONST:.*]] = "tf.Const"()
  %1 = "tf.Const"() {value = dense<10.0> : tensor<f32>} : () -> tensor<f32>
  // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[VH]])
  // CHECK: %[[CLUSTER:.*]] = "tf_device.cluster"()
  %2 = "tf_device.cluster"() ( {
    // CHECK: %[[PC0:.*]] = "tf.PartitionedCall"(%[[CONST]], %[[READ]], %[[CONST]])
    // CHECK-SAME: f = @callee_resource_lifted
    %3 = "tf.PartitionedCall"(%1, %0, %1) {f = @callee, config = "", config_proto = "", executor_type = ""}
      : (tensor<f32>, tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> tensor<f32>
    // CHECK: %[[PC1:.*]] = "tf.PartitionedCall"(%[[CONST]], %[[READ]], %[[CONST]])
    // CHECK-SAME: f = @callee_resource_lifted
    %4 = "tf.PartitionedCall"(%1, %0, %1) {f = @callee, config = "", config_proto = "", executor_type = ""}
      : (tensor<f32>, tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> tensor<f32>
    // CHECK: %[[ADD:.*]] = "tf.AddV2"(%[[PC0]], %[[PC1]])
    %5 = "tf.AddV2"(%3, %4) : (tensor<f32>, tensor<f32>) -> tensor<f32>
    // CHECK: tf_device.return %[[ADD]] : tensor<f32>
    tf_device.return %5 : tensor<f32>
  }) {cluster_attr = "cluster_attr"} : () -> tensor<f32>
  return %2 : tensor<f32>
}
// CHECK: @callee(%[[OA0:.*]]: tensor<f32>, %[[OA1:.*]]: tensor<*x!tf.resource<tensor<f32>>>, %[[OA2:.*]]: tensor<f32>) -> tensor<f32>
func @callee(%arg0: tensor<f32>, %arg1: tensor<*x!tf.resource<tensor<f32>>>, %arg2: tensor<f32>) -> tensor<f32> {
  // CHECK: "tf.ReadVariableOp"(%[[OA1]])
  %0 = "tf.ReadVariableOp"(%arg1) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  %1 = "tf.AddV2"(%0, %arg0) : (tensor<f32>, tensor<f32>) -> tensor<f32>
  %2 = "tf.AddV2"(%1, %arg2) : (tensor<f32>, tensor<f32>) -> tensor<f32>
  return %2 : tensor<f32>
}
// CHECK: func @callee_resource_lifted(%[[A0:.*]]: tensor<f32>, %[[A1:.*]]: tensor<f32>, %[[A2:.*]]: tensor<f32>) -> tensor<f32>
// CHECK-NEXT:   %[[ADD0:.*]] = "tf.AddV2"(%[[A1]], %[[A0]])
// CHECK-NEXT:   %[[ADD1:.*]] = "tf.AddV2"(%[[ADD0]], %[[A2]])
// CHECK-NEXT:   return %[[ADD1]]


// -----

// Tests that the pass lifts resources on two stateful partitioned call ops
// sharing the same callee. The lifting should clone the callee then modify the
// clone.

// CHECK-LABEL: @cluster_with_stateful_partitioned_call
func @cluster_with_stateful_partitioned_call() -> () {
  // CHECK: %[[VH0:.*]] = "tf.VarHandleOp"()
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[VH1:.*]] = "tf.VarHandleOp"()
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  // CHECK: %[[CONST:.*]] = "tf.Const"()
  %2 = "tf.Const"() {value = dense<10.0> : tensor<f32>} : () -> tensor<f32>
  // CHECK-DAG: %[[READ0:.*]] = "tf.ReadVariableOp"(%[[VH0]])
  // CHECK-DAG: %[[READ1:.*]] = "tf.ReadVariableOp"(%[[VH1]])
  // CHECK: %[[CLUSTER:.*]] = "tf_device.cluster"()
  "tf_device.cluster"() ( {
    // CHECK: %[[PC0:.*]] = "tf.StatefulPartitionedCall"(%[[READ0]], %[[READ1]], %[[CONST]])
    // CHECK-SAME: f = @callee_resource_lifted
    %3 = "tf.StatefulPartitionedCall"(%0, %1, %2) {f = @callee, config = "", config_proto = "", executor_type = ""}
      : (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>>
    // CHECK: %[[PC1:.*]] = "tf.StatefulPartitionedCall"(%[[PC0]], %[[READ1]], %[[CONST]])
    // CHECK-SAME: f = @callee_resource_lifted
    %4 = "tf.StatefulPartitionedCall"(%3, %1, %2) {f = @callee, config = "", config_proto = "", executor_type = ""}
      : (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>>
    // CHECK: tf_device.return %[[PC1]] : tensor<f32>
    tf_device.return
    // CHECK: {cluster_attr = "cluster_attr"} : () -> tensor<f32>
  }) {cluster_attr = "cluster_attr"} : () -> ()
  // CHECK: "tf.AssignVariableOp"(%[[VH0]], %[[CLUSTER]])
  return
}
// CHECK: @callee(%[[OA0:.*]]: tensor<*x!tf.resource<tensor<f32>>>, %[[OA1:.*]]: tensor<*x!tf.resource<tensor<f32>>>, %[[OA2:.*]]: tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>>
func @callee(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>, %arg2: tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>> {
  // CHECK: "tf.ReadVariableOp"(%[[OA1]])
  %0 = "tf.ReadVariableOp"(%arg1) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<f32>
  %1 = "tf.AddV2"(%0, %arg2) : (tensor<f32>, tensor<f32>) -> tensor<f32>
  "tf.AssignVariableOp"(%arg0, %1) {dtype = i32} : (tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> ()
  return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
}
// CHECK: func @callee_resource_lifted(%[[A0:.*]]: tensor<f32>, %[[A1:.*]]: tensor<f32>, %[[A2:.*]]: tensor<f32>) -> tensor<f32>
// CHECK-NEXT:   %[[ADD:.*]] = "tf.AddV2"(%[[A1]], %[[A2]])
// CHECK-NEXT:   return %[[ADD]]


// -----

// Tests that the pass reports error on called function that has resource output
// which doesn't alias an input.

func @cluster_with_stateful_partitioned_call() -> () {
  %0 = "tf.VarHandleOp"() {container = "c", shared_name = "v"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  %1 = "tf.VarHandleOp"() {container = "c", shared_name = "v2"} : () -> tensor<*x!tf.resource<tensor<f32>>>
  %2 = "tf.Const"() {value = dense<10.0> : tensor<f32>} : () -> tensor<f32>
  "tf_device.cluster"() ( {
    %3 = "tf.StatefulPartitionedCall"(%0, %1, %2) {f = @callee, config = "", config_proto = "", executor_type = ""}
      : (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>>
    %4 = "tf.StatefulPartitionedCall"(%3, %1, %2) {f = @callee, config = "", config_proto = "", executor_type = ""}
      : (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>, tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>>
    tf_device.return
  }) {cluster_attr = "cluster_attr"} : () -> ()
  return
}
// expected-error @+1 {{unsupported function call: resource return value does not alias an input.}}
func @callee(%arg0: tensor<*x!tf.resource<tensor<f32>>>, %arg1: tensor<*x!tf.resource<tensor<f32>>>, %arg2: tensor<f32>) -> tensor<*x!tf.resource<tensor<f32>>> {
  %0 = "tf._Unknown_"() : () -> tensor<*x!tf.resource<tensor<f32>>>
  return %0 : tensor<*x!tf.resource<tensor<f32>>>
}
