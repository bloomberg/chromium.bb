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
 * NaCl service library.
 */

#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

    .text

    .globl    __nacl_multimedia_init
    .p2align  NACLENTRYALIGN
__nacl_multimedia_init:
.MULTIMEDIA_INIT_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_multimedia_init)
    jmp .MULTIMEDIA_INIT_SYSCALL_ADDR

    .globl    __nacl_multimedia_shutdown
    .p2align  NACLENTRYALIGN
__nacl_multimedia_shutdown:
.MULTIMEDIA_SHUTDOWN_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_multimedia_shutdown)
    jmp .MULTIMEDIA_SHUTDOWN_SYSCALL_ADDR

    .globl    __nacl_video_init
    .p2align  NACLENTRYALIGN
__nacl_video_init:
.VIDEO_INIT_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_video_init)
    jmp .VIDEO_INIT_SYSCALL_ADDR

    .globl    __nacl_video_shutdown
    .p2align  NACLENTRYALIGN
__nacl_video_shutdown:
.VIDEO_SHUTDOWN_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_video_shutdown)
    jmp .VIDEO_SHUTDOWN_SYSCALL_ADDR

    .globl    __nacl_video_update
    .p2align  NACLENTRYALIGN
__nacl_video_update:
.VIDEO_UPDATE_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_video_update)
    jmp .VIDEO_UPDATE_SYSCALL_ADDR

    .globl    __nacl_video_poll_event
    .p2align  NACLENTRYALIGN
__nacl_video_poll_event:
.VIDEO_POLL_EVENT_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_video_poll_event)
    jmp .VIDEO_POLL_EVENT_SYSCALL_ADDR

    .globl    __nacl_audio_init
    .p2align  NACLENTRYALIGN
__nacl_audio_init:
.AUDIO_INIT_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_audio_init)
    jmp .AUDIO_INIT_SYSCALL_ADDR

    .globl    __nacl_audio_shutdown
    .p2align  NACLENTRYALIGN
__nacl_audio_shutdown:
.AUDIO_SHUTDOWN_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_audio_shutdown)
    jmp .AUDIO_SHUTDOWN_SYSCALL_ADDR

    .globl    __nacl_audio_stream
    .p2align  NACLENTRYALIGN
__nacl_audio_stream:
.AUDIO_STREAM_SYSCALL_ADDR = NACL_SYSCALL_ADDR(NACL_sys_audio_stream)
    jmp .AUDIO_STREAM_SYSCALL_ADDR

