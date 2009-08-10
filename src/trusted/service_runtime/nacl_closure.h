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
 * NaCl service runtime closure.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CLOSURE_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CLOSURE_H_

#include "native_client/src/trusted/platform/nacl_sync.h"

/*
 * This file contains declarations for simple closures.  They
 * self-delete when run.  The ctors are factory fns, rather than
 * placement-new style ctors.  If there is no memory, we immediately
 * abort.  The dtor frees.
 *
 * There are currently subclasses to handle at most 10 arguments.
 * Beyond that, wrap the args in a struct and pass a pointer to it!
 *
 * This should be machine-generated code to allow more args.
 *
 * Primary deficiency: we use void * as the generic argument type, and
 * while it suffices for much, it is neither type-safe nor complete
 * (viz, 64-bit types such as int64_t, double, etc won't work).
 */

struct NaClClosure;

/* pure virtual */

struct NaClClosureVtbl {
  void (*Dtor)(struct NaClClosure *);
  void (*Run)(struct NaClClosure *);
};

struct NaClClosure {
  struct NaClClosureVtbl const  *vtbl;
};

struct NaClClosure0 {
  struct NaClClosure  base;
  void                (*fn)(void);
};

struct NaClClosure0 *NaClClosure0Ctor(void  (*fn)(void)) NACL_WUR;
void NaClClosure0Dtor(struct NaClClosure *vself);
void NaClClosure0Run(struct NaClClosure *vself);

struct NaClClosure1 {
  struct NaClClosure  base;
  void                (*fn)(void *arg1);
  void                *arg1;
};

struct NaClClosure1 *NaClClosure1Ctor(void  (*fn)(void *arg1),
                                      void  *arg1);
void NaClClosure1Dtor(struct NaClClosure *vself);
void NaClClosure1Run(struct NaClClosure *vself);

struct NaClClosure2 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *);
  void                *arg1;
  void                *arg2;
};

struct NaClClosure2 *NaClClosure2Ctor(void  (*fn)(void *arg1,
                                                  void *arg2),
                                      void  *arg1,
                                      void  *arg2) NACL_WUR;
void NaClClosure2Dtor(struct NaClClosure *self);
void NaClClosure2Run(struct NaClClosure *vself);

struct NaClClosure3 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
};

struct NaClClosure3 *NaClClosure3Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3) NACL_WUR;
void NaClClosure3Dtor(struct NaClClosure *self);
void NaClClosure3Run(struct NaClClosure *vself);

struct NaClClosure4 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
  void                *arg4;
};

struct NaClClosure4 *NaClClosure4Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3,
                                                  void *arg4),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4) NACL_WUR;
void NaClClosure4Dtor(struct NaClClosure *self);
void NaClClosure4Run(struct NaClClosure *vself);

struct NaClClosure5 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *,
                            void *);
  void                *arg1, *arg2, *arg3, *arg4;
  void                *arg5;
};

struct NaClClosure5 *NaClClosure5Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3,
                                                  void *arg4,
                                                  void *arg5),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5) NACL_WUR;
void NaClClosure5Dtor(struct NaClClosure *self);
void NaClClosure5Run(struct NaClClosure *vself);

struct NaClClosure6 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *,
                            void *, void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
  void                *arg4;
  void                *arg5;
  void                *arg6;
};

struct NaClClosure6 *NaClClosure6Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3,
                                                  void *arg4,
                                                  void *arg5,
                                                  void *arg6),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6) NACL_WUR;
void NaClClosure6Dtor(struct NaClClosure *self);
void NaClClosure6Run(struct NaClClosure *vself);

struct NaClClosure7 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *,
                            void *, void *, void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
  void                *arg4;
  void                *arg5;
  void                *arg6;
  void                *arg7;
};

struct NaClClosure7 *NaClClosure7Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3,
                                                  void *arg4,
                                                  void *arg5,
                                                  void *arg6,
                                                  void *arg7),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7) NACL_WUR;
void NaClClosure7Dtor(struct NaClClosure *self);
void NaClClosure7Run(struct NaClClosure *vself);

struct NaClClosure8 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *,
                            void *, void *, void *, void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
  void                *arg4;
  void                *arg5;
  void                *arg6;
  void                *arg7;
  void                *arg8;
};

struct NaClClosure8 *NaClClosure8Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3,
                                                  void *arg4,
                                                  void *arg5,
                                                  void *arg6,
                                                  void *arg7,
                                                  void *arg8),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7,
                                      void  *arg8) NACL_WUR;
void NaClClosure8Dtor(struct NaClClosure *self);
void NaClClosure8Run(struct NaClClosure *vself);

struct NaClClosure9 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *,
                            void *, void *, void *, void *,
                            void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
  void                *arg4;
  void                *arg5;
  void                *arg6;
  void                *arg7;
  void                *arg8;
  void                *arg9;
};

struct NaClClosure9 *NaClClosure9Ctor(void  (*fn)(void *arg1,
                                                  void *arg2,
                                                  void *arg3,
                                                  void *arg4,
                                                  void *arg5,
                                                  void *arg6,
                                                  void *arg7,
                                                  void *arg8,
                                                  void *arg9),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7,
                                      void  *arg8,
                                      void  *arg9) NACL_WUR;
void NaClClosure9Dtor(struct NaClClosure *self);
void NaClClosure9Run(struct NaClClosure *vself);

struct NaClClosure10 {
  struct NaClClosure  base;
  void                (*fn)(void *, void *, void *, void *,
                            void *, void *, void *, void *,
                            void *, void *);
  void                *arg1;
  void                *arg2;
  void                *arg3;
  void                *arg4;
  void                *arg5;
  void                *arg6;
  void                *arg7;
  void                *arg8;
  void                *arg9;
  void                *arg10;
};

struct NaClClosure10 *NaClClosure10Ctor(void  (*fn)(void *arg1,
                                                    void *arg2,
                                                    void *arg3,
                                                    void *arg4,
                                                    void *arg5,
                                                    void *arg6,
                                                    void *arg7,
                                                    void *arg8,
                                                    void *arg9,
                                                    void *arg10),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7,
                                      void  *arg8,
                                      void  *arg9,
                                      void  *arg10) NACL_WUR;
void NaClClosure10Dtor(struct NaClClosure *self);
void NaClClosure10Run(struct NaClClosure *vself);

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CLOSURE_H_ */
