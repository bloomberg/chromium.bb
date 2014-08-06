// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_WEBUI_VK_WEBUI_CONTROLLER_H_
#define UI_KEYBOARD_WEBUI_VK_WEBUI_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/core.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/keyboard/webui/keyboard.mojom.h"

namespace content {
class WebUIDataSource;
}

namespace keyboard {

class VKMojoHandler;

class VKWebUIController : public content::WebUIController {
 public:
  explicit VKWebUIController(content::WebUI* web_ui);
  virtual ~VKWebUIController();

 private:
  void CreateAndStoreUIHandler(
      mojo::InterfaceRequest<KeyboardUIHandlerMojo> request);

  // content::WebUIController:
  virtual void RenderViewCreated(content::RenderViewHost* host) OVERRIDE;

  scoped_ptr<VKMojoHandler> ui_handler_;
  base::WeakPtrFactory<VKWebUIController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VKWebUIController);
};

class KEYBOARD_EXPORT VKWebUIControllerFactory
    : public content::WebUIControllerFactory {
 public:
  // WebUIControllerFactory:
  virtual content::WebUI::TypeID GetWebUIType(
      content::BrowserContext* browser_context,
      const GURL& url) const OVERRIDE;
  virtual bool UseWebUIForURL(content::BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE;
  virtual bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE;
  virtual content::WebUIController* CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) const OVERRIDE;

  static VKWebUIControllerFactory* GetInstance();

 protected:
  VKWebUIControllerFactory();
  virtual ~VKWebUIControllerFactory();

 private:
  friend struct DefaultSingletonTraits<VKWebUIControllerFactory>;

  DISALLOW_COPY_AND_ASSIGN(VKWebUIControllerFactory);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_WEBUI_VK_WEBUI_CONTROLLER_H_
