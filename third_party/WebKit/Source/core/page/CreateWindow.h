/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CreateWindow_h
#define CreateWindow_h

#include "core/frame/LocalDOMWindow.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/wtf/text/WTFString.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef CreateWindow

namespace blink {
class ExceptionState;
class LocalFrame;
struct FrameLoadRequest;
struct WebWindowFeatures;

DOMWindow* CreateWindow(const String& url_string,
                        const AtomicString& frame_name,
                        const String& window_features_string,
                        LocalDOMWindow& calling_window,
                        LocalFrame& first_frame,
                        LocalFrame& opener_frame,
                        ExceptionState&);

void CreateWindowForRequest(const FrameLoadRequest&,
                            LocalFrame& opener_frame,
                            NavigationPolicy);

// Exposed for testing
CORE_EXPORT NavigationPolicy
EffectiveNavigationPolicy(NavigationPolicy,
                          const WebInputEvent* current_event,
                          const WebWindowFeatures&);

// Exposed for testing
CORE_EXPORT WebWindowFeatures GetWindowFeaturesFromString(const String&);

}  // namespace blink

#endif  // CreateWindow_h
