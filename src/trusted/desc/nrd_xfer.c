/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Native Client Resource Descriptor Transfer protocol for trusted code.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_cond.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_mutex.h"
#include "native_client/src/trusted/desc/nacl_desc_dir.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"

#include "native_client/src/shared/platform/nacl_global_secure_random.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


/*
 * The NRD xfer header that describes the types of NRDs being
 * transferred should be rounded to a multiple of 16 bytes in length.
 * This allows fast, SSE-based memcpy routines to work, giving us
 * 16-bytes-per-cycle memory copies for in-cache data.
 */

static INLINE nacl_abi_size_t min_abi_size(nacl_abi_size_t  a,
                                           nacl_abi_size_t  b) {
  if (a < b) return a;
  return b;
}

static INLINE size_t min_size(size_t  a,
                              size_t  b) {
  if (a < b) return a;
  return b;
}

ssize_t NaClImcSendTypedMessage(struct NaClDesc                 *channel,
                                struct NaClDescEffector         *effp,
                                const struct NaClImcTypedMsgHdr *nitmhp,
                                int                              flags) {
  int                       supported_flags;
  ssize_t                   retval = -NACL_ABI_EINVAL;
  struct NaClMessageHeader  kern_msg_hdr;  /* xlated interface */
  size_t                    i;
  /*
   * BEWARE: NaClImcMsgIoVec has the same layout as NaClIOVec, so
   * there will be type punning below to avoid copying from a struct
   * NaClImcMsgIoVec array to a struct NaClIOVec array in order to
   * call the IMC library code using kern_msg_hdr.
   */
  struct NaClImcMsgIoVec    kern_iov[NACL_ABI_IMC_IOVEC_MAX + 1];
  struct NaClDesc           **kern_desc;
  NaClHandle                kern_handle[NACL_ABI_IMC_DESC_MAX];
  size_t                    user_bytes;
  size_t                    tmp_sum;
  size_t                    sys_bytes;
  size_t                    sys_handles;
  size_t                    desc_bytes;
  size_t                    desc_handles;
  struct NaClInternalHeader *hdr;
  char                      *hdr_buf;
  struct NaClDescXferState  xfer_state;

  static struct NaClInternalHeader const kNoHandles = {
    { NACL_HANDLE_TRANSFER_PROTOCOL, 0, },
    /* and implicit zeros for pad bytes */
  };

  NaClLog(4,
          ("Entered"
           " NaClImcSendTypedMessage(0x%08"NACL_PRIxPTR", "
           "0x%08"NACL_PRIxPTR", 0x%x)\n"),
          (uintptr_t) channel, (uintptr_t) nitmhp, flags);
  /*
   * What flags do we know now?  If a program was compiled using a
   * newer ABI than what this implementation knows, the extra flag
   * bits are ignored but will generate a warning.
   */
  supported_flags = NACL_ABI_IMC_NONBLOCK;
  if (0 != (flags & ~supported_flags)) {
    NaClLog(LOG_WARNING,
            "WARNING: NaClImcSendTypedMessage: unknown IMC flag used: 0x%x\n",
            flags);
    flags &= supported_flags;
  }

  /*
   * use (ahem) RTTI -- or a virtual function that's been partially
   * evaluated/memoized -- to short circuit the error check, so that
   * cleanups are easier (rather than letting it fail at the
   * ExternalizeSize virtual function call).
   */
  if (0 != nitmhp->ndesc_length
      && NACL_DESC_IMC_SOCKET != channel->vtbl->typeTag) {
    NaClLog(4, "not an IMC socket and trying to send descriptors!\n");
    return -NACL_ABI_EINVAL;
  }

  if (nitmhp->iov_length > NACL_ABI_IMC_IOVEC_MAX) {
    NaClLog(4, "gather/scatter array too large\n");
    return -NACL_ABI_EINVAL;
  }
  if (nitmhp->ndesc_length > NACL_ABI_IMC_USER_DESC_MAX) {
    NaClLog(4, "handle vector too long\n");
    return -NACL_ABI_EINVAL;
  }

  memcpy(kern_iov + 1, (void *) nitmhp->iov,
         nitmhp->iov_length * sizeof *nitmhp->iov);
  /*
   * kern_iov[0] is the message header that tells the recipient where
   * the user data and capabilities data (e.g., NaClDescConnCap)
   * boundary is, as well as types of objects in the handles vector of
   * NaClHandle objects, which must be internalized.
   *
   * NB: we assume that communication is secure, and no external
   * entity can listen in or will inject bogus information.  This is a
   * strong assumption, since falsification can cause loss of
   * type-safety.
   */
  user_bytes = 0;
  for (i = 0; i < nitmhp->iov_length; ++i) {
    tmp_sum = user_bytes + kern_iov[i+1].length;
    /* unsigned addition -- check for integer oveflow */
    if (tmp_sum < user_bytes) {
      return -NACL_ABI_EINVAL;
    }
    user_bytes = tmp_sum;
  }
  if (user_bytes > NACL_ABI_IMC_USER_BYTES_MAX) {
    return -NACL_ABI_EINVAL;
  }

  kern_desc = nitmhp->ndescv;
  hdr_buf = NULL;

  /*
   * NOTE: type punning w/ NaclImcMsgIoVec and NaClIOVec.
   * This breaks ansi type aliasing rules and hence we may use
   * soemthing like '-fno-strict-aliasing'
   */
  kern_msg_hdr.iov = (struct NaClIOVec *) kern_iov;
  kern_msg_hdr.iov_length = nitmhp->iov_length + 1;  /* header */

  if (0 == nitmhp->ndesc_length) {
    kern_msg_hdr.handles = NULL;
    kern_msg_hdr.handle_count = 0;
    kern_iov[0].base = (void *) &kNoHandles;
    kern_iov[0].length = sizeof kNoHandles;
  } else {
    /*
     * \forall i \in [0, nitmhp->desc_length): kern_desc[i] != NULL.
     */

    sys_bytes = 0;
    sys_handles = 0;
    /* nitmhp->desc_length <= NACL_ABI_IMC_USER_DESC_MAX */
    for (i = 0; i < nitmhp->ndesc_length; ++i) {
      retval = (*kern_desc[i]->vtbl->ExternalizeSize)(kern_desc[i],
                                                      &desc_bytes,
                                                      &desc_handles);
      if (retval < 0) {
        NaClLog(1,
                ("NaClImcSendTypedMessage: ExternalizeSize"
                 " returned %"NACL_PRIdS"\n"),
                retval);
        goto cleanup;
      }
      /*
       * No integer overflow should be possible given the max-handles
       * limits and actual resource use per handle involved.
       */
      if (desc_bytes > NACL_ABI_SIZE_T_MAX - 1
          || (desc_bytes + 1) > NACL_ABI_SIZE_T_MAX - sys_bytes
          || desc_handles > NACL_ABI_SIZE_T_MAX - sys_handles) {
        retval = -NACL_ABI_EOVERFLOW;
        goto cleanup;
      }
      sys_bytes += (1 + desc_bytes);
      sys_handles += desc_handles;
    }
    if (sys_handles > NACL_ABI_IMC_DESC_MAX) {
      NaClLog(LOG_FATAL, ("User had %"NACL_PRIdNACL_SIZE" descriptors,"
                          " which expanded into %"NACL_PRIdS
                          "handles, more than"
                          " the max of %d.\n"),
              nitmhp->ndesc_length, sys_handles,
              NACL_ABI_IMC_DESC_MAX);
    }
    /* a byte for NACL_DESC_TYPE_END_TAG, then rounded up to 0 mod 16 */
    sys_bytes = (sys_bytes + 1 + 0xf) & ~0xf;

    /* bsy notes that sys_bytes should never exceed NACL_ABI_IMC_DESC_MAX
     * times the maximum protocol transfer size, which is currently set to
     * NACL_PATH_MAX, so it would be abnormal for this check to fail.
     * Including it anyway just in case something changes.
     */
    if (sys_bytes > NACL_ABI_SIZE_T_MAX - sizeof *hdr) {
      NaClLog(LOG_FATAL, "NaClImcSendTypedMessage: "
              "Buffer size overflow (%"NACL_PRIdS" bytes)",
              sys_bytes);
      retval = -NACL_ABI_EOVERFLOW;
      goto cleanup;
    }
    hdr_buf = malloc(sys_bytes + sizeof *hdr);
    if (NULL == hdr_buf) {
      NaClLog(4, "NaClImcSendTypedMessage: out of memory for iov");
      retval = -NACL_ABI_ENOMEM;
      goto cleanup;
    }
    kern_iov[0].base = (void *) hdr_buf;

    /* Checked above that sys_bytes <= NACL_ABI_SIZE_T_MAX - sizeof *hdr */
    kern_iov[0].length = (nacl_abi_size_t)(sys_bytes + sizeof *hdr);

    hdr = (struct NaClInternalHeader *) hdr_buf;
    memset(hdr, 0, sizeof(*hdr));  /* Initilize the struct's padding bytes. */
    hdr->h.xfer_protocol_version = NACL_HANDLE_TRANSFER_PROTOCOL;
    hdr->h.descriptor_data_bytes = nacl_abi_size_t_saturate(sys_bytes);

    xfer_state.next_byte = (char *) (hdr + 1);
    xfer_state.byte_buffer_end = xfer_state.next_byte + sys_bytes;
    xfer_state.next_handle = kern_handle;
    xfer_state.handle_buffer_end = xfer_state.next_handle
        + NACL_ABI_IMC_DESC_MAX;

    for (i = 0; i < nitmhp->ndesc_length; ++i) {
      *xfer_state.next_byte++ = (char) kern_desc[i]->vtbl->typeTag;
      /*
       * Externalize should expose the object as NaClHandles that must
       * be sent, but no ownership is transferred.  The NaClHandle
       * objects cannot be deallocated until after the actual SendMsg
       * completes, so multithreaded code beware!  By using
       * NaClDescRef and NaClDescUnref properly, there shouldn't be
       * any problems, since all entries in the ndescv should have had
       * their refcount incremented, and the NaClHandles will not be
       * closed until the NaClDesc objects' refcount goes to zero.
       */
      retval = (*kern_desc[i]->vtbl->Externalize)(kern_desc[i],
                                                  &xfer_state);
      if (0 != retval) {
        NaClLog(4,
                ("NaClImcSendTypedMessage: Externalize for"
                 " descriptor %"NACL_PRIdS" returned %"NACL_PRIdS"\n"),
                i, retval);
        goto cleanup;
      }
    }
    *xfer_state.next_byte++ = (char) NACL_DESC_TYPE_END_TAG;
    /*
     * zero fill the rest of memory to avoid leaking info from
     * otherwise uninitialized malloc'd memory.
     */
    while (xfer_state.next_byte < xfer_state.byte_buffer_end) {
      *xfer_state.next_byte++ = '\0';
    }

    kern_msg_hdr.handles = kern_handle;
    kern_msg_hdr.handle_count = sys_handles;
  }

  NaClLog(4, "Invoking SendMsg, flags 0x%x\n", flags);

  retval = (*channel->vtbl->SendMsg)(channel, effp,
                                     &kern_msg_hdr, flags);
  NaClLog(4, "SendMsg returned %"NACL_PRIdS"\n", retval);
  if (NaClIsNegErrno(retval)) {
    /*
     * NaClWouldBlock uses TSD (for both the errno-based and
     * GetLastError()-based implementations), so this is threadsafe.
     */
    if (0 != (flags & NACL_DONT_WAIT) && NaClWouldBlock()) {
      retval = -NACL_ABI_EAGAIN;
    } else {
      /*
       * TODO(bsy): the else case is some mysterious internal error.
       * should we destroy the channel?  Was the failure atomic?  Did
       * it send some partial data?  Linux implementation appears
       * okay.
       *
       * We return EIO and let the caller deal with it.
       */
      retval = -NACL_ABI_EIO;
    }
  } else if ((unsigned) retval < kern_iov[0].length) {
    /*
     * retval >= 0, so cast to unsigned is value preserving.
     */
    retval = -NACL_ABI_ENOBUFS;
  } else {
    /*
     * The return value (number of bytes sent) should not include the
     * "out of band" additional information added by the service runtime.
     */
    retval -= kern_iov[0].length;
  }

cleanup:

  free(hdr_buf);

  NaClLog(4, "NaClImcSendTypedMessage: returning %"NACL_PRIdS"\n", retval);

  return retval;
}

