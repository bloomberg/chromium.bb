#include <stdio.h>

const char* names[4] = {"_ZN6SkPath4IterC1ERKS_b",
    "_ZN6SkPath4Iter4nextEP7SkPoint",
    "_ZN6SkScan8HairLineERK7SkPointS2_PK8SkRegionP9SkBlitter",
    NULL};

extern char *__cxa_demangle (const char *mangled, char *buf, size_t *len,
                             int *status);

char* demangle_symbol_name(const char*name) {
    return name ? __cxa_demangle(name, 0, 0, 0) : "";
}

int main() {
  int i;
  for (i=0; i<4; ++i) {
    printf ("%s -> %s\n", names[i], demangle_symbol_name(names[i]));
  }
  return 0;
}
