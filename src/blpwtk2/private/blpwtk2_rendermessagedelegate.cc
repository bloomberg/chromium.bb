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

#include <blpwtk2_rendermessagedelegate.h>

#include <base/lazy_instance.h>
#include <mojo/public/cpp/bindings/lib/multiplex_router.h>
#include <mojo/public/cpp/system/message_pipe.h>

namespace blpwtk2 {

namespace {

static base::LazyInstance<RenderMessageDelegate>::DestructorAtExit
    s_instance = LAZY_INSTANCE_INITIALIZER;

} // close namespace

RenderMessageDelegate::RenderMessageDelegate()
{
    mojo::MessagePipe pipe;

    d_router0 =
        new mojo::internal::MultiplexRouter(
            std::move(pipe.handle0), mojo::internal::MultiplexRouter::MULTI_INTERFACE,
            false, base::SequencedTaskRunnerHandle::Get());

    d_router1 =
        new mojo::internal::MultiplexRouter(
            std::move(pipe.handle1), mojo::internal::MultiplexRouter::MULTI_INTERFACE,
            true, base::SequencedTaskRunnerHandle::Get());
}

RenderMessageDelegate::~RenderMessageDelegate()
{
}

RenderMessageDelegate* RenderMessageDelegate::GetInstance()
{
    return s_instance.Pointer();
}

// IPC::MessageRouter overrides:
bool RenderMessageDelegate::OnControlMessageReceived(const IPC::Message& msg)
{
    return false;
}

} // close namespace blpwtk2
