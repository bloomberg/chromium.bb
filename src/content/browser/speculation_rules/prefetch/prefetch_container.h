// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_CONTAINER_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class CookieManager;
}  // namespace mojom
}  // namespace network

namespace content {

class PrefetchCookieListener;
class PrefetchDocumentManager;
class PrefetchNetworkContext;
class PrefetchService;
class PrefetchedMainframeResponseContainer;

// This class contains the state for a request to prefetch a specific URL.
class CONTENT_EXPORT PrefetchContainer {
 public:
  PrefetchContainer(
      const GlobalRenderFrameHostId& referring_render_frame_host_id,
      const GURL& url,
      const PrefetchType& prefetch_type,
      base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager);
  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // Defines the key to uniquely identify a prefetch.
  using Key = std::pair<GlobalRenderFrameHostId, GURL>;
  Key GetPrefetchContainerKey() const {
    return std::make_pair(referring_render_frame_host_id_, url_);
  }

  // The ID of the render frame host that triggered the prefetch.
  GlobalRenderFrameHostId GetReferringRenderFrameHostId() const {
    return referring_render_frame_host_id_;
  }

  // The URL that will potentially be prefetched.
  GURL GetURL() const { return url_; }

  // The type of this prefetch. Controls how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  base::WeakPtr<PrefetchContainer> GetWeakPtr() const {
    return weak_method_factory_.GetWeakPtr();
  }

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called.
  void SetPrefetchStatus(PrefetchStatus prefetch_status) {
    prefetch_status_ = prefetch_status;
  }
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const;

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // After the initial eligiblity check for |url_|, a
  // |PrefetchCookieListener| listens for any changes to the cookies
  // associated with |url_|. If these cookies change, then no prefetched
  // resources will be served.
  void RegisterCookieListener(network::mojom::CookieManager* cookie_manager);
  void StopCookieListener();
  bool HaveDefaultContextCookiesChanged() const;

  // Before a prefetch can be served, any cookies added to the isolated network
  // context must be copied over to the default network context. These functions
  // are used to check and update the status of this process.
  bool IsIsolatedCookieCopyInProgress() const;
  void OnIsolatedCookieCopyStart();
  void OnIsolatedCookieCopyComplete();
  void SetOnCookieCopyCompleteCallback(base::OnceClosure callback);

  // The network context used to make network requests for this prefetch.
  PrefetchNetworkContext* GetOrCreateNetworkContext(
      PrefetchService* prefetch_service);
  PrefetchNetworkContext* GetNetworkContext() { return network_context_.get(); }

  // The URL loader used to make the network requests for this prefetch.
  void TakeURLLoader(std::unique_ptr<network::SimpleURLLoader> loader);
  network::SimpleURLLoader* GetLoader() { return loader_.get(); }
  void ResetURLLoader();

  // The |PrefetchDocumentManager| that requested |this|.
  PrefetchDocumentManager* GetPrefetchDocumentManager() const;

  // Whether or not |this| has a prefetched response.
  bool HasValidPrefetchedResponse(base::TimeDelta cacheable_duration) const;

  // |this| takes ownership of the given |prefetched_response|.
  void TakePrefetchedResponse(
      std::unique_ptr<PrefetchedMainframeResponseContainer>
          prefetched_response);

  // Releases ownership of |prefetched_response_| from |this| and gives it to
  // the caller.
  std::unique_ptr<PrefetchedMainframeResponseContainer>
  ReleasePrefetchedResponse();

 private:
  // The ID of the render frame host that triggered the prefetch.
  GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The URL that will potentially be prefetched
  GURL url_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The |PrefetchDocumentManager| that requested |this|. Initially it owns
  // |this|, but once the network request for the prefetch is started,
  // ownernship is transferred to |PrefetchService|.
  base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager_;

  // The current status, if any, of the prefetch.
  absl::optional<PrefetchStatus> prefetch_status_;

  // Whether this prefetch is a decoy or not. If the prefetch is a decoy then
  // any prefetched resources will not be served.
  bool is_decoy_ = false;

  // This tracks whether the cookies associated with |url_| have changed at some
  // point after the initial eligibility check.
  std::unique_ptr<PrefetchCookieListener> cookie_listener_;

  // The network context used to prefetch |url_|.
  std::unique_ptr<PrefetchNetworkContext> network_context_;

  // The URL loader used to prefetch |url_|.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // The prefetched response for |url_|.
  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response_;

  // The time at which |prefetched_response_| was received. This is used to
  // determine whether or not |prefetched_response_| is stale.
  absl::optional<base::TimeTicks> prefetch_received_time_;

  // The different possible states of the cookie copy process.
  enum class CookieCopyStatus {
    kNotStarted,
    kInProgress,
    kCompleted,
  };

  // The current state of the cookie copy process for this prefetch.
  CookieCopyStatus cookie_copy_status_ = CookieCopyStatus::kNotStarted;

  // A callback that runs once |cookie_copy_status_| is set to |kCompleted|.
  base::OnceClosure on_cookie_copy_complete_callback_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_CONTAINER_H_
