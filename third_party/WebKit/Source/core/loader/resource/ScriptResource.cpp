/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "core/loader/resource/ScriptResource.h"

#include "core/frame/SubresourceIntegrity.h"
#include "platform/SharedBuffer.h"
#include "platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "platform/instrumentation/tracing/web_process_memory_dump.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

ScriptResource* ScriptResource::Fetch(FetchParameters& params,
                                      ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  params.SetRequestContext(WebURLRequest::kRequestContextScript);
  ScriptResource* resource = ToScriptResource(
      fetcher->RequestResource(params, ScriptResourceFactory()));
  if (resource && !params.IntegrityMetadata().IsEmpty())
    resource->SetIntegrityMetadata(params.IntegrityMetadata());
  return resource;
}

ScriptResource::ScriptResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : TextResource(resource_request, kScript, options, decoder_options) {}

ScriptResource::~ScriptResource() {}

void ScriptResource::DidAddClient(ResourceClient* client) {
  DCHECK(ScriptResourceClient::IsExpectedType(client));
  Resource::DidAddClient(client);
}

void ScriptResource::AppendData(const char* data, size_t length) {
  Resource::AppendData(data, length);
  ResourceClientWalker<ScriptResourceClient> walker(Clients());
  while (ScriptResourceClient* client = walker.Next())
    client->NotifyAppendData(this);
}

void ScriptResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                                  WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/decoded_script";
  auto dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", source_text_.CharactersSizeInBytes());
  memory_dump->AddSuballocation(
      dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

const String& ScriptResource::SourceText() {
  DCHECK(IsLoaded());

  if (source_text_.IsNull() && Data()) {
    String source_text = DecodedText();
    ClearData();
    SetDecodedSize(source_text.CharactersSizeInBytes());
    source_text_ = AtomicString(source_text);
  }

  return source_text_;
}

void ScriptResource::DestroyDecodedDataForFailedRevalidation() {
  source_text_ = AtomicString();
}

// static
bool ScriptResource::MimeTypeAllowedByNosniff(
    const ResourceResponse& response) {
  return ParseContentTypeOptionsHeader(
             response.HttpHeaderField(HTTPNames::X_Content_Type_Options)) !=
             kContentTypeOptionsNosniff ||
         MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
             response.HttpContentType());
}

AccessControlStatus ScriptResource::CalculateAccessControlStatus() const {
  if (GetCORSStatus() == CORSStatus::kServiceWorkerOpaque)
    return kOpaqueResource;

  if (IsSameOriginOrCORSSuccessful())
    return kSharableCrossOrigin;

  return kNotSharableCrossOrigin;
}

void ScriptResource::CheckResourceIntegrity(Document& document) {
  // Already checked? Retain existing result.
  //
  // TODO(vogelheim): If IntegrityDisposition() is kFailed, this should
  // probably also generate a console message identical to the one produced
  // by the CheckSubresourceIntegrity call below. See crbug.com/585267.
  if (IntegrityDisposition() != ResourceIntegrityDisposition::kNotChecked)
    return;

  // Loading error occurred? Then result is uncheckable.
  if (ErrorOccurred())
    return;

  // No integrity attributes to check? Then we're passing.
  if (IntegrityMetadata().IsEmpty()) {
    SetIntegrityDisposition(ResourceIntegrityDisposition::kPassed);
    return;
  }

  CHECK(!!ResourceBuffer());
  bool passed = SubresourceIntegrity::CheckSubresourceIntegrity(
      IntegrityMetadata(), document, ResourceBuffer()->Data(),
      ResourceBuffer()->size(), Url(), *this);
  SetIntegrityDisposition(passed ? ResourceIntegrityDisposition::kPassed
                                 : ResourceIntegrityDisposition::kFailed);
  DCHECK_NE(IntegrityDisposition(), ResourceIntegrityDisposition::kNotChecked);
}

}  // namespace blink
