/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_RENDERMESSAGEDELEGATE_H
#define INCLUDED_BLPWTK2_RENDERMESSAGEDELEGATE_H

#include <base/lazy_instance.h>
#include <ipc/message_router.h>

namespace blpwtk2 {

class RenderMessageDelegate : public IPC::MessageRouter {
  public:

    static RenderMessageDelegate* GetInstance();

    IPC::MessageRouter* GetRouter() { return &d_router; }

    // IPC::MessageRouter overrides:
    bool OnControlMessageReceived(const IPC::Message& msg) override;

  private:

    friend struct base::LazyInstanceTraitsBase<RenderMessageDelegate>;

    RenderMessageDelegate();
    ~RenderMessageDelegate() final;

    IPC::MessageRouter d_router;
};

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERMESSAGEDELEGATE_H
