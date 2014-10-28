// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDataConsumerHandle_h
#define WebDataConsumerHandle_h

#include <stddef.h>

namespace blink {

// WebDataConsumerHandle represents the "consumer" side of a data pipe. A user
// can read data from it.
// This data type is basically a wrapper of mojo data pipe consumer handle.
// See mojo/public/c/system/data_pipe.h for details.
//
// Note:
//  - If you register a client, all of read / beginRead / endRead /
//    registerClient / unregisterClient must be called on the same thread.
//    Client notification is called on the thread.
class WebDataConsumerHandle {
public:
    // This corresponds to MojoReadDataFlags.
    using Flags = unsigned;
    static const Flags FlagNone = 0;

    enum Result {
        Ok,
        Done,
        Busy,
        ShouldWait,
        ResourceExhausted,
        UnexpectedError,
    };

    // Client gets notification from the pipe.
    class Client {
    public:
        virtual ~Client() { }
        // The associated handle gets readable.
        virtual void didGetReadable() = 0;
    };

    virtual ~WebDataConsumerHandle() { }

    // Reads data into |data| up to |size| bytes. The actual read size will be
    // stored in |*readSize|. This function cannot be called when a two-phase
    // read is in progress.
    // Returns Done when it reaches to the end of the data.
    virtual Result read(void* data, size_t /* size */, Flags, size_t* readSize) { return UnexpectedError; }

    // Begins a two-phase read. On success, the function stores a buffer that
    // contains the read data of length |*available| into |*buffer|.
    // Returns Done when it reaches to the end of the data.
    // On fail, you don't have to (and should not) call endRead, because the
    // read session implicitly ends in that case.
    virtual Result beginRead(const void** buffer, Flags, size_t* available) { return UnexpectedError; }

    // Ends a two-phase read.
    // |readSize| indicates the actual read size.
    virtual Result endRead(size_t readSize) { return UnexpectedError; }

    // Registers |client| to this handle. The client must not be null and must
    // be valid until it is unregistered (or the handle is destructed).
    // Only one registration can be made for a handle at a time.
    virtual void registerClient(Client* /* client */)  { }

    // Unregisters |client| from this handle.
    virtual void unregisterClient() { }
};

} // namespace blink

#endif // WebDataConsumerHandle_h
