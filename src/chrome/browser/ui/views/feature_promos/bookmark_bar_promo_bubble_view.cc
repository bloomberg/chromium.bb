// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/bookmark_bar_promo_bubble_view.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/views/controls/button/label_button.h"

// BookmarkBarViewObserverImpl is used to observe when the bookmark bar has
// finished animating, then show the bookmark bubble. It's important to wait
// for the bookmark bar to finish animating because otherwise this bubble will
// overlap it.
// BookmarkBarViewObserverImpl will delete itself once it shows the bubble to
// prevent showing the bubble multiple times.
// BookmarkBarViewObserverImpl will add and remove itself as an observer to the
// bookmark_bar.
class BookmarkBarPromoBubbleView::BookmarkBarViewObserverImpl
    : public BookmarkBarViewObserver {
 public:
  BookmarkBarViewObserverImpl(BookmarkBarView* bookmark_bar,
                              int string_specifier,
                              const bookmarks::BookmarkNode* node)
      : bookmark_bar(bookmark_bar),
        string_specifier(string_specifier),
        node(node) {
    bookmark_bar->AddObserver(this);
  }
  virtual ~BookmarkBarViewObserverImpl() {}

 private:
  BookmarkBarView* bookmark_bar;
  int string_specifier;
  const bookmarks::BookmarkNode* node;

  void OnBookmarkBarVisibilityChanged() override {}

  void OnBookmarkBarAnimationEnded() override {
    ShowPromoDelegate::CreatePromoDelegate(string_specifier)->ShowForNode(node);
    bookmark_bar->RemoveObserver(this);
    delete this;
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewObserverImpl);
};

std::unique_ptr<ShowPromoDelegate> ShowPromoDelegate::CreatePromoDelegate(
    int string_specifier) {
  return std::make_unique<BookmarkBarPromoBubbleView>(string_specifier);
}

struct BookmarkBarPromoBubbleView::BubbleImpl : public FeaturePromoBubbleView {
  // Anchors the BookmarkBarPromoBubbleView to |anchor_view|.
  // The bubble widget and promo are owned by their native widget.
  explicit BubbleImpl(views::View* anchor_view, int string_specifier)
      : FeaturePromoBubbleView(anchor_view,
                               views::BubbleBorder::TOP_LEFT,
                               string_specifier,
                               ActivationAction::DO_NOT_ACTIVATE) {}

  ~BubbleImpl() override = default;
};

BookmarkBarPromoBubbleView::BookmarkBarPromoBubbleView(int string_specifier)
    : string_specifier(string_specifier) {}

void BookmarkBarPromoBubbleView::ShowForNode(
    const bookmarks::BookmarkNode* node) {
  BookmarkBarView* bookmark_bar =
      BrowserView::GetBrowserViewForBrowser(
          BrowserList::GetInstance()->GetLastActive())
          ->bookmark_bar();

  if (bookmark_bar->visible()) {
    views::LabelButton* anchor_view =
        bookmark_bar->GetBookmarkButtonForNode(node);
    new BookmarkBarPromoBubbleView::BubbleImpl(anchor_view, string_specifier);
  } else {
    new BookmarkBarViewObserverImpl(bookmark_bar, string_specifier, node);
  }
}
