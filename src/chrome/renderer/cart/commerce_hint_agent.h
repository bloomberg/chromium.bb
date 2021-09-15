// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_
#define CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "url/gurl.h"

namespace cart {

// The renderer-side agent for CommerceHint.
class CommerceHintAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CommerceHintAgent> {
 public:
  explicit CommerceHintAgent(content::RenderFrame* render_frame);
  ~CommerceHintAgent() override;

  CommerceHintAgent(const CommerceHintAgent&) = delete;
  CommerceHintAgent& operator=(const CommerceHintAgent&) = delete;

  // Whether the string, either from path of URL or XHR form contents, matches
  // the add-to-cart heuristics.
  static bool IsAddToCart(base::StringPiece str);
  // Whether the main frame URL is a shopping cart.
  static bool IsVisitCart(const GURL& main_frame_url);
  // Whether the main frame URL is a checkout page.
  static bool IsVisitCheckout(const GURL& main_frame_url);
  // Whether the main frame URL is a purchase page.
  static bool IsPurchase(const GURL& main_frame_url);
  // Whether the button text in a page with |url| corresponds to a purchase.
  static bool IsPurchase(const GURL& url, base::StringPiece button_text);
  // Whether the product should be skipped, based on product name.
  static bool ShouldSkip(base::StringPiece product_name);
  // Whether the request with navigation URL as |navigation_url| and request URL
  // as |request_url| should be skipped for AddToCart detection.
  static bool ShouldSkipAddToCartRequest(const GURL& navigation_url,
                                         const GURL& request_url);
  void OnProductsExtracted(std::unique_ptr<base::Value> result);
  static const std::vector<std::string> ExtractButtonTexts(
      const blink::WebFormElement& form);

 private:
  void MaybeExtractProducts();
  void ExtractProducts();
  void ExtractCartFromCurrentFrame();

  GURL starting_url_;
  base::TimeTicks last_extraction_time_;
  base::WeakPtrFactory<CommerceHintAgent> weak_factory_{this};

  class JavaScriptRequest : public blink::WebScriptExecutionCallback {
   public:
    explicit JavaScriptRequest(base::WeakPtr<CommerceHintAgent> agent);
    JavaScriptRequest(const JavaScriptRequest&) = delete;
    JavaScriptRequest& operator=(const JavaScriptRequest&) = delete;
    void WillExecute() override;
    void Completed(
        const blink::WebVector<v8::Local<v8::Value>>& result) override;

   private:
    ~JavaScriptRequest() override;
    base::WeakPtr<CommerceHintAgent> agent_;
    base::TimeTicks start_time_;
  };

  // content::RenderFrameObserver overrides
  void OnDestruct() override;
  void WillSendRequest(const blink::WebURLRequest& request) override;
  void DidStartNavigation(
      const GURL& url,
      absl::optional<blink::WebNavigationType> navigation_type) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
  void OnMainFrameIntersectionChanged(const gfx::Rect& intersect_rect) override;
};

}  // namespace cart

#endif  // CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_
