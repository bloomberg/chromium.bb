// v-0033: variable 'a' store type is 'i32' however the initializer type is 'f32'.

var<out> a : i32  = 123.0;

[[stage(vertex)]]
fn main() -> void {
}
