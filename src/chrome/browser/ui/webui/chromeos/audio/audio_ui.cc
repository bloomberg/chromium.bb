// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/audio/audio_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/audio_resources.h"
#include "chrome/grit/audio_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

AudioUI::AudioUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://audio source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIAudioHost);

  webui::SetupWebUIDataSource(
      html_source, base::make_span(kAudioResources, kAudioResourcesSize),
      IDR_AUDIO_AUDIO_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(AudioUI)

AudioUI::~AudioUI() = default;

void AudioUI::BindInterface(
    mojo::PendingReceiver<audio::mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void AudioUI::CreatePageHandler(
    mojo::PendingRemote<audio::mojom::Page> page,
    mojo::PendingReceiver<audio::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<AudioHandler>(std::move(receiver), std::move(page));
}

}  // namespace chromeos
