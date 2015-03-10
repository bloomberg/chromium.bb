// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationController_h
#define WebPresentationController_h

namespace blink {

class WebPresentationSessionClient;

// The delegate Blink provides to WebPresentationClient in order to get updates.
class WebPresentationController {
public:
    virtual ~WebPresentationController() { }

    // Called when the presentation screen availability changes.
    // May not be called if there's no registered listener for the event.
    virtual void didChangeAvailability(bool available) = 0;

    // Indicates if the frame has listeners to the |availablechange| event.
    virtual bool isAvailableChangeWatched() const = 0;

    // Called when the presentation session is started by the embedder using
    // the default presentation URL and id.
    virtual void didStartDefaultSession(WebPresentationSessionClient*) = 0;
};

} // namespace blink

#endif // WebPresentationController_h
