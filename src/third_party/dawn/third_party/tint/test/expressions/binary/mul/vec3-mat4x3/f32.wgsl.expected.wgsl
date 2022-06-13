[[block]]
struct S {
  matrix : mat4x3<f32>;
  vector : vec3<f32>;
};

[[group(0), binding(0)]] var<uniform> data : S;

[[stage(fragment)]]
fn main() {
  let x = (data.vector * data.matrix);
}
