#include <pthread.h>

int main(void) {
  pthread_exit(NULL);
  /* Should never get here */
  return 1;
}
