/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>

#if NACL_WINDOWS
# include "io.h"
# include "fcntl.h"
#endif

#define COPY_CHUNKSIZE  (4 * 4096)


/*
 * cp via gio on top of NaClDescIoDesc, as a test.
 */

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_global_secure_random.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/nacl_base/nacl_refcount.h"

#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/gio/gio_nacl_desc.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

void Usage(void) {
  fprintf(stderr, "Usage: gio_nacl_desc_test src_file dst_file\n");
}

int WriteAll(struct Gio *dst, char *buffer, size_t nbytes) {
  size_t bytes_written;
  ssize_t written;
  int num_errors = 0;

  for (bytes_written = 0;
       bytes_written < nbytes;
       bytes_written += written) {
    written = (*NACL_VTBL(Gio, dst)->Write)(dst,
                                            buffer + bytes_written,
                                            nbytes - bytes_written);
    if (written < 0) {
      written = 0;
      ++num_errors;
      fprintf(stderr, "Write error\n");
    }
  }
  return num_errors;
}

int ReadAll(struct Gio *src, char *buffer, size_t nbytes) {
  size_t bytes_read;
  ssize_t readden;  /* for parallelism :-) */
  int num_errors = 0;

  for (bytes_read = 0;
       bytes_read < nbytes;
       bytes_read += readden) {
    readden = (*NACL_VTBL(Gio, src)->Read)(src,
                                           buffer + bytes_read,
                                           nbytes - bytes_read);
    if (readden < 0) {
      readden = 0;
      ++num_errors;
      fprintf(stderr, "read error\n");
    }
  }
  return num_errors;
}

/*
 * Returns number of detected but correctable errors.
 */
int GioCopy(struct Gio *src, struct Gio *dst) {
  char buffer[COPY_CHUNKSIZE];
  ssize_t bytes_read;
  int num_errors = 0;

  while (0 != (bytes_read = (*NACL_VTBL(Gio, src)->
                             Read)(src, buffer, sizeof buffer))) {
    if (bytes_read < 0) {
      fprintf(stderr, "negative read?!?\n");
      return ++num_errors;
    }
    num_errors += WriteAll(dst, buffer, (size_t) bytes_read);
  }
  return num_errors;
}

int ComparePosixFiles(char *file1, char *file2) {
  FILE *f1 = fopen(file1, "r");
  FILE *f2 = fopen(file2, "r");
  int num_errors = 0;
  int byte;
  int byte2;

  if (NULL == f1) return 1;
  if (NULL == f2) return 1;

  while (EOF != (byte = getc(f1))) {
    if (byte != (byte2 = getc(f2))) {
      ++num_errors;
    }
    if (EOF == byte2) {
      fprintf(stderr, "file length differs (%s short)\n", file2);
      /* num_errors already incremented above */
      break;
    }
  }
  if (EOF != getc(f2)) {
    fprintf(stderr, "file length differs (%s short)\n", file1);
    ++num_errors;
  }
  if (0 != num_errors) {
    fprintf(stderr, "files %s and %s differ\n", file1, file2);
  }
  return num_errors;
}

void RemoveFile(char *fname) {
#if NACL_WINDOWS
  _unlink(fname);
#else
  unlink(fname);
#endif
}

int GioRevCopy(struct Gio *src, struct Gio *dst) {
  off_t file_size;
  char buffer[COPY_CHUNKSIZE];
  off_t start_offset;
  int num_errors = 0;
  size_t expected_bytes;

  file_size = (*NACL_VTBL(Gio, src)->Seek)(src, 0, SEEK_END);
  if (file_size <= 0) {
    fprintf(stderr, "non-positive file size?!?\n");
  }

  start_offset = file_size;
  do {
    start_offset -= COPY_CHUNKSIZE;
    expected_bytes = COPY_CHUNKSIZE;
    if (start_offset < 0) {
      expected_bytes = start_offset + COPY_CHUNKSIZE;
      start_offset = 0;
    }
    if (start_offset != (*NACL_VTBL(Gio, src)->
                         Seek)(src, start_offset, SEEK_SET)) {
      fprintf(stderr, "seek in source file failed, offset %"NACL_PRId64"\n",
              (int64_t) start_offset);
      return ++num_errors;
    }
    num_errors += ReadAll(src, buffer, expected_bytes);
    if (start_offset != (*NACL_VTBL(Gio, dst)->
                         Seek)(dst, start_offset, SEEK_SET)) {
      fprintf(stderr, "seek in destination file failed, offset"
              " %"NACL_PRId64"\n", (int64_t) start_offset);
      return ++num_errors;
    }
    num_errors += WriteAll(dst, buffer, expected_bytes);
  } while (start_offset > 0);

  return num_errors;
}

int main(int ac,
         char **av) {
  struct NaClDescIoDesc *src;
  struct NaClDescIoDesc *dst;
  struct NaClGioNaClDesc gsrc;
  struct NaClGioNaClDesc gdst;
  int num_errors = 0;

  if (ac != 3) {
    Usage();
    return -1;
  }

  NaClLogModuleInit();
  NaClTimeInit();
  NaClSecureRngModuleInit();
  NaClGlobalSecureRngInit();

  src = NaClDescIoDescOpen(av[1], NACL_ABI_O_RDONLY, 0);
  if (NULL == src) {
    fprintf(stderr, "could not open %s for read\n", av[1]);
    Usage();
    return -2;
  }
  dst = NaClDescIoDescOpen(av[2],
                           NACL_ABI_O_WRONLY|
                           NACL_ABI_O_TRUNC|
                           NACL_ABI_O_CREAT,
                           0666);
  if (NULL == dst) {
    fprintf(stderr, "could not open %s for write\n", av[2]);
    Usage();
    return -3;
  }
  if (!NaClGioNaClDescCtor(&gsrc, (struct NaClDesc *) src)) {
    fprintf(stderr, "NaClGioNaClDescCtor failed for source file\n");
    return -4;
  }
  if (!NaClGioNaClDescCtor(&gdst, (struct NaClDesc *) dst)) {
    fprintf(stderr, "NaClGioNaClDescCtor failed for destination file\n");
    return -5;
  }

  num_errors += GioCopy((struct Gio *) &gsrc, (struct Gio *) &gdst);
  num_errors += ComparePosixFiles(av[1], av[2]);

  (*NACL_VTBL(Gio, &gdst)->Close)(&gdst.base);
  (*NACL_VTBL(Gio, &gdst)->Dtor)(&gdst.base);

  if (0 < num_errors) {
    return num_errors;
  }

  RemoveFile(av[2]);

  /* reverse copy; reuse gsrc */

  dst = NaClDescIoDescOpen(av[2],
                           NACL_ABI_O_WRONLY|
                           NACL_ABI_O_TRUNC|
                           NACL_ABI_O_CREAT,
                           0666);
  if (NULL == dst) {
    fprintf(stderr, "could not open %s for write\n", av[2]);
    Usage();
    return -6;
  }
  if (!NaClGioNaClDescCtor(&gdst, (struct NaClDesc *) dst)) {
    fprintf(stderr, "NaClGioNaClDescCtor faied for destination file\n");
    return -7;
  }

  /*
   * We run GioRevCopy twice because if Seek failed to move the file
   * pointer but reported the file size correctly, it would still
   * result in a correct output.  By running it twice, the destination
   * data is just overwritten if the implementation is correct, and if
   * it isn't, we would end up with a file that is twice the source
   * file size.
   */
  num_errors += GioRevCopy((struct Gio *) &gsrc, (struct Gio *) &gdst);
  num_errors += GioRevCopy((struct Gio *) &gsrc, (struct Gio *) &gdst);
  num_errors += ComparePosixFiles(av[1], av[2]);

  (*NACL_VTBL(Gio, &gdst)->Close)((struct Gio *) &gdst);
  (*NACL_VTBL(Gio, &gdst)->Dtor)((struct Gio *) &gdst);

  (*NACL_VTBL(Gio, &gsrc)->Close)((struct Gio *) &gsrc);
  (*NACL_VTBL(Gio, &gsrc)->Dtor)((struct Gio *) &gsrc);

  if (0 < num_errors) {
    return num_errors;
  }

  RemoveFile(av[2]);

  return num_errors;
}
