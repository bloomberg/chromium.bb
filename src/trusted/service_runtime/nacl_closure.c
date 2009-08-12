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

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"

/*
 * This file implements simple closures.  See the header file for
 * usage info.
 *
 * This should be machine-generated code to allow more args.
 *
 * Primary deficiency: we use void * as the generic argument type, and
 * while it suffices for much, it is neither type-safe nor complete
 * (viz, 64-bit types such as int64_t, double, etc won't work).
 */

static struct NaClClosureVtbl const kNaClClosure0Vtbl = {
  NaClClosure0Dtor,
  NaClClosure0Run,
};

struct NaClClosure0 *NaClClosure0Ctor(void  (*fn)(void)) {
  struct NaClClosure0 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure0 ctor\n");
  }
  self->base.vtbl = &kNaClClosure0Vtbl;
  self->fn = fn;
  return self;
}

void NaClClosure0Dtor(struct NaClClosure *vself) {
  struct NaClClosure0 *self = (struct NaClClosure0 *) vself;
  free(self);
}

void NaClClosure0Run(struct NaClClosure *vself) {
  struct NaClClosure0 *self = (struct NaClClosure0 *) vself;
  (*self->fn)();
  NaClClosure0Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure1Vtbl = {
  NaClClosure1Dtor,
  NaClClosure1Run,
};

struct NaClClosure1 *NaClClosure1Ctor(void  (*fn)(void *),
                                      void  *arg1) {
  struct NaClClosure1 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure1 ctor\n");
  }
  self->base.vtbl = &kNaClClosure1Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  return self;
}

void NaClClosure1Dtor(struct NaClClosure *vself) {
  struct NaClClosure1 *self = (struct NaClClosure1 *) vself;
  free(self);
}

void NaClClosure1Run(struct NaClClosure *vself) {
  struct NaClClosure1 *self = (struct NaClClosure1 *) vself;
  (*self->fn)(self->arg1);
  NaClClosure1Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure2Vtbl = {
  NaClClosure2Dtor,
  NaClClosure2Run,
};

struct NaClClosure2 *NaClClosure2Ctor(void  (*fn)(void *, void *),
                                      void  *arg1,
                                      void  *arg2) {
  struct NaClClosure2 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure2 ctor\n");
  }
  self->base.vtbl = &kNaClClosure2Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  return self;
}

void NaClClosure2Dtor(struct NaClClosure *vself) {
  struct NaClClosure2 *self = (struct NaClClosure2 *) vself;
  free(self);
}

