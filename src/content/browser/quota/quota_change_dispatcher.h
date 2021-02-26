// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_QUOTA_CHANGE_DISPATCHER_H_
#define CONTENT_BROWSER_QUOTA_QUOTA_CHANGE_DISPATCHER_H_

#include <map>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom.h"
#include "url/origin.h"

class TimeDelta;

namespace content {

// Dispatches a storage pressure event to listeners across multiple origins.
//
// This class handles dispatching the event with randomized delays,
// to avoid creating a cross-origin user identifier.
//
// There is one instance per QuotaContext instance. All methods must
// be called on the same sequence.
class CONTENT_EXPORT QuotaChangeDispatcher
    : public base::RefCountedThreadSafe<QuotaChangeDispatcher> {
 public:
  QuotaChangeDispatcher();

  QuotaChangeDispatcher(const QuotaChangeDispatcher&) = delete;
  QuotaChangeDispatcher& operator=(const QuotaChangeDispatcher&) = delete;

  // Dispatch OnQuotaChange for every origin and its corresponding listeners.
  void DispatchEvents();
  void DispatchEventsForOrigin(const url::Origin& origin);
  void AddChangeListener(
      const url::Origin& origin,
      mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener);
  void OnRemoteDisconnect(const url::Origin& origin,
                          mojo::RemoteSetElementId id);

 private:
  friend class QuotaChangeDispatcherTest;
  friend class QuotaContext;
  friend class base::RefCountedThreadSafe<QuotaChangeDispatcher>;

  ~QuotaChangeDispatcher();

  struct DelayedOriginListener {
    DelayedOriginListener();
    ~DelayedOriginListener();
    // This delay is used to introduce noise to the event, to prevent
    // bad actors from using the event to determine cross-origin
    // resource size or to correlate and identify users across origins/profiles.
    base::TimeDelta delay;
    mojo::RemoteSet<blink::mojom::QuotaChangeListener> listeners;
  };
  // Stores all of the listeners associated with a unique origin
  // corresponding to a randomized delay for that origin.
  std::map<url::Origin, DelayedOriginListener> listeners_by_origin_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<QuotaChangeDispatcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_CHANGE_DISPATCHER_H_