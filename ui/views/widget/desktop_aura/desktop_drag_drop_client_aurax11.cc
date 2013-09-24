// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"

#include <X11/Xatom.h>

#include "base/event_types.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

using aura::client::DragDropDelegate;
using ui::OSExchangeData;

namespace {

const int kMinXdndVersion = 5;

const int kWillAcceptDrop = 1;
const int kWantFurtherPosEvents = 2;

const char kXdndActionCopy[] = "XdndActionCopy";
const char kXdndActionMove[] = "XdndActionMove";
const char kXdndActionLink[] = "XdndActionLink";

const char kChromiumDragReciever[] = "_CHROMIUM_DRAG_RECEIVER";
const char kXdndSelection[] = "XdndSelection";

const char* kAtomsToCache[] = {
  kChromiumDragReciever,
  "XdndActionAsk",
  kXdndActionCopy,
  kXdndActionLink,
  "XdndActionList",
  kXdndActionMove,
  "XdndActionPrivate",
  "XdndAware",
  "XdndDrop",
  "XdndEnter",
  "XdndFinished",
  "XdndLeave",
  "XdndPosition",
  "XdndProxy",  // Proxy windows?
  kXdndSelection,
  "XdndStatus",
  "XdndTypeList",
  NULL
};

// Helper class to FindWindowFor which looks for a drag target under the
// cursor.
class DragTargetWindowFinder : public ui::EnumerateWindowsDelegate {
 public:
  DragTargetWindowFinder(XID ignored_icon_window,
                         gfx::Point screen_loc)
      : ignored_icon_window_(ignored_icon_window),
        output_window_(None),
        screen_loc_(screen_loc) {
    ui::EnumerateTopLevelWindows(this);
  }

  virtual ~DragTargetWindowFinder() {}

  XID window() const { return output_window_; }

 protected:
  virtual bool ShouldStopIterating(XID window) OVERRIDE {
    if (window == ignored_icon_window_)
      return false;

    if (!ui::IsWindowVisible(window))
      return false;

    if (!ui::WindowContainsPoint(window, screen_loc_))
      return false;

    if (ui::PropertyExists(window, "WM_STATE")) {
      output_window_ = window;
      return true;
    }

    return false;
  }

 private:
  XID ignored_icon_window_;
  XID output_window_;
  gfx::Point screen_loc_;

  DISALLOW_COPY_AND_ASSIGN(DragTargetWindowFinder);
};

// Returns the topmost X11 window at |screen_point| if it is advertising that
// is supports the Xdnd protocol. Will return the window under the pointer as
// |mouse_window|. If there's a Xdnd aware window, it will be returned in
// |dest_window|.
void FindWindowFor(const gfx::Point& screen_point,
                   ::Window* mouse_window, ::Window* dest_window) {
  DragTargetWindowFinder finder(None, screen_point);
  *mouse_window = finder.window();
  *dest_window = None;

  if (finder.window() == None)
    return;

  // Figure out which window we should test as XdndAware. If mouse_window has
  // XdndProxy, it will set that proxy on target, and if not, |target|'s
  // original value will remain.
  XID target = *mouse_window;
  ui::GetXIDProperty(*mouse_window, "XdndProxy", &target);

  int version;
  if (ui::GetIntProperty(target, "XdndAware", &version) &&
      version >= kMinXdndVersion) {
    *dest_window = target;
  }
}

}  // namespace

