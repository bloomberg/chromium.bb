// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_H_

#include "content/public/common/referrer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace navigation_interception {

class NavigationParams {
 public:
  NavigationParams(const GURL& url,
                   const content::Referrer& referrer,
                   int64_t navigation_id,
                   bool has_user_gesture,
                   bool is_post,
                   ui::PageTransition page_transition_type,
                   bool is_redirect,
                   bool is_external_protocol,
                   bool is_main_frame,
                   bool is_renderer_initiated,
                   const GURL& base_url_for_data_url,
                   const absl::optional<url::Origin>& initiator_origin);
  ~NavigationParams();
  NavigationParams(const NavigationParams&);
  NavigationParams& operator=(const NavigationParams&) = delete;

  const GURL& url() const { return url_; }
  GURL& url() { return url_; }
  const content::Referrer& referrer() const { return referrer_; }

  // The ID of the NavigationHandle that this instance corresponds to, or 0 if
  // this instance was not constructed from a NavigationHandle.
  int64_t navigation_id() const { return navigation_id_; }
  bool has_user_gesture() const { return has_user_gesture_; }
  bool is_post() const { return is_post_; }
  ui::PageTransition transition_type() const { return transition_type_; }
  bool is_redirect() const { return is_redirect_; }
  bool is_external_protocol() const { return is_external_protocol_; }
  bool is_main_frame() const { return is_main_frame_; }
  bool is_renderer_initiated() const { return is_renderer_initiated_; }
  const GURL& base_url_for_data_url() const { return base_url_for_data_url_; }
  const absl::optional<url::Origin>& initiator_origin() const {
    return initiator_origin_;
  }

 private:

  GURL url_;
  content::Referrer referrer_;
  int64_t navigation_id_;
  bool has_user_gesture_;
  bool is_post_;
  ui::PageTransition transition_type_;
  bool is_redirect_;
  bool is_external_protocol_;
  bool is_main_frame_;
  bool is_renderer_initiated_;
  GURL base_url_for_data_url_;
  absl::optional<url::Origin> initiator_origin_;
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_NAVIGATION_PARAMS_H_
