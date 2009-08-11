/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebStorageArea_h
#define WebStorageArea_h

#include "WebCommon.h"

namespace WebKit {
    class WebString;

    // In WebCore, there's one distinct StorageArea per origin per StorageNamespace. This
    // class wraps a StorageArea. With the possible exception of lock/unlock all of these
    // methods map very obviously to the WebStorage (often called DOM Storage) spec.
    // http://dev.w3.org/html5/webstorage/
    class WebStorageArea {
    public:
        virtual ~WebStorageArea() { }

        // Lock the storage area. Before calling any other other methods on this interface,
        // you should always call lock and wait for it to return. InvalidateCache tells you
        // that since the last time you locked the cache, someone else has modified it.
        // BytesLeftInQuota tells you how many bytes are currently unused in the quota.
        // These are both optimizations and can be ignored if you'd like.
        virtual void lock(bool& invalidateCache, size_t& bytesLeftInQuota) = 0;

        // Unlock the storage area. You should call this at the end of the JavaScript context
        // or when you're about to execute anything synchronous per the DOM Storage spec.
        virtual void unlock() = 0;

        // The number of key/value pairs in the storage area.
        virtual unsigned length() = 0;

        // Get a value for a specific key. Valid key indices are 0 through length() - 1.
        // Indexes may change on any set/removeItem call. Will return null if the index
        // provided is out of range.
        virtual WebString key(unsigned index) = 0;

        // Get the value that corresponds to a specific key. This returns null if there is
        // no entry for that key.
        virtual WebString getItem(const WebString& key) = 0;

        // Set the value that corresponds to a specific key. QuotaException is set if we've
        // the StorageArea would have exceeded its quota. The value is NOT set when there's
        // an exception.
        virtual void setItem(const WebString& key, const WebString& value, bool& quotaException) = 0;

        // Remove the value associated with a particular key.
        virtual void removeItem(const WebString& key) = 0;

        // Clear all key/value pairs.
        virtual void clear() = 0;
    };

} // namespace WebKit

#endif // WebStorageArea_h