namespace views {

std::map< ::Window, DesktopDragDropClientAuraX11*>
    DesktopDragDropClientAuraX11::g_live_client_map;

class DesktopDragDropClientAuraX11::X11DragContext :
    public base::MessageLoop::Dispatcher {
 public:
  X11DragContext(ui::X11AtomCache* atom_cache,
                 ::Window local_window,
                 const XClientMessageEvent& event);
  virtual ~X11DragContext();

  // When we receive an XdndPosition message, we need to have all the data
  // copied from the other window before we process the XdndPosition
  // message. If we have that data already, dispatch immediately. Otherwise,
  // delay dispatching until we do.
  void OnStartXdndPositionMessage(DesktopDragDropClientAuraX11* client,
                                  ::Window source_window,
                                  const gfx::Point& screen_point);

  // Called to request the next target from the source window. This is only
  // done on the first XdndPosition; after that, we cache the data offered by
  // the source window.
  void RequestNextTarget();

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  // Clones the fetched targets.
  const ui::SelectionFormatMap& fetched_targets() { return fetched_targets_; }

  // Reads the "XdndActionList" property from |source_window| and copies it
  // into |actions|.
  void ReadActions();

  // Creates a ui::DragDropTypes::DragOperation representation of the current
  // action list.
  int GetDragOperation() const;

 private:
  // Overridden from MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // The atom cache owned by our parent.
  ui::X11AtomCache* atom_cache_;

  // The XID of our chrome local aura window handling our events.
  ::Window local_window_;

  // The XID of the window that's initiated the drag.
  unsigned long source_window_;

  // The client we inform once we're done with requesting data.
  DesktopDragDropClientAuraX11* drag_drop_client_;

  // Whether we're blocking the handling of an XdndPosition message by waiting
  // for |unfetched_targets_| to be fetched.
  bool waiting_to_handle_position_;

  // Where the cursor is on screen.
  gfx::Point screen_point_;

  // A SelectionFormatMap of data that we have in our process.
  ui::SelectionFormatMap fetched_targets_;

  // The names of various data types offered by the other window that we
  // haven't fetched and put in |fetched_targets_| yet.
  std::vector<Atom> unfetched_targets_;

  // Possible actions.
  std::vector<Atom> actions_;

  DISALLOW_COPY_AND_ASSIGN(X11DragContext);
};

DesktopDragDropClientAuraX11::X11DragContext::X11DragContext(
    ui::X11AtomCache* atom_cache,
    ::Window local_window,
    const XClientMessageEvent& event)
    : atom_cache_(atom_cache),
      local_window_(local_window),
      source_window_(event.data.l[0]),
      drag_drop_client_(NULL),
      waiting_to_handle_position_(false) {
  bool get_types = ((event.data.l[1] & 1) != 0);

  if (get_types) {
    if (!ui::GetAtomArrayProperty(source_window_,
                                  "XdndTypeList",
                                  &unfetched_targets_)) {
      return;
    }
  } else {
    // data.l[2,3,4] contain the first three types. Unused slots can be None.
    for (int i = 0; i < 3; ++i) {
      if (event.data.l[2+i] != None) {
        unfetched_targets_.push_back(event.data.l[2+i]);
      }
    }
  }

  DesktopDragDropClientAuraX11* client =
      DesktopDragDropClientAuraX11::GetForWindow(source_window_);
  if (!client) {
    // The window doesn't have a DesktopDragDropClientAuraX11, that means it's
    // created by some other process. Listen for messages on it.
    base::MessagePumpX11::Current()->AddDispatcherForWindow(
        this, source_window_);
    XSelectInput(gfx::GetXDisplay(), source_window_, PropertyChangeMask);

    // We must perform a full sync here because we could be racing
    // |source_window_|.
    XSync(gfx::GetXDisplay(), False);
  } else {
    // This drag originates from an aura window within our process. This means
    // that we can shortcut the X11 server and ask the owning SelectionOwner
    // for the data it's offering.
    fetched_targets_ = client->GetFormatMap();
    unfetched_targets_.clear();
  }

  ReadActions();
}

DesktopDragDropClientAuraX11::X11DragContext::~X11DragContext() {
  DesktopDragDropClientAuraX11* client =
      DesktopDragDropClientAuraX11::GetForWindow(source_window_);
  if (!client) {
    // Unsubscribe from message events.
    base::MessagePumpX11::Current()->RemoveDispatcherForWindow(
        source_window_);
  }
}

void DesktopDragDropClientAuraX11::X11DragContext::OnStartXdndPositionMessage(
    DesktopDragDropClientAuraX11* client,
    ::Window source_window,
    const gfx::Point& screen_point) {
  DCHECK_EQ(source_window_, source_window);

  if (!unfetched_targets_.empty()) {
    // We have unfetched targets. That means we need to pause the handling of
    // the position message and ask the other window for its data.
    screen_point_ = screen_point;
    drag_drop_client_ = client;
    waiting_to_handle_position_ = true;

    fetched_targets_ = ui::SelectionFormatMap();
    RequestNextTarget();
  } else {
    client->CompleteXdndPosition(source_window, screen_point);
  }
}

void DesktopDragDropClientAuraX11::X11DragContext::RequestNextTarget() {
  ::Atom target = unfetched_targets_.back();
  unfetched_targets_.pop_back();

  XConvertSelection(gfx::GetXDisplay(),
                    atom_cache_->GetAtom(kXdndSelection),
                    target,
                    atom_cache_->GetAtom(kChromiumDragReciever),
                    local_window_,
                    CurrentTime);
}

void DesktopDragDropClientAuraX11::X11DragContext::OnSelectionNotify(
    const XSelectionEvent& event) {
  DCHECK(waiting_to_handle_position_);
  DCHECK(drag_drop_client_);
  DCHECK_EQ(event.property, atom_cache_->GetAtom(kChromiumDragReciever));

  scoped_refptr<base::RefCountedMemory> data;
  ::Atom type = None;
  if (ui::GetRawBytesOfProperty(local_window_, event.property,
                                &data, NULL, NULL, &type)) {
    fetched_targets_.Insert(event.target, data);
  }

  if (!unfetched_targets_.empty()) {
    RequestNextTarget();
  } else {
    waiting_to_handle_position_ = false;
    drag_drop_client_->CompleteXdndPosition(source_window_, screen_point_);
    drag_drop_client_ = NULL;
  }
}

void DesktopDragDropClientAuraX11::X11DragContext::ReadActions() {
  DesktopDragDropClientAuraX11* client =
      DesktopDragDropClientAuraX11::GetForWindow(source_window_);
  if (!client) {
    std::vector<Atom> atom_array;
    if (!ui::GetAtomArrayProperty(source_window_,
                                  "XdndActionList",
                                  &atom_array)) {
      actions_.clear();
    } else {
      actions_.swap(atom_array);
    }
  } else {
    // We have a property notify set up for other windows in case they change
    // their action list. Thankfully, the views interface is static and you
    // can't change the action list after you enter StartDragAndDrop().
    actions_ = client->GetOfferedDragOperations();
  }
}

int DesktopDragDropClientAuraX11::X11DragContext::GetDragOperation() const {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  for (std::vector<Atom>::const_iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    if (*it == atom_cache_->GetAtom(kXdndActionCopy))
      drag_operation |= ui::DragDropTypes::DRAG_COPY;
    else if (*it == atom_cache_->GetAtom(kXdndActionMove))
      drag_operation |= ui::DragDropTypes::DRAG_MOVE;
    else if (*it == atom_cache_->GetAtom(kXdndActionLink))
      drag_operation |= ui::DragDropTypes::DRAG_LINK;
  }

  return drag_operation;
}

bool DesktopDragDropClientAuraX11::X11DragContext::Dispatch(
    const base::NativeEvent& event) {
  if (event->type == PropertyNotify &&
      event->xproperty.atom == atom_cache_->GetAtom("XdndActionList")) {
    ReadActions();
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

DesktopDragDropClientAuraX11::DesktopDragDropClientAuraX11(
    aura::RootWindow* root_window,
    views::DesktopNativeCursorManager* cursor_manager,
    Display* xdisplay,
    ::Window xwindow)
    : move_loop_(this),
      root_window_(root_window),
      xdisplay_(xdisplay),
      xwindow_(xwindow),
      atom_cache_(xdisplay_, kAtomsToCache),
      target_window_(NULL),
      source_provider_(NULL),
      source_current_window_(None),
      drag_drop_in_progress_(false),
      drag_operation_(0),
      resulting_operation_(0),
      grab_cursor_(cursor_manager->GetInitializedCursor(ui::kCursorGrabbing)),
      copy_grab_cursor_(cursor_manager->GetInitializedCursor(ui::kCursorCopy)),
      move_grab_cursor_(cursor_manager->GetInitializedCursor(ui::kCursorMove)) {
  DCHECK(g_live_client_map.find(xwindow) == g_live_client_map.end());
  g_live_client_map.insert(std::make_pair(xwindow, this));

  // Mark that we are aware of drag and drop concepts.
  unsigned long xdnd_version = kMinXdndVersion;
  XChangeProperty(xdisplay_, xwindow_, atom_cache_.GetAtom("XdndAware"),
                  XA_ATOM, 32, PropModeReplace,
                  reinterpret_cast<unsigned char*>(&xdnd_version), 1);
}

DesktopDragDropClientAuraX11::~DesktopDragDropClientAuraX11() {
  g_live_client_map.erase(xwindow_);
}

// static
DesktopDragDropClientAuraX11* DesktopDragDropClientAuraX11::GetForWindow(
    ::Window window) {
  std::map< ::Window, DesktopDragDropClientAuraX11*>::const_iterator it =
      g_live_client_map.find(window);
  if (it == g_live_client_map.end())
    return NULL;
  return it->second;
}

void DesktopDragDropClientAuraX11::OnXdndEnter(
    const XClientMessageEvent& event) {
  DVLOG(1) << "XdndEnter";

  int version = (event.data.l[1] & 0xff000000) >> 24;
  if (version < 3) {
    LOG(ERROR) << "Received old XdndEnter message.";
    return;
  }

  // Make sure that we've run ~X11DragContext() before creating another one.
  target_current_context_.reset();
  target_current_context_.reset(
      new X11DragContext(&atom_cache_, xwindow_, event));

  // In the Windows implementation, we immediately call DesktopDropTargetWin::
  // Translate(). Here, we wait until we receive an XdndPosition message
  // because the enter message doesn't convey any positioning
  // information.
}

void DesktopDragDropClientAuraX11::OnXdndLeave(
    const XClientMessageEvent& event) {
  DVLOG(1) << "XdndLeave";
  NotifyDragLeave();
  target_current_context_.reset();
}

void DesktopDragDropClientAuraX11::OnXdndPosition(
    const XClientMessageEvent& event) {
  DVLOG(1) << "XdndPosition";

  unsigned long source_window = event.data.l[0];
  int x_root_window = event.data.l[2] >> 16;
  int y_root_window = event.data.l[2] & 0xffff;

  if (!target_current_context_.get()) {
    NOTREACHED();
    return;
  }

  // If we already have all the data from this drag, we complete it
  // immediately.
  target_current_context_->OnStartXdndPositionMessage(
      this, source_window, gfx::Point(x_root_window, y_root_window));
}

void DesktopDragDropClientAuraX11::OnXdndStatus(
    const XClientMessageEvent& event) {
  DVLOG(1) << "XdndStatus";

  unsigned long source_window = event.data.l[0];
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  if (event.data.l[1] & 1) {
    ::Atom atom_operation = event.data.l[4];
    negotiated_operation_[source_window] = atom_operation;
    drag_operation = AtomToDragOperation(atom_operation);
  }

  switch (drag_operation) {
    case ui::DragDropTypes::DRAG_COPY:
      move_loop_.UpdateCursor(copy_grab_cursor_);
      break;
    case ui::DragDropTypes::DRAG_MOVE:
      move_loop_.UpdateCursor(move_grab_cursor_);
      break;
    default:
      move_loop_.UpdateCursor(grab_cursor_);
      break;
  }

  // Note: event.data.[2,3] specify a rectangle. It is a request by the other
  // window to not send further XdndPosition messages while the cursor is
  // within it. However, it is considered advisory and (at least according to
  // the spec) the other side must handle further position messages within
  // it. GTK+ doesn't bother with this, so neither should we.

  waiting_on_status_.erase(source_window);

  if (ContainsKey(pending_drop_, source_window)) {
    // We were waiting on the status message so we could send the XdndDrop.
    SendXdndDrop(source_window);
    return;
  }

  NextPositionMap::iterator it = next_position_message_.find(source_window);
  if (it != next_position_message_.end()) {
    // We were waiting on the status message so we could send off the next
    // position message we queued up.
    gfx::Point p = it->second.first;
    unsigned long time = it->second.second;
    next_position_message_.erase(it);

    SendXdndPosition(source_window, p, time);
  }
}

void DesktopDragDropClientAuraX11::OnXdndFinished(
    const XClientMessageEvent& event) {
  DVLOG(1) << "XdndFinished";
  resulting_operation_ = AtomToDragOperation(
      negotiated_operation_[event.data.l[0]]);
  move_loop_.EndMoveLoop();
}

void DesktopDragDropClientAuraX11::OnXdndDrop(
    const XClientMessageEvent& event) {
  DVLOG(1) << "XdndDrop";

  unsigned long source_window = event.data.l[0];

  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  if (target_window_) {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(target_window_);
    if (delegate) {
      ui::OSExchangeData data(new ui::OSExchangeDataProviderAuraX11(
          xwindow_, target_current_context_->fetched_targets()));

      ui::DropTargetEvent event(data,
                                target_window_location_,
                                target_window_root_location_,
                                target_current_context_->GetDragOperation());
      drag_operation = delegate->OnPerformDrop(event);
    }

    target_window_->RemoveObserver(this);
    target_window_ = NULL;
  }

  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = atom_cache_.GetAtom("XdndFinished");
  xev.xclient.format = 32;
  xev.xclient.window = source_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (drag_operation != 0) ? 1 : 0;
  xev.xclient.data.l[2] = DragOperationToAtom(drag_operation);

  SendXClientEvent(source_window, &xev);
}

void DesktopDragDropClientAuraX11::OnSelectionNotify(
    const XSelectionEvent& xselection) {
  if (!target_current_context_) {
    NOTIMPLEMENTED();
    return;
  }

  target_current_context_->OnSelectionNotify(xselection);
}

int DesktopDragDropClientAuraX11::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::RootWindow* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  source_current_window_ = None;
  drag_drop_in_progress_ = true;
  drag_operation_ = operation;
  resulting_operation_ = ui::DragDropTypes::DRAG_NONE;

  const ui::OSExchangeData::Provider* provider = &data.provider();
  source_provider_ = static_cast<const ui::OSExchangeDataProviderAuraX11*>(
      provider);

  source_provider_->TakeOwnershipOfSelection();

  std::vector< ::Atom> actions = GetOfferedDragOperations();
  ui::SetAtomArrayProperty(xwindow_, "XdndActionList", "ATOM", actions);

  // Windows has a specific method, DoDragDrop(), which performs the entire
  // drag. We have to emulate this, so we spin off a nested runloop which will
  // track all cursor movement and reroute events to a specific handler.
  move_loop_.RunMoveLoop(source_window, grab_cursor_);

  source_provider_ = NULL;
  drag_drop_in_progress_ = false;
  drag_operation_ = 0;
  XDeleteProperty(xdisplay_, xwindow_, atom_cache_.GetAtom("XdndActionList"));

  return resulting_operation_;
}

void DesktopDragDropClientAuraX11::DragUpdate(aura::Window* target,
                                              const ui::LocatedEvent& event) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientAuraX11::Drop(aura::Window* target,
                                        const ui::LocatedEvent& event) {
  NOTIMPLEMENTED();
}

void DesktopDragDropClientAuraX11::DragCancel() {
  move_loop_.EndMoveLoop();
}

bool DesktopDragDropClientAuraX11::IsDragDropInProgress() {
  return drag_drop_in_progress_;
}

void DesktopDragDropClientAuraX11::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(target_window_, window);
  target_window_ = NULL;
}

void DesktopDragDropClientAuraX11::OnMouseMovement(XMotionEvent* event) {
  gfx::Point screen_point(event->x_root, event->y_root);

  // Find the current window the cursor is over.
  ::Window mouse_window = None;
  ::Window dest_window = None;
  FindWindowFor(screen_point, &mouse_window, &dest_window);

  if (source_current_window_ != dest_window) {
    if (source_current_window_ != None)
      SendXdndLeave(source_current_window_);

    source_current_window_ = dest_window;

    if (source_current_window_ != None) {
      negotiated_operation_.erase(source_current_window_);
      SendXdndEnter(source_current_window_);
    }
  }

  if (source_current_window_ != None) {
    if (ContainsKey(waiting_on_status_, dest_window)) {
      next_position_message_[dest_window] =
          std::make_pair(screen_point, event->time);
    } else {
      SendXdndPosition(dest_window, screen_point, event->time);
    }
  }
}

void DesktopDragDropClientAuraX11::OnMouseReleased() {
  if (source_current_window_ != None) {
    if (ContainsKey(waiting_on_status_, source_current_window_)) {
      // If we are waiting for an XdndStatus message, we need to wait for it to
      // complete.
      pending_drop_.insert(source_current_window_);
      return;
    }

    std::map< ::Window, ::Atom>::iterator it =
        negotiated_operation_.find(source_current_window_);
    if (it != negotiated_operation_.end() && it->second != None) {
      // We have negotiated an action with the other end.
      SendXdndDrop(source_current_window_);
      return;
    }

    SendXdndLeave(source_current_window_);
    source_current_window_ = None;
  }

  move_loop_.EndMoveLoop();
}

void DesktopDragDropClientAuraX11::OnMoveLoopEnded() {
  target_current_context_.reset();
}

void DesktopDragDropClientAuraX11::DragTranslate(
    const gfx::Point& root_window_location,
    scoped_ptr<ui::OSExchangeData>* data,
    scoped_ptr<ui::DropTargetEvent>* event,
    aura::client::DragDropDelegate** delegate) {
  gfx::Point root_location = root_window_location;
  root_window_->ConvertPointFromNativeScreen(&root_location);
  aura::Window* target_window =
      root_window_->GetEventHandlerForPoint(root_location);
  bool target_window_changed = false;
  if (target_window != target_window_) {
    if (target_window_)
      NotifyDragLeave();
    target_window_ = target_window;
    if (target_window_)
      target_window_->AddObserver(this);
    target_window_changed = true;
  }
  *delegate = NULL;
  if (!target_window_)
    return;
  *delegate = aura::client::GetDragDropDelegate(target_window_);
  if (!*delegate)
    return;

  data->reset(new OSExchangeData(new ui::OSExchangeDataProviderAuraX11(
      xwindow_, target_current_context_->fetched_targets())));
  gfx::Point location = root_location;
  aura::Window::ConvertPointToTarget(root_window_, target_window_, &location);

  target_window_location_ = location;
  target_window_root_location_ = root_location;

  event->reset(new ui::DropTargetEvent(
      *(data->get()),
      location,
      root_location,
      target_current_context_->GetDragOperation()));
  if (target_window_changed)
    (*delegate)->OnDragEntered(*event->get());
}

void DesktopDragDropClientAuraX11::NotifyDragLeave() {
  if (!target_window_)
    return;
  DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target_window_);
  if (delegate)
    delegate->OnDragExited();
  target_window_->RemoveObserver(this);
  target_window_ = NULL;
}

