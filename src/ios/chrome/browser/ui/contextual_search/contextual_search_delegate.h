// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ios/chrome/browser/ui/contextual_search/contextual_search_context.h"

class TemplateURLService;

namespace ios {
class ChromeBrowserState;
}

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

// Handles tasks for the ContextualSearchManager in a separable and testable
// way, without the complication of being connected to a Java object.
class ContextualSearchDelegate
    : public base::SupportsWeakPtr<ContextualSearchDelegate> {
 public:
  typedef struct {
    bool is_invalid;
    int response_code;
    std::string search_term;
    std::string display_text;
    std::string alternate_term;
    bool prevent_preload;
    int start_offset;
    int end_offset;
  } SearchResolution;

  typedef base::Callback<void(SearchResolution)> SearchTermResolutionCallback;

  // Constructs a delegate for the given user browser state that will always
  // call back to the given callbacks when search term resolution or surrounding
  // text responses are available.
  ContextualSearchDelegate(
      ios::ChromeBrowserState* browser_state,
      const SearchTermResolutionCallback& search_term_callback);
  virtual ~ContextualSearchDelegate();

  // Requests the search term using |context| and parameters that specify the
  // surrounding text. if |context| represents a local text selection, then no
  // server request is made and the selection contents are returned as the
  // relevant text.
  // When the response is available the callback specified in the constructor
  // is run.
  // This method is asynchronous and may delay the request to avoid sending
  // too many requests to the server. If this function is called multiple times
  // during that delay, only the latest request is executed. Each new call
  // cancels previous requests.
  void PostSearchTermRequest(std::shared_ptr<ContextualSearchContext> context);

  // This method will immediately start the pending request previously posted by
  // |PostSearchTermRequest|.
  void StartPendingSearchTermRequest();

  // Cancels the current search term network request.
  void CancelSearchTermRequest();

  // Get the URL for the search to be run for the query in |resolution|.
  GURL GetURLForResolvedSearch(SearchResolution resolution,
                               bool should_prefetch);

 private:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Builds the request URL.
  GURL BuildRequestUrl();

  // Copy search context term as SearchResolution and calls the
  // search_term_callback_.
  void RequestLocalSearchTerm();

  // Initiates a network request to get the relevant query context.
  void RequestServerSearchTerm();

  // Uses the TemplateURL service to construct a search term resolution URL from
  // the given parameters.
  std::string GetSearchTermResolutionUrlString(
      const std::string& selected_text,
      const std::string& base_page_url);

  // Populates the discourse context and adds it to the HTTP header of the
  // search term resolution request.
  void SetDiscourseContextAndAddToHeader(
      const ContextualSearchContext& context,
      network::ResourceRequest* resource_request);

  // The current request in progress, or NULL.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Holds the TemplateURLService. Not owned.
  TemplateURLService* template_url_service_;

  // The browser state associated with the Contextual Search Manager.
  // This reference is not owned by us.
  ios::ChromeBrowserState* browser_state_;

  // The callback for notifications of completed URL fetches.
  SearchTermResolutionCallback search_term_callback_;

  // Used to hold the context until an upcoming search term request is started.
  std::shared_ptr<ContextualSearchContext> context_;

  base::Time last_request_startup_time_;
  bool request_pending_;

  // Used to ensure |StartRequestSearchTerm()| is not run when this object is
  // destroyed.
  base::WeakPtrFactory<ContextualSearchDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchDelegate);
};

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_
