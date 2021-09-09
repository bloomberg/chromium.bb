/*
 * Copyright (C) 2020 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ADDRESS_SPACE_FEATURE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ADDRESS_SPACE_FEATURE_H_

#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class LocalFrame;
class ResourceError;
class ResourceResponse;

// Describes a type of fetch for the purposes of categorizing feature use.
enum class FetchType {
  // A document fetching a subresource (image, script, etc.).
  kSubresource,

  // A navigation from one document to the next.
  kNavigation,
};

// Returns the kAddressSpace* WebFeature enum value corresponding to a client
// in |client_address_space| fetching data from |response_address_space|, if
// any.
//
// |fetch_type| describes the fetch itself.
//
// |client_is_secure_context| specifies whether the client execution context is
// a secure context, as defined in
// https://html.spec.whatwg.org/multipage/webappapis.html#secure-context.
//
// Returns nullopt if the load is not a private network request, as defined in
// https://wicg.github.io/cors-rfc1918/#private-network-request.
absl::optional<mojom::blink::WebFeature> CORE_EXPORT AddressSpaceFeature(
    FetchType fetch_type,
    network::mojom::blink::IPAddressSpace client_address_space,
    bool client_is_secure_context,
    network::mojom::blink::IPAddressSpace response_address_space);

// Increments the correct kAddressSpace* WebFeature UseCounter corresponding to
// the given |client_frame| performing a fetch of type |fetch_type| and
// receiving the given |response|.
//
// Does nothing if |client_frame| is nullptr.
void RecordAddressSpaceFeature(FetchType fetch_type,
                               LocalFrame* client_frame,
                               const ResourceResponse& response);

// Same as above, for cases where the fetch failed.
// Does nothing if the fetch failed due to an error other than a failed Private
// Network Access check.
void RecordAddressSpaceFeature(FetchType fetch_type,
                               LocalFrame* client_frame,
                               const ResourceError& error);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_ADDRESS_SPACE_FEATURE_H_
