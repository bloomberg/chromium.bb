builtin.func @test(%V__0: tensor<?x?xf32>) -> tensor<?x?xf32> {
  %1 = "tf.AddV2"(%V__0, %V__0) : (tensor<?x?xf32>, tensor<?x?xf32>) -> tensor<?x?xf32>
  %2 = "tf.FloorMod"(%1, %V__0) : (tensor<?x?xf32>, tensor<?x?xf32>) -> tensor<?x?xf32>
  %3 = "tf.AddV2"(%2, %V__0) : (tensor<?x?xf32>, tensor<?x?xf32>) -> tensor<?x?xf32>
  return %3 : tensor<?x?xf32>
}
