// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/constrained_web_dialog_ui.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/property_bag.h"
#include "base/values.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::RenderViewHost;
using content::WebContents;
using content::WebUIMessageHandler;

static base::LazyInstance<
    base::PropertyAccessor<ui::ConstrainedWebDialogDelegate*> >
    g_constrained_web_dialog_ui_property_accessor = LAZY_INSTANCE_INITIALIZER;

namespace ui {

ConstrainedWebDialogUI::ConstrainedWebDialogUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
}

ConstrainedWebDialogUI::~ConstrainedWebDialogUI() {
}

void ConstrainedWebDialogUI::RenderViewCreated(
    RenderViewHost* render_view_host) {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;

  WebDialogDelegate* dialog_delegate = delegate->GetWebDialogDelegate();
  std::vector<WebUIMessageHandler*> handlers;
  dialog_delegate->GetWebUIMessageHandlers(&handlers);
  render_view_host->SetWebUIProperty("dialogArguments",
                                     dialog_delegate->GetDialogArgs());
  for (std::vector<WebUIMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    web_ui()->AddMessageHandler(*it);
  }

  // Add a "DialogClose" callback which matches WebDialogUI behavior.
  web_ui()->RegisterMessageCallback("DialogClose",
      base::Bind(&ConstrainedWebDialogUI::OnDialogCloseMessage,
                 base::Unretained(this)));

  dialog_delegate->OnDialogShown(web_ui(), render_view_host);
}

void ConstrainedWebDialogUI::OnDialogCloseMessage(const ListValue* args) {
  ConstrainedWebDialogDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;

  std::string json_retval;
  if (!args->empty() && !args->GetString(0, &json_retval))
    NOTREACHED() << "Could not read JSON argument";
  delegate->GetWebDialogDelegate()->OnDialogClosed(json_retval);
  delegate->OnDialogCloseFromWebUI();
}

ConstrainedWebDialogDelegate* ConstrainedWebDialogUI::GetConstrainedDelegate() {
  ConstrainedWebDialogDelegate** property = GetPropertyAccessor().GetProperty(
      web_ui()->GetWebContents()->GetPropertyBag());
  return property ? *property : NULL;
}

// static
base::PropertyAccessor<ConstrainedWebDialogDelegate*>&
    ConstrainedWebDialogUI::GetPropertyAccessor() {
  return g_constrained_web_dialog_ui_property_accessor.Get();
}

}  // namespace ui
