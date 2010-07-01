// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_plugin_instance.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_instance.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_var.h"

using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebPluginContainer;

namespace pepper {

namespace {

void RectToPPRect(const gfx::Rect& input, PP_Rect* output) {
  *output = PP_MakeRectFromXYWH(input.x(), input.y(),
                                input.width(), input.height());
}

PP_Event_Type ConvertEventTypes(WebInputEvent::Type wetype) {
  switch (wetype) {
    case WebInputEvent::MouseDown:
      return PP_Event_Type_MouseDown;
    case WebInputEvent::MouseUp:
      return PP_Event_Type_MouseUp;
    case WebInputEvent::MouseMove:
      return PP_Event_Type_MouseMove;
    case WebInputEvent::MouseEnter:
      return PP_Event_Type_MouseEnter;
    case WebInputEvent::MouseLeave:
      return PP_Event_Type_MouseLeave;
    case WebInputEvent::MouseWheel:
      return PP_Event_Type_MouseWheel;
    case WebInputEvent::RawKeyDown:
      return PP_Event_Type_RawKeyDown;
    case WebInputEvent::KeyDown:
      return PP_Event_Type_KeyDown;
    case WebInputEvent::KeyUp:
      return PP_Event_Type_KeyUp;
    case WebInputEvent::Char:
      return PP_Event_Type_Char;
    case WebInputEvent::Undefined:
    default:
      return PP_Event_Type_Undefined;
  }
}

void BuildKeyEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebKit::WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKit::WebKeyboardEvent*>(event);
  pp_event->u.key.modifier = key_event->modifiers;
  pp_event->u.key.normalizedKeyCode = key_event->windowsKeyCode;
}

void BuildCharEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebKit::WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKit::WebKeyboardEvent*>(event);
  pp_event->u.character.modifier = key_event->modifiers;
  // For consistency, check that the sizes of the texts agree.
  DCHECK(sizeof(pp_event->u.character.text) == sizeof(key_event->text));
  DCHECK(sizeof(pp_event->u.character.unmodifiedText) ==
         sizeof(key_event->unmodifiedText));
  for (size_t i = 0; i < WebKit::WebKeyboardEvent::textLengthCap; ++i) {
    pp_event->u.character.text[i] = key_event->text[i];
    pp_event->u.character.unmodifiedText[i] = key_event->unmodifiedText[i];
  }
}

void BuildMouseEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebKit::WebMouseEvent* mouse_event =
      reinterpret_cast<const WebKit::WebMouseEvent*>(event);
  pp_event->u.mouse.modifier = mouse_event->modifiers;
  pp_event->u.mouse.button = mouse_event->button;
  pp_event->u.mouse.x = mouse_event->x;
  pp_event->u.mouse.y = mouse_event->y;
  pp_event->u.mouse.clickCount = mouse_event->clickCount;
}

void BuildMouseWheelEvent(const WebInputEvent* event, PP_Event* pp_event) {
  const WebKit::WebMouseWheelEvent* mouse_wheel_event =
      reinterpret_cast<const WebKit::WebMouseWheelEvent*>(event);
  pp_event->u.wheel.modifier = mouse_wheel_event->modifiers;
  pp_event->u.wheel.deltaX = mouse_wheel_event->deltaX;
  pp_event->u.wheel.deltaY = mouse_wheel_event->deltaY;
  pp_event->u.wheel.wheelTicksX = mouse_wheel_event->wheelTicksX;
  pp_event->u.wheel.wheelTicksY = mouse_wheel_event->wheelTicksY;
  pp_event->u.wheel.scrollByPage = mouse_wheel_event->scrollByPage;
}

PP_Var GetWindowObject(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->GetWindowObject();
}

PP_Var GetOwnerElementObject(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->GetOwnerElementObject();
}

bool BindGraphicsDeviceContext(PP_Instance instance_id, PP_Resource device_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return false;
  return instance->BindGraphicsDeviceContext(device_id);
}

bool IsFullFrame(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return false;
  return instance->full_frame();
}

const PPB_Instance ppb_instance = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &BindGraphicsDeviceContext,
  &IsFullFrame,
};

}  // namespace

PluginInstance::PluginInstance(PluginDelegate* delegate,
                               PluginModule* module,
                               const PPP_Instance* instance_interface)
    : delegate_(delegate),
      module_(module),
      instance_interface_(instance_interface),
      container_(NULL),
      full_frame_(false) {
  DCHECK(delegate);
  module_->InstanceCreated(this);
  delegate_->InstanceCreated(this);
}

PluginInstance::~PluginInstance() {
  delegate_->InstanceDeleted(this);
  module_->InstanceDeleted(this);
}

// static
const PPB_Instance* PluginInstance::GetInterface() {
  return &ppb_instance;
}

// static
PluginInstance* PluginInstance::FromPPInstance(PP_Instance instance) {
  return reinterpret_cast<PluginInstance*>(instance);
}

PP_Instance PluginInstance::GetPPInstance() {
  return reinterpret_cast<intptr_t>(this);
}

