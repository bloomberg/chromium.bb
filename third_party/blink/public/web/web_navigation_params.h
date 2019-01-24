// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_PARAMS_H_

#include <memory>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_source_location.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_navigation_timings.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"

#if INSIDE_BLINK
#include "base/memory/scoped_refptr.h"
#endif

namespace blink {

class KURL;
class SharedBuffer;

// This structure holds all information collected by Blink when
// navigation is being initiated.
struct BLINK_EXPORT WebNavigationInfo {
  WebNavigationInfo() = default;
  ~WebNavigationInfo() = default;

  // The main resource request.
  WebURLRequest url_request;

  // The navigation type. See WebNavigationType.
  WebNavigationType navigation_type = kWebNavigationTypeOther;

  // The navigation policy. See WebNavigationPolicy.
  WebNavigationPolicy navigation_policy = kWebNavigationPolicyCurrentTab;

  // Whether the frame had a transient user activation
  // at the time this request was issued.
  bool has_transient_user_activation = false;

  // The load type. See WebFrameLoadType.
  WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;

  // During a history load, a child frame can be initially navigated
  // to an url from the history state. This flag indicates it.
  bool is_history_navigation_in_new_child_frame = false;

  // Whether the navigation is a result of client redirect.
  bool is_client_redirect = false;

  // Whether this is a navigation in the opener frame initiated
  // by the window.open'd frame.
  bool is_opener_navigation = false;

  // Whether the runtime feature
  // |BlockingDownloadsInSandboxWithoutUserActivation| is enabled.
  bool blocking_downloads_in_sandbox_without_user_activation_enabled = false;

  // Event information. See WebTriggeringEventInfo.
  WebTriggeringEventInfo triggering_event_info =
      WebTriggeringEventInfo::kUnknown;

  // If the navigation is a result of form submit, the form element is provided.
  WebFormElement form;

  // The location in the source which triggered the navigation.
  // Used to help web developers understand what caused the navigation.
  WebSourceLocation source_location;

  // The initiator of this navigation used by DevTools.
  WebString devtools_initiator_info;

  // Whether this navigation should check CSP. See
  // WebContentSecurityPolicyDisposition.
  WebContentSecurityPolicyDisposition
      should_check_main_world_content_security_policy =
          kWebContentSecurityPolicyDispositionCheck;

  // When navigating to a blob url, this token specifies the blob.
  mojo::ScopedMessagePipeHandle blob_url_token;

  // When navigation initiated from the user input, this tracks
  // the input start time.
  base::TimeTicks input_start;

  // This is the navigation relevant CSP to be used during request and response
  // checks.
  WebContentSecurityPolicyList initiator_csp;

  // The navigation initiator, if any.
  mojo::ScopedMessagePipeHandle navigation_initiator_handle;

  // Specifies whether or not a MHTML Archive can be used to load a subframe
  // resource instead of doing a network request.
  enum class ArchiveStatus { Absent, Present };
  ArchiveStatus archive_status = ArchiveStatus::Absent;

  // The value of hrefTranslate attribute of a link, if this navigation was
  // inititated by clicking a link.
  WebString href_translate;
};

// This structure holds all information provided by the embedder that is
// needed for blink to load a Document. This is hence different from
// WebDocumentLoader::ExtraData, which is an opaque structure stored in the
// DocumentLoader and used by the embedder.
struct BLINK_EXPORT WebNavigationParams {
  WebNavigationParams();
  ~WebNavigationParams();

  // Allows to specify |devtools_navigation_token|, instead of creating
  // a new one.
  explicit WebNavigationParams(const base::UnguessableToken&);

  // Shortcut for navigating based on WebNavigationInfo parameters.
  static std::unique_ptr<WebNavigationParams> CreateFromInfo(
      const WebNavigationInfo&);

  // Shortcut for loading html with "text/html" mime type and "UTF8" encoding.
  static std::unique_ptr<WebNavigationParams> CreateWithHTMLString(
      base::span<const char> html,
      const WebURL& base_url);

#if INSIDE_BLINK
  // Shortcut for loading html with "text/html" mime type and "UTF8" encoding.
  static std::unique_ptr<WebNavigationParams> CreateWithHTMLBuffer(
      scoped_refptr<SharedBuffer> buffer,
      const KURL& base_url);
#endif

  // The request to navigate to. Its URL indicates the security origin
  // and will be used as a base URL to resolve links in the committed document.
  // TODO(dgozman): do we actually need a request here? Maybe just a URL?
  WebURLRequest request;

  // If the data is non null, it will be used as a main resource content
  // instead of loading the request above.
  WebData data;
  // Specifies the mime type of the raw data. Must be set together with the
  // data.
  WebString mime_type;
  // The encoding of the raw data. Must be set together with the data.
  WebString text_encoding;
  // If non-null, used as a URL which we weren't able to load. For example,
  // history item will contain this URL instead of request's URL.
  // This URL can be retrieved through WebDocumentLoader::UnreachableURL.
  WebURL unreachable_url;

  // The load type. See WebFrameLoadType for definition.
  WebFrameLoadType frame_load_type = WebFrameLoadType::kStandard;
  // History item should be provided for back-forward load types.
  WebHistoryItem history_item;
  // Whether this navigation is a result of client redirect.
  bool is_client_redirect = false;

  // The origin in which a navigation should commit. When provided, Blink
  // should use this origin directly and not compute locally the new document
  // origin.
  WebSecurityOrigin origin_to_commit;

  // The devtools token for this navigation. See DocumentLoader
  // for details.
  base::UnguessableToken devtools_navigation_token;
  // Known timings related to navigation. If the navigation has
  // started in another process, timings are propagated from there.
  WebNavigationTimings navigation_timings;
  // Whether this navigation had a transient user activation.
  bool is_user_activated = false;

  // The service worker network provider to be used in the new
  // document.
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
      service_worker_network_provider;
};

}  // namespace blink

#endif
