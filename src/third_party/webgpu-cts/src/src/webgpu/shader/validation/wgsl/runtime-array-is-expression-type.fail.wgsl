// v-0031: in 'y = x', x is a runtime array and it's used as an expression.

type RTArr = [[stride (16)]] array<i32>;
struct S{
  [[offset(0)]] data : RTArr;
};

[[stage(vertex)]]
fn main() {
  var <storage> x : S;
  var y : array<i32,2>;
  y = x;
}
