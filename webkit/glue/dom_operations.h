// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DOM_OPERATIONS_H__
#define WEBKIT_GLUE_DOM_OPERATIONS_H__

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"

namespace WebKit {
class WebDocument;
class WebElement;
class WebString;
class WebView;
}

// A collection of operations that access the underlying WebKit DOM directly.
namespace webkit_glue {

// Structure for storage the result of getting all savable resource links
// for current page. The consumer of the SavableResourcesResult is responsible
// for keeping these pointers valid for the lifetime of the
// SavableResourcesResult instance.
struct SavableResourcesResult {
  // vector which contains all savable links of sub resource.
  std::vector<GURL>* resources_list;
  // vector which contains corresponding all referral links of sub resource,
  // it matched with links one by one.
  std::vector<GURL>* referrers_list;
  // vector which contains all savable links of main frame and sub frames.
  std::vector<GURL>* frames_list;

  // Constructor.
  SavableResourcesResult(std::vector<GURL>* resources_list,
                         std::vector<GURL>* referrers_list,
                         std::vector<GURL>* frames_list)
      : resources_list(resources_list),
        referrers_list(referrers_list),
        frames_list(frames_list) { }

 private:
  DISALLOW_COPY_AND_ASSIGN(SavableResourcesResult);
};

// Get all savable resource links from current webview, include main frame
// and sub-frame. After collecting all savable resource links, this function
// will send those links to embedder. Return value indicates whether we get
// all saved resource links successfully.
bool GetAllSavableResourceLinksForCurrentPage(WebKit::WebView* view,
    const GURL& page_url, SavableResourcesResult* savable_resources_result,
    const char** savable_schemes);

// Invokes pauseAnimationAtTime on the AnimationController associated with the
// |view|s main frame.
// This is used by test shell.
bool PauseAnimationAtTimeOnElementWithId(WebKit::WebView* view,
                                         const std::string& animation_name,
                                         double time,
                                         const std::string& element_id);

// Invokes pauseTransitionAtTime on the AnimationController associated with the
// |view|s main frame.
// This is used by test shell.
bool PauseTransitionAtTimeOnElementWithId(WebKit::WebView* view,
                                          const std::string& property_name,
                                          double time,
                                          const std::string& element_id);

// Returns true if the element with |element_id| as its id has autocomplete
// on.
bool ElementDoesAutoCompleteForElementWithId(WebKit::WebView* view,
                                             const std::string& element_id);

// Returns the number of animations currently running.
int NumberOfActiveAnimations(WebKit::WebView* view);

// Returns the value in an elements resource url attribute. For IMG, SCRIPT or
// INPUT TYPE=image, returns the value in "src". For LINK TYPE=text/css, returns
// the value in "href". For BODY, TABLE, TR, TD, returns the value in
// "background". For BLOCKQUOTE, Q, DEL, INS, returns the value in "cite"
// attribute. Otherwise returns a null WebString.
WebKit::WebString GetSubResourceLinkFromElement(
    const WebKit::WebElement& element);

// Puts the meta-elements of |document| that have the attribute |attribute_name|
// with a value of |attribute_value| in |meta_elements|.
void GetMetaElementsWithAttribute(
    WebKit::WebDocument* document,
    const string16& attribute_name,
    const string16& atribute_value,
    std::vector<WebKit::WebElement>* meta_elements);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_DOM_OPERATIONS_H__
