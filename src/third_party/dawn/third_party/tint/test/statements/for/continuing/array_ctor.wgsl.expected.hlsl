[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  int i = 0;
  [loop] while (true) {
    if (!(false)) {
      break;
    }
    {
      const int tint_symbol[1] = {1};
      i = (i + tint_symbol[0]);
    }
  }
}
