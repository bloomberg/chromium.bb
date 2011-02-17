// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_chooser_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

class MyInstance : public pp::Instance {
 public:
  MyInstance(PP_Instance instance)
      : pp::Instance(instance) {
    callback_factory_.Initialize(this);
  }

  virtual bool HandleInputEvent(const PP_InputEvent& event) {
    switch (event.type) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
        const PP_InputEvent_Mouse& mouse_event = event.u.mouse;
        if (mouse_event.button == PP_INPUTEVENT_MOUSEBUTTON_LEFT)
          ShowFileChooser(false);
        else if (mouse_event.button == PP_INPUTEVENT_MOUSEBUTTON_RIGHT)
          ShowFileChooser(true);
        else
          return false;

        return true;
      }
      default:
        return false;
    }
  }

 private:
  void ShowFileChooser(bool multi_select) {
    RecreateConsole();

    PP_FileChooserOptions_Dev options;
    options.mode = (multi_select ? PP_FILECHOOSERMODE_OPENMULTIPLE :
                    PP_FILECHOOSERMODE_OPEN);
    options.accept_mime_types = (multi_select ? "" : "plain/text");

    // Deleted in ShowSelectedFileNames().
    pp::FileChooser_Dev* file_chooser = new pp::FileChooser_Dev(
        *this, options);
    file_chooser->Show(callback_factory_.NewCallback(
        &MyInstance::ShowSelectedFileNames, file_chooser));
  }

  void ShowSelectedFileNames(int32_t, pp::FileChooser_Dev* file_chooser) {
    if (!file_chooser)
      return;

    pp::FileRef_Dev file_ref = file_chooser->GetNextChosenFile();
    while (!file_ref.is_null()) {
      Log(file_ref.GetName());
      file_ref = file_chooser->GetNextChosenFile();
    }

    delete file_chooser;
  }

  void RecreateConsole() {
    pp::Var doc = GetWindowObject().GetProperty("document");
    pp::Var body = doc.GetProperty("body");
    if (!console_.is_undefined())
      body.Call("removeChild", console_);

    console_ = doc.Call("createElement", "pre");
    console_.SetProperty("id", "console");
    console_.GetProperty("style").SetProperty("backgroundColor", "lightgray");
    body.Call("appendChild", console_);
  }

  void Log(const pp::Var& var) {
    pp::Var doc = GetWindowObject().GetProperty("document");
    console_.Call("appendChild", doc.Call("createTextNode", var));
    console_.Call("appendChild", doc.Call("createTextNode", "\n"));
  }

  pp::CompletionCallbackFactory<MyInstance> callback_factory_;
  pp::Var console_;
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
