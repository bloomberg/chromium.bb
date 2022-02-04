// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace extensions {

namespace {

const char kDesktopCaptureApiNoTabIdError[] =
    "targetTab doesn't have id field set.";
const char kDesktopCaptureApiNoUrlError[] =
    "targetTab doesn't have URL field set.";
const char kDesktopCaptureApiInvalidOriginError[] =
    "targetTab.url is not a valid URL.";
const char kDesktopCaptureApiInvalidTabIdError[] = "Invalid tab specified.";
const char kDesktopCaptureApiTabUrlNotSecure[] =
    "URL scheme for the specified tab is not secure.";
}  // namespace

DesktopCaptureChooseDesktopMediaFunction::
    DesktopCaptureChooseDesktopMediaFunction() {
}

DesktopCaptureChooseDesktopMediaFunction::
    ~DesktopCaptureChooseDesktopMediaFunction() {
}

ExtensionFunction::ResponseAction
DesktopCaptureChooseDesktopMediaFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);
  const auto& request_id_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(request_id_value.is_int());
  request_id_ = request_id_value.GetInt();
  DesktopCaptureRequestsRegistry::GetInstance()->AddRequest(source_process_id(),
                                                            request_id_, this);

  mutable_args().erase(args().begin());

  std::unique_ptr<api::desktop_capture::ChooseDesktopMedia::Params> params =
      api::desktop_capture::ChooseDesktopMedia::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // |target_render_frame_host| is the RenderFrameHost for which the stream is
  // created, and will also be used to determine where to show the picker's UI.
  content::RenderFrameHost* target_render_frame_host = nullptr;
  std::u16string target_name;
  GURL origin;
  if (params->target_tab) {
    if (!params->target_tab->url) {
      return RespondNow(Error(kDesktopCaptureApiNoUrlError));
    }
    origin = GURL(*(params->target_tab->url)).DeprecatedGetOriginAsURL();

    if (!origin.is_valid()) {
      return RespondNow(Error(kDesktopCaptureApiInvalidOriginError));
    }

    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kAllowHttpScreenCapture) &&
        !network::IsUrlPotentiallyTrustworthy(origin)) {
      return RespondNow(Error(kDesktopCaptureApiTabUrlNotSecure));
    }
    target_name = base::UTF8ToUTF16(network::IsUrlPotentiallyTrustworthy(origin)
                                        ? net::GetHostAndOptionalPort(origin)
                                        : origin.spec());

    if (!params->target_tab->id ||
        *params->target_tab->id == api::tabs::TAB_ID_NONE) {
      return RespondNow(Error(kDesktopCaptureApiNoTabIdError));
    }

    content::WebContents* web_contents = nullptr;
    if (!ExtensionTabUtil::GetTabById(
            *(params->target_tab->id),
            Profile::FromBrowserContext(browser_context()), true,
            &web_contents)) {
      return RespondNow(Error(kDesktopCaptureApiInvalidTabIdError));
    }
    // The |target_render_frame_host| is the main frame of the tab that
    // was requested for capture.
    target_render_frame_host = web_contents->GetMainFrame();
  } else {
    origin = extension()->url();
    target_name = base::UTF8ToUTF16(GetExtensionTargetName());
    target_render_frame_host = render_frame_host();
  }
  DCHECK(target_render_frame_host);

  return Execute(params->sources, target_render_frame_host, origin,
                 target_name);
}

std::string DesktopCaptureChooseDesktopMediaFunction::GetExtensionTargetName()
    const {
  return GetCallerDisplayName();
}

DesktopCaptureCancelChooseDesktopMediaFunction::
    DesktopCaptureCancelChooseDesktopMediaFunction() {}

DesktopCaptureCancelChooseDesktopMediaFunction::
    ~DesktopCaptureCancelChooseDesktopMediaFunction() {}

}  // namespace extensions
