// pass v-0034: The access decoration appears on a type used as the store type of variable
// 'particles', which is in 'storage' storage class.

[[block]]
struct Particles {
  particles : [[stride(16)]] array<f32, 4>;
};

[[group(1), binding(0)]] var<storage, read_write> particles : Particles;

[[stage(vertex)]]
fn main() -> [[builtin(position)]] vec4<f32> {
  return vec4<f32>();
}
