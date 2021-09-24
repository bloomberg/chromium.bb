// RUN: lhlo-tfrt-opt %s              \
// RUN:   -lmhlo-gpu-async-conversion \
// RUN:   -gpu-async-region           \
// RUN:   -async-gpu-tfrt-conversion  \
// RUN: | FileCheck %s

// CHECK:      func @gemm(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @gemm(%lhs: memref<5x4xf32>, %rhs: memref<4x5xf32>, %output:memref<100xi8>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute
  // CHECK-NOT: memref.view

  %c0 = constant 0 : index
  %view = memref.view %output[%c0][] : memref<100xi8> to memref<5x5xf32>

  // CHECK: [[M:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[N:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[K:%[0-9]+]] = tfrt.constant.i32 4
  // CHECK: [[ALPHA:%[0-9]+]] = tfrt.constant.f32 5.000000e-01
  // CHECK: [[LDA:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[LDB:%[0-9]+]] = tfrt.constant.i32 4
  // CHECK: [[BETA:%[0-9]+]] = tfrt.constant.f32 0.000000e+00
  // CHECK: [[LDC:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[ALGO:%[0-9]+]] = tfrt_gpu.blas.gemm.algo CUBLAS_GEMM_DEFAULT
  // CHECK: [[HANDLE:%[0-9]+]] = tfrt_gpu.blas.create %arg1

  // CHECK: [[CHAIN:%[0-9]+]] = tfrt_gpu.blas.gemm [[HANDLE]],
  // CHECK-SAME: CUBLAS_OP_N, CUBLAS_OP_N, [[M]], [[N]], [[K]], [[ALPHA]],
  // CHECK-SAME: %arg3, CUDA_R_32F, [[LDA]],
  // CHECK-SAME: %arg2, CUDA_R_32F, [[LDB]], [[BETA]],
  // CHECK-SAME: %arg4, CUDA_R_32F, [[LDC]],
  // CHECK-SAME: CUDA_R_32F, [[ALGO]], %arg0

  "lmhlo_gpu.gemm"(%lhs, %rhs, %view) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 0.5,
       alpha_imag = 0.0,
       batch_size = 1,
       lhs_stride = 20,
       rhs_stride = 20}
    : (memref<5x4xf32>, memref<4x5xf32>, memref<5x5xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @gemm_batch(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @gemm_batch(%lhs: memref<5x4xf32>, %rhs: memref<4x5xf32>, %output:memref<5x5xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[M:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[N:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[K:%[0-9]+]] = tfrt.constant.i32 4
  // CHECK: [[ALPHA:%[0-9]+]] = tfrt.constant.f32 5.000000e-01
  // CHECK: [[LDA:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[LDB:%[0-9]+]] = tfrt.constant.i32 4
  // CHECK: [[BETA:%[0-9]+]] = tfrt.constant.f32 0.000000e+00
  // CHECK: [[LDC:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[ALGO:%[0-9]+]] = tfrt_gpu.blas.gemm.algo CUBLAS_GEMM_DEFAULT
  // CHECK: [[HANDLE:%[0-9]+]] = tfrt_gpu.blas.create %arg1
  // CHECK: [[STRIDEA:%[0-9]+]] = tfrt.constant.i64 20
  // CHECK: [[STRIDEB:%[0-9]+]] = tfrt.constant.i64 20
  // CHECK: [[STRIDEC:%[0-9]+]] = tfrt.constant.i64 25
  // CHECK: [[BATCH:%[0-9]+]] = tfrt.constant.i32 42

  // CHECK: [[CHAIN:%[0-9]+]] = tfrt_gpu.blas.gemm.batch [[HANDLE]],
  // CHECK-SAME: CUBLAS_OP_N, CUBLAS_OP_N, [[M]], [[N]], [[K]], [[ALPHA]],
  // CHECK-SAME: %arg3, CUDA_R_32F, [[LDA]], [[STRIDEA]],
  // CHECK-SAME: %arg2, CUDA_R_32F, [[LDB]], [[STRIDEB]], [[BETA]],
  // CHECK-SAME: %arg4, CUDA_R_32F, [[LDC]], [[STRIDEC]], [[BATCH]],
  // CHECK-SAME: CUDA_R_32F, [[ALGO]], %arg0

  "lmhlo_gpu.gemm"(%lhs, %rhs, %output) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 0.5,
       alpha_imag = 0.0,
       batch_size = 42,
       lhs_stride = 20,
       rhs_stride = 20}
    : (memref<5x4xf32>, memref<4x5xf32>, memref<5x5xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @gemm_bias(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg5: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @gemm_bias(%lhs: memref<5x4xf32>, %rhs: memref<4x5xf32>,
                %bias: memref<5x5xf32>, %output:memref<5x5xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[M:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[N:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[K:%[0-9]+]] = tfrt.constant.i32 4
  // CHECK: [[ALPHA:%[0-9]+]] = tfrt.constant.f32 5.000000e-01
  // CHECK: [[LDA:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[LDB:%[0-9]+]] = tfrt.constant.i32 4
  // CHECK: [[BETA:%[0-9]+]] = tfrt.constant.f32 1.000000e+00
  // CHECK: [[LDC:%[0-9]+]] = tfrt.constant.i32 5
  // CHECK: [[ALGO:%[0-9]+]] = tfrt_gpu.blas.gemm.algo CUBLAS_GEMM_DEFAULT
  // CHECK: [[HANDLE:%[0-9]+]] = tfrt_gpu.blas.create %arg1

  // CHECK: [[CHAIN:%[0-9]+]] = tfrt_gpu.blas.gemm [[HANDLE]],
  // CHECK-SAME: CUBLAS_OP_N, CUBLAS_OP_N, [[M]], [[N]], [[K]], [[ALPHA]],
  // CHECK-SAME: %arg3, CUDA_R_32F, [[LDA]],
  // CHECK-SAME: %arg2, CUDA_R_32F, [[LDB]], [[BETA]],
  // CHECK-SAME: %arg5, CUDA_R_32F, [[LDC]],
  // CHECK-SAME: CUDA_R_32F, [[ALGO]], %arg0

  "lmhlo_gpu.gemm_bias"(%lhs, %rhs, %bias, %output) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 0.5,
       alpha_imag = 0.0,
       beta = 1.0,
       batch_size = 1,
       lhs_stride = 20,
       rhs_stride = 20}
    : (memref<5x4xf32>, memref<4x5xf32>, memref<5x5xf32>, memref<5x5xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @all_gather(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg5: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @all_gather(%operand0: memref<2x2xf32>, %operand1: memref<2x2xf32>, %result0: memref<2x2xf32>, %result1: memref<2x2xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[CONTEXT:%[0-9]+]] = tfrt_gpu.stream.get_context %arg1
  // CHECK: [[HANDLE:%[0-9]+]] = xlir.ccl.create [[CONTEXT]]
  // CHECK: [[CHAIN1:%[0-9]+]] = tfrt_gpu.ccl.all_gather [[HANDLE]],
  // CHECK-SAME: %arg2, %arg4, ncclFloat32, %arg0
  // CHECK: [[CHAIN2:%[0-9]+]] = tfrt_gpu.ccl.all_gather [[HANDLE]],
  // CHECK-SAME: %arg3, %arg5, ncclFloat32, [[CHAIN1]]
  // CHECK: [[CHAIN3:%[0-9]+]] = tfrt_gpu.ccl.execute %arg1, [[HANDLE]],
  // CHECK-SAME: [[CHAIN2]]

  "lmhlo.all_gather"(%operand0, %operand1, %result0, %result1) {
      all_gather_dimension = 0 : i64,
      replica_groups = dense<0> : tensor<1x1xi64>,
      constrain_layout = false,
      use_global_device_ids = false
  } : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN3]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @all_reduce(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg5: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @all_reduce(%operand0: memref<2x2xf32>, %operand1: memref<2x2xf32>, %result0: memref<2x2xf32>, %result1: memref<2x2xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[CONTEXT:%[0-9]+]] = tfrt_gpu.stream.get_context %arg1
  // CHECK: [[HANDLE:%[0-9]+]] = xlir.ccl.create [[CONTEXT]]
  // CHECK: [[CHAIN1:%[0-9]+]] = tfrt_gpu.ccl.all_reduce [[HANDLE]],
  // CHECK-SAME: %arg2, %arg4, ncclFloat32, ncclSum, %arg0
  // CHECK: [[CHAIN2:%[0-9]+]] = tfrt_gpu.ccl.all_reduce [[HANDLE]],
  // CHECK-SAME: %arg3, %arg5, ncclFloat32, ncclSum, [[CHAIN1]]
  // CHECK: [[CHAIN3:%[0-9]+]] = tfrt_gpu.ccl.execute %arg1, [[HANDLE]],
  // CHECK-SAME: [[CHAIN2]]

  "lmhlo.all_reduce"(%operand0, %operand1, %result0, %result1) ( {
      ^bb0(%lhs: tensor<f32>, %rhs: tensor<f32>):
          %0 = mhlo.add %lhs, %rhs : tensor<f32>
          "mhlo.return"(%0) : (tensor<f32>) -> ()
      }) {
          replica_groups = dense<[[0, 2, 4, 6], [1, 3, 5, 7]]> : tensor<2x4xi64>,
          channel_id = { handle = 5 : i64, type = 2 : i64 },
          constrain_layout = true,
          use_global_device_ids = true
      } : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN3]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @reduce_scatter(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg5: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @reduce_scatter(%operand0: memref<2x2xf32>, %operand1: memref<2x2xf32>, %result0: memref<2x2xf32>, %result1: memref<2x2xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[CONTEXT:%[0-9]+]] = tfrt_gpu.stream.get_context %arg1
  // CHECK: [[HANDLE:%[0-9]+]] = xlir.ccl.create [[CONTEXT]]
  // CHECK: [[CHAIN1:%[0-9]+]] = tfrt_gpu.ccl.reduce_scatter [[HANDLE]],
  // CHECK-SAME: %arg2, %arg4, ncclFloat32, ncclSum, %arg0
  // CHECK: [[CHAIN2:%[0-9]+]] = tfrt_gpu.ccl.reduce_scatter [[HANDLE]],
  // CHECK-SAME: %arg3, %arg5, ncclFloat32, ncclSum, [[CHAIN1]]
  // CHECK: [[CHAIN3:%[0-9]+]] = tfrt_gpu.ccl.execute %arg1, [[HANDLE]],
  // CHECK-SAME: [[CHAIN2]]

  "lmhlo.reduce_scatter"(%operand0, %operand1, %result0, %result1) ( {
      ^bb0(%lhs: tensor<f32>, %rhs: tensor<f32>):
          %0 = mhlo.add %lhs, %rhs : tensor<f32>
          "mhlo.return"(%0) : (tensor<f32>) -> ()
      }) {
          replica_groups = dense<[[0, 1, 2, 3]]> : tensor<1x4xi64>,
          scatter_dimension = 1 : i64
      } : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN3]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @all_to_all(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg5: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @all_to_all(%operand0: memref<2x2xf32>, %operand1: memref<2x2xf32>, %result0: memref<2x2xf32>, %result1: memref<2x2xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[CONTEXT:%[0-9]+]] = tfrt_gpu.stream.get_context %arg1
  // CHECK: [[HANDLE:%[0-9]+]] = xlir.ccl.create [[CONTEXT]]
  // CHECK: [[PEER0:%[0-9]+]] = tfrt.constant.i32 0
  // CHECK: [[CHAIN1:%[0-9]+]] = tfrt_gpu.ccl.send [[HANDLE]],
  // CHECK-SAME: %arg2, [[PEER0]], ncclFloat32, %arg0
  // CHECK: [[CHAIN2:%[0-9]+]] = tfrt_gpu.ccl.recv [[HANDLE]],
  // CHECK-SAME: %arg4, [[PEER0]], ncclFloat32, [[CHAIN1]]
  // CHECK: [[PEER1:%[0-9]+]] = tfrt.constant.i32 1
  // CHECK: [[CHAIN3:%[0-9]+]] = tfrt_gpu.ccl.send [[HANDLE]],
  // CHECK-SAME: %arg3, [[PEER1]], ncclFloat32, [[CHAIN2]]
  // CHECK: [[CHAIN4:%[0-9]+]] = tfrt_gpu.ccl.recv [[HANDLE]],
  // CHECK-SAME: %arg5, [[PEER1]], ncclFloat32, [[CHAIN3]]
  // CHECK: [[CHAIN5:%[0-9]+]] = tfrt_gpu.ccl.execute %arg1, [[HANDLE]],
  // CHECK-SAME: [[CHAIN4]]

  "lmhlo.all_to_all"(%operand0, %operand1, %result0, %result1) {
      replica_groups = dense<[[0, 1, 2, 3], [4, 5, 6, 7]]> : tensor<2x4xi64>,
      channel_id = {handle = 1 : i64, type = 0 : i64},
      constrain_layout = false,
      use_global_device_ids = false
  } : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN5]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @all_to_all_split_dimension(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg4: !tfrt_gpu.buffer,
// CHECK-SAME:   %arg5: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @all_to_all_split_dimension(%operand0: memref<2x2xf32>, %operand1: memref<2x2xf32>, %result0: memref<2x2xf32>, %result1: memref<2x2xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: [[CONTEXT:%[0-9]+]] = tfrt_gpu.stream.get_context %arg1
  // CHECK: [[HANDLE:%[0-9]+]] = xlir.ccl.create [[CONTEXT]]
  // CHECK: [[CHAIN1:%[0-9]+]] = tfrt_gpu.ccl.all_to_all [[HANDLE]],
  // CHECK-SAME: %arg2, %arg4, ncclFloat32, %arg0
  // CHECK: [[CHAIN2:%[0-9]+]] = tfrt_gpu.ccl.all_to_all [[HANDLE]],
  // CHECK-SAME: %arg3, %arg5, ncclFloat32, [[CHAIN1]]
  // CHECK: [[CHAIN3:%[0-9]+]] = tfrt_gpu.ccl.execute %arg1, [[HANDLE]],
  // CHECK-SAME: [[CHAIN2]]

  "lmhlo.all_to_all"(%operand0, %operand1, %result0, %result1) {
      replica_groups = dense<[[0, 1, 2, 3], [4, 5, 6, 7]]> : tensor<2x4xi64>,
      channel_id = {handle = 1 : i64, type = 0 : i64},
      constrain_layout = false,
      use_global_device_ids = false,
      split_dimension = 0
  } : (memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>, memref<2x2xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return [[CHAIN3]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @two_ops(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @two_ops(%memref: memref<4x4xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: tfrt.constant.f32 3.14159274
  // CHECK: tfrt_gpu.blas.gemm
  "lmhlo_gpu.gemm"(%memref, %memref, %memref) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 3.14159274,
       alpha_imag = 0.0,
       batch_size = 1,
       lhs_stride = 16,
       rhs_stride = 16}
    : (memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>) -> ()

  // CHECK: tfrt.constant.f32 2.71828175
  // CHECK: tfrt_gpu.blas.gemm
  "lmhlo_gpu.gemm"(%memref, %memref, %memref) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 2.71828175,
       alpha_imag = 0.0,
       batch_size = 1,
       lhs_stride = 16,
       rhs_stride = 16}
    : (memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return {{.*}} : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @async(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @async(%memref: memref<4x4xf32>) {
  // CHECK-NOT: cast

  // CHECK: %[[a0:.*]], %[[t0:.*]] = async.execute
  // CHECK-SAME: -> !async.value<!tfrt_gpu.event>
  %a0 = async.execute () {
    // CHECK: %[[e0:.*]] = tfrt_gpu.event.create
    // CHECK: %[[ch0:.*]] = tfrt_gpu.event.record %[[e0]], %arg1
    // CHECK: %[[s0:.*]] = tfrt_gpu.stream.create
    // CHECK: %[[ch1:.*]] = tfrt_gpu.stream.wait %[[s0]], %[[e0]]
    // CHECK: %[[h0:.*]] = tfrt_gpu.blas.create %[[s0]]
    // CHECK: %[[ch2:.*]] = tfrt_gpu.blas.gemm %[[h0]]
    // CHECK-SAME: %[[ch1]]
    "lmhlo_gpu.gemm"(%memref, %memref, %memref) { dot_dimension_numbers = {
         lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
         rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
         lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
         rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
         alpha_real = 0.5,
         alpha_imag = 0.0,
         batch_size = 1,
         lhs_stride = 16,
         rhs_stride = 16}
      : (memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>) -> ()
    // CHECK: %[[e1:.*]] = tfrt_gpu.event.create
    // CHECK: %[[ch3:.*]] = tfrt_gpu.event.record %[[e1]], %[[s0]], %[[ch2]]
    // CHECK: async.yield %[[e1]] : !tfrt_gpu.event
    async.yield
  }

  // CHECK: %[[a1:.*]], %[[t1:.*]] = async.execute [%[[a0]]]
  // CHECK-SAME: (%[[t0]] as %[[e2:.*]]:
  // CHECK-SAME: !async.value<!tfrt_gpu.event>) -> !async.value<!tfrt_gpu.event>
  %a1 = async.execute [%a0] () {
    // CHECK: %[[s1:.*]] = tfrt_gpu.stream.create
    // CHECK: %[[ch4:.*]] = tfrt_gpu.stream.wait %[[s1]], %[[e2]]
    // CHECK: %[[h1:.*]] = tfrt_gpu.blas.create %[[s1]]
    // CHECK: %[[ch5:.*]] = tfrt_gpu.blas.gemm %[[h1]]
    // CHECK-SAME: %[[ch4]]
    "lmhlo_gpu.gemm"(%memref, %memref, %memref) { dot_dimension_numbers = {
         lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
         rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
         lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
         rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
         alpha_real = 0.5,
         alpha_imag = 0.0,
         batch_size = 1,
         lhs_stride = 16,
         rhs_stride = 16}
      : (memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>) -> ()
    // CHECK: %[[e3:.*]] = tfrt_gpu.event.create
    // CHECK: %[[ch6:.*]] = tfrt_gpu.event.record %[[e3]], %[[s1]], %[[ch5]]
    // CHECK: async.yield %[[e3]] : !tfrt_gpu.event
    async.yield
  }

  // CHECK: async.await %[[a1]] : !async.token
  // CHECK: %[[e4:.*]] = async.await %[[t1]] : !async.value<!tfrt_gpu.event>
  // CHECK: %[[ch7:.*]] = tfrt_gpu.stream.wait %arg1, %[[e4]]
  async.await %a1 : !async.token

  // CHECK: %[[h2:.*]] = tfrt_gpu.blas.create %arg1
  // CHECK: %[[ch8:.*]] = tfrt_gpu.blas.gemm %[[h2]]
  // CHECK-SAME: %[[ch7]]
  "lmhlo_gpu.gemm"(%memref, %memref, %memref) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 0.5,
       alpha_imag = 0.0,
       batch_size = 1,
       lhs_stride = 16,
       rhs_stride = 16}
    : (memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>) -> ()

  // CHECK: tfrt.return %[[ch8]] : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}

// CHECK:      func @return(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer
// CHECK-SAME: ) -> (!tfrt.chain, !tfrt_gpu.buffer)
func @return(%memref: memref<4x4xf32>) -> memref<4x4xf32> {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: tfrt_gpu.blas.gemm
  "lmhlo_gpu.gemm"(%memref, %memref, %memref) { dot_dimension_numbers = {
       lhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       rhs_batching_dimensions = dense<[]> : tensor<0xi64>,
       lhs_contracting_dimensions = dense<[1]> : tensor<1xi64>,
       rhs_contracting_dimensions = dense<[0]> : tensor<1xi64>},
       alpha_real = 1.0,
       alpha_imag = 0.0,
       batch_size = 1,
       lhs_stride = 16,
       rhs_stride = 16}
    : (memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return {{.*}}, %arg2 : !tfrt.chain, !tfrt_gpu.buffer
  return %memref : memref<4x4xf32>
}

// CHECK:      func @memcpy(
// CHECK-SAME:   %arg0: !tfrt.chain,
// CHECK-SAME:   %arg1: !tfrt_gpu.stream,
// CHECK-SAME:   %arg2: !tfrt_gpu.buffer
// CHECK-SAME:   %arg3: !tfrt_gpu.buffer
// CHECK-SAME: ) -> !tfrt.chain
func @memcpy(%dst: memref<4x4xf32>, %src: memref<4x4xf32>) {
  // CHECK-NOT: cast
  // CHECK-NOT: async.execute

  // CHECK: tfrt_gpu.mem.copy
  "gpu.memcpy"(%dst, %src)
    : (memref<4x4xf32>, memref<4x4xf32>) -> ()

  // CHECK-NOT: cast
  // CHECK: tfrt.return {{.*}} : !tfrt.chain
  "lmhlo.terminator"() : () -> ()
}
