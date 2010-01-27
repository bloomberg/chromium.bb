/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime Object Proxy.  Used for the NaCl virtual file
 * system and as a generic object proxy mechanism.
 *
 * NB: this code has not been integrated with the rest of NaCl, and is
 * likely to change.
 *
 * Note that for NaCl file system use, the proxy is an NFS-like "file
 * handle" and the object is a NaClDesc object.  These objects may
 * contain access rights -- names of kernel managed objects, which are
 * reference counted -- and, at least for some uses, the proxy may be
 * used to acquire the orignal object via access-rights transfer.
 * Note that two transfers of the same object results in two distinct
 * names referring to the same underlying kernel object.
 *
 * In the latter, generic object proxy case, the object being proxied
 * is typically a generic pointer (void *) in the proxy name space
 * owner's address space, and all operations (method calls, etc) are
 * remoted.
 *
 * Transfers of proxies is just a data copy and thus should be
 * efficient.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_FS_OBJ_PROXY_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_FS_OBJ_PROXY_H_

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/generic_container/container.h"

EXTERN_C_BEGIN

/*
 * An object proxy object is used with two primary operations: (1)
 * when sending an object out, we check if we have already assigned an
 * ID to the object, and if not, generate one; we remember the object
 * and its ID and return the ID.  (2) when receiving an object ID (for
 * an object that we had sent out), find the corresponding object.
 */

struct NaClObjProxyEntry;  /* fwd */

struct NaClObjProxy {
  /* private: */
  struct NaClMutex          mu;
  struct NaClContainer      *obj_to_name;
  struct NaClContainer      *name_to_obj;
  int                       name_bytes;
  struct NaClHashFunctor    *obj_to_name_functor;
  struct NaClHashFunctor    *name_to_obj_functor;
  struct NaClSecureRngIf    *rng;
  struct NaClObjProxyEntry  *key;
};

/**
 * Constructor for a NaClObjProxy object.  Return zero on failure (bool).
 *
 * name_bytes is the length of the randomly generated names to be used
 * with the mapping.  Whenever an object is inserted into the object
 * proxy, a randomly chosen name of that many bytes is generated to
 * represent that object.  No explicit collision detection is done, so
 * name_bytes should be sufficiently large that birthday collisions
 * would be extremely unlikely.  For example, if name_bytes 8, then
 * assuming that the random number generator is good, the probability
 * of collision becomes unacceptable approximately when 2**(8*8/2) =
 * 2**32 objects have been allocated.
 *
 * The choice of the RNG is fixed.  The NaClSecureRng interface does
 * not use virtual functions, so we cannot (yet) provide a
 * debug/testing ctor that accepts a custom RNG.
 *
 * Since the object proxy does not take ownership of the objects, it
 * is the responsibility of the caller to ensure that the objects'
 * lifetime is at least that of the lifetime of the object proxy
 * object.
 *
 * Ownership of the rng is NOT passed to the NaClObjProxy object; it
 * is the responsibility of the caller to ensure that (1) its lifetime
 * is at least that of the NaClObjProxy object, and (2) the
 * NaClSecureRng is either itself thread-safe, or that no other thread
 * will access its methods.
 *
 * NB: NaClSecureRngModuleInit must have been invoked prior to any
 * invocation of NaClObjProxyCtor.
 */
int NaClObjProxyCtor(struct NaClObjProxy    *self,
                     struct NaClSecureRngIf *rng,
                     int                    name_bytes);

/**
 * Destroys the ObjProxy object.
 */
void NaClObjProxyDtor(struct NaClObjProxy *self);

/**
 * Given an object, find its name.  May return NULL if the object is
 * not found.
 */
uint8_t const *NaClObjProxyFindNameByObj(struct NaClObjProxy *self, void *obj);

/**
 * Given an object's name, find it and return the object via the out
 * argument.  Returns non-zero if found, so a NULL pointer can be
 * proxied.
 */
int NaClObjProxyFindObjByName(struct NaClObjProxy *self,
                              uint8_t const       *name,
                              void                **out);

/**
 * Given a new object, assign a new name to it and insert the mapping
 * into the NaClObjProxy object; returns the new name.  The returned
 * name is valid as long as the object remains in the object proxy.
 */
uint8_t const *NaClObjProxyInsert(struct NaClObjProxy *self, void *obj);

/**
 * Remove a previously inserted object.
 */
int NaClObjProxyRemove(struct NaClObjProxy *self, void *obj);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_FS_OBJ_PROXY_H_ */