void NaClClosure2Run(struct NaClClosure *vself) {
  struct NaClClosure2 *self = (struct NaClClosure2 *) vself;
  (*self->fn)(self->arg1, self->arg2);
  NaClClosure2Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure3Vtbl = {
  NaClClosure3Dtor,
  NaClClosure3Run,
};

struct NaClClosure3 *NaClClosure3Ctor(void  (*fn)(void *, void *, void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3) {
  struct NaClClosure3 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure3 ctor\n");
  }
  self->base.vtbl = &kNaClClosure3Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  return self;
}

void NaClClosure3Dtor(struct NaClClosure *vself) {
  struct NaClClosure3 *self = (struct NaClClosure3 *) vself;
  free(self);
}

void NaClClosure3Run(struct NaClClosure *vself) {
  struct NaClClosure3 *self = (struct NaClClosure3 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3);
  NaClClosure3Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure4Vtbl = {
  NaClClosure4Dtor,
  NaClClosure4Run,
};

struct NaClClosure4 *NaClClosure4Ctor(void  (*fn)(void *, void *,
                                                  void *, void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4) {
  struct NaClClosure4 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure4 ctor\n");
  }
  self->base.vtbl = &kNaClClosure4Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  return self;
}

void NaClClosure4Dtor(struct NaClClosure *vself) {
  struct NaClClosure4 *self = (struct NaClClosure4 *) vself;
  free(self);
}

void NaClClosure4Run(struct NaClClosure *vself) {
  struct NaClClosure4 *self = (struct NaClClosure4 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4);
  NaClClosure4Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure5Vtbl = {
  NaClClosure5Dtor,
  NaClClosure5Run,
};

struct NaClClosure5 *NaClClosure5Ctor(void  (*fn)(void *, void *,
                                                  void *, void *,
                                                  void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5) {
  struct NaClClosure5 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure5 ctor\n");
  }
  self->base.vtbl = &kNaClClosure5Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  self->arg5 = arg5;
  return self;
}

void NaClClosure5Dtor(struct NaClClosure *vself) {
  struct NaClClosure5 *self = (struct NaClClosure5 *) vself;
  free(self);
}

void NaClClosure5Run(struct NaClClosure *vself) {
  struct NaClClosure5 *self = (struct NaClClosure5 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4,
              self->arg5);
  NaClClosure5Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure6Vtbl = {
  NaClClosure6Dtor,
  NaClClosure6Run,
};

struct NaClClosure6 *NaClClosure6Ctor(void  (*fn)(void *, void *,
                                                  void *, void *,
                                                  void *, void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6) {
  struct NaClClosure6 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure6 ctor\n");
  }
  self->base.vtbl = &kNaClClosure6Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  self->arg5 = arg5;
  self->arg6 = arg6;
  return self;
}

void NaClClosure6Dtor(struct NaClClosure *vself) {
  struct NaClClosure6 *self = (struct NaClClosure6 *) vself;
  free(self);
}

void NaClClosure6Run(struct NaClClosure *vself) {
  struct NaClClosure6 *self = (struct NaClClosure6 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4,
              self->arg5, self->arg6);
  NaClClosure6Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure7Vtbl = {
  NaClClosure7Dtor,
  NaClClosure7Run,
};

struct NaClClosure7 *NaClClosure7Ctor(void  (*fn)(void *, void *,
                                                  void *, void *,
                                                  void *, void *,
                                                  void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7) {
  struct NaClClosure7 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure7 ctor\n");
  }
  self->base.vtbl = &kNaClClosure7Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  self->arg5 = arg5;
  self->arg6 = arg6;
  self->arg7 = arg7;
  return self;
}

void NaClClosure7Dtor(struct NaClClosure *vself) {
  struct NaClClosure7 *self = (struct NaClClosure7 *) vself;
  free(self);
}

void NaClClosure7Run(struct NaClClosure *vself) {
  struct NaClClosure7 *self = (struct NaClClosure7 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4,
              self->arg5, self->arg6, self->arg7);
  NaClClosure7Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure8Vtbl = {
  NaClClosure8Dtor,
  NaClClosure8Run,
};

struct NaClClosure8 *NaClClosure8Ctor(void  (*fn)(void *, void *,
                                                  void *, void *,
                                                  void *, void *,
                                                  void *, void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7,
                                      void  *arg8) {
  struct NaClClosure8 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure8 ctor\n");
  }
  self->base.vtbl = &kNaClClosure8Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  self->arg5 = arg5;
  self->arg6 = arg6;
  self->arg7 = arg7;
  self->arg8 = arg8;
  return self;
}

void NaClClosure8Dtor(struct NaClClosure *vself) {
  struct NaClClosure8 *self = (struct NaClClosure8 *) vself;
  free(self);
}

void NaClClosure8Run(struct NaClClosure *vself) {
  struct NaClClosure8 *self = (struct NaClClosure8 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4,
              self->arg5, self->arg6, self->arg7, self->arg8);
  NaClClosure8Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure9Vtbl = {
  NaClClosure9Dtor,
  NaClClosure9Run,
};

struct NaClClosure9 *NaClClosure9Ctor(void  (*fn)(void *, void *,
                                                  void *, void *,
                                                  void *, void *,
                                                  void *, void *,
                                                  void *),
                                      void  *arg1,
                                      void  *arg2,
                                      void  *arg3,
                                      void  *arg4,
                                      void  *arg5,
                                      void  *arg6,
                                      void  *arg7,
                                      void  *arg8,
                                      void  *arg9) {
  struct NaClClosure9 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure9 ctor\n");
  }
  self->base.vtbl = &kNaClClosure9Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  self->arg5 = arg5;
  self->arg6 = arg6;
  self->arg7 = arg7;
  self->arg8 = arg8;
  self->arg9 = arg9;
  return self;
}

void NaClClosure9Dtor(struct NaClClosure *vself) {
  struct NaClClosure9 *self = (struct NaClClosure9 *) vself;
  free(self);
}

void NaClClosure9Run(struct NaClClosure *vself) {
  struct NaClClosure9 *self = (struct NaClClosure9 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4,
              self->arg5, self->arg6, self->arg7, self->arg8,
              self->arg9);
  NaClClosure9Dtor(vself);
}

static struct NaClClosureVtbl const kNaClClosure10Vtbl = {
  NaClClosure10Dtor,
  NaClClosure10Run,
};

struct NaClClosure10 *NaClClosure10Ctor(void  (*fn)(void *, void *,
                                                    void *, void *,
                                                    void *, void *,
                                                    void *, void *,
                                                    void *, void *),
                                        void  *arg1,
                                        void  *arg2,
                                        void  *arg3,
                                        void  *arg4,
                                        void  *arg5,
                                        void  *arg6,
                                        void  *arg7,
                                        void  *arg8,
                                        void  *arg9,
                                        void  *arg10) {
  struct NaClClosure10 *self = malloc(sizeof *self);
  if (NULL == self) {
    NaClLog(LOG_FATAL, "No memory in NaClClosure10 ctor\n");
  }
  self->base.vtbl = &kNaClClosure10Vtbl;
  self->fn = fn;
  self->arg1 = arg1;
  self->arg2 = arg2;
  self->arg3 = arg3;
  self->arg4 = arg4;
  self->arg5 = arg5;
  self->arg6 = arg6;
  self->arg7 = arg7;
  self->arg8 = arg8;
  self->arg9 = arg9;
  self->arg10 = arg10;
  return self;
}

void NaClClosure10Dtor(struct NaClClosure *vself) {
  struct NaClClosure10 *self = (struct NaClClosure10 *) vself;
  free(self);
}

void NaClClosure10Run(struct NaClClosure *vself) {
  struct NaClClosure10 *self = (struct NaClClosure10 *) vself;
  (*self->fn)(self->arg1, self->arg2, self->arg3, self->arg4,
              self->arg5, self->arg6, self->arg7, self->arg8,
              self->arg9, self->arg10);
  NaClClosure10Dtor(vself);
}
