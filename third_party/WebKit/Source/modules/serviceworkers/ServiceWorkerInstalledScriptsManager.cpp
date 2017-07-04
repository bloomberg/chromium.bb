// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerInstalledScriptsManager.h"

#include "core/html/parser/TextResourceDecoder.h"
#include "modules/serviceworkers/ServiceWorkerThread.h"

namespace blink {

ServiceWorkerInstalledScriptsManager::ServiceWorkerInstalledScriptsManager(
    std::unique_ptr<WebServiceWorkerInstalledScriptsManager> manager)
    : manager_(std::move(manager)) {}

bool ServiceWorkerInstalledScriptsManager::IsScriptInstalled(
    const KURL& script_url) const {
  return manager_->IsScriptInstalled(script_url);
}

Optional<InstalledScriptsManager::ScriptData>
ServiceWorkerInstalledScriptsManager::GetScriptData(const KURL& script_url) {
  DCHECK(!IsMainThread());
  // This blocks until the script is received from the browser.
  std::unique_ptr<WebServiceWorkerInstalledScriptsManager::RawScriptData>
      raw_script_data = manager_->GetRawScriptData(script_url);

  if (!raw_script_data)
    return WTF::nullopt;

  // This is from WorkerScriptLoader::DidReceiveData.
  std::unique_ptr<TextResourceDecoder> decoder;
  if (!raw_script_data->Encoding().IsEmpty()) {
    decoder = TextResourceDecoder::Create(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        WTF::TextEncoding(raw_script_data->Encoding())));
  } else {
    decoder = TextResourceDecoder::Create(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent));
  }

  InstalledScriptsManager::ScriptData script_data;
  // TODO(shimazu): Read the headers for ContentSecurityPolicy, ReferrerPolicy,
  // and OriginTrialTokens and set them to |script_data|.
  for (const auto& chunk : raw_script_data->ScriptTextChunks()) {
    script_data.source_text.append(decoder->Decode(chunk.Data(), chunk.size()));
  }
  if (raw_script_data->MetaDataChunks().size() > 0) {
    size_t total_metadata_size = 0;
    for (const auto& chunk : raw_script_data->MetaDataChunks())
      total_metadata_size += chunk.size();
    script_data.meta_data = WTF::MakeUnique<Vector<char>>();
    script_data.meta_data->ReserveInitialCapacity(total_metadata_size);
    for (const auto& chunk : raw_script_data->MetaDataChunks())
      script_data.meta_data->Append(chunk.Data(), chunk.size());
  }
  script_data.headers.Adopt(raw_script_data->TakeHeaders());
  return Optional<ScriptData>(std::move(script_data));
}

}  // namespace blink
