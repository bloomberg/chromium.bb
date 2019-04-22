// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
#define CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "content/public/common/previews_state.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebDocumentLoader;
}

namespace content {

class DocumentState;
class NavigationState;

// Stores internal state per WebDocumentLoader.
class InternalDocumentStateData : public base::SupportsUserData::Data {
 public:
  InternalDocumentStateData();
  ~InternalDocumentStateData() override;

  static InternalDocumentStateData* FromDocumentLoader(
      blink::WebDocumentLoader* document_loader);
  static InternalDocumentStateData* FromDocumentState(DocumentState* ds);

  void CopyFrom(InternalDocumentStateData* other);

  int http_status_code() const { return http_status_code_; }
  void set_http_status_code(int http_status_code) {
    http_status_code_ = http_status_code;
  }

  // True if the user agent was overridden for this page.
  bool is_overriding_user_agent() const { return is_overriding_user_agent_; }
  void set_is_overriding_user_agent(bool state) {
    is_overriding_user_agent_ = state;
  }

  // True if we have to reset the scroll and scale state of the page
  // after the provisional load has been committed.
  bool must_reset_scroll_and_scale_state() const {
    return must_reset_scroll_and_scale_state_;
  }
  void set_must_reset_scroll_and_scale_state(bool state) {
    must_reset_scroll_and_scale_state_ = state;
  }

  // Sets the cache policy. The cache policy is only used if explicitly set and
  // by default is not set. You can mark a NavigationState as not having a cache
  // state by way of clear_cache_policy_override.
  void set_cache_policy_override(blink::mojom::FetchCacheMode cache_policy) {
    cache_policy_override_ = cache_policy;
    cache_policy_override_set_ = true;
  }
  blink::mojom::FetchCacheMode cache_policy_override() const {
    return cache_policy_override_;
  }
  void clear_cache_policy_override() {
    cache_policy_override_set_ = false;
    cache_policy_override_ = blink::mojom::FetchCacheMode::kDefault;
  }
  bool is_cache_policy_override_set() const {
    return cache_policy_override_set_;
  }

  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }
  void set_effective_connection_type(
      net::EffectiveConnectionType effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

  PreviewsState previews_state() const { return previews_state_; }
  void set_previews_state(PreviewsState previews_state) {
    previews_state_ = previews_state;
  }

  // This is a fake navigation request id, which we send to the browser process
  // together with metrics. Note that renderer does not actually issue a request
  // for navigation (browser does it instead), but still reports metrics for it.
  // See content::mojom::ResourceLoadInfo.
  int request_id() const { return request_id_; }
  void set_request_id(int request_id) { request_id_ = request_id; }

  NavigationState* navigation_state() { return navigation_state_.get(); }
  void set_navigation_state(std::unique_ptr<NavigationState> navigation_state);

 private:
  int http_status_code_;
  bool is_overriding_user_agent_;
  bool must_reset_scroll_and_scale_state_;
  bool cache_policy_override_set_;
  blink::mojom::FetchCacheMode cache_policy_override_;
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  PreviewsState previews_state_ = PREVIEWS_UNSPECIFIED;
  int request_id_ = -1;
  std::unique_ptr<NavigationState> navigation_state_;

  DISALLOW_COPY_AND_ASSIGN(InternalDocumentStateData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