::Atom DesktopDragDropClientAuraX11::DragOperationToAtom(
    int drag_operation) {
  if (drag_operation & ui::DragDropTypes::DRAG_COPY)
    return atom_cache_.GetAtom(kXdndActionCopy);
  if (drag_operation & ui::DragDropTypes::DRAG_MOVE)
    return atom_cache_.GetAtom(kXdndActionMove);
  if (drag_operation & ui::DragDropTypes::DRAG_LINK)
    return atom_cache_.GetAtom(kXdndActionLink);

  return None;
}

int DesktopDragDropClientAuraX11::AtomToDragOperation(::Atom atom) {
  if (atom == atom_cache_.GetAtom(kXdndActionCopy))
    return ui::DragDropTypes::DRAG_COPY;
  if (atom == atom_cache_.GetAtom(kXdndActionMove))
    return ui::DragDropTypes::DRAG_MOVE;
  if (atom == atom_cache_.GetAtom(kXdndActionLink))
    return ui::DragDropTypes::DRAG_LINK;

  return ui::DragDropTypes::DRAG_NONE;
}

std::vector< ::Atom> DesktopDragDropClientAuraX11::GetOfferedDragOperations() {
  std::vector< ::Atom> operations;
  if (drag_operation_ & ui::DragDropTypes::DRAG_COPY)
    operations.push_back(atom_cache_.GetAtom(kXdndActionCopy));
  if (drag_operation_ & ui::DragDropTypes::DRAG_MOVE)
    operations.push_back(atom_cache_.GetAtom(kXdndActionMove));
  if (drag_operation_ & ui::DragDropTypes::DRAG_LINK)
    operations.push_back(atom_cache_.GetAtom(kXdndActionLink));
  return operations;
}

