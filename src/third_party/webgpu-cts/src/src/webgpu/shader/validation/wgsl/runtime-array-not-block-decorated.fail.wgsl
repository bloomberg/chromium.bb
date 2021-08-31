// v-0031: struct 'S' has runtime-sized member but it's storage class is not 'storage'.

type RTArr = [[stride (16)]] array<vec4<f32>>;
struct S{
  [[offset(0)]] data : RTArr;
};

[[stage(vertex)]]
fn main() {
  var <storage> x : S;
}
