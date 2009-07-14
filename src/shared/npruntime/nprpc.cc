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


#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"


#include "native_client/src/shared/npruntime/npnavigator.h"

namespace nacl {

void* RpcStack::Push(NPIdentifier name) {
  if (NPNavigator::IdentifierIsString(name)) {
    NPUTF8* utf8 = NPN_UTF8FromIdentifier(name);
    assert(utf8 != NULL);
    size_t length = strlen(utf8) + 1;
    void* buffer = Alloc(length);
    if (buffer != NULL) {
      memmove(buffer, utf8, length);
    }
    NPN_MemFree(utf8);
    return buffer;
  }
  return top();
}

NPIdentifier RpcArg::GetIdentifier() {
  NPIdentifier* name = reinterpret_cast<NPIdentifier*>(base_);
  base_ = Step(base_, sizeof(NPIdentifier));
  if (base_ == NULL) {  // TODO(shiki): Should be checked up-front.
    return reinterpret_cast<NPIdentifier*>(~static_cast<uintptr_t>(0u));
  }
  if (!NPNavigator::IdentifierIsString(*name)) {
    if (bridge_->is_webkit()) {
      uintptr_t intid = reinterpret_cast<uintptr_t>(*name);
      char index[11];
      // NOTE(robertm): there a signedness issues and this
      //                and the behavior for large ids is unclear
      SNPRINTF(index, sizeof(index), "%u", (unsigned)(intid >> 1));
      *name = NPN_GetStringIdentifier(index);
    }
    return *name;
  }
  if (opt_) {
    *name = NPN_GetStringIdentifier(opt_);
    opt_ = Step(opt_, strlen(opt_) + 1);
  } else {              // TODO(shiki): Should be checked up-front.
    return reinterpret_cast<NPIdentifier*>(~static_cast<uintptr_t>(0u));
  }
  return *name;
}

void* RpcStack::Push(const NPVariant& variant, bool param) {
  if (NPVARIANT_IS_STRING(variant)) {
    void* buffer = Alloc(NPVARIANT_TO_STRING(variant).utf8length + 1);
    if (buffer != NULL) {
      // Note in Safari under OS X, strings are not terminated by zero.
      memmove(buffer,
              NPVARIANT_TO_STRING(variant).utf8characters,
              NPVARIANT_TO_STRING(variant).utf8length);
      static_cast<char*>(buffer)[NPVARIANT_TO_STRING(variant).utf8length] = 0;
    }
    return buffer;
  }
  if (NPVARIANT_IS_OBJECT(variant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(variant);
    return Push(object);
  }
  if (NPVARIANT_IS_HANDLE(variant)) {
    NPCapability* capability =
        static_cast<NPCapability*>(Alloc(sizeof(NPCapability)));
    if (capability != NULL) {
      bridge_->add_handle(NPVARIANT_TO_HANDLE(variant));
      capability->pid = NPCapability::kHandle;
      capability->object = NULL;
    }
    return capability;
  }
  return top();
}

const NPVariant* RpcArg::GetVariant(bool param) {
  static const NPVariant null = { NPVariantType_Null, {0} };

  NPVariant* variant = reinterpret_cast<NPVariant*>(base_);
  base_ = Step(base_, sizeof(NPVariant));
  if (base_ == NULL) {
    return &null;
  }
  if (NPVARIANT_IS_STRING(*variant)) {
    if (opt_ && NPVARIANT_TO_STRING(*variant).utf8characters) {
      STRINGZ_TO_NPVARIANT(opt_, *variant);
      uint32_t length = NPVARIANT_TO_STRING(*variant).utf8length;
      if (!param) {
        // Note we cannot use strdup() here since NPN_ReleaseVariantValue() is
        // unhappy with the string allocated by strdup() in some browser
        // versions.
        void* copy;
        if (0 < length && (copy = NPN_MemAlloc(length + 1))) {
          memmove(copy,
                  NPVARIANT_TO_STRING(*variant).utf8characters,
                  length + 1);
          STRINGN_TO_NPVARIANT(static_cast<NPUTF8*>(copy), length, *variant);
        } else {
          STRINGN_TO_NPVARIANT(NULL, 0, *variant);
        }
      }
      opt_ = Step(opt_, length + 1);
    } else {
      STRINGN_TO_NPVARIANT(NULL, 0, *variant);
    }
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    NPCapability* capability = reinterpret_cast<NPCapability*>(opt_);
    opt_ = Step(opt_, sizeof(NPCapability));
    if (opt_ == NULL) {
      NULL_TO_NPVARIANT(*variant);
    } else if (!capability->IsHandle()) {
      NPObject* object = bridge_->CreateProxy(*capability);
      if (object == NULL) {
        NULL_TO_NPVARIANT(*variant);
      } else {
        OBJECT_TO_NPVARIANT(object, *variant);
      }
    } else if (0 < GetHandleCount()) {
      // Convert to NPVariantType_Handle.
      HtpHandle handle = GetHandle();
      HANDLE_TO_NPVARIANT(handle, *variant);
    } else {
      NULL_TO_NPVARIANT(*variant);
    }
  } else if (NPVARIANT_IS_HANDLE(*variant)) {
    NPObject* object = GetHandleObject();
    if (object == NULL) {
      NULL_TO_NPVARIANT(*variant);
    } else {
      OBJECT_TO_NPVARIANT(object, *variant);
    }
  }
  return variant;
}

void* RpcStack::Push(NPObject* object) {
  NPCapability* capability =
      static_cast<NPCapability*>(Alloc(sizeof(NPCapability)));
  if (capability == NULL) {
    return NULL;
  }
  if (NPHandleObject::IsInstance(object)) {
    NPHandleObject* object_handle = static_cast<NPHandleObject*>(object);
    bridge_->add_handle(object_handle->handle());
    capability->pid = NPCapability::kHandle;
    capability->object = NULL;
  } else {
    bridge_->CreateStub(object, static_cast<NPCapability*>(capability));
  }
  return capability;
}

NPObject* RpcArg::GetObject() {
  NPCapability* capability = reinterpret_cast<NPCapability*>(opt_);
  opt_ = Step(opt_, sizeof(NPCapability));
  if (opt_ == NULL) {
    return NULL;
  }
  if (capability->IsHandle()) {
    return GetHandleObject();
  }
  return bridge_->CreateProxy(*capability);
}

const char* RpcArg::GetString() {
  const char* string = reinterpret_cast<const char*>(base_);
  if (base_ == NULL) {
    return NULL;
  }
  size_t length = strnlen(string, limit_ - base_);
  if (length == static_cast<size_t>(limit_ - base_)) {
    return NULL;
  }
  base_ = Step(base_, length + 1);
  if (base_ == NULL) {
    return NULL;
  }
  return string;
}

NPCapability* RpcArg::GetCapability() {
  NPCapability* capability = reinterpret_cast<NPCapability*>(base_);
  base_ = Step(base_, sizeof(NPCapability));
  if (base_ == NULL) {
    return NULL;
  }
  return capability;
}

NPSize* RpcArg::GetSize() {
  NPSize* size = reinterpret_cast<NPSize*>(base_);
  base_ = Step(base_, sizeof(*size));
  if (base_ == NULL) {
    return NULL;
  }
  return size;
}

NPRect* RpcArg::GetRect() {
  NPRect* rect = reinterpret_cast<NPRect*>(base_);
  base_ = Step(base_, sizeof(*rect));
  if (base_ == NULL) {
    return NULL;
  }
  return rect;
}

void* ConvertNPVariants(const NPVariant* variant, void* target,
                        size_t peer_npvariant_size,
                        size_t arg_count) {
  // NPVariant is defined in npruntime.h.
  // If peer_npvariant_size is 12, offsetof(NPVariant, value) is 4.
  // If peer_npvariant_size is 16, offsetof(NPVariant, value) is 8.
  if (kParamMax < arg_count) {
    return NULL;
  }
  if (sizeof(NPVariant) == 12 && peer_npvariant_size == 16) {
    while (0 < arg_count--) {
      memmove(target, variant, 4);
      memmove(static_cast<char*>(target) + 8,
              reinterpret_cast<const char*>(variant) + 4,
              8);
      ++variant;
      target = static_cast<char*>(target) + peer_npvariant_size;
    }
  } else if (sizeof(NPVariant) == 16 && peer_npvariant_size == 12) {
    while (0 < arg_count--) {
      memmove(target, variant, 4);
      memmove(static_cast<char*>(target) + 4,
              reinterpret_cast<const char*>(variant) + 8,
              8);
      ++variant;
      target = static_cast<char*>(target) + peer_npvariant_size;
    }
  } else if (sizeof(NPVariant) == peer_npvariant_size) {
    memmove(target, variant, arg_count * peer_npvariant_size);
    target = static_cast<char*>(target) + arg_count * peer_npvariant_size;
  } else {
    fprintf(stderr, "convertNPVariant: unexpected NPVariant size.\n");
    exit(EXIT_FAILURE);
  }
  return target;
}

}  // namespace nacl
