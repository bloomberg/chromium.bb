builtin.func @test(%V__1 : tensor<i1> { python_test_attrs.static_type = tensor<i1> }) -> tensor<?x?x?x?xi1> {
  %0 = "tf.Const"() { value = dense<[1, 44, 1, 1]> : tensor<4xi32> } : () -> tensor<4xi32>
  %1 = "tf.BroadcastTo"(%V__1, %0) : (tensor<i1>, tensor<4xi32>) -> tensor<?x?x?x?xi1>
  %2 = "tf.ZerosLike"(%1) : (tensor<?x?x?x?xi1>) -> tensor<?x?x?x?xi1>
  %dims3 = "tf.Const"() { value = dense<[1]> : tensor<1xi32> } : () -> tensor<1xi32>
  %3 = "tf.Any"(%2, %dims3) { keep_dims = true } : (tensor<?x?x?x?xi1>, tensor<1xi32>) -> tensor<?x?x?x?xi1>
  return %3 : tensor<?x?x?x?xi1>
}