ssize_t NaClImcRecvTypedMessage(struct NaClDesc           *channel,
                                struct NaClDescEffector   *effp,
                                struct NaClImcTypedMsgHdr *nitmhp,
                                int                       flags) {
  int                       supported_flags;
  ssize_t                   retval;
  char                      *recv_buf;
  size_t                    user_bytes;
  size_t                    tmp_sum;
  NaClHandle                kern_handle[NACL_ABI_IMC_DESC_MAX];
  struct NaClIOVec          recv_iov;
  struct NaClMessageHeader  recv_hdr;
  ssize_t                   total_recv_bytes;
  struct NaClInternalHeader intern_hdr;
  size_t                    recv_user_bytes_avail;
  size_t                    tmp;
  char                      *user_data;
  size_t                    iov_copy_size;
  struct NaClDescXferState  xfer;
  size_t                    type_tag;
  struct NaClDesc           *new_desc[NACL_ABI_IMC_DESC_MAX];
  int                       xfer_status;
  size_t                    i;
  size_t                    num_user_desc;

  NaClLog(4,
          "Entered NaClImcRecvTypedMsg(0x%08"NACL_PRIxPTR", "
          "0x%08"NACL_PRIxPTR", %d)\n",
          (uintptr_t) channel, (uintptr_t) nitmhp, flags);

  supported_flags = NACL_ABI_IMC_NONBLOCK;
  if (0 != (flags & ~supported_flags)) {
    NaClLog(LOG_WARNING,
            "WARNING: NaClImcRecvTypedMsg: unknown IMC flag used: 0x%x\n",
            flags);
    flags &= supported_flags;
  }

  if (nitmhp->iov_length > NACL_ABI_IMC_IOVEC_MAX) {
    NaClLog(4, "gather/scatter array too large\n");
    return -NACL_ABI_EINVAL;
  }
  if (nitmhp->ndesc_length > NACL_ABI_IMC_USER_DESC_MAX) {
    NaClLog(4, "handle vector too long\n");
    return -NACL_ABI_EINVAL;
  }

  user_bytes = 0;
  for (i = 0; i < nitmhp->iov_length; ++i) {
    tmp_sum = user_bytes + nitmhp->iov[i].length;
    /* Check for unsigned addition overflow */
    if (tmp_sum < user_bytes) {
      NaClLog(4, "integer overflow in iov length summation\n");
      return -NACL_ABI_EINVAL;
    }
    user_bytes = tmp_sum;
  }
  /*
   * if user_bytes > NACL_ABI_IMC_USER_BYTES_MAX,
   * we will just never fill up all the buffer space.
   */
  user_bytes = min_size(user_bytes, NACL_ABI_IMC_USER_BYTES_MAX);
  /*
   * user_bytes = \min(\sum_{i=0}{nitmhp->iov_length-1} nitmhp->iov[i].length,
   *                   NACL_ABI_IMC_USER_BYTES_MAX)
   */

  recv_buf = NULL;
  memset(new_desc, 0, sizeof new_desc);
  /*
   * from here on, set retval and jump to cleanup code.
   */

  recv_buf = malloc(NACL_ABI_IMC_BYTES_MAX);
  if (NULL == recv_buf) {
    NaClLog(4, "no memory for receive buffer\n");
    retval = -NACL_ABI_ENOMEM;
    goto cleanup;
  }

  recv_iov.base = (void *) recv_buf;
  recv_iov.length = NACL_ABI_IMC_BYTES_MAX;

  recv_hdr.iov = &recv_iov;
  recv_hdr.iov_length = 1;

  for (i = 0; i < NACL_ARRAY_SIZE(kern_handle); ++i) {
    kern_handle[i] = NACL_INVALID_HANDLE;
  }

  if (NACL_DESC_IMC_SOCKET == channel->vtbl->typeTag) {
    /*
     * Channel can transfer access rights.
     */

    recv_hdr.handles = kern_handle;
    recv_hdr.handle_count = NACL_ARRAY_SIZE(kern_handle);
    NaClLog(4, "Connected socket, may transfer descriptors\n");
  } else {
    /*
     * Channel cannot transfer access rights.  The syscall would fail
     * if recv_iov.length is non-zero.
     */

    recv_hdr.handles = (NaClHandle *) NULL;
    recv_hdr.handle_count = 0;
    NaClLog(4, "Transferable Data Only socket\n");
  }

  recv_hdr.flags = 0;  /* just to make it obvious; IMC will clear it for us */

  total_recv_bytes = (*channel->vtbl->RecvMsg)(channel,
                                               effp,
                                               &recv_hdr,
                                               flags);
  if (NaClIsNegErrno(total_recv_bytes)) {
    NaClLog(1, "RecvMsg failed, returned %"NACL_PRIdS"\n", total_recv_bytes);
    retval = total_recv_bytes;
    goto cleanup;
  }
  /* total_recv_bytes >= 0 */

  /*
   * NB: recv_hdr.flags may already contain NACL_ABI_MESSAGE_TRUNCATED
   * and/or NACL_ABI_HANDLES_TRUNCATED.
   *
   * First, parse the NaClInternalHeader and any subsequent fields to
   * extract and internalize the NaClDesc objects from the array of
   * NaClHandle values.
   *
   * Copy out to user buffer.  Possibly additional truncation may occur.
   *
   * Since total_recv_bytes >= 0, the cast to size_t is value preserving.
   */
  if ((size_t) total_recv_bytes < sizeof intern_hdr) {
    NaClLog(4, ("only received %"NACL_PRIdS" (0x%"NACL_PRIxS") bytes,"
                " but internal header is %"NACL_PRIdS" (0x%"NACL_PRIxS
                ") bytes\n"),
            total_recv_bytes, total_recv_bytes,
            sizeof intern_hdr, sizeof intern_hdr);
    retval = -NACL_ABI_EIO;
    goto cleanup;
  }
  memcpy(&intern_hdr, recv_buf, sizeof intern_hdr);
  /*
   * Future code should handle old versions in a backward compatible way.
   */
  if (NACL_HANDLE_TRANSFER_PROTOCOL != intern_hdr.h.xfer_protocol_version) {
    NaClLog(4, ("protocol version mismatch:"
                " got %x, but can only handle %x\n"),
            intern_hdr.h.xfer_protocol_version, NACL_HANDLE_TRANSFER_PROTOCOL);
    retval = -NACL_ABI_EIO;
    goto cleanup;
  }
  if ((size_t) total_recv_bytes < (intern_hdr.h.descriptor_data_bytes
                                   + sizeof intern_hdr)) {
    NaClLog(4, ("internal header (size %"NACL_PRIdS" (0x%"NACL_PRIxS")) "
                "says there are "
                "%d (0x%x) NRD xfer descriptor bytes, "
                "but we received %"NACL_PRIdS" (0x%"NACL_PRIxS") bytes\n"),
            sizeof intern_hdr, sizeof intern_hdr,
            intern_hdr.h.descriptor_data_bytes,
            intern_hdr.h.descriptor_data_bytes,
            total_recv_bytes, total_recv_bytes);
    retval = -NACL_ABI_EIO;
    goto cleanup;
  }
  recv_user_bytes_avail = (total_recv_bytes
                           - intern_hdr.h.descriptor_data_bytes
                           - sizeof intern_hdr);
  /*
   * NaCl app asked for user_bytes, and we have recv_user_bytes_avail.
   * Set recv_user_bytes_avail to the min of these two values, as well
   * as inform the caller if data truncation occurred.
   */
  if (user_bytes < recv_user_bytes_avail) {
    recv_hdr.flags |= NACL_ABI_RECVMSG_DATA_TRUNCATED;
  }
  recv_user_bytes_avail = min_size(recv_user_bytes_avail, user_bytes);

  retval = recv_user_bytes_avail;  /* default from hence forth */

  /*
   * Let UserDataSize := recv_user_bytes_avail.  (bind to current value)
   */

  user_data = recv_buf + sizeof intern_hdr + intern_hdr.h.descriptor_data_bytes;
  /*
   * Let StartUserData := user_data
   */

  /*
   * Precondition: user_data in [StartUserData, StartUserData + UserDataSize].
   *
   * Invariant:
   *  user_data + recv_user_bytes_avail == StartUserData + UserDataSize
   */
  for (i = 0; i < nitmhp->iov_length && 0 < recv_user_bytes_avail; ++i) {
    iov_copy_size = min_size(nitmhp->iov[i].length, recv_user_bytes_avail);

    memcpy(nitmhp->iov[i].base, user_data, iov_copy_size);

    user_data += iov_copy_size;
    /*
     * subtraction could not underflow due to how recv_user_bytes_avail was
     * computed; however, we are paranoid, in case the code changes.
     */
    tmp = recv_user_bytes_avail - iov_copy_size;
    if (tmp > recv_user_bytes_avail) {
      NaClLog(LOG_FATAL,
              "NaClImcRecvTypedMessage: impossible underflow occurred");
    }
    recv_user_bytes_avail = tmp;

  }
  /*
   * postcondition:  recv_user_bytes_avail == 0.
   *
   * NB: 0 < recv_user_bytes_avail \rightarrow i < nitmhp->iov_length
   * must hold, due to how user_bytes is computed.  We leave the
   * unnecessary test in the loop condition to avoid future code
   * changes from causing problems as defensive programming.
   */

  /*
   * Now extract/internalize the NaClHandles as NaClDesc objects.
   * Note that we will extract beyond nitmhp->desc_length, since we
   * must still destroy the ones that are dropped.
   */
  xfer.next_byte = recv_buf + sizeof intern_hdr;
  xfer.byte_buffer_end = xfer.next_byte + intern_hdr.h.descriptor_data_bytes;
  xfer.next_handle = kern_handle;
  xfer.handle_buffer_end = kern_handle + recv_hdr.handle_count;

  i = 0;
  while (xfer.next_byte < xfer.byte_buffer_end) {
    if (i >= NACL_ARRAY_SIZE(kern_handle)) {
      NaClLog(LOG_FATAL,
              ("NaClImcRecvTypedMsg: trusted peer tried to send too many"
               " descriptors!\n"));
    }
    type_tag = 0xff & *xfer.next_byte++;
    /* 0 <= type_tag */
    if (NACL_DESC_TYPE_END_TAG == type_tag) {
      break;
    }
    if (type_tag >= NACL_DESC_TYPE_MAX) {
      NaClLog(4, ("illegal type tag %"NACL_PRIdS" (0x%"NACL_PRIxS")\n"),
              type_tag, type_tag);
      retval = -NACL_ABI_EIO;
      goto cleanup;
    }
    if ((int (*)(struct NaClDesc **, struct NaClDescXferState *)) NULL ==
        NaClDescInternalize[type_tag]) {
      NaClLog(LOG_FATAL,
              "No internalization function for type %"NACL_PRIdS"\n",
              type_tag);
      /* fatal, but in case we change it later */
      retval = -NACL_ABI_EIO;
      goto cleanup;
    }
    xfer_status = (*NaClDescInternalize[type_tag])(&new_desc[i], &xfer);
    /* constructs new_desc, transferring ownership of any handles consumed */

    if (xfer_status != 0) {
      NaClLog(0,
              "non-zero xfer_status %d, desc type tag %s (%"NACL_PRIdS")\n",
              xfer_status,
              NaClDescTypeString(type_tag),
              type_tag);
      retval = xfer_status;
      goto cleanup;
    }
    ++i;
  }
  num_user_desc = i;  /* actual number of descriptors received */
  if (nitmhp->ndesc_length < num_user_desc) {
    nitmhp->flags |= NACL_ABI_RECVMSG_DESC_TRUNCATED;
    num_user_desc = nitmhp->ndesc_length;
  }

  /* transfer ownership to nitmhp->ndescv; some may be left behind */
  for (i = 0; i < num_user_desc; ++i) {
    nitmhp->ndescv[i] = new_desc[i];
    new_desc[i] = NULL;
  }

  /* cast is safe because we clamped num_user_desc earlier to
   * be no greater than the original value of nithmp->ndesc_length.
   */
  nitmhp->ndesc_length = (nacl_abi_size_t)num_user_desc;

  /* retval is number of bytes received */

cleanup:
  free(recv_buf);

  /*
   * Note that we must exercise discipline when constructing NaClDesc
   * objects from NaClHandles -- the NaClHandle values *must* be set
   * to NACL_INVALID_HANDLE after the construction of the NaClDesc
   * where ownership of the NaClHandle is transferred into the NaCDesc
   * object. Otherwise, between new_desc and kern_handle cleanup code,
   * a NaClHandle might be closed twice.
   */
  for (i = 0; i < NACL_ARRAY_SIZE(new_desc); ++i) {
    if (NULL != new_desc[i]) {
      NaClDescUnref(new_desc[i]);
      new_desc[i] = NULL;
    }
  }
  for (i = 0; i < NACL_ARRAY_SIZE(kern_handle); ++i) {
    if (NACL_INVALID_HANDLE != kern_handle[i]) {
      NaClClose(kern_handle[i]);
    }
  }

  NaClLog(3, "NaClImcRecvTypedMsg: returning %"NACL_PRIdS"\n", retval);
  return retval;
}

