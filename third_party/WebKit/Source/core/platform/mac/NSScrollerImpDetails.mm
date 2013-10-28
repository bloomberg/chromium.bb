/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "RuntimeEnabledFeatures.h"
#include "core/platform/mac/NSScrollerImpDetails.h"

namespace {

// Declare notification names from the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
NSString* NSPreferredScrollerStyleDidChangeNotification = @"NSPreferredScrollerStyleDidChangeNotification";
#endif

// Storing the current NSScrollerStyle as a global is appreciably faster than
// having it be a property of ScrollerStylerObserver.
NSScrollerStyle g_scrollerStyle = NSScrollerStyleLegacy;

}  // anonymous namespace

@interface ScrollerStyleObserver : NSObject
- (id)init;
- (void)preferredScrollerStyleDidChange:(NSNotification*)notification;
@end

@implementation ScrollerStyleObserver
- (id)init
{
    if ((self = [super init])) {
        if ([NSScroller respondsToSelector:@selector(preferredScrollerStyle)]) {
            g_scrollerStyle = [NSScroller preferredScrollerStyle];
            NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
            [center addObserver:self
                       selector:@selector(preferredScrollerStyleDidChange:)
                           name:NSPreferredScrollerStyleDidChangeNotification
                         object:nil];
        }
    }
    return self;
}

- (void)preferredScrollerStyleDidChange:(NSNotification*)notification
{
    g_scrollerStyle = [NSScroller preferredScrollerStyle];
}
@end

namespace WebCore {

bool isScrollbarOverlayAPIAvailable()
{
    static bool apiAvailable =
        [NSClassFromString(@"NSScrollerImp") respondsToSelector:@selector(scrollerImpWithStyle:controlSize:horizontal:replacingScrollerImp:)]
        && [NSClassFromString(@"NSScrollerImpPair") instancesRespondToSelector:@selector(scrollerStyle)];
    return apiAvailable;
}

NSScrollerStyle recommendedScrollerStyle()
{
    if (RuntimeEnabledFeatures::overlayScrollbarsEnabled())
        return NSScrollerStyleOverlay;

    // The ScrollerStyleObserver will update g_scrollerStyle at init and when needed.
    // This function is hot.
    // http://crbug.com/303205
    static ScrollerStyleObserver* scrollerStyleObserver = [[ScrollerStyleObserver alloc] init];
    (void)scrollerStyleObserver;
    return g_scrollerStyle;
}

}
