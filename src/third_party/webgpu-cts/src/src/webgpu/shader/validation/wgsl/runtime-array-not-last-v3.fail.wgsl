// v-0030 - This fails because the runtime array must not be used as a store type.

type RTArr = [[stride (16)]] array<vec4<f32>>;

[[stage(fragment)]]
fn main() {
}
