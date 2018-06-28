// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_CLIENT_PUSH_PROMISE_INDEX_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_CLIENT_PUSH_PROMISE_INDEX_H_

#include "net/third_party/quic/core/quic_spdy_client_session_base.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

// QuicClientPushPromiseIndex is the interface to support rendezvous
// between client requests and resources delivered via server push.
// The same index can be shared across multiple sessions (e.g. for the
// same browser users profile), since cross-origin pushes are allowed
// (subject to authority constraints).

class QUIC_EXPORT_PRIVATE QuicClientPushPromiseIndex {
 public:
  // Delegate is used to complete the rendezvous that began with
  // |Try()|.
  class QUIC_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() {}

    // The primary lookup matched request with push promise by URL.  A
    // secondary match is necessary to ensure Vary (RFC 2616, 14.14)
    // is honored.  If Vary is not present, return true.  If Vary is
    // present, return whether designated header fields of
    // |promise_request| and |client_request| match.
    virtual bool CheckVary(const spdy::SpdyHeaderBlock& client_request,
                           const spdy::SpdyHeaderBlock& promise_request,
                           const spdy::SpdyHeaderBlock& promise_response) = 0;

    // On rendezvous success, provides the promised |stream|.  Callee
    // does not inherit ownership of |stream|.  On rendezvous failure,
    // |stream| is |nullptr| and the client should retry the request.
    // Rendezvous can fail due to promise validation failure or RST on
    // promised stream.  |url| will have been removed from the index
    // before |OnRendezvousResult()| is invoked, so a recursive call to
    // |Try()| will return |QUIC_FAILURE|, which may be convenient for
    // retry purposes.
    virtual void OnRendezvousResult(QuicSpdyStream* stream) = 0;
  };

  class QUIC_EXPORT_PRIVATE TryHandle {
   public:
    // Cancel the request.
    virtual void Cancel() = 0;

   protected:
    TryHandle() {}
    ~TryHandle();

   private:
    DISALLOW_COPY_AND_ASSIGN(TryHandle);
  };

  QuicClientPushPromiseIndex();
  virtual ~QuicClientPushPromiseIndex();

  // Called by client code, used to enforce affinity between requests
  // for promised streams and the session the promise came from.
  QuicClientPromisedInfo* GetPromised(const QuicString& url);

  // Called by client code, to initiate rendezvous between a request
  // and a server push stream.  If |request|'s url is in the index,
  // rendezvous will be attempted and may complete immediately or
  // asynchronously.  If the matching promise and response headers
  // have already arrived, the delegate's methods will fire
  // recursively from within |Try()|.  Returns |QUIC_SUCCESS| if the
  // rendezvous was a success. Returns |QUIC_FAILURE| if there was no
  // matching promise, or if there was but the rendezvous has failed.
  // Returns QUIC_PENDING if a matching promise was found, but the
  // rendezvous needs to complete asynchronously because the promised
  // response headers are not yet available.  If result is
  // QUIC_PENDING, then |*handle| will set so that the caller may
  // cancel the request if need be.  The caller does not inherit
  // ownership of |*handle|, and it ceases to be valid if the caller
  // invokes |handle->Cancel()| or if |delegate->OnReponse()| fires.
  QuicAsyncStatus Try(const spdy::SpdyHeaderBlock& request,
                      Delegate* delegate,
                      TryHandle** handle);

  QuicPromisedByUrlMap* promised_by_url() { return &promised_by_url_; }

 private:
  QuicPromisedByUrlMap promised_by_url_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientPushPromiseIndex);
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_CLIENT_PUSH_PROMISE_INDEX_H_
