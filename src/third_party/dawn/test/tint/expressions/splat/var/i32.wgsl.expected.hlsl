[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  int v = (1 + 2);
  int2 v2 = int2((v).xx);
  int3 v3 = int3((v).xxx);
  int4 v4 = int4((v).xxxx);
}
