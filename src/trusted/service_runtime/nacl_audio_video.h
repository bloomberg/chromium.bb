/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service run-time multimedia module init/fini
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_AUDIO_VIDEO_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_AUDIO_VIDEO_H_

#if defined(HAVE_SDL)
extern void NaClMultimediaModuleInit();
extern void NaClMultimediaModuleFini();
#endif

#endif /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_AUDIO_VIDEO_H_ */
