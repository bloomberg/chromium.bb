// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
#define IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_

#include <list>
#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#import "ios/web/public/find_in_page/find_in_page_manager.h"
#include "ios/web/public/web_state/web_state_observer.h"

@class NSString;

namespace web {

class WebState;
class WebFrame;

class FindInPageManagerImpl : public FindInPageManager,
                              public web::WebStateObserver {
 public:
  explicit FindInPageManagerImpl(web::WebState* web_state);
  ~FindInPageManagerImpl() override;

  static void CreateForWebState(WebState* web_state);

  // FindInPageManager overrides
  void Find(NSString* query, FindInPageOptions options) override;
  void StopFinding() override;
  bool CanSearchContent() override;
  FindInPageManagerDelegate* GetDelegate() override;
  void SetDelegate(FindInPageManagerDelegate* delegate) override;

 private:
  friend class web::WebStateUserData<FindInPageManagerImpl>;
  // Keeps track of the state of an ongoing Find() request.
  struct FindRequest {
    FindRequest();
    ~FindRequest();
    // Clears properties and sets new |query| and |pending_frame_call_count|.
    void Reset(NSString* query, int pending_frame_call_count);
    int GetTotalMatchCount() const;
    // Returns the index of the currently selected match for all matches on the
    // page. If no match is selected, then returns -1.
    int GetCurrentSelectedMatchIndex();
    // Sets |selected_frame_id| and |selected_match_index_in_selected_frame| to
    // the first match on the page. No-op if no known matches exist. Returns
    // true if selected a match, false otherwise.
    bool GoToFirstMatch();
    // Sets |selected_frame_id| and |selected_match_index_in_selected_frame| to
    // the next match on the page. No-op if no known matches exist. Returns true
    // if selected a match, false otherwise.
    bool GoToNextMatch();
    // Sets |selected_frame_id| and |selected_match_index_in_selected_frame| to
    // the previous match on the page. No-op if no known matches exist. Returns
    // true if selected a match, false otherwise.
    bool GoToPreviousMatch();
    // Removes |web_frame| from |frame_order| and |frame_match_count|. Resets
    // |selected_frame_id| and |selected_match_index_in_selected_frame| if
    // |web_frame| contains currently selected match. |web_frame| must not be
    // null.
    void RemoveFrame(WebFrame* web_frame);
    // Unique identifier for each find used to check that it is the most recent
    // find. This ensures that an old find doesn't decrement
    // |pending_frame_calls_count| after it has been reset by the new find.
    int unique_id = 0;
    // Query string of find request. NSString type to ensure query passed to
    // delegate methods is the same type as what is passed into Find().
    NSString* query = nil;
    // Counter to keep track of pending frame JavaScript calls.
    int pending_frame_call_count = 0;
    // Holds number of matches found for each frame keyed by frame_id.
    std::map<std::string, int> frame_match_count;
    // List of frame_ids used for sorting matches.
    std::list<std::string> frame_order;
    // Id of frame which has the currently selected match. Set to
    // frame_order.end() if there is no currently selected match. All matches
    // from the last find will be highlighted. However, the match at
    // |selected_match_index_in_selected_frame| will be highlighted in a
    // visually unique manner. This match is referred to as the "selected match"
    // and can be changed with the FindInPageNext and FindInPagePrevious
    // commands.
    std::list<std::string>::iterator selected_frame_id = frame_order.end();
    // Index of the currently selected match or -1 if there is none.
    int selected_match_index_in_selected_frame = -1;

   private:
    // Returns true if |web_frame| contains the currently selected match, false
    // otherwise. |web_frame| must not be null.
    bool IsSelectedFrame(WebFrame* web_frame);
  };

  // Executes find logic for |FindInPageSearch| option.
  void StartSearch(NSString* query);
  // Executes find logic for |FindInPageNext| option.
  void SelectNextMatch();
  // Executes find logic for |FindInPagePrevious| option.
  void SelectPreviousMatch();
  // Determines whether find is finished. If not, calls pumpSearch to
  // continue. If it is, calls UpdateFrameMatchesCount(). If find returned
  // null, then does nothing more.
  void ProcessFindInPageResult(const std::string& frame_id,
                               const int request_id,
                               const base::Value* result);
  // Calls delegate DidHighlightMatches() method if |delegate_| is set and
  // starts a FindInPageNext find. Called when the last frame returns results
  // from a Find request.
  void LastFindRequestCompleted();
  // Calls delegate DidSelectMatch() method to pass back index selected if
  // |delegate_| is set. |result| is a byproduct of using base::BindOnce() to
  // call this method after making a web_frame->CallJavaScriptFunction() call.
  void NotifyDelegateDidSelectMatch(const base::Value* result);
  // Executes highlightResult() JavaScript function in frame which contains the
  // currently selected match.
  void SelectCurrentMatch();

  // WebStateObserver overrides
  void WebFrameDidBecomeAvailable(WebState* web_state,
                                  WebFrame* web_frame) override;
  void WebFrameWillBecomeUnavailable(WebState* web_state,
                                     WebFrame* web_frame) override;
  void WebStateDestroyed(WebState* web_state) override;

  // Holds the state of the most recent find in page request.
  FindRequest last_find_request_;
  FindInPageManagerDelegate* delegate_ = nullptr;
  web::WebState* web_state_ = nullptr;
  base::WeakPtrFactory<FindInPageManagerImpl> weak_factory_;
};
}  // namespace web

#endif  // IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_IMPL_H_
