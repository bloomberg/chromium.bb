// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"

#import <WebKit/WebKit.h>
#include <cmath>
#include <limits>

#include "base/logging.h"
#import "ios/web/public/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Background color RGB values for the content view which is displayed when the
// |_webView| is offset from the screen due to user interaction. Displaying this
// background color is handled by UIWebView but not WKWebView, so it needs to be
// set in CRWWebViewContentView to support both. The color value matches that
// used by UIWebView.
const CGFloat kBackgroundRGBComponents[] = {0.75f, 0.74f, 0.76f};

}  // namespace

@implementation CRWWebViewContentView
@synthesize contentOffset = _contentOffset;
@synthesize contentInset = _contentInset;
@synthesize shouldUseViewContentInset = _shouldUseViewContentInset;
@synthesize scrollView = _scrollView;
@synthesize webView = _webView;

- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(webView);
    DCHECK(scrollView);
    DCHECK([scrollView isDescendantOfView:webView]);
    _webView = webView;
    _scrollView = scrollView;
  }
  return self;
}

- (instancetype)initForTesting {
  return [super initWithFrame:CGRectZero];
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (self.superview) {
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:_webView];
    self.backgroundColor = [UIColor colorWithRed:kBackgroundRGBComponents[0]
                                           green:kBackgroundRGBComponents[1]
                                            blue:kBackgroundRGBComponents[2]
                                           alpha:1.0];
  }
}

- (BOOL)becomeFirstResponder {
  return [_webView becomeFirstResponder];
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  if (!base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen)) {
    CGRect frame = self.bounds;
    frame = UIEdgeInsetsInsetRect(frame, _contentInset);
    frame = CGRectOffset(frame, _contentOffset.x, _contentOffset.y);
    self.webView.frame = frame;
  }
}

- (BOOL)isViewAlive {
  return YES;
}

- (void)setContentOffset:(CGPoint)contentOffset {
  if (CGPointEqualToPoint(_contentOffset, contentOffset))
    return;
  _contentOffset = contentOffset;
  [self setNeedsLayout];
}

- (UIEdgeInsets)contentInset {
  return self.shouldUseViewContentInset ? [_scrollView contentInset]
                                        : _contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  UIEdgeInsets oldInsets = self.contentInset;
  CGFloat delta = std::fabs(oldInsets.top - contentInset.top) +
                  std::fabs(oldInsets.left - contentInset.left) +
                  std::fabs(oldInsets.bottom - contentInset.bottom) +
                  std::fabs(oldInsets.right - contentInset.right);
  if (delta <= std::numeric_limits<CGFloat>::epsilon())
    return;
  _contentInset = contentInset;
  if (self.shouldUseViewContentInset) {
    [_scrollView setContentInset:contentInset];
  } else {
    [self resizeViewportForContentInsetChangeFromInsets:oldInsets];
  }
}

- (void)setShouldUseViewContentInset:(BOOL)shouldUseViewContentInset {
  if (_shouldUseViewContentInset != shouldUseViewContentInset) {
    UIEdgeInsets oldContentInset = self.contentInset;
    self.contentInset = UIEdgeInsetsZero;
    _shouldUseViewContentInset = shouldUseViewContentInset;
    self.contentInset = oldContentInset;
  }
}

#pragma mark Private methods

// Updates the viewport by updating the web view frame after self.contentInset
// is changed to a new value from |oldInsets|.
- (void)resizeViewportForContentInsetChangeFromInsets:(UIEdgeInsets)oldInsets {
  // Update the content offset of the scroll view to match the padding
  // that will be included in the frame.
  CGFloat topPaddingChange = self.contentInset.top - oldInsets.top;
  CGPoint contentOffset = [_scrollView contentOffset];
  contentOffset.y += topPaddingChange;
  [_scrollView setContentOffset:contentOffset];
  // Update web view frame immediately to make |contentInset| animatable.
  [self setNeedsLayout];
  [self layoutIfNeeded];
  // Setting WKWebView frame can mistakenly reset contentOffset. Change it
  // back to the initial value if necessary.
  // TODO(crbug.com/645857): Remove this workaround once WebKit bug is
  // fixed.
  if ([_scrollView contentOffset].y != contentOffset.y) {
    [_scrollView setContentOffset:contentOffset];
  }
}

@end
