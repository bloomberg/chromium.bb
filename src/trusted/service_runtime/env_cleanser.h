/*
 * Copyright 2009, Google Inc.
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
 * Simple environment cleanser to remove all but a whitelisted set of
 * environment variables deemed safe/appropriate to export to NaCl
 * modules.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ENV_CLEANSER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ENV_CLEANSER_H_

struct NaClEnvCleanser {
  /* private */
  char const **cleansed_environ;
};

void NaClEnvCleanserCtor(struct NaClEnvCleanser *self);

/*
 * Initializes the NaClEnvCleanser.  Filters the environment at envp
 * and saves the result in the object.  The lifetime of the string
 * table and associated strings at envp must be at least that of the
 * NaClEnvCleanser object, and should not change between the call to
 * NaClEnvCleanserInit and to NaClEnvCleanserDtor.
 */
int NaClEnvCleanserInit(struct NaClEnvCleanser *self, char const *const *envp);

/*
 * Returns the filtered environment.
 */
char const *const *NaClEnvCleanserEnvironment(struct NaClEnvCleanser *self);

/*
 * Frees memory associated with the NaClEnvCleanser object.
 */
void NaClEnvCleanserDtor(struct NaClEnvCleanser *self);

#endif  // NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ENV_CLEANSER_H_
