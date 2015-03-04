// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.application;

import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.system.MessagePipeHandle;

/**
 * ServiceFactoryBinder holds the necessary information to bind a service interface to a message
 * pipe.
 *
 * @param <T> A mojo service interface.
 */
public interface ServiceFactoryBinder<T extends Interface> {
    /**
     * An application implements to bind a service implementation to |pipe|.
     *
     * @param pipe A handle to the incoming connection pipe.
     */
    public void bindNewInstanceToMessagePipe(MessagePipeHandle pipe);

    /**
     * Name of the service interface being implemented.
     *
     * @return Service interface name.
     */
    public String getInterfaceName();
}