int32_t NaClCommonDescMakeBoundSock(struct NaClDesc   *pair[2]) {
  int32_t                     retval;
  struct NaClSocketAddress    sa;
  struct NaClDescConnCap      *ccp;
  NaClHandle                  h;
  struct NaClDescImcBoundDesc *idp;

  retval = -NACL_ABI_ENOMEM;
  ccp = NULL;
  idp = NULL;
  h = NACL_INVALID_HANDLE;
  /*
   * create NaClDescConnCap object, invoke NaClBoundSocket, create
   * an NaClDescImcDesc object.  put both into open file table.
   */
  ccp = malloc(sizeof *ccp);
  if (NULL == ccp) {
    goto cleanup;
  }
  idp = malloc(sizeof *idp);
  if (NULL == idp) {
    goto cleanup;
  }

  do {
    NaClGenerateRandomPath(&sa.path[0], NACL_PATH_MAX);
    h = NaClBoundSocket(&sa);
    NaClLog(3, "NaClCommonDescMakeBoundSock: sa: %s, h 0x%"NACL_PRIxPTR"\n",
            sa.path, (uintptr_t) h);
  } while (NACL_INVALID_HANDLE == h);

  if (!NaClDescConnCapCtor(ccp, &sa)) {
    goto cleanup;
  }

  if (!NaClDescImcBoundDescCtor(idp, h)) {
    NaClDescConnCapDtor((struct NaClDesc *) ccp);
    goto cleanup;
  }
  h = NACL_INVALID_HANDLE;  /* idp took ownership */

  pair[0] = (struct NaClDesc *) idp;
  idp = NULL;

  pair[1] = (struct NaClDesc *) ccp;
  ccp = NULL;

  retval = 0;

 cleanup:
  free(idp);
  free(ccp);
  if (NACL_INVALID_HANDLE != h) {
    NaClClose(h);
  }
  return retval;
}

