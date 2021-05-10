// v-0033: variable 'flag' store type is 'bool' however the initializer type is 'u32'.

var<out> flag : bool  = 1u;

[[stage(vertex)]]
fn main() -> void {
}
