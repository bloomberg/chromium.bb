type ArrayType = array<vec4<i32>, 4>;

struct S {
  arr : ArrayType,
}

var<private> src_private : ArrayType;

var<workgroup> src_workgroup : ArrayType;

@group(0) @binding(0) var<uniform> src_uniform : S;

@group(0) @binding(1) var<storage, read_write> src_storage : S;

fn ret_arr() -> ArrayType {
  return ArrayType();
}

fn ret_struct_arr() -> S {
  return S();
}

fn foo(src_param : ArrayType) {
  var src_function : ArrayType;
  var dst : ArrayType;
  dst = ArrayType(vec4(1), vec4(2), vec4(3), vec4(3));
  dst = src_param;
  dst = ret_arr();
  let src_let : ArrayType = ArrayType();
  dst = src_let;
  dst = src_function;
  dst = src_private;
  dst = src_workgroup;
  dst = ret_struct_arr().arr;
  dst = src_uniform.arr;
  dst = src_storage.arr;
  var dst_nested : array<array<array<i32, 2>, 3>, 4>;
  var src_nested : array<array<array<i32, 2>, 3>, 4>;
  dst_nested = src_nested;
}
