// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller_container_view.h"

#include "base/check.h"
#include "base/notreached.h"
#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"
#import "ios/web/common/crw_web_view_content_view.h"
#include "ios/web/common/features.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWebControllerContainerView () <CRWViewportAdjustmentContainer>

// Redefine properties as readwrite.
@property(nonatomic, strong, readwrite)
    CRWWebViewContentView* webViewContentView;
@property(nonatomic, strong, readwrite) CRWContentView* transientContentView;

// Convenience getter for the proxy object.
@property(nonatomic, weak, readonly) CRWWebViewProxyImpl* contentViewProxy;

@end

@implementation CRWWebControllerContainerView
@synthesize webViewContentView = _webViewContentView;
@synthesize transientContentView = _transientContentView;
@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:
        (id<CRWWebControllerContainerViewDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    self.backgroundColor = [UIColor whiteColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  self.contentViewProxy.contentView = nil;
}

#pragma mark Accessors

- (UIView<CRWViewportAdjustment>*)fullscreenViewportAdjuster {
  if (![self.webViewContentView
          conformsToProtocol:@protocol(CRWViewportAdjustment)]) {
    return nil;
  }
  return self.webViewContentView;
}

- (void)setWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  if (![_webViewContentView isEqual:webViewContentView]) {
    [_webViewContentView removeFromSuperview];
    _webViewContentView = webViewContentView;
    [_webViewContentView setFrame:self.bounds];
    [self addSubview:_webViewContentView];
  }
}

- (void)setTransientContentView:(CRWContentView*)transientContentView {
  if (![_transientContentView isEqual:transientContentView]) {
    [_transientContentView removeFromSuperview];
    _transientContentView = transientContentView;
  }
}

- (CRWWebViewProxyImpl*)contentViewProxy {
  return [_delegate contentViewProxyForContainerView:self];
}

#pragma mark Layout

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    // In case the preferred content size changes, the layout is dirty.
    [self setNeedsLayout];
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // webViewContentView layout.  |-setNeedsLayout| is called in case any webview
  // layout updates need to occur despite the bounds size staying constant.
  self.webViewContentView.frame = self.bounds;
  [self.webViewContentView setNeedsLayout];

  // TODO(crbug.com/570114): Move adding of the following subviews to another
  // place.
  // transientContentView layout.
  if (self.transientContentView) {
    if (!self.transientContentView.superview)
      [self addSubview:self.transientContentView];
    [self bringSubviewToFront:self.transientContentView];
    self.transientContentView.frame = self.bounds;
  }
}

- (BOOL)isViewAlive {
  return self.webViewContentView || self.transientContentView;
}

- (void)willMoveToWindow:(UIWindow*)newWindow {
  [super willMoveToWindow:newWindow];
  [self updateWebViewContentViewForContainerWindow:newWindow];
}

- (void)updateWebViewContentViewForContainerWindow:(UIWindow*)containerWindow {
  if (!base::FeatureList::IsEnabled(web::features::kKeepsRenderProcessAlive))
    return;

  if (!self.webViewContentView)
    return;

  // If there's a containerWindow or |webViewContentView| is inactive, put it
  // back where it belongs.
  if (containerWindow ||
      ![_delegate shouldKeepRenderProcessAliveForContainerView:self]) {
    if (self.webViewContentView.superview != self) {
      [_webViewContentView setFrame:self.bounds];
      // Insert the content view on the back of the container view so any view
      // that was presented on top of the content view can still appear.
      [self insertSubview:_webViewContentView atIndex:0];
    }
    return;
  }

  // There's no window and |webViewContentView| is active, stash it.
  [_delegate containerView:self storeWebViewInWindow:self.webViewContentView];
}

#pragma mark Content Setters

- (void)resetContent {
  self.webViewContentView = nil;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = nil;
}

- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  DCHECK(webViewContentView);
  self.webViewContentView = webViewContentView;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = self.webViewContentView;
  [self updateWebViewContentViewForContainerWindow:self.window];
  [self setNeedsLayout];
}

- (void)displayTransientContent:(CRWContentView*)transientContentView {
  DCHECK(transientContentView);
  self.transientContentView = transientContentView;
  self.contentViewProxy.contentView = self.transientContentView;
  [self setNeedsLayout];
}

- (void)clearTransientContentView {
  self.transientContentView = nil;
  self.contentViewProxy.contentView = self.webViewContentView;
}

#pragma mark UIView (printing)

// Only print the web view by returning the web view printformatter.
- (UIViewPrintFormatter*)viewPrintFormatter {
  return [self.webViewContentView.webView viewPrintFormatter];
}

- (void)drawRect:(CGRect)rect
    forViewPrintFormatter:(UIViewPrintFormatter*)formatter {
  [self.webViewContentView.webView drawRect:rect];
}

@end
