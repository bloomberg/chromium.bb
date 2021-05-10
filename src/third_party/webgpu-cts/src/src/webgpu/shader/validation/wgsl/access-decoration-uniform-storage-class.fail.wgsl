// v-0034: The access decoration appears on a type used as the store type of variable
// 'particles', which is in the 'uniform' storage class.

[[block]]
struct Particles {
  [[offset(0)]] particles : [[stride(16)]] array<f32, 4>;
};

[[group(0), binding(0)]] var<uniform> particles : [[access(read_write)]] Particles;

[[stage(vertex)]]
fn main() -> void {
}
