/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Service Runtime.  IMC API.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_NACL_SYSCALLS_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_NACL_SYSCALLS_H_

/**
 * @file
 * Defines <a href="group__syscalls.html">Service Runtime Calls</a>.
 * The ABI is implicitly defined.
 *
 * @addtogroup syscalls
 * @{
 */

#include <sys/types.h>  /* off_t */
#include <sys/stat.h>
#include <time.h>  /* clock_t, to be deprecated */
                   /* TODO(bsy): newlib configuration needs to be customized to
                    *            our own ABI. */

#ifdef __cplusplus
extern "C" {
#endif

struct timeval;  /* sys/time.h */
struct timezone;
struct NaClImcMsgHdr;  /* sys/nacl_imc_api.h */

/**
 *  @nacl
 *  A stub library routine used to evaluate syscall performance.
 */
extern void null_syscall(void);
/**
 *  @nqPosix
 *  Opens a file descriptor from a given pathname.
 *
 *  @param pathname The posix pathname (e.g., /tmp/foo) to be opened.
 *  @param flags The access modes allowed.  One of O_RDONLY, O_WRONLY, or
 *  O_RDWR is required.  Otherwise, only O_CREAT, O_TRUNC, and O_APPEND are
 *  supported.
 *  @param mode (optional) Specifies the permissions to use if a new file is
 *  created.  Limited only to user permissions.
 *  @return Returns non-negative file descriptor if successful.
 *  If open fails, it returns -1 and sets errno appropriately.
 */
extern int open(char const *pathname, int flags, ...);
/**
 *  @posix
 *  Closes the specified file descriptor.
 *  @param desc The file descriptor to be closed.
 *  @return Open returns zero on success.  On error, it returns -1 and sets
 *  errno appropriately.
 */
extern int close(int desc);
/**
 *  @posix
 *  Reads count bytes from the resource designated by desc
 *  into the buffer pointed to by buf.
 *  @param desc The file descriptor to be read from
 *  @param buf Where the read data is to be written
 *  @param count The number of bytes to be read
 *  @return On success, read returns the number of bytes read.  On failure,
 *  returns -1 and sets errno appropriately.
 */
extern int read(int desc, void *buf, size_t count);
/**
 *  @posix
 *  Writes count bytes from the buffer pointed to by buf to the
 *  resource designated by desc.
 *  @param desc The file descriptor to be written to
 *  @param buf Where the read data is to be read from
 *  @param count The number of bytes to be written
 *  @return On success, write returns the number of bytes written.  On failure,
 *  returns -1 and sets errno appropriately.
 */
extern int write(int desc, void const *buf, size_t count);
/**
 *  @posix
 *  Moves the file offset associated with desc.
 *  @param desc The file descriptor
 *  @param offset If whence is SEEK_SET, the descriptor offset is set to this
 *  value.  If whence is SEEK_CUR, the descriptor offset is set to its current
 *  location plus this value.  If whence is SEEK_END, the descriptor offset is
 *  set to the size of the file plus this value.
 *  @param whence Directive for setting the descriptor offset.
 *  @return On success, lseek returns the resulting descriptor offset in bytes
 *  from the beginning of the file.  On failure, it returns -1 and sets errno
 *  appropriately.
 */
extern off_t lseek(int desc, off_t offset, int whence);
/**
 *  @posix
 *  Manipulates device parameters of special files.
 *  @param desc The file descriptor
 *  @param request The device-dependent request code.
 *  @param arg An untyped pointer to memory used to convey request information.
 *  @return Ioctl usually returns zero on success.  Some requests return
 *  positive values.  On failure, it returns -1 and sets errno appropriately.
 */
extern int ioctl(int desc, int request, void *arg);
/**
 *  @nacl
 *  Sets the system break to the given address and return the address after
 *  the update.  If new_break is NULL, simply returns the current break address.
 *  @param new_break The address to set the break to.
 *  @return On success, sysbrk returns the value of the break address.  On
 *  failure, it returns -1 and sets errno appropriately.
 */
extern void *sysbrk(void *new_break);
/**
 *  @posix
 *  Maps a specified descriptor into the application address space.
 *  @param start The requested start address for the mapping
 *  @param length The number of bytes to be mapped (must be a multiple of 4K)
 *  @param prot The protection
 *  @param flags The modes the descriptor should be mapped under.  The only
 *  allowed flags are MAP_FIXED, MAP_SHARED, MAP_PRIVATE and MAP_ANONYMOUS.
 *  @param desc The descriptor to map
 *  @param offset The offset within the file specified by desc.
 *  @return On success, mmap returns the address the descriptor was mapped
 *  into. On failure it returns MAP_FAILED and sets errno appropriately.
 */
extern void *mmap(void *start, size_t length, int prot, int flags,
                  int desc, off_t offset);
/**
 *  @posix
 *  Unmaps the region of memory starting at a given start address with a given
 *  length.
 *  @param start The start address of the region to be unmapped
 *  @param length The length of the region to be unmapped.
 *  @return On success, munmap returns zero.  On failure, it returns -1 and
 *  sets errno appropriately.
 */
extern int munmap(void *start, size_t length);
/**
 *  @posix
 *  Terminates the program, returning a specified exit status.
 *  @param status The status to be returned.
 *  @return Exit does not return.
 */
extern void _exit(int status);
/**
 *  @nacl
 *  Terminates the current thread.
 *  @return Thread_exit does not return.
 */
extern void thread_exit(void); /**< PENDING: describe */
/**
 *  @posix
 *  Gets the current time of day.
 *  @param tv A pointer to struct timeval, where the time is written.
 *  @param tz Unused.
 *  @return On success, gettimeofday returns zero.  On failure, it
 *  returns -1 and sets errno appropriately.
 */
extern int gettimeofday(struct timeval *tv, void *tz);
/**
 *  @posix
 *  Returns approximate processor time used by a program.  (Deprecated.)
 *  @return clock returns a clock_t time value on success.  On failure, it
 *  returns -1 and sets errno appropriately.
 */
extern clock_t clock(void);
/**
 *  @posix
 *  Suspends execution of the current thread by the specified time.
 *  @param req A pointer to a struct timespec containing the requested
 *  sleep time.
 *  @param rem A pointer to a struct timespec where, if non-NULL and the
 *  call to nanosleep were interrupted, the remaining sleep time is written
 *  (and nanosleep returns -1, with errno set to EINTR).
 *  @return On success, nanosleep returns 0.  On error, it returns -1 and
 *  sets errno appropriately.
 */
extern int nanosleep(const struct timespec *req, struct timespec *rem);

/**
 *  @nqPosix
 *  Returns information about a file.
 *  @param path The posix pathname of the file.
 *  @param buf The struct stat to store the information about the file.  Only
 *  size (st_size) is valid.
 *  @return On success, stat returns zero.  On error, it returns -1 and sets
 *  errno appropriately.
 */
extern int stat(const char *path, struct stat *buf);

#if defined(HAVE_SDL)

/**
 *  does this show up? (how do we define HAVE_SDL?)
 */
extern int video_init(int width, int height);

/**
 *  does this show up? (how do we define HAVE_SDL?)
 */
extern int video_fini(void);

/**
 *  does this show up? (how do we define HAVE_SDL?)
 */
extern int video_blt_rgb(int width, int height, unsigned char const *data);

#endif

/**
 *  @nacl
 *  Returns the value of the -P parameter passed on the command line to
 *  sel_ldr.  This is where sel_ldr instances created by either sel_universal
 *  or the SRPC interaction of the browser plugin convey the file descriptor of
 *  the socket used to perform RPCs.
 *  @return srcp_get_fd returns the file descriptor passed as -P on the
 *  command line, or -1 if it was not passed.
 */
extern int srpc_get_fd(void);
/**
 *  @nacl
 *  Creates a bound IMC socket and returns a descriptor for the socket and the
 *  socket address.
 *  @param sock_and_addr An array of two file descriptors.  On successful
 *  return, the first is set to the descriptor of the bound socket, and can be
 *  used for, e.g., imc_accept.  On successful return, the second descriptor is
 *  set to a socket address object that may be sent to other NativeClient
 *  modules or the browser plugin.
 *  @return On success, imcmakeboundsock returns zero.  On failure, it returns
 *  -1 and sets errno appropriately.
 */
extern int imc_makeboundsock(int sock_and_addr[2]);
/**
 *  @nacl
 *  Accepts a connection on a bound IMC socket, returning a connected IMC socket
 *  descriptor.
 *  @param desc The file descriptor of an IMC bound socket.
 *  @return On success, imc_accept returns a non-negative file descriptor for
 *  the connected socket.  On failure, it returns -1 and sets errno
 *  appropriately.
 */
extern int imc_accept(int desc);
/**
 *  @nacl
 *  Connects to an IMC socket address, returning a connected IMC socket
 *  descriptor.
 *  @param desc The file descriptor of an IMC socket address.
 *  @return On success, imc_connect returns a non-negative file descriptor for
 *  the connected socket.  On failure, it returns -1 and sets errno.
 *  The returned descriptor may be used to transfer data and descriptors
 *  but is itself not transferable.
 *  appropriately.
 */
extern int imc_connect(int desc);
/**
 *  @nacl
 *  Sends a message over a specified IMC socket descriptor.
 *  descriptor.
 *  @param desc The file descriptor of an IMC socket.
 *  @param nmhp The message header structure containing information to be sent.
 *  @param flags TBD
 *  @return On success, imc_sendmsg returns a non-negative number of bytes sent
 *  to the socket.  On failure, it returns -1 and sets errno appropriately.
 *  The returned descriptor may be used to transfer data and descriptors
 *  but is itself not transferable.
 */
extern int imc_sendmsg(int desc, struct NaClImcMsgHdr const *nmhp, int flags);
/**
 *  @nacl
 *  Receives a message over a specified IMC socket descriptor.
 *  @param desc The file descriptor of an IMC socket.
 *  @param nmhp The message header structure to be populated when receiving the
 *  message.
 *  @param flags TBD
 *  @return On success, imc_recvmsg returns a non-negative number of bytes
 *  read. On failure, it returns -1 and sets errno appropriately.
 */
extern int imc_recvmsg(int desc, struct NaClImcMsgHdr *nmhp, int flags);
/**
 *  @nacl
 *  Creates an IMC shared memory region, returning a file descriptor.
 *  @param nbytes The number of bytes in the requested shared memory region.
 *  @return On success, imc_mem_obj_create returns a non-negative file
 *  descriptor for the shared memory region.  On failure, it returns -1 and
 *  sets errno appropriately.
 */
extern int imc_mem_obj_create(size_t nbytes);
/**
 *  @nacl
 *  Creates an IMC socket pair, returning a pair of file descriptors.
 *  These descriptors are data-only, i.e., they may be used with
 *  imc_sendmsg and imc_recvmsg, but only if the descriptor count
 *  (desc_length) is zero.
 *  @param pair An array of two file descriptors for the two ends of the
 *  socket.
 *  @return On success imc_socketpair returns zero.  On failure, it returns -1
 *  and sets errno appropriately.
 *  The returned descriptor may only be used to transfer data
 *  and is itself transferable.
 */
extern int imc_socketpair(int pair[2]);
#ifdef __cplusplus
}
#endif

/**
 * @}
 * End of System Calls group
 */

#endif  // NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_NACL_SYSCALLS_H_
