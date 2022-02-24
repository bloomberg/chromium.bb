#version 310 es

struct strided_arr {
  vec2 el;
};

struct SSBO {
  strided_arr m[2];
};

layout(binding = 0, std430) buffer SSBO_1 {
  strided_arr m[2];
} ssbo;
mat2 arr_to_mat2x2_stride_16(strided_arr arr[2]) {
  return mat2(arr[0u].el, arr[1u].el);
}

strided_arr[2] mat2x2_stride_16_to_arr(mat2 mat) {
  strided_arr tint_symbol = strided_arr(mat[0u]);
  strided_arr tint_symbol_1 = strided_arr(mat[1u]);
  strided_arr tint_symbol_2[2] = strided_arr[2](tint_symbol, tint_symbol_1);
  return tint_symbol_2;
}

void f_1() {
  mat2 x_15 = arr_to_mat2x2_stride_16(ssbo.m);
  ssbo.m = mat2x2_stride_16_to_arr(x_15);
  return;
}

void f() {
  f_1();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
