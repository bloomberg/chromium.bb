// v-0035: The access decoration is required for 'particles'.

[[block]]
struct Particles {
  [[offset(0)]] particles : [[stride(16)]] array<f32, 4>;
};

[[group(0), binding(1)]] var<storage> particles : Particles;

[[stage(vertex)]]
fn main() {
}