ui::SelectionFormatMap DesktopDragDropClientAuraX11::GetFormatMap() const {
  return source_provider_ ? source_provider_->GetFormatMap() :
      ui::SelectionFormatMap();
}

void DesktopDragDropClientAuraX11::CompleteXdndPosition(
    ::Window source_window,
    const gfx::Point& screen_point) {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;
  scoped_ptr<ui::OSExchangeData> data;
  scoped_ptr<ui::DropTargetEvent> drop_target_event;
  DragDropDelegate* delegate = NULL;
  DragTranslate(screen_point, &data, &drop_target_event, &delegate);
  if (delegate)
    drag_operation = delegate->OnDragUpdated(*drop_target_event);

  // Sends an XdndStatus message back to the source_window. l[2,3]
  // theoretically represent an area in the window where the current action is
  // the same as what we're returning, but I can't find any implementation that
  // actually making use of this. A client can return (0, 0) and/or set the
  // first bit of l[1] to disable the feature, and it appears that gtk neither
  // sets this nor respects it if set.
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = atom_cache_.GetAtom("XdndStatus");
  xev.xclient.format = 32;
  xev.xclient.window = source_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (drag_operation != 0) ?
      (kWantFurtherPosEvents | kWillAcceptDrop) : 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = DragOperationToAtom(drag_operation);

  SendXClientEvent(source_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndEnter(::Window dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = atom_cache_.GetAtom("XdndEnter");
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = (kMinXdndVersion << 24);  // The version number.
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  std::vector<Atom> targets;
  source_provider_->RetrieveTargets(&targets);

  if (targets.size() > 3) {
    xev.xclient.data.l[1] |= 1;
    ui::SetAtomArrayProperty(xwindow_, "XdndTypeList", "ATOM", targets);
  } else {
    // Pack the targets into the enter message.
    for (size_t i = 0; i < targets.size(); ++i)
      xev.xclient.data.l[2 + i] = targets[i];
  }

  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndLeave(::Window dest_window) {
  // If we're sending a leave message, don't wait for status messages anymore.
  waiting_on_status_.erase(dest_window);
  NextPositionMap::iterator it = next_position_message_.find(dest_window);
  if (it != next_position_message_.end())
    next_position_message_.erase(it);

  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = atom_cache_.GetAtom("XdndLeave");
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;
  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndPosition(
    ::Window dest_window,
    const gfx::Point& screen_point,
    unsigned long time) {
  waiting_on_status_.insert(dest_window);

  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = atom_cache_.GetAtom("XdndPosition");
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = (screen_point.x() << 16) | screen_point.y();
  xev.xclient.data.l[3] = time;
  xev.xclient.data.l[4] = DragOperationToAtom(drag_operation_);
  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXdndDrop(::Window dest_window) {
  XEvent xev;
  xev.xclient.type = ClientMessage;
  xev.xclient.message_type = atom_cache_.GetAtom("XdndDrop");
  xev.xclient.format = 32;
  xev.xclient.window = dest_window;
  xev.xclient.data.l[0] = xwindow_;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = CurrentTime;
  xev.xclient.data.l[3] = None;
  xev.xclient.data.l[4] = None;
  SendXClientEvent(dest_window, &xev);
}

void DesktopDragDropClientAuraX11::SendXClientEvent(::Window xid,
                                                    XEvent* xev) {
  DCHECK_EQ(ClientMessage, xev->type);

  // Don't send messages to the X11 message queue if we can help it.
  DesktopDragDropClientAuraX11* short_circuit = GetForWindow(xid);
  if (short_circuit) {
    Atom message_type = xev->xclient.message_type;
    if (message_type == atom_cache_.GetAtom("XdndEnter")) {
      short_circuit->OnXdndEnter(xev->xclient);
      return;
    } else if (message_type == atom_cache_.GetAtom("XdndLeave")) {
      short_circuit->OnXdndLeave(xev->xclient);
      return;
    } else if (message_type == atom_cache_.GetAtom("XdndPosition")) {
      short_circuit->OnXdndPosition(xev->xclient);
      return;
    } else if (message_type == atom_cache_.GetAtom("XdndStatus")) {
      short_circuit->OnXdndStatus(xev->xclient);
      return;
    } else if (message_type == atom_cache_.GetAtom("XdndFinished")) {
      short_circuit->OnXdndFinished(xev->xclient);
      return;
    } else if (message_type == atom_cache_.GetAtom("XdndDrop")) {
      short_circuit->OnXdndDrop(xev->xclient);
      return;
    }
  }

  // I don't understand why the GTK+ code is doing what it's doing here. It
  // goes out of its way to send the XEvent so that it receives a callback on
  // success or failure, and when it fails, it then sends an internal
  // GdkEvent about the failed drag. (And sending this message doesn't appear
  // to go through normal xlib machinery, but instead passes through the low
  // level xProto (the x11 wire format) that I don't understand.
  //
  // I'm unsure if I have to jump through those hoops, or if XSendEvent is
  // sufficient.
  XSendEvent(xdisplay_, xid, False, 0, xev);
}

}  // namespace views
