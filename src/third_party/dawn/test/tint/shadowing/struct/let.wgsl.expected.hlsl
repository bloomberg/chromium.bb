[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

struct a {
  int a;
};

void f() {
  {
    const a a_1 = (a)0;
    const a b = a_1;
  }
  const a a_2 = (a)0;
  const a b = a_2;
}
