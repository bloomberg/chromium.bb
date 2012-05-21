#include <stdio.h>
#include <pthread.h>

void* func2(void* arg) {
  *((int*)arg) = 2;
  printf("func2\n");
  return NULL;
}

void* func(void* arg) {
  int x = 0;
  pthread_t thread;
  pthread_create(&thread, NULL, func2, (void*)&x);
  pthread_join(thread, NULL);
  printf("after join in func\n");
  pthread_exit((void*)x);
  printf("not reached\n");
  return NULL;
}

void before_exit() {
}

int main() {
  pthread_t thread;
  int result;
  pthread_create(&thread, NULL, func, NULL);
  pthread_join(thread, (void**)&result);
  printf("after join\n");
  before_exit();
  return result;
}
