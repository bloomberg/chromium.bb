builtin.func @test(%V__0 : tensor<?x?x?x?xf32> { python_test_attrs.static_type = tensor<1x93x54x1xf32> }, %V__1 : tensor<4xi32> { python_test_attrs.shape_value = dense<[1, 93, 54, 27]> : tensor<4xi32> }) -> tensor<?x?x?xf32> {
  %1 = "tf.BroadcastTo"(%V__0, %V__1) : (tensor<?x?x?x?xf32>, tensor<4xi32>) -> tensor<?x?x?x?xf32>
  %2 = "tf.Squeeze"(%1) { squeeze_dims = [ 0 : i64 ] } : (tensor<?x?x?x?xf32>) -> tensor<?x?x?xf32>
  return %2 : tensor<?x?x?xf32>
}
