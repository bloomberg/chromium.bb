// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include "base/message_loop/message_pump_x11.h"
#include "base/run_loop.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";

const char* kAtomsToCache[] = {
  kChromeSelection,
  NULL
};

}  // namespace

SelectionRequestor::SelectionRequestor(Display* x_display,
                                       Window x_window,
                                       Atom selection_name)
    : x_display_(x_display),
      x_window_(x_window),
      in_nested_loop_(false),
      selection_name_(selection_name),
      current_target_(None),
      returned_property_(None),
      atom_cache_(x_display_, kAtomsToCache) {
}

SelectionRequestor::~SelectionRequestor() {}

bool SelectionRequestor::PerformBlockingConvertSelection(
    Atom target,
    scoped_refptr<base::RefCountedMemory>* out_data,
    size_t* out_data_bytes,
    size_t* out_data_items,
    Atom* out_type) {
  // The name of the property we're asking to be set on |x_window_|.
  Atom property_to_set = atom_cache_.GetAtom(kChromeSelection);

  XConvertSelection(x_display_,
                    selection_name_,
                    target,
                    property_to_set,
                    x_window_,
                    CurrentTime);

  // Now that we've thrown our message off to the X11 server, we block waiting
  // for a response.
  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop(base::MessagePumpX11::Current());

  current_target_ = target;
  in_nested_loop_ = true;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  in_nested_loop_ = false;
  current_target_ = None;

  if (returned_property_ != property_to_set)
    return false;

  return ui::GetRawBytesOfProperty(x_window_, returned_property_,
                                   out_data, out_data_bytes, out_data_items,
                                   out_type);
}

SelectionData SelectionRequestor::RequestAndWaitForTypes(
    const std::vector< ::Atom>& types) {
  for (std::vector< ::Atom>::const_iterator it = types.begin();
       it != types.end(); ++it) {
    scoped_refptr<base::RefCountedMemory> data;
    size_t data_bytes = 0;
    ::Atom type = None;
    if (PerformBlockingConvertSelection(*it,
                                        &data,
                                        &data_bytes,
                                        NULL,
                                        &type) &&
        type == *it) {
      return SelectionData(type, data);
    }
  }

  return SelectionData();
}

void SelectionRequestor::OnSelectionNotify(const XSelectionEvent& event) {
  if (!in_nested_loop_) {
    // This shouldn't happen; we're not waiting on the X server for data, but
    // any client can send any message...
    return;
  }

  if (selection_name_ == event.selection &&
      current_target_ == event.target) {
    returned_property_ = event.property;
  } else {
    // I am assuming that if some other client sent us a message after we've
    // asked for data, but it's malformed, we should just treat as if they sent
    // us an error message.
    returned_property_ = None;
  }

  quit_closure_.Run();
}

}  // namespace ui
