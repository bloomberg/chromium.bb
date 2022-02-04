// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_state.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/common/content_export.h"
#include "content/public/browser/page.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/frame/text_autosizer_page_info.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {

class PageDelegate;
class RenderFrameHostImpl;
class RenderViewHostImpl;

// This implements the Page interface that is exposed to embedders of content,
// and adds things only visible to content.

// Please refer to content/public/browser/page.h for more details.
class CONTENT_EXPORT PageImpl : public Page {
 public:
  explicit PageImpl(RenderFrameHostImpl& rfh, PageDelegate& delegate);

  ~PageImpl() override;

  // Page implementation.
  const absl::optional<GURL>& GetManifestUrl() const override;
  void GetManifest(GetManifestCallback callback) override;
  bool IsPrimary() override;
  void WriteIntoTrace(perfetto::TracedValue context) override;
  base::WeakPtr<Page> GetWeakPtr() override;
  bool IsPageScaleFactorOne() override;

  void UpdateManifestUrl(const GURL& manifest_url);

  RenderFrameHostImpl& GetMainDocument() const;

  bool is_on_load_completed_in_main_document() const {
    return is_on_load_completed_in_main_document_;
  }
  void set_is_on_load_completed_in_main_document(bool completed) {
    is_on_load_completed_in_main_document_ = completed;
  }

  bool is_document_available_in_main_document() const {
    return is_document_available_in_main_document_;
  }
  void set_is_document_available_in_main_document(bool completed) {
    is_document_available_in_main_document_ = completed;
  }

  bool uses_temporary_zoom_level() const { return uses_temporary_zoom_level_; }
  void set_uses_temporary_zoom_level(bool level) {
    uses_temporary_zoom_level_ = level;
  }

  void OnFirstVisuallyNonEmptyPaint();
  bool did_first_visually_non_empty_paint() const {
    return did_first_visually_non_empty_paint_;
  }

  const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls() const {
    return favicon_urls_;
  }
  void set_favicon_urls(std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
    favicon_urls_ = std::move(favicon_urls);
  }

  void OnThemeColorChanged(const absl::optional<SkColor>& theme_color);

  void DidChangeBackgroundColor(SkColor background_color, bool color_adjust);

  // Notifies the page's color scheme was inferred.
  void DidInferColorScheme(blink::mojom::PreferredColorScheme color_scheme);

  absl::optional<SkColor> theme_color() const {
    return main_document_theme_color_;
  }

  absl::optional<SkColor> background_color() const {
    return main_document_background_color_;
  }

  absl::optional<blink::mojom::PreferredColorScheme> inferred_color_scheme()
      const {
    return main_document_inferred_color_scheme_;
  }

  void SetContentsMimeType(std::string mime_type);
  const std::string& contents_mime_type() { return contents_mime_type_; }

