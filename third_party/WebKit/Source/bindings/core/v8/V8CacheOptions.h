/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef V8CacheOptions_h
#define V8CacheOptions_h

namespace blink {

enum V8CacheOptions {
    V8CacheOptionsDefault, // Use whatever the current default is.
    V8CacheOptionsParse, // Use parser caching.
    V8CacheOptionsCode, // Use code caching.
    V8CacheOptionsCodeCompressed, // Use code caching and compress the code.
    V8CacheOptionsNone, // V8 caching turned off.
    V8CacheOptionsParseMemory, // Use parser in-memory caching (no disk).
    V8CacheOptionsHeuristics, // Mixed strategy: Cache code if it's a likely win.
    V8CacheOptionsHeuristicsMobile, // As above, but tuned for mobile.
    V8CacheOptionsHeuristicsDefault, // Cache code or default.
    V8CacheOptionsHeuristicsDefaultMobile, // As above, but tuned for mobile.
    V8CacheOptionsRecent, // Cache recently compiled code.
    V8CacheOptionsRecentSmall // Cache small recently compiled code.
};

} // namespace blink

#endif // V8CacheOptions_h
