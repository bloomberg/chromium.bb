int f(int a, int b, int c) {
  return ((a * b) + c);
}

[numthreads(1, 1, 1)]
void main() {
  const int tint_symbol = f(1, 2, 3);
  const int tint_symbol_1 = f(4, 5, 6);
  const int tint_symbol_2 = f(8, 9, 10);
  const int tint_symbol_3 = f(7, tint_symbol_2, 11);
  return;
}
