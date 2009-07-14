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


// NaCl-NPAPI Interface

#include "native_client/src/include/portability_string.h"


#include <stdlib.h>
#include "native_client/src/shared/npruntime/npnavigator.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

namespace nacl {

NPNavigator::NPNavigator(NPP npp, int* argc, char* argv[])
    : NPBridge(npp),
      argc_(argc),
      argv_(argv),
      notify_(0),
      notify_data_(NULL),
      bitmap_shm_(kInvalidHtpHandle) {
  if (3 <= *argc_) {
    // The last two arguments are the handle number for the connection to the
    // plungin, and for the size of NPVariant in the plugin.
    // TODO(robertm): explain reinterpret_cast vs static_cast
#if NACL_WINDOWS
    set_channel(reinterpret_cast<Handle>(STRTOULL(argv[*argc - 2], NULL, 10)));
#else
    set_channel(static_cast<Handle>(STRTOULL(argv[*argc - 2], NULL, 10)));
#endif
    set_peer_npvariant_size(strtol(argv[*argc - 1], NULL, 10));
    *argc_ -= 2;
  }
}

NPNavigator::~NPNavigator() {
  std::set<const NPUTF8*, StringCompare>::iterator i;
  for (i = string_set_.begin(); i != string_set_.end(); ++i) {
    free(const_cast<char*>(*i));
  }
  Close(bitmap_shm_);
}

int NPNavigator::Dispatch(RpcHeader* request, int len) {
  int result;
  switch (request->type) {
    case RPC_NEW:
      result = New(request, len);
      break;
    case RPC_DESTROY:
      result = Destroy(request, len);
      break;
    case RPC_NOTIFY_URL:
      return URLNotify(request, len);
      break;
    default:
      return NPBridge::Dispatch(request, len);
      break;
  }
  return result;
}

int NPNavigator::New(RpcHeader* request, int len) {
  RpcArg arg(this, request, len);
  arg.Step(sizeof(RpcHeader));
  NPSize* window_size = arg.GetSize();
  if (0 < arg.GetHandleCount()) {
    bitmap_shm_ = arg.GetHandle();
    arg.CloseUnusedHandles();
  }

  set_peer_pid(request->pid);
  IOVec vecv[2];
  IOVec* vecp = vecv;
  vecp->base = request;
  vecp->length = sizeof(RpcHeader);
  ++vecp;
  int argc = 0;
  char* argn[kMaxArg / 2];
  char* argv[kMaxArg / 2];
  for (int i = 1; i < *argc_; ++argc) {
    argn[argc] = argv_[i++];
    argv[argc] = argv_[i++];
  }
  request->error_code = NPP_New(
      const_cast<char *>("application/nacl-npapi-plugin"),
      npp(),
      0,  // TBD mode
      argc, argn, argv,
      NULL);

  if (request->error_code == NPERR_NO_ERROR) {
    NPObject* object = NPP_GetScriptableInstance(npp());
    if (object) {
      NPCapability capability;
      CreateStub(object, &capability);
      vecp->base = &capability;
      vecp->length = sizeof capability;
      ++vecp;
    }

    if (bitmap_shm_ != kInvalidHtpHandle &&
        0 < window_size->width && window_size->height) {
      window_.window = reinterpret_cast<void*>(bitmap_shm_);
      window_.x = 0;
      window_.y = 0;
      window_.width = window_size->width;
      window_.height = window_size->height;
      window_.clipRect.top = 0;
      window_.clipRect.left = 0;
      window_.clipRect.right = window_.width;
      window_.clipRect.bottom = window_.height;
      window_.type = NPWindowTypeDrawable;
      NPP_SetWindow(npp(), &window_);
    }
  }

  return Respond(request, vecv, vecp - vecv);
}

int NPNavigator::Destroy(RpcHeader* request, int len) {
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = request;
  vecp->length = sizeof(RpcHeader);
  ++vecp;
  request->error_code = NPP_Destroy(npp(), NULL);
  return Respond(request, vecv, vecp - vecv);
}

int NPNavigator::GetPluginScriptableObject(RpcHeader* request, int len) {
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = request;
  vecp->length = sizeof(RpcHeader);
  ++vecp;
  return Respond(request, vecv, vecp - vecv);
}

void NPNavigator::SetStatus(const char* message) {
  RpcHeader request;
  request.type = RPC_SET_STATUS;
  IOVec vecv[2];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = const_cast<char*>(message);
  vecp->length = strlen(message) + 1;
  ++vecp;
  Request(&request, vecv, vecp - vecv, NULL);
}

NPError NPNavigator::GetValue(nacl::RpcType type, void* value) {
  RpcHeader request;
  request.type = type;
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  int length;
  RpcHeader* reply = Request(&request, vecv, vecp - vecv, &length);
  if (reply == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  RpcArg result(this, reply, length);
  result.Step(sizeof(RpcHeader));
  if (reply->error_code == NPERR_NO_ERROR) {
    *static_cast<NPObject**>(value) = result.GetObject();
  }
  return reply->error_code;
}

void NPNavigator::InvalidateRect(NPRect* invalid_rect) {
  RpcHeader request;
  request.type = RPC_INVALIDATE_RECT;
  IOVec vecv[2];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = invalid_rect;
  vecp->length = sizeof(*invalid_rect);
  ++vecp;
  int length;
  Request(&request, vecv, vecp - vecv, &length);
}

void NPNavigator::ForceRedraw() {
  RpcHeader request;
  request.type = RPC_FORCE_REDRAW;
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  int length;
  Request(&request, vecv, vecp - vecv, &length);
}

NPObject* NPNavigator::CreateArray() {
  RpcHeader request;
  request.type = RPC_CREATE_ARRAY;
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  int length;
  RpcHeader* reply = Request(&request, vecv, vecp - vecv, &length);
  if (reply == NULL) {
    return NULL;
  }
  RpcArg result(this, reply, length);
  result.Step(sizeof request);
  return result.GetObject();
}

NPError NPNavigator::OpenURL(const char* url, void* notify_data,
                             void (*notify)(const char* url, void* notify_data,
                                            HtpHandle handle)) {
  if (notify_ != NULL || url == NULL || notify == NULL) {
    return NPERR_GENERIC_ERROR;
  }

  RpcHeader request;
  request.type = RPC_OPEN_URL;
  IOVec vecv[2];
  IOVec* vecp = vecv;
  vecp->base = &request;
  vecp->length = sizeof request;
  ++vecp;
  vecp->base = const_cast<char*>(url);
  vecp->length = strlen(url) + 1;
  ++vecp;
  int length;
  RpcHeader* reply = Request(&request, vecv, vecp - vecv, &length);
  if (reply == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  NPError nperr = static_cast<NPError>(reply->error_code);
  if (nperr == NPERR_NO_ERROR) {
    notify_ = notify;
    notify_data_ = notify_data;
    url_ = STRDUP(url);
  }
  return nperr;
}

int NPNavigator::URLNotify(RpcHeader* request, int len) {
  IOVec vecv[1];
  IOVec* vecp = vecv;
  vecp->base = request;
  vecp->length = sizeof(RpcHeader);
  ++vecp;

  RpcArg arg(this, request, len);
  HtpHandle received_handle = kInvalidHtpHandle;
  if (0 < arg.GetHandleCount()) {
    received_handle = arg.GetHandle();
    arg.CloseUnusedHandles();
  }
  arg.Step(sizeof(RpcHeader));
  uint32_t reason = request->error_code;
  if (reason != NPRES_DONE) {
    Close(received_handle);
    received_handle = kInvalidHtpHandle;
    // Cleanup, ensuring that the notify_ callback won't get a closed
    // handle; This is also how the callback knows that there was a
    // problem.
  }
  if (notify_) {
    notify_(url_, notify_data_, received_handle);
    notify_ = 0;
    notify_data_ = NULL;
    free(url_);
    url_ = 0;
  }
  clear_handle_count();
  return Respond(request, vecv, vecp - vecv);
}

const NPUTF8* NPNavigator::GetStringIdentifier(const NPUTF8* name) {
  std::set<const NPUTF8*, StringCompare>::iterator i;
  i = string_set_.find(name);
  if (i != string_set_.end()) {
    return *i;
  }
  name = STRDUP(name);
  if (!name) {
    return NULL;
  }
  std::pair<std::set<const NPUTF8*, StringCompare>::iterator, bool> result;
  result = string_set_.insert(name);
  return *result.first;
}

}  // namespace nacl
