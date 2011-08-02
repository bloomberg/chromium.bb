/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PEPPER_EMU_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PEPPER_EMU_H_

class NaClCommandLoop;
class IMultimedia;
struct UserEvent;

void PepperEmuInitAudio(NaClCommandLoop* ncl, IMultimedia* im);
void PepperEmuInitCore(NaClCommandLoop* ncl, IMultimedia* im);
void PepperEmuInitFileIO(NaClCommandLoop* ncl, IMultimedia* im);
void PepperEmuInitPostMessage(NaClCommandLoop* ncl, IMultimedia* im);
void PepperEmuInit2D(NaClCommandLoop* ncl, IMultimedia* im);
void PepperEmuInit3D(NaClCommandLoop* ncl, IMultimedia* im);

void InvokeAudioStreamCreatedCallback(NaClCommandLoop* ncl,
                                      const UserEvent* event);
void InvokeInputEventCallback(NaClCommandLoop* ncl,
                              UserEvent* event);
#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PEPPER_HANDLER_H_ */