void PluginInstance::Paint(WebKit::WebCanvas* canvas,
                           const gfx::Rect& plugin_rect,
                           const gfx::Rect& paint_rect) {
  if (device_context_2d_)
    device_context_2d_->Paint(canvas, plugin_rect, paint_rect);
}

void PluginInstance::InvalidateRect(const gfx::Rect& rect) {
  if (!container_ || position_.IsEmpty())
    return;  // Nothing to do.
  if (rect.IsEmpty())
    container_->invalidate();
  else
    container_->invalidateRect(rect);
}

PP_Var PluginInstance::GetWindowObject() {
  if (!container_)
    return PP_MakeVoid();

  WebFrame* frame = container_->element().document().frame();
  if (!frame)
    return PP_MakeVoid();

  return NPObjectToPPVar(frame->windowObject());
}

PP_Var PluginInstance::GetOwnerElementObject() {
  if (!container_)
    return PP_MakeVoid();

  return NPObjectToPPVar(container_->scriptableObjectForElement());
}

bool PluginInstance::BindGraphicsDeviceContext(PP_Resource device_id) {
  if (!device_id) {
    // Special-case clearing the current device.
    if (device_context_2d_) {
      device_context_2d_->BindToInstance(NULL);
      device_context_2d_ = NULL;
      InvalidateRect(gfx::Rect());
    }
    return true;
  }

  scoped_refptr<DeviceContext2D> device_2d =
      Resource::GetAs<DeviceContext2D>(device_id);

  if (device_2d) {
    if (!device_2d->BindToInstance(this))
      return false;  // Can't bind to more than one instance.
    device_context_2d_ = device_2d;
    // BindToInstance will have invalidated the plugin if necessary.
  }

  return true;
}

void PluginInstance::Delete() {
  instance_interface_->Delete(GetPPInstance());

  container_ = NULL;
}

bool PluginInstance::Initialize(WebPluginContainer* container,
                                const std::vector<std::string>& arg_names,
                                const std::vector<std::string>& arg_values,
                                bool full_frame) {
  container_ = container;
  full_frame_ = full_frame;

  if (!instance_interface_->New(GetPPInstance()))
    return false;

  size_t argc = 0;
  scoped_array<const char*> argn(new const char*[arg_names.size()]);
  scoped_array<const char*> argv(new const char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = arg_names[i].c_str();
    argv[argc] = arg_values[i].c_str();
    argc++;
  }

  return instance_interface_->Initialize(GetPPInstance(),
                                         argc, argn.get(), argv.get());
}

bool PluginInstance::HandleDocumentLoad(URLLoader* loader) {
  return instance_interface_->HandleDocumentLoad(GetPPInstance(),
                                                 loader->GetResource());
}

bool PluginInstance::HandleInputEvent(const WebKit::WebInputEvent& event,
                                      WebKit::WebCursorInfo* cursor_info) {
  PP_Event pp_event;

  pp_event.type = ConvertEventTypes(event.type);
  pp_event.size = sizeof(pp_event);
  pp_event.time_stamp_seconds = event.timeStampSeconds;
  switch (pp_event.type) {
    case PP_Event_Type_Undefined:
      return false;
    case PP_Event_Type_MouseDown:
    case PP_Event_Type_MouseUp:
    case PP_Event_Type_MouseMove:
    case PP_Event_Type_MouseEnter:
    case PP_Event_Type_MouseLeave:
      BuildMouseEvent(&event, &pp_event);
      break;
    case PP_Event_Type_MouseWheel:
      BuildMouseWheelEvent(&event, &pp_event);
      break;
    case PP_Event_Type_RawKeyDown:
    case PP_Event_Type_KeyDown:
    case PP_Event_Type_KeyUp:
      BuildKeyEvent(&event, &pp_event);
      break;
    case PP_Event_Type_Char:
      BuildCharEvent(&event, &pp_event);
      break;
  }
  return instance_interface_->HandleEvent(GetPPInstance(), &pp_event);
}

PP_Var PluginInstance::GetInstanceObject() {
  return instance_interface_->GetInstanceObject(GetPPInstance());
}

void PluginInstance::ViewChanged(const gfx::Rect& position,
                                 const gfx::Rect& clip) {
  position_ = position;
  if (clip.IsEmpty()) {
    // WebKit can give weird (x,y) positions for empty clip rects (since the
    // position technically doesn't matter). But we want to make these
    // consistent since this is given to the plugin, so force everything to 0
    // in the "everything is clipped" case.
    clip_ = gfx::Rect();
  } else {
    clip_ = clip;
  }

  PP_Rect pp_position, pp_clip;
  RectToPPRect(position_, &pp_position);
  RectToPPRect(clip_, &pp_clip);
  instance_interface_->ViewChanged(GetPPInstance(), &pp_position, &pp_clip);
}

void PluginInstance::ViewInitiatedPaint() {
  if (device_context_2d_)
    device_context_2d_->ViewInitiatedPaint();
}

void PluginInstance::ViewFlushedPaint() {
  if (device_context_2d_)
    device_context_2d_->ViewFlushedPaint();
}

}  // namespace pepper
