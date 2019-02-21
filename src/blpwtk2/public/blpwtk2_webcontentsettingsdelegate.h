/*
 * Copyright (C) 2016 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_WEBCONTENTSETTINGSDELEGATE_H
#define INCLUDED_BLPWTK2_WEBCONTENTSETTINGSDELEGATE_H

#include <blpwtk2_config.h>

namespace blpwtk2 {

// This class can be implemented by the application to establish the various
// permissions of a blpwtk2::WebFrame.  The delegate is passed to the WebFrame
// after it has been created and made accessible to the application (after it
// has been loaded).
//
// All methods on the delegate are invoked in the application's renderer thread.

class BLPWTK2_EXPORT WebContentSettingsDelegate {
  public:
    virtual ~WebContentSettingsDelegate();

    // Controls whether insecure content is allowed to display for this frame.
    virtual bool allowDisplayingInsecureContent(bool enabledPerSettings) = 0;

    // Controls whether insecure scripts are allowed to execute for this frame.
    virtual bool allowRunningInsecureContent(bool enabledPerSettings) = 0;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBCONTENTSETTINGSDELEGATE_H

// vim: ts=4 et

