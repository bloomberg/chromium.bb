# v-0011 - This fails because of the duplicated `a` variable.

const a : vec2<f32> = vec2<f32>(0.1, 1.0);
[[location 0]] var<in> a : vec4<f32>;

fn main() -> void {
  return;
}
entry_point vertex = main;

