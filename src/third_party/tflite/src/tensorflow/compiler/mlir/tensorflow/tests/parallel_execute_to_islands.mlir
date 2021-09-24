// RUN: tf-opt %s -tf-parallel-execute-to-islands | FILECHECK_OPTS="" FileCheck %s

// CHECK-LABEL: func @testEmptyRegions
func @testEmptyRegions() {
  tf_executor.graph {
    tf_executor.island() {
      "tf_device.parallel_execute"() ({
        tf_device.return
      }, {
        tf_device.return
      }) {} : () -> ()
      tf_executor.yield
    }
    tf_executor.fetch
  }
  return
}

// CHECK:      [[ISLAND_0_CTRL:%.+]] = tf_executor.island {
// CHECK:        tf_executor.yield
// CHECK:      [[ISLAND_1_CTRL:%.+]] = tf_executor.island {
// CHECK:        tf_executor.yield
// CHECK:      tf_executor.fetch [[ISLAND_0_CTRL]], [[ISLAND_1_CTRL]] :


// CHECK-LABEL: func @testDataOperandsAndResults
// CHECK-SAME: ([[ARG_0:%.+]]: tensor<i1>)
func @testDataOperandsAndResults(%arg0 : tensor<i1>) {
  %0:2 = tf_executor.graph {
    %1:2 = tf_executor.island {
      %2 = "tf.opA"(%arg0) : (tensor<i1>) -> tensor<i1>
      tf_executor.yield %2 : tensor<i1>
    }
    %3:3 = tf_executor.island() {
      %4:2 = "tf_device.parallel_execute"() ({
        %5 = "tf.opB"(%1#0) : (tensor<i1>) -> tensor<i1>
        tf_device.return %5 : tensor<i1>
      }, {
        %5 = "tf.opC"(%1#0) : (tensor<i1>) -> tensor<i32>
        tf_device.return %5 : tensor<i32>
      }) {} : () -> (tensor<i1>, tensor<i32>)
      tf_executor.yield %4#0, %4#1 : tensor<i1>, tensor<i32>
    }
    tf_executor.fetch %3#0, %3#1 : tensor<i1>, tensor<i32>
  }
  return
}

// CHECK:      [[INPUT_A:%.+]], {{%.+}} = tf_executor.island {
// CHECK-NEXT:   [[OP_A_OUTPUT:%.+]] = "tf.opA"([[ARG_0]])
// CHECK-NEXT:   tf_executor.yield [[OP_A_OUTPUT]] :
// CHECK:      [[ISLAND_0_OUTPUT:%.+]], {{%.+}} = tf_executor.island {
// CHECK-NEXT:   [[OP_B_OUTPUT:%.+]] = "tf.opB"([[INPUT_A]])
// CHECK:        tf_executor.yield [[OP_B_OUTPUT]] :
// CHECK:      [[ISLAND_1_OUTPUT:%.+]], {{%.+}} = tf_executor.island {
// CHECK-NEXT:   [[OP_C_OUTPUT:%.+]] = "tf.opC"([[INPUT_A]])
// CHECK:        tf_executor.yield [[OP_C_OUTPUT]] :
// CHECK:      tf_executor.fetch [[ISLAND_0_OUTPUT]], [[ISLAND_1_OUTPUT]] :


// CHECK-LABEL: func @testControlOperands
func @testControlOperands() {
  %0:2 = tf_executor.graph {
    %1 = tf_executor.island {
      tf_executor.yield
    }
    %2:3 = tf_executor.island(%1) {
      %3:2 = "tf_device.parallel_execute"() ({
        %4 = "tf.opA"() : () -> tensor<i1>
        tf_device.return %4 : tensor<i1>
      }, {
        %4 = "tf.opB"() : () -> tensor<i32>
        tf_device.return %4 : tensor<i32>
      }) {} : () -> (tensor<i1>, tensor<i32>)
      tf_executor.yield %3#0, %3#1 : tensor<i1>, tensor<i32>
    }
    tf_executor.fetch %2#0, %2#1 : tensor<i1>, tensor<i32>
  }
  return
}

// CHECK:      [[INPUT_CTRL:%.+]] = tf_executor.island {
// CHECK:      [[ISLAND_0_OUTPUT:%.+]], {{%.+}} = tf_executor.island([[INPUT_CTRL]]) {
// CHECK-NEXT:   [[OP_A_OUTPUT:%.+]] = "tf.opA"()
// CHECK:        tf_executor.yield [[OP_A_OUTPUT]] :
// CHECK:      [[ISLAND_1_OUTPUT:%.+]], {{%.+}} = tf_executor.island([[INPUT_CTRL]]) {
// CHECK-NEXT:   [[OP_B_OUTPUT:%.+]] = "tf.opB"()
// CHECK:        tf_executor.yield [[OP_B_OUTPUT]] :
// CHECK:      tf_executor.fetch [[ISLAND_0_OUTPUT]], [[ISLAND_1_OUTPUT]] :


// CHECK-LABEL: func @testControlResults
func @testControlResults() {
  tf_executor.graph {
    %0:3 = tf_executor.island {
      %1:2 = "tf_device.parallel_execute"() ({
        %2 = "tf.opA"() : () -> tensor<i1>
        tf_device.return %2 : tensor<i1>
      }, {
        %2 = "tf.opB"() : () -> tensor<i32>
        tf_device.return %2 : tensor<i32>
      }) {} : () -> (tensor<i1>, tensor<i32>)
      tf_executor.yield %1#0, %1#1 : tensor<i1>, tensor<i32>
    }
    %3 = tf_executor.island(%0#2) {
      tf_executor.yield
    }
    tf_executor.fetch %3 : !tf_executor.control
  }
  return
}

// CHECK:      {{%.+}}, [[ISLAND_0_CTRL:%.+]] = tf_executor.island {
// CHECK-NEXT:   [[OP_A_OUTPUT:%.+]] = "tf.opA"()
// CHECK:        tf_executor.yield [[OP_A_OUTPUT]] :
// CHECK:      {{%.+}}, [[ISLAND_1_CTRL:%.+]] = tf_executor.island {
// CHECK-NEXT:   [[OP_B_OUTPUT:%.+]] = "tf.opB"()
// CHECK:        tf_executor.yield [[OP_B_OUTPUT]] :
// CHECK:      [[OUTPUT_CTRL:%.+]] = tf_executor.island([[ISLAND_0_CTRL]], [[ISLAND_1_CTRL]]) {
// CHECK:      [[FETCH_ISLAND:%.+]] = tf_executor.island([[OUTPUT_CTRL]]) {
// CHECK:      tf_executor.fetch [[FETCH_ISLAND]] : !tf_executor.control


// CHECK-LABEL: func @testSomeRegionNoUsers
func @testSomeRegionNoUsers() {
  %0 = tf_executor.graph {
    %1:3 = tf_executor.island {
      %2:2 = "tf_device.parallel_execute"() ({
        %3 = "tf.opA"() : () -> tensor<i1>
        tf_device.return %3 : tensor<i1>
      }, {
        %3 = "tf.opB"() : () -> tensor<i32>
        tf_device.return %3 : tensor<i32>
      }) {} : () -> (tensor<i1>, tensor<i32>)
      tf_executor.yield %2#0, %2#1 : tensor<i1>, tensor<i32>
    }
    tf_executor.fetch %1#0 : tensor<i1>
  }
  return
}

// CHECK:      [[ISLAND_0_OUTPUT:%.+]], {{%.+}} = tf_executor.island {
// CHECK-NEXT:   [[OP_A_OUTPUT:%.+]] = "tf.opA"()
// CHECK:        tf_executor.yield [[OP_A_OUTPUT]] :
// CHECK:      {{%.+}}, [[ISLAND_1_CTRL:%.+]] = tf_executor.island {
// CHECK-NEXT:   [[OP_B_OUTPUT:%.+]] = "tf.opB"()
// CHECK:        tf_executor.yield [[OP_B_OUTPUT]] :
// CHECK:      tf_executor.fetch [[ISLAND_0_OUTPUT]], [[ISLAND_1_CTRL]] :
