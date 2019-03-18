/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_INPROCESSRENDERER_H
#define INCLUDED_BLPWTK2_INPROCESSRENDERER_H

#include <blpwtk2_config.h>

#include <base/memory/ref_counted.h>

#include <string>

namespace base {
class SingleThreadTaskRunner;
}  // close namespace base

namespace mojo {
    class OutgoingInvitation;
}

namespace blpwtk2 {

                        // ========================
                        // struct InProcessRenderer
                        // ========================

// Utility for initializing and cleaning up the in-process renderer.  If we are
// running in ORIGINAL thread mode, then the in-process renderer will be
// initialized on a separate thread.  See blpwtk2_toolkit.h for an explanation
// about thread modes.
// Note that even if an application does not use an in-process renderer, we
// must still initialize it because there is code in the chromium browser
// process that tries to use facilities that is initialized during Blink
// startup.
// TODO: See if this is still the case once browser & renderer are split into
// TODO: separate dlls.
struct InProcessRenderer {
    // Initialize the renderer.  This will initialize Blink on the main thread,
    // or on a secondary thread, depending on the thread mode.  The browserIOTaskRunner
    // should be null if the browser is not in the same process.
    static void init(bool                                               isHost,
                     const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
                     mojo::OutgoingInvitation* broker_client_invitation,
                     const std::string&                                 serviceToken,
                     int                                                mojoHandle);

    // Perform any cleanup, such as shutting down the secondary thread if we
    // are in the original chromium thread mode.
    static void cleanup();

    // Return the task runner for the renderer's IO thread.  This can only be
    // called between calls to 'init()' and 'cleanup()'.
    static scoped_refptr<base::SingleThreadTaskRunner> ioTaskRunner();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_INPROCESSRENDERER_H

// vim: ts=4 et