int32_t NaClCommonDescSocketPair(struct NaClDesc *pair[2]) {
  int32_t                         retval = -NACL_ABI_EIO;
  struct NaClDescXferableDataDesc *d0;
  struct NaClDescXferableDataDesc *d1;
  NaClHandle                      sock_pair[2];

  /*
   * mark resources to enable easy cleanup
   */
  d0 = NULL;
  d1 = NULL;
  sock_pair[0] = NACL_INVALID_HANDLE;
  sock_pair[1] = NACL_INVALID_HANDLE;

  if (0 != NaClSocketPair(sock_pair)) {
    NaClLog(1,
            "NaClCommonSysImc_Socket_Pair: IMC socket pair creation failed\n");
    retval = -NACL_ABI_ENFILE;
    goto cleanup;
  }
  if (NULL == (d0 = malloc(sizeof *d0))) {
    retval = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (NULL == (d1 = malloc(sizeof *d1))) {
    free((void *) d0);
    d0 = NULL;
    retval = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (!NaClDescXferableDataDescCtor(d0, sock_pair[0])) {
    free((void *) d0);
    d0 = NULL;
    free((void *) d1);
    d1 = NULL;
    retval = -NACL_ABI_ENFILE;
    goto cleanup;
  }
  sock_pair[0] = NACL_INVALID_HANDLE;  /* ctor took ownership */
  if (!NaClDescXferableDataDescCtor(d1, sock_pair[1])) {
    free((void *) d1);
    d1 = NULL;
    retval = -NACL_ABI_ENFILE;
    goto cleanup;
  }
  sock_pair[1] = NACL_INVALID_HANDLE;  /* ctor took ownership */

  pair[0] = (struct NaClDesc *) d0;
  d0 = NULL;

  pair[1] = (struct NaClDesc *) d1;
  d1 = NULL;

  retval = 0;

 cleanup:
  /*
   * pre: d0 and d1 must either be NULL or point to fully constructed
   * NaClDesc objects
   */
  if (NULL != d0) {
    NaClDescUnref((struct NaClDesc *) d0);
  }
  if (NULL != d1) {
    NaClDescUnref((struct NaClDesc *) d1);
  }
  if (NACL_INVALID_HANDLE != sock_pair[0]) {
    NaClClose(sock_pair[0]);
  }
  if (NACL_INVALID_HANDLE != sock_pair[1]) {
    NaClClose(sock_pair[1]);
  }

  free(d0);
  free(d1);

  return retval;
}
