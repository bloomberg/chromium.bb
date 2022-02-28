// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

SerialChooserController::SerialChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback)
    : ChooserController(CreateExtensionAwareChooserTitle(
          render_frame_host,
          IDS_SERIAL_PORT_CHOOSER_PROMPT_ORIGIN,
          IDS_SERIAL_PORT_CHOOSER_PROMPT_EXTENSION_NAME)),
      filters_(std::move(filters)),
      callback_(std::move(callback)),
      frame_tree_node_id_(render_frame_host->GetFrameTreeNodeId()) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  origin_ = web_contents->GetMainFrame()->GetLastCommittedOrigin();

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ =
      SerialChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);

  chooser_context_->GetPortManager()->GetDevices(base::BindOnce(
      &SerialChooserController::OnGetDevices, weak_factory_.GetWeakPtr()));
  observation_.Observe(chooser_context_.get());
}

SerialChooserController::~SerialChooserController() {
  if (callback_)
    RunCallback(/*port=*/nullptr);
}

bool SerialChooserController::ShouldShowHelpButton() const {
  return true;
}

std::u16string SerialChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string SerialChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
SerialChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_LOADING_LABEL),
      l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t SerialChooserController::NumOptions() const {
  return ports_.size();
}

std::u16string SerialChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, ports_.size());
  const device::mojom::SerialPortInfo& port = *ports_[index];

  // Get the last component of the device path i.e. COM1 or ttyS0 to show the
  // user something similar to other applications that ask them to choose a
  // serial port and to differentiate between ports with similar display names.
  std::u16string display_path = port.path.BaseName().LossyDisplayName();

  if (port.display_name && !port.display_name->empty()) {
    return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_NAME_WITH_PATH,
                                      base::UTF8ToUTF16(*port.display_name),
                                      display_path);
  }

  return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_PATH_ONLY,
                                    display_path);
}

bool SerialChooserController::IsPaired(size_t index) const {
  DCHECK_LE(index, ports_.size());

  if (!chooser_context_)
    return false;

  return chooser_context_->HasPortPermission(origin_, *ports_[index]);
}

void SerialChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, ports_.size());

  if (!chooser_context_) {
    RunCallback(/*port=*/nullptr);
    return;
  }

  chooser_context_->GrantPortPermission(origin_, *ports_[index]);
  RunCallback(ports_[index]->Clone());
}

void SerialChooserController::Cancel() {}

void SerialChooserController::Close() {}

void SerialChooserController::OpenHelpCenterUrl() const {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  web_contents->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserSerialOverviewUrl), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false));
}

void SerialChooserController::OnPortAdded(
    const device::mojom::SerialPortInfo& port) {
  if (!DisplayDevice(port))
    return;

  ports_.push_back(port.Clone());
  if (view())
    view()->OnOptionAdded(ports_.size() - 1);
}

void SerialChooserController::OnPortRemoved(
    const device::mojom::SerialPortInfo& port) {
  const auto it = std::find_if(
      ports_.begin(), ports_.end(),
      [&port](const auto& ptr) { return ptr->token == port.token; });
  if (it != ports_.end()) {
    const size_t index = it - ports_.begin();
    ports_.erase(it);
    if (view())
      view()->OnOptionRemoved(index);
  }
}

void SerialChooserController::OnPortManagerConnectionError() {
  observation_.Reset();
}

void SerialChooserController::OnGetDevices(
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  // Sort ports by file paths.
  std::sort(ports.begin(), ports.end(),
            [](const auto& port1, const auto& port2) {
              return port1->path.BaseName() < port2->path.BaseName();
            });

  for (auto& port : ports) {
    if (DisplayDevice(*port))
      ports_.push_back(std::move(port));
  }

  if (view())
    view()->OnOptionsInitialized();
}

bool SerialChooserController::DisplayDevice(
    const device::mojom::SerialPortInfo& port) const {
  if (SerialBlocklist::Get().IsExcluded(port))
    return false;

  if (filters_.empty())
    return true;

  for (const auto& filter : filters_) {
    if (filter->has_vendor_id &&
        (!port.has_vendor_id || filter->vendor_id != port.vendor_id)) {
      continue;
    }
    if (filter->has_product_id &&
        (!port.has_product_id || filter->product_id != port.product_id)) {
      continue;
    }
    return true;
  }

  return false;
}

void SerialChooserController::RunCallback(
    device::mojom::SerialPortInfoPtr port) {
  auto outcome = ports_.empty() ? SerialChooserOutcome::kCancelledNoDevices
                                : SerialChooserOutcome::kCancelled;

  if (port) {
    outcome = SerialChooserContext::CanStorePersistentEntry(*port)
                  ? SerialChooserOutcome::kPermissionGranted
                  : SerialChooserOutcome::kEphemeralPermissionGranted;
  }

  UMA_HISTOGRAM_ENUMERATION("Permissions.Serial.ChooserClosed", outcome);
  std::move(callback_).Run(std::move(port));
}
