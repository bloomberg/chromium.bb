/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <unistd.h>

#define NTHREADS 16

void* test_getpagesize(void* arg) {
  int pagesize = getpagesize();
  assert(pagesize > 0);
  return NULL;
}

void* test_opendir_readdir(void* arg) {
  int count;
  struct dirent* entrance;
  DIR* dir = NULL;
  char dir_name[] = "./";
  dir = opendir(dir_name);
  assert(dir != NULL);
  for (count = 0; NULL != (entrance = readdir(dir)) ; ++count) {
    printf("test_opendir_readdir: %s contains: %s\n", dir_name,
           entrance->d_name);
  }
  assert(count > 0);
  assert(0 == closedir(dir));
  return NULL;
}

void* test_getcwd(void* arg) {
  char buf1[256], buf2[256];
  char* ptr = getcwd(buf1, 256);
  assert(NULL != ptr);
  printf("test_getcwd: pwd = %s\n", buf1);
  assert(NULL != getwd(buf2));
  assert(0 == strncmp(buf1, buf2, strlen(buf1)));
  return NULL;
}

void* test_pathconf(void* arg) {
  char file_name[] = "/root/";
  int mask = pathconf(file_name, _PC_CHOWN_RESTRICTED);
  assert(mask != -1);
  return NULL;
}

char test_file_name[] = "some_impossible_file_name.txt";

void* test_open_read(void* arg) {
  char buf[32];
  int nbytes = 16;
  int fd = open(test_file_name, O_RDONLY);
  int r;
  assert(fd > 0);
  r = read(fd, buf, nbytes);
  assert(-1 != r);
  buf[r] = '\0';
  printf("test_open_read: %d bytes from %s(fd = %d): %s\n", nbytes,
         test_file_name, fd, buf);
  assert(0 == close(fd));
  return NULL;
}

void* test_lseek(void* arg) {
  char file_name[] = "some_impossible_file_name.txt";
  char buf[32];
  int fd = open(file_name, O_RDONLY);
  assert(fd > 0);
  printf("test_lseek: here go even characters from %s:",
         file_name);
  for (; ;) {
    lseek(fd, 1, SEEK_CUR);
    if (read(fd, buf, 1) <= 0) {
      break;
    }
    printf("%c", buf[0]);
  }
  printf("\n");
  assert(0 == close(fd));
  return NULL;
}

void* test_fstat(void* arg) {
  char file_name[] = "some_impossible_file_name.txt";
  int fd = open(file_name, O_RDONLY);
  struct stat st;
  int result_code;
  assert(fd > 0);
  result_code = fstat(fd, &st);
  assert(-1 != result_code);
  assert(0 == close(fd));
  return NULL;
}

void* test_times(void* arg) {
  struct tms t;
  int r = times(&t);
  assert(-1 != r);
  printf("test_times: tms_utime = %d, tms_stime = %d result=%d\n",
         (int)t.tms_utime, (int)t.tms_stime, (int)r);
  return NULL;
}

void* test_send_wrong_handles(void* arg) {
  closedir((DIR*)123);
  closedir((DIR*)100500);
  closedir((DIR*)123456);
  return NULL;
}

void* test_pipe(void* arg) {
  int fd[2];
  char buf[64];
  char str[] = "some string to write\nto pipe\nblah blah blah";
  int r;
  assert(-1 != pipe(fd));
  write(fd[1], str, strlen(str));
  r = read(fd[0], buf, 64);
  assert(r >= 0);
  buf[r] = '\0';
  printf("test_pipe: string passed through pipe: %s\n", buf);
  assert(0 == strcmp(str, buf));
  return NULL;
}

void* empty_routine(void* arg) {
  return NULL;
}

int main(int argc, char **argv) {
  int y;
  int fd = open(test_file_name, O_CREAT | O_RDWR);
  char test_string[] = "abracadabra";
  pthread_t threads[NTHREADS];

  assert(strlen(test_string) == write(fd, test_string, strlen(test_string)));
  close(fd);

  pthread_create(&threads[0], NULL, empty_routine, NULL);
  pthread_create(&threads[1], NULL, test_open_read, NULL);
  pthread_create(&threads[2], NULL, test_pathconf, NULL);
  pthread_create(&threads[3], NULL, test_getcwd, NULL);
  pthread_create(&threads[4], NULL, test_opendir_readdir, NULL);
  pthread_create(&threads[5], NULL, test_getpagesize, NULL);
  pthread_create(&threads[6], NULL, test_lseek, NULL);
  pthread_create(&threads[7], NULL, test_fstat, NULL);
  pthread_create(&threads[8], NULL, test_times, NULL);
  pthread_create(&threads[9], NULL, test_send_wrong_handles, NULL);
  pthread_create(&threads[10], NULL, test_pipe, NULL);
  for (y = 11; y < NTHREADS; ++y) {
    pthread_create(&threads[y], NULL, test_times, NULL);
  }
  for(y = 0; y < NTHREADS; ++y) {
    pthread_join(threads[y], NULL);
  }
  return 0;
}
