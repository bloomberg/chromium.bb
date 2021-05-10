// v-0033: variable 'u' store type is 'u32' however the initializer type is 'f32'.

var<out> f : u32  = 0.0;

[[stage(vertex)]]
fn main() -> void {
}
