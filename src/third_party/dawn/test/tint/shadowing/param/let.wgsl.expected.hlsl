[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f(int a) {
  {
    const int b = a;
  }
}
