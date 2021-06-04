// v-0015 - This fails because of the aliased runtime array is not last member of the struct.

type RTArr = [[stride (16)]] array<vec4<f32>>;
[[block]]
struct S {
  [[offset(0)]] data : RTArr;
  [[offset(4)]] b : f32;
};

[[stage(vertex)]]
fn main() {
}
