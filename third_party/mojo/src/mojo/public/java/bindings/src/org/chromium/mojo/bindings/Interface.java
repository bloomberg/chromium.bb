// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;

import java.io.Closeable;

/**
 * Base class for mojo generated interfaces.
 */
public interface Interface extends ConnectionErrorHandler, Closeable {

    /**
     * The close method is called when the connection to the interface is closed.
     *
     * @see java.io.Closeable#close()
     */
    @Override
    public void close();

    /**
     * A proxy to a mojo interface. This is base class for all generated proxies. It implements the
     * Interface and each time a method is called, the parameters are serialized and sent to the
     * {@link MessageReceiverWithResponder}, along with the response callback if needed.
     */
    public interface Proxy extends Interface {
        /**
         * Class allowing to interact with the proxy itself.
         */
        public interface Handler extends Closeable {
            /**
             * Sets the {@link ConnectionErrorHandler} that will be notified of errors.
             */
            public void setErrorHandler(ConnectionErrorHandler errorHandler);
        }

        /**
         * Returns the {@link Handler} object allowing to interact with the proxy itself.
         */
        public Handler getProxyHandler();
    }

    /**
     * Base implementation of {@link Proxy}.
     */
    abstract class AbstractProxy implements Proxy {
        /**
         * Implementation of {@link Handler}.
         */
        protected static class HandlerImpl implements Proxy.Handler, ConnectionErrorHandler {
            /**
             * The {@link Core} implementation to use.
             */
            private final Core mCore;

            /**
             * The {@link MessageReceiverWithResponder} that will receive a serialized message for
             * each method call.
             */
            private final MessageReceiverWithResponder mMessageReceiver;

            /**
             * The {@link ConnectionErrorHandler} that will be notified of errors.
             */
            private ConnectionErrorHandler mErrorHandler = null;

            /**
             * Constructor.
             *
             * @param core the Core implementation used to create pipes and access the async waiter.
             * @param messageReceiver the message receiver to send message to.
             */
            protected HandlerImpl(Core core, MessageReceiverWithResponder messageReceiver) {
                this.mCore = core;
                this.mMessageReceiver = messageReceiver;
            }

            /**
             * Returns the message receiver to send message to.
             */
            public MessageReceiverWithResponder getMessageReceiver() {
                return mMessageReceiver;
            }

            /**
             * Returns the Core implementation.
             */
            public Core getCore() {
                return mCore;
            }

            /**
             * Sets the {@link ConnectionErrorHandler} that will be notified of errors.
             */
            @Override
            public void setErrorHandler(ConnectionErrorHandler errorHandler) {
                this.mErrorHandler = errorHandler;
            }

            /**
             * @see ConnectionErrorHandler#onConnectionError(MojoException)
             */
            @Override
            public void onConnectionError(MojoException e) {
                if (mErrorHandler != null) {
                    mErrorHandler.onConnectionError(e);
                }
            }

            /**
             * @see Closeable#close()
             */
            @Override
            public void close() {
                mMessageReceiver.close();
            }
        }

        /**
         * The handler associated with this proxy.
         */
        private final HandlerImpl mHandler;

        protected AbstractProxy(Core core, MessageReceiverWithResponder messageReceiver) {
            mHandler = new HandlerImpl(core, messageReceiver);
        }

        /**
         * @see Interface#close()
         */
        @Override
        public void close() {
            mHandler.close();
        }

        /**
         * @see Proxy#getProxyHandler()
         */
        @Override
        public HandlerImpl getProxyHandler() {
            return mHandler;
        }

        /**
         * @see ConnectionErrorHandler#onConnectionError(org.chromium.mojo.system.MojoException)
         */
        @Override
        public void onConnectionError(MojoException e) {
            mHandler.onConnectionError(e);
        }
    }

    /**
     * Base implementation of Stub. Stubs are message receivers that deserialize the payload and
     * call the appropriate method in the implementation. If the method returns result, the stub
     * serializes the response and sends it back.
     *
     * @param <I> the type of the interface to delegate calls to.
     */
    abstract class Stub<I extends Interface> implements MessageReceiverWithResponder {

        /**
         * The {@link Core} implementation to use.
         */
        private final Core mCore;

        /**
         * The implementation to delegate calls to.
         */
        private final I mImpl;

        /**
         * Constructor.
         *
         * @param core the {@link Core} implementation to use.
         * @param impl the implementation to delegate calls to.
         */
        public Stub(Core core, I impl) {
            mCore = core;
            mImpl = impl;
        }

        /**
         * Returns the Core implementation.
         */
        protected Core getCore() {
            return mCore;
        }

        /**
         * Returns the implementation to delegate calls to.
         */
        protected I getImpl() {
            return mImpl;
        }

        /**
         * @see org.chromium.mojo.bindings.MessageReceiver#close()
         */
        @Override
        public void close() {
            mImpl.close();
        }

    }

    /**
     * The |Manager| object enables building of proxies and stubs for a given interface.
     *
     * @param <I> the type of the interface the manager can handle.
     * @param <P> the type of the proxy the manager can handle. To be noted, P always extends I.
     */
    abstract class Manager<I extends Interface, P extends Proxy> {

        /**
         * Returns the name of the interface. This is an opaque (but human readable) identifier used
         * by the service provider to identify services.
         */
        public abstract String getName();

        /**
         * Binds the given implementation to the handle.
         */
        public void bind(I impl, MessagePipeHandle handle) {
            // The router (and by consequence the handle) is intentionally leaked. It will close
            // itself when the connected handle is closed and the proxy receives the connection
            // error.
            Router router = new RouterImpl(handle);
            bind(handle.getCore(), impl, router);
            router.start();
        }

        /**
         * Binds the given implementation to the InterfaceRequest.
         */
        public final void bind(I impl, InterfaceRequest<I> request) {
            bind(impl, request.passHandle());
        }

        /**
         * Returns a Proxy that will send messages to the given |handle|. This implies that the
         * other end of the handle must be binded to an implementation of the interface.
         */
        public final P attachProxy(MessagePipeHandle handle) {
            RouterImpl router = new RouterImpl(handle);
            P proxy = attachProxy(handle.getCore(), router);
            DelegatingConnectionErrorHandler handlers = new DelegatingConnectionErrorHandler();
            handlers.addConnectionErrorHandler(proxy);
            router.setErrorHandler(handlers);
            router.start();
            return proxy;
        }

        /**
         * Constructs a new |InterfaceRequest| for the interface. This method returns a Pair where
         * the first element is a proxy, and the second element is the request. The proxy can be
         * used immediately.
         */
        public final Pair<P, InterfaceRequest<I>> getInterfaceRequest(Core core) {
            Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
            P proxy = attachProxy(handles.first);
            return Pair.create(proxy, new InterfaceRequest<I>(handles.second));
        }

        /**
         * Binds the implementation to the given |router|.
         */
        final void bind(Core core, I impl, Router router) {
            router.setErrorHandler(impl);
            router.setIncomingMessageReceiver(buildStub(core, impl));
        }

        /**
         * Returns a Proxy that will send messages to the given |router|.
         */
        final P attachProxy(Core core, Router router) {
            return buildProxy(core, new AutoCloseableRouter(core, router));
        }

        /**
         * Creates a new array of the given |size|.
         */
        protected abstract I[] buildArray(int size);

        /**
         * Constructs a Stub delegating to the given implementation.
         */
        protected abstract Stub<I> buildStub(Core core, I impl);

        /**
         * Constructs a Proxy forwarding the calls to the given message receiver.
         */
        protected abstract P buildProxy(Core core, MessageReceiverWithResponder messageReceiver);

    }
}
