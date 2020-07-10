#ifndef IMAGER_IOLAYERT_H
#define IMAGER_IOLAYERT_H

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>

typedef enum { FDSEEK, FDNOSEEK, BUFFER, CBSEEK, CBNOSEEK, BUFCHAIN } io_type;

#ifdef _MSC_VER
typedef int ssize_t;
#endif

typedef struct i_io_glue_t i_io_glue_t;

/* compatibility for now */
typedef i_io_glue_t io_glue;

/* Callbacks we give out */

typedef ssize_t(*i_io_readp_t) (io_glue *ig, void *buf, size_t count);
typedef ssize_t(*i_io_writep_t)(io_glue *ig, const void *buf, size_t count);
typedef off_t  (*i_io_seekp_t) (io_glue *ig, off_t offset, int whence);
typedef int    (*i_io_closep_t)(io_glue *ig);
typedef ssize_t(*i_io_sizep_t) (io_glue *ig);

typedef void   (*i_io_closebufp_t)(void *p);
typedef void (*i_io_destroyp_t)(i_io_glue_t *ig);


/* Callbacks we get */

typedef ssize_t(*i_io_readl_t) (void *p, void *buf, size_t count);
typedef ssize_t(*i_io_writel_t)(void *p, const void *buf, size_t count);
typedef off_t  (*i_io_seekl_t) (void *p, off_t offset, int whence);
typedef int    (*i_io_closel_t)(void *p);
typedef void   (*i_io_destroyl_t)(void *p);
typedef ssize_t(*i_io_sizel_t) (void *p);

extern char *io_type_names[];



/* Structures to describe data sources */

struct i_io_glue_t {
  io_type type;
  void *exdata;
  i_io_readp_t	readcb;
  i_io_writep_t	writecb;
  i_io_seekp_t	seekcb;
  i_io_closep_t	closecb;
  i_io_sizep_t	sizecb;
  i_io_destroyp_t destroycb;
  unsigned char *buffer;
  unsigned char *read_ptr;
  unsigned char *read_end;
  unsigned char *write_ptr;
  unsigned char *write_end;
  size_t buf_size;

  /* non-zero if we encountered EOF */
  int buf_eof;

  /* non-zero if we've seen an error */
  int error;

  /* if non-zero we do write buffering (enabled by default) */
  int buffered;

  im_context_t context;
};

#define I_IO_DUMP_CALLBACKS 1
#define I_IO_DUMP_BUFFER 2
#define I_IO_DUMP_STATUS 4
#define I_IO_DUMP_DEFAULT (I_IO_DUMP_BUFFER | I_IO_DUMP_STATUS)

#define i_io_type(ig) ((ig)->source.ig_type)
#define i_io_raw_read(ig, buf, size) ((ig)->readcb((ig), (buf), (size)))
#define i_io_raw_write(ig, data, size) ((ig)->writecb((ig), (data), (size)))
#define i_io_raw_seek(ig, offset, whence) ((ig)->seekcb((ig), (offset), (whence)))
#define i_io_raw_close(ig) ((ig)->closecb(ig))
#define i_io_is_buffered(ig) ((int)((ig)->buffered))

#define i_io_getc(ig) \
  ((ig)->read_ptr < (ig)->read_end ? \
     *((ig)->read_ptr++) : \
     i_io_getc_imp(ig))
#define i_io_nextc(ig) \
  ((void)((ig)->read_ptr < (ig)->read_end ?	\
	  ((ig)->read_ptr++, 0) :		\
	  i_io_getc_imp(ig)))
#define i_io_peekc(ig) \
  ((ig)->read_ptr < (ig)->read_end ? \
   *((ig)->read_ptr) :		     \
     i_io_peekc_imp(ig))
#define i_io_putc(ig, c) \
  ((ig)->write_ptr < (ig)->write_end && !(ig)->error ?	\
    *(ig)->write_ptr++ = (c) :	       \
      i_io_putc_imp(ig, (c)))
#define i_io_eof(ig) \
  ((ig)->read_ptr == (ig)->read_end && (ig)->buf_eof)
#define i_io_error(ig) \
  ((ig)->read_ptr == (ig)->read_end && (ig)->error)

#endif
