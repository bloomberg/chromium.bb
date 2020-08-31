// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_FAKE_TRANSLATE_AGENT_H_
#define CHROME_BROWSER_TRANSLATE_FAKE_TRANSLATE_AGENT_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

class FakeTranslateAgent : public translate::mojom::TranslateAgent {
 public:
  FakeTranslateAgent();
  ~FakeTranslateAgent() override;

  // TODO(crbug.com/1064974) Remove with subframe translation launch.
  mojo::PendingRemote<translate::mojom::TranslateAgent> BindToNewPageRemote();

  // translate::mojom::TranslateAgent implementation.
  void GetWebLanguageDetectionDetails(
      GetWebLanguageDetectionDetailsCallback callback) override;

  void TranslateFrame(const std::string& translate_script,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateFrameCallback callback) override;

  void RevertTranslation() override;

  void PageTranslated(bool cancelled,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      translate::TranslateErrors::Type error);

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle);

  bool called_translate_;
  base::Optional<std::string> source_lang_;
  base::Optional<std::string> target_lang_;
  bool called_revert_translation_;
  std::string next_page_lang_;
  bool next_page_translatable_;

 private:
  TranslateFrameCallback translate_callback_pending_;
  // TODO(crbug.com/1064974) Remove with subframe translation launch.
  mojo::Receiver<translate::mojom::TranslateAgent> receiver_{this};
  mojo::AssociatedReceiverSet<translate::mojom::TranslateAgent>
      per_frame_translate_agent_receivers_;
  DISALLOW_COPY_AND_ASSIGN(FakeTranslateAgent);
};

#endif  // CHROME_BROWSER_TRANSLATE_FAKE_TRANSLATE_AGENT_H_
