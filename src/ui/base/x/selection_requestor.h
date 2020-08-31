// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_REQUESTOR_H_
#define UI_BASE_X_SELECTION_REQUESTOR_H_

#include <stddef.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {
class XEventDispatcher;
class SelectionData;

// Requests and later receives data from the X11 server through the selection
// system.
//
// X11 uses a system called "selections" to implement clipboards and drag and
// drop. This class interprets messages from the stateful selection request
// API. SelectionRequestor should only deal with the X11 details; it does not
// implement per-component fast-paths.
class UI_BASE_EXPORT SelectionRequestor {
 public:
  SelectionRequestor(XDisplay* xdisplay,
                     XID xwindow,
                     XEventDispatcher* dispatcher);
  ~SelectionRequestor();

  // Does the work of requesting |target| from |selection|, spinning up the
  // nested run loop, and reading the resulting data back. The result is
  // stored in |out_data|.
  // |out_data_items| is the length of |out_data| in |out_type| items.
  bool PerformBlockingConvertSelection(
      XAtom selection,
      XAtom target,
      scoped_refptr<base::RefCountedMemory>* out_data,
      size_t* out_data_items,
      XAtom* out_type);

  // Requests |target| from |selection|, passing |parameter| as a parameter to
  // XConvertSelection().
  void PerformBlockingConvertSelectionWithParameter(
      XAtom selection,
      XAtom target,
      const std::vector<XAtom>& parameter);

  // Returns the first of |types| offered by the current owner of |selection|.
  // Returns an empty SelectionData object if none of |types| are available.
  SelectionData RequestAndWaitForTypes(XAtom selection,
                                       const std::vector<XAtom>& types);

  // It is our owner's responsibility to plumb X11 SelectionNotify events on
  // |xwindow_| to us.
  void OnSelectionNotify(const XEvent& event);

  // Returns true if SelectionOwner can process the XChangeProperty event,
  // |event|.
  bool CanDispatchPropertyEvent(const XEvent& event);

  void OnPropertyEvent(const XEvent& event);

 private:
  friend class SelectionRequestorTest;

  // A request that has been issued.
  struct Request {
    Request(XAtom selection, XAtom target, base::TimeTicks timeout);
    ~Request();

    // The target and selection requested in the XConvertSelection() request.
    // Used for error detection.
    XAtom selection;
    XAtom target;

    // Whether the result of the XConvertSelection() request is being sent
    // incrementally.
    bool data_sent_incrementally;

    // The result data for the XConvertSelection() request.
    std::vector<scoped_refptr<base::RefCountedMemory> > out_data;
    size_t out_data_items;
    XAtom out_type;

    // Whether the XConvertSelection() request was successful.
    bool success;

    // The time when the request should be aborted.
    base::TimeTicks timeout;

    // Called to terminate the nested run loop.
    base::OnceClosure quit_closure;

    // True if the request is complete.
    bool completed;
  };

  // Aborts requests which have timed out.
  void AbortStaleRequests();

  // Mark |request| as completed. If the current request is completed, converts
  // the selection for the next request.
  void CompleteRequest(size_t index, bool success);

  // Converts the selection for the request at |current_request_index_|.
  void ConvertSelectionForCurrentRequest();

  // Blocks till SelectionNotify is received for the target specified in
  // |request|.
  void BlockTillSelectionNotifyForRequest(Request* request);

  // Returns the request at |current_request_index_| or NULL if there isn't any.
  Request* GetCurrentRequest();

  // Our X11 state.
  XDisplay* x_display_;
  XID x_window_;

  // The property on |x_window_| set by the selection owner with the value of
  // the selection.
  XAtom x_property_;

  // Dispatcher which handles SelectionNotify and SelectionRequest for
  // |selection_name_|. PerformBlockingConvertSelection() calls the
  // dispatcher directly if PerformBlockingConvertSelection() is called after
  // the PlatformEventSource is destroyed.
  // Not owned.
  XEventDispatcher* dispatcher_;

  // In progress requests. Requests are added to the list at the start of
  // PerformBlockingConvertSelection() and are removed and destroyed right
  // before the method terminates.
  std::vector<Request*> requests_;

  // The index of the currently active request in |requests_|. The active
  // request is the request for which XConvertSelection() has been
  // called and for which we are waiting for a SelectionNotify response.
  size_t current_request_index_;

  // Used to abort requests if the selection owner takes too long to respond.
  base::RepeatingTimer abort_timer_;

  DISALLOW_COPY_AND_ASSIGN(SelectionRequestor);
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_REQUESTOR_H_
