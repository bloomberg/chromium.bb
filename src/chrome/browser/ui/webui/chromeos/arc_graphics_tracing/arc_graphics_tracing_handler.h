// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
class RefCountedString;
}  // namespace base

namespace chromeos {

class ArcGraphicsTracingHandler : public content::WebUIMessageHandler {
 public:
  ArcGraphicsTracingHandler();
  ~ArcGraphicsTracingHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void StartTracing(double duration);
  void StopTracing();

  void OnTracingStarted(double duration);
  void OnTracingStopped(std::unique_ptr<const base::DictionaryValue> metadata,
                        base::RefCountedString* trace_data);

  // Called when graphics model is built and ready.
  void OnGraphicsModelReady(std::unique_ptr<base::Value> model);

  // Handlers for calls from JS.
  void HandleStartTracing(const base::ListValue* args);

  // To implement start/stop tracing.
  base::OneShotTimer tracing_timer_;

  base::WeakPtrFactory<ArcGraphicsTracingHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcGraphicsTracingHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_
