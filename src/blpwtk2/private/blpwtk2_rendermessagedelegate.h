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

#include <base/memory/ref_counted.h>
#include <ipc/message_router.h>
#include <mojo/public/cpp/bindings/associated_receiver.h>
#include <mojo/public/cpp/bindings/associated_remote.h>

namespace mojo {
namespace internal {
class MultiplexRouter;
}
}

namespace blpwtk2 {

class RenderMessageDelegate : public IPC::MessageRouter {
  public:

    static RenderMessageDelegate* GetInstance();

    RenderMessageDelegate();
    ~RenderMessageDelegate() final;

    // IPC::MessageRouter overrides:
    bool OnControlMessageReceived(const IPC::Message& msg) override;

    template<typename Interface>
    mojo::PendingAssociatedReceiver<Interface>
    InitWithNewAssociatedEndpointAndPassReceiver(mojo::PendingAssociatedRemote<Interface>& pending_remote);

    template<typename Interface>
    mojo::PendingAssociatedRemote<Interface>
    BindNewAssociatedEndpointAndPassRemote(mojo::AssociatedReceiver<Interface>& receiver);

    template<typename Interface>
    mojo::PendingAssociatedReceiver<Interface>
    BindNewAssociatedEndpointAndPassReceiver(mojo::AssociatedRemote<Interface>& remote);

  private:

    IPC::MessageRouter d_router;

    scoped_refptr<mojo::internal::MultiplexRouter> d_router0, d_router1;
};

template<typename Interface>
mojo::PendingAssociatedReceiver<Interface>
RenderMessageDelegate::InitWithNewAssociatedEndpointAndPassReceiver(mojo::PendingAssociatedRemote<Interface>& pending_remote) {
    mojo::PendingAssociatedReceiver<Interface> receiver = pending_remote.InitWithNewEndpointAndPassReceiver();
    mojo::ScopedInterfaceEndpointHandle receiver_handle = receiver.PassHandle();
    mojo::InterfaceId id = d_router1->AssociateInterface(std::move(receiver_handle));
    receiver_handle = d_router0->CreateLocalEndpointHandle(id);
    receiver.set_handle(std::move(receiver_handle));

    return receiver;
}

template<typename Interface>
mojo::PendingAssociatedRemote<Interface>
RenderMessageDelegate::BindNewAssociatedEndpointAndPassRemote(mojo::AssociatedReceiver<Interface>& receiver) {
    mojo::PendingAssociatedRemote<Interface> remote;
    receiver.Bind(InitWithNewAssociatedEndpointAndPassReceiver(remote));
    return remote;
}

template<typename Interface>
mojo::PendingAssociatedReceiver<Interface>
RenderMessageDelegate::BindNewAssociatedEndpointAndPassReceiver(mojo::AssociatedRemote<Interface>& remote) {
  mojo::PendingAssociatedRemote<Interface> newPendingRemote;
  auto receiver = InitWithNewAssociatedEndpointAndPassReceiver(newPendingRemote);
  remote.Bind(std::move(newPendingRemote));
  return receiver;
}

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERMESSAGEDELEGATE_H
