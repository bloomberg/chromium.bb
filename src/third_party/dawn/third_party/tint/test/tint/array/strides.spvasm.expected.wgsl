struct strided_arr {
  @size(8)
  el : f32;
}

type Arr = array<strided_arr, 2u>;

type Arr_1 = array<Arr, 3u>;

struct strided_arr_1 {
  @size(128)
  el : Arr_1;
}

type Arr_2 = array<strided_arr_1, 4u>;

struct S {
  a : Arr_2;
}

@group(0) @binding(0) var<storage, read_write> s : S;

fn f_1() {
  let x_19 : Arr_2 = s.a;
  let x_24 : Arr_1 = s.a[3].el;
  let x_28 : Arr = s.a[3].el[2];
  let x_32 : f32 = s.a[3].el[2][1].el;
  s.a = array<strided_arr_1, 4u>();
  s.a[3].el[2][1].el = 5.0;
  return;
}

@stage(compute) @workgroup_size(1, 1, 1)
fn f() {
  f_1();
}
