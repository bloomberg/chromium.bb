typedef struct S1 {
  int x;
} T1;

int parse_decl_node_2() { int arr[3]; }

int parse_decl_node_3() { int *a; }

int parse_decl_node_4() { T1 t1[3]; }

int parse_decl_node_5() { T1 *t2[3]; }

int parse_decl_node_6() { T1 t3[3][3]; }

int main() {
  int a;
  T1 t1;
  struct S1 s1;
  T1 *t2;
}
