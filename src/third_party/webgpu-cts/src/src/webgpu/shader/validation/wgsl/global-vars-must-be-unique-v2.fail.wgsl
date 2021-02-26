# v-0011 - This fails because of the duplicate name `a`.

const a : vec2<f32> = vec2<f32>(0.1, 1.0);

fn a()->void {
  return;
}

[[stage(vertex)]]
fn main() -> void {
  return;
}
