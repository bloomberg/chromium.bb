// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing_handler.h"

#include <map>
#include <string>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper_chromeos.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {

namespace {

constexpr char kKeyIcon[] = "icon";
constexpr char kKeyTitle[] = "title";
constexpr char kKeyTasks[] = "tasks";
constexpr char kTaskIdPrefix[] = "org.chromium.arc.";

// Scans for all ARC windows and and extracts the title and optionally icon.
void CreateTaskMap(aura::Window* window, base::DictionaryValue* tasks) {
  if (!window->IsVisible())
    return;

  // ARC window is top level window, all its parents have type not set.
  if (window->type() == aura::client::WINDOW_TYPE_UNKNOWN) {
    for (aura::Window* child_window : window->children())
      CreateTaskMap(child_window, tasks);
    return;
  }

  // Verifies if this is top-level ARC window.
  const std::string* arc_app_id = exo::GetShellApplicationId(window);
  if (!arc_app_id)
    return;

  // Root surface may not have task id.
  const size_t prefix_pos = arc_app_id->find(kTaskIdPrefix);
  if (prefix_pos)
    return;

  base::DictionaryValue task_information;
  task_information.SetKey(kKeyTitle, base::Value(window->GetTitle()));

  const gfx::ImageSkia* app_icon =
      window->GetProperty(aura::client::kAppIconKey);
  if (app_icon) {
    std::vector<unsigned char> png_data;
    if (gfx::PNGCodec::EncodeBGRASkBitmap(
            app_icon->GetRepresentation(1.0f).GetBitmap(),
            false /* discard_transparency */, &png_data)) {
      const std::string png_data_as_string(
          reinterpret_cast<const char*>(&png_data[0]), png_data.size());
      std::string icon_content;
      base::Base64Encode(png_data_as_string, &icon_content);
      task_information.SetKey(kKeyIcon, base::Value(icon_content));
    }
  }

  tasks->SetKey(arc_app_id->c_str() + strlen(kTaskIdPrefix),
                std::move(task_information));
}

std::unique_ptr<base::Value> BuildGraphicsModel(const std::string& data) {
  arc::ArcTracingModel common_model;
  if (!common_model.Build(data)) {
    LOG(ERROR) << "Failed to build common model";
    return nullptr;
  }

  arc::ArcTracingGraphicsModel graphics_model;
  if (!graphics_model.Build(common_model)) {
    LOG(ERROR) << "Failed to build graphic buffers model";
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> model = graphics_model.Serialize();
  base::DictionaryValue tasks;
  // Scan for ARC++ windows
  // TODO(https://crbug.com/887156): Fix the mash.
  if (!features::IsMultiProcessMash()) {
    CreateTaskMap(
        exo::WMHelperChromeOS::GetInstance()->GetPrimaryDisplayContainer(
            ash::kShellWindowId_DefaultContainer),
        &tasks);
  }
  model->SetKey(kKeyTasks, std::move(tasks));

  return model;
}

}  // namespace

ArcGraphicsTracingHandler::ArcGraphicsTracingHandler()
    : weak_ptr_factory_(this) {}

ArcGraphicsTracingHandler::~ArcGraphicsTracingHandler() = default;

void ArcGraphicsTracingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "startTracing",
      base::BindRepeating(&ArcGraphicsTracingHandler::HandleStartTracing,
                          base::Unretained(this)));
}

void ArcGraphicsTracingHandler::StartTracing(double duration) {
  base::trace_event::TraceConfig config(
      "-*,exo,viz,toplevel,gpu,cc,blink,disabled-by-default-android "
      "gfx,disabled-by-default-android hal",
      base::trace_event::RECORD_CONTINUOUSLY);
  config.EnableSystrace();
  content::TracingController::GetInstance()->StartTracing(
      config, base::BindOnce(&ArcGraphicsTracingHandler::OnTracingStarted,
                             weak_ptr_factory_.GetWeakPtr(), duration));
}

void ArcGraphicsTracingHandler::StopTracing() {
  content::TracingController::GetInstance()->StopTracing(
      content::TracingController::CreateStringEndpoint(
          base::BindRepeating(&ArcGraphicsTracingHandler::OnTracingStopped,
                              weak_ptr_factory_.GetWeakPtr())));
}

void ArcGraphicsTracingHandler::OnTracingStarted(double duration) {
  tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSecondsD(duration),
                       base::BindOnce(&ArcGraphicsTracingHandler::StopTracing,
                                      base::Unretained(this)));
}

void ArcGraphicsTracingHandler::OnTracingStopped(
    std::unique_ptr<const base::DictionaryValue> metadata,
    base::RefCountedString* trace_data) {
  std::string string_data;
  string_data.swap(trace_data->data());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildGraphicsModel, std::move(string_data)),
      base::BindOnce(&ArcGraphicsTracingHandler::OnGraphicsModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcGraphicsTracingHandler::OnGraphicsModelReady(
    std::unique_ptr<base::Value> model) {
  if (!model)
    return;
  AllowJavascript();
  CallJavascriptFunction("cr.ArcGraphicsTracing.setModel", *model);
}

void ArcGraphicsTracingHandler::HandleStartTracing(
    const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  if ((!args->GetList()[0].is_double() && !args->GetList()[0].is_int()) ||
      (!args->GetList()[1].is_double() && !args->GetList()[1].is_int())) {
    LOG(ERROR) << "Invalid input";
    return;
  }
  const double delay = args->GetList()[0].GetDouble();
  const double duration = args->GetList()[1].GetDouble();
  tracing_timer_.Start(FROM_HERE, base::TimeDelta::FromSecondsD(delay),
                       base::BindOnce(&ArcGraphicsTracingHandler::StartTracing,
                                      base::Unretained(this), duration));
}

}  // namespace chromeos
