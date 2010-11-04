/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Test deprecated AV syscalls and verify that they fail in Chrome.
 * These tests intentionally invoke at the syscall level.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <nacl/nacl_av.h>
#include <nacl/nacl_srpc.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/untrusted/av/nacl_av.h"
#include "native_client/src/untrusted/av/nacl_av_priv.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#define kWidth 640
#define kHeight 480
#define kSampleCount 4096
#define kMessageSize 256

uint32_t pixels[kWidth * kHeight];
uint16_t samples[kSampleCount];

NaClSrpcError ErrorMsg(NaClSrpcArg **out_args, const char *func, int retval) {
  char message[kMessageSize];
  snprintf(message, kMessageSize, "Function %s returned %d\n", func, retval);
  /* SRPC will free string, so use strndup to make a copy */
  out_args[0]->u.sval = strndup(message, kMessageSize);
  return NACL_SRPC_RESULT_OK;
}

/*
 *  Return a string.
 *   "SUCCESS" - all tests passed
 *  !"SUCCESS" - string contains name and return value of failed test.
 *
 * AVTest will test deprecated multimedia syscalls (50..58)
 * These deprecated syscalls are expected to return -ENOSYS when nexe is
 * launched from Chrome.  This is a Chrome specific test.
 */
void AVTest(NaClSrpcRpc *rpc,
            NaClSrpcArg **in_args,
            NaClSrpcArg **out_args,
            NaClSrpcClosure *done) {
  int ret;
  int desired_samples = kSampleCount;
  int obtained_samples = 0;
  size_t count = 0;

  ret = NACL_SYSCALL(multimedia_init)(NACL_SUBSYSTEM_AUDIO);
  if (-ENOSYS != ret) {
    rpc->result =
        ErrorMsg(out_args, "multimedia_init(NACL_SUBSYSTEM_AUDIO)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(multimedia_init)(NACL_SUBSYSTEM_VIDEO);
  if (-ENOSYS != ret) {
    rpc->result =
        ErrorMsg(out_args, "multimedia_init(NACL_SUBSYSTEM_VIDEO)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(multimedia_init)(NACL_SUBSYSTEM_AUDIO |
                                      NACL_SUBSYSTEM_VIDEO);
  if (-ENOSYS != ret) {
    rpc->result =
        ErrorMsg(out_args,
                 "multimedia_init(NACL_SUBSYSTEM_AUDIO | NACL_SUBSYSTEM_VIDEO",
                 ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(multimedia_shutdown)();
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "multimedia_shutdown()", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(video_init)(kWidth, kHeight);
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "video_init(width, height)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(video_shutdown)();
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "video_shutdown()", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(video_update)(pixels);
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "video_update(pixels)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(video_update)(NULL);
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "video_update(NULL)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(video_poll_event)(NULL);
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "video_poll_event(NULL)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(audio_init)(NACL_AUDIO_FORMAT_STEREO_48K,
      desired_samples, &obtained_samples);
  if (-ENOSYS != ret) {
    rpc->result =
        ErrorMsg(
            out_args,
            "audio_init(NACL_AUDIO_FORMAT_STEREO_48K, desired, &obtained)",
            ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(audio_init)(NACL_AUDIO_FORMAT_STEREO_44K,
      desired_samples, &obtained_samples);
  if (-ENOSYS != ret) {
    rpc->result =
        ErrorMsg(
            out_args,
            "audio_init(NACL_AUDIO_FORMAT_STEREO_44K, desired, &obtained)",
            ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(audio_init)(NACL_AUDIO_FORMAT_STEREO_48K,
      desired_samples, NULL);
  if (-ENOSYS != ret) {
    rpc->result =
        ErrorMsg(
            out_args,
            "audio_init(NACL_AUDIO_FORMAT_STEREO_48K, desired, NULL)",
            ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(audio_shutdown)();
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "audio_shutdown()", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(audio_stream)(NULL, NULL);
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "audio_stream(NULL, NULL)", ret);
    done->Run(done);
    return;
  }

  ret = NACL_SYSCALL(audio_stream)(samples, &count);
  if (-ENOSYS != ret) {
    rpc->result = ErrorMsg(out_args, "audio_stream(samples, &count)", ret);
    done->Run(done);
    return;
  }

  out_args[0]->u.sval = strdup("SUCCESS");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "avtest::s", AVTest },
  { NULL, NULL },
};

int main() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
