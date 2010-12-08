#include <string.h>

extern const char* hello();
extern int forty_two();

int main(int argc, char** argv) {
  return (strcmp(hello(), "Hello, world.\n") == 0) && (forty_two() == 42);
}
