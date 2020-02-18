#ifndef _IOLAYER_H_
#define _IOLAYER_H_


/* How the IO layer works:
 * 
 * Start by getting an io_glue object by calling the appropriate
 * io_new...() function.  After that data can be read via the
 * io_glue->readcb() method.
 *
 */


#include "iolayert.h"

/* #define BBSIZ 1096 */
#define BBSIZ 16384
#define IO_FAKE_SEEK 1<<0L
#define IO_TEMP_SEEK 1<<1L


void io_glue_gettypes    (io_glue *ig, int reqmeth);

/* XS functions */
io_glue *im_io_new_fd(pIMCTX, int fd);
io_glue *im_io_new_bufchain(pIMCTX);
io_glue *im_io_new_buffer(pIMCTX, const char *data, size_t len, i_io_closebufp_t closecb, void *closedata);
io_glue *im_io_new_cb(pIMCTX, void *p, i_io_readl_t readcb, i_io_writel_t writecb, i_io_seekl_t seekcb, i_io_closel_t closecb, i_io_destroyl_t destroycb);
size_t   io_slurp(io_glue *ig, unsigned char **c);
void     io_glue_destroy(io_glue *ig);

void i_io_dump(io_glue *ig, int flags);

/* Buffered I/O */
extern int i_io_getc_imp(io_glue *ig);
extern int i_io_peekc_imp(io_glue *ig);
extern ssize_t i_io_peekn(io_glue *ig, void *buf, size_t size);
extern int i_io_putc_imp(io_glue *ig, int c);
extern ssize_t i_io_read(io_glue *ig, void *buf, size_t size);
extern ssize_t i_io_write(io_glue *ig, const void *buf, size_t size);
extern off_t i_io_seek(io_glue *ig, off_t offset, int whence);
extern int i_io_flush(io_glue *ig);
extern int i_io_close(io_glue *ig);
extern int i_io_set_buffered(io_glue *ig, int buffered);
extern ssize_t i_io_gets(io_glue *ig, char *, size_t, int);

#endif /* _IOLAYER_H_ */