  void OnTextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr page_info);

  blink::mojom::TextAutosizerPageInfo text_autosizer_page_info() const {
    return text_autosizer_page_info_;
  }

  FencedFrameURLMapping& fenced_frame_urls_map() {
    return fenced_frame_urls_map_;
  }

  void set_last_main_document_source_id(ukm::SourceId id) {
    last_main_document_source_id_ = id;
  }
  ukm::SourceId last_main_document_source_id() const {
    return last_main_document_source_id_;
  }

  const base::UnguessableToken& anonymous_iframes_nonce() const {
    return anonymous_iframes_nonce_;
  }

  // Sets the start time of the prerender activation navigation for this page.
  // TODO(falken): Plumb NavigationRequest to
  // RenderFrameHostManager::CommitPending and remove this.
  void SetActivationStartTime(base::TimeTicks activation_start);

  // Called during the prerender activation navigation. Sends an IPC to the
  // RenderViews in the renderers, instructing them to transition their
  // documents from prerendered to activated. Tells the corresponding
  // RenderFrameHostImpls that the renderer will be activating their documents.
  void ActivateForPrerendering(
      std::set<RenderViewHostImpl*>& render_view_hosts_to_activate);

  // Prerender2:
  // Dispatches load events that were deferred to be dispatched after
  // activation. Please note that this should only be called on prerender
  // activation.
  void MaybeDispatchLoadEventsOnPrerenderActivation();

  // Hide or show the browser controls for the given Page, based on allowed
  // states, desired state and whether the transition should be animated or
  // not.
  void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                  cc::BrowserControlsState current,
                                  bool animate);

  void set_load_progress(double load_progress) {
    load_progress_ = load_progress;
  }
  double load_progress() const { return load_progress_; }

  void set_page_scale_factor(float scale) { page_scale_factor_ = scale; }
  float page_scale_factor() const { return page_scale_factor_; }

  void set_virtual_keyboard_overlays_content(bool vk_overlays_content) {
    virtual_keyboard_overlays_content_ = vk_overlays_content;
  }
  bool virtual_keyboard_overlays_content() const {
    return virtual_keyboard_overlays_content_;
  }

  const std::string& GetEncoding() { return canonical_encoding_; }
  void UpdateEncoding(const std::string& encoding_name);

 private:
  void DidActivateAllRenderViewsForPrerendering();

  // This method is needed to ensure that PageImpl can both implement a Page's
  // method and define a new GetMainDocument(). Please refer to page.h for more
  // details.
  RenderFrameHost& GetMainDocumentHelper() override;

  // True if we've received a notification that the onload() handler has
  // run for the main document.
  bool is_on_load_completed_in_main_document_ = false;

  // True if we've received a notification that the window.document was created
  // for the main document.
  bool is_document_available_in_main_document_ = false;

  // True if plugin zoom level is set for the main document.
  bool uses_temporary_zoom_level_ = false;

  // Overall load progress of this Page. Initial load progress value is 0.0
  // before the load has begun.
  double load_progress_ = 0.0;

  // Web application manifest URL for this page.
  // See https://w3c.github.io/manifest/#web-application-manifest.
  //
  // This is non-nullopt when the page gets an update of the manifest URL. It
  // can be the empty URL when the manifest url is removed and a non-empty
  // URL when it has a valid URL for the manifest. If this is non-nullopt,
  // WebContentsObserver::DidUpdateWebManifestURL() will be called
  // (either immediately on document load, or on activation in the case
  // of a prerendered page).
  //
  // nullopt indicates that the page did not get an update of the
  // manifest URL, and DidUpdateWebManifestURL() will not be called.
  absl::optional<GURL> manifest_url_;

  // Candidate favicon URLs. Each page may have a collection and will be
  // displayed when active (i.e., upon activation for prerendering).
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls_;

  // Whether the first visually non-empty paint has occurred.
  bool did_first_visually_non_empty_paint_ = false;

  // The theme color for the underlying document as specified
  // by theme-color meta tag.
  absl::optional<SkColor> main_document_theme_color_;

  // The background color for the underlying document as computed by CSS.
  absl::optional<SkColor> main_document_background_color_;

  // The inferred color scheme of the document.
  absl::optional<blink::mojom::PreferredColorScheme>
      main_document_inferred_color_scheme_;

  // Contents MIME type for the main document. It can be used to check whether
  // we can do something for special contents.
  std::string contents_mime_type_;

  // Fenced frames:
  // Any fenced frames created within this page will access this map.
  FencedFrameURLMapping fenced_frame_urls_map_;

  // This class is owned by the main RenderFrameHostImpl and it's safe to keep a
  // reference to it.
  RenderFrameHostImpl& main_document_;

  // SourceId of the navigation in this page's main frame. Note that a same
  // document navigation is the only case where this source id can change, since
  // all other navigations create a new PageImpl instance.
  ukm::SourceId last_main_document_source_id_ = ukm::kInvalidSourceId;

  // This page is owned by the RenderFrameHostImpl, which in turn does not
  // outlive the delegate (the contents).
  PageDelegate& delegate_;

  // Stores information from the main frame's renderer that needs to be shared
  // with OOPIF renderers.
  blink::mojom::TextAutosizerPageInfo text_autosizer_page_info_;

  // Nonce to be used for initializing the storage key and the network isolation
  // key of anonymous iframes which are children of this page's document.
  const base::UnguessableToken anonymous_iframes_nonce_ =
      base::UnguessableToken::Create();

  // Prerender2: The start time of the activation navigation for prerendering,
  // which is passed to the renderer process, and will be accessible in the
  // prerendered page as PerformanceNavigationTiming.activationStart. Set after
  // navigation commit.
  // TODO(falken): Plumb NavigationRequest to
  // RenderFrameHostManager::CommitPending and remove this.
  absl::optional<base::TimeTicks> activation_start_time_for_prerendering_;

  // The most recent page scale factor sent by the main frame's renderer.
  // Note that the renderer uses a different mechanism to persist its page
  // scale factor when performing session history navigations (see
  // blink::PageState).
  float page_scale_factor_ = 1.f;

  // If true, then the Virtual keyboard rectangle that occludes the content is
  // sent to the VirtualKeyboard API where it fires overlaygeometrychange JS
  // event notifying the web authors that Virtual keyboard has occluded the
  // content.
  bool virtual_keyboard_overlays_content_ = false;

  // The last reported character encoding, not canonicalized.
  std::string last_reported_encoding_;
  // The canonicalized character encoding.
  std::string canonical_encoding_;

  base::WeakPtrFactory<PageImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_IMPL_H_
