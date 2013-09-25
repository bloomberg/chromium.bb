// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_

#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/point.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop_delegate.h"

namespace aura {
class RootWindow;
namespace client {
class DragDropDelegate;
}
}

namespace gfx {
class Point;
}

namespace ui {
class DragSource;
class DropTargetEvent;
class OSExchangeData;
class OSExchangeDataProviderAuraX11;
class RootWindow;
class SelectionFormatMap;
}

namespace views {
class DesktopNativeCursorManager;

// Implements drag and drop on X11 for aura. On one side, this class takes raw
// X11 events forwarded from DesktopRootWindowHostLinux, while on the other, it
// handles the views drag events.
class VIEWS_EXPORT DesktopDragDropClientAuraX11
    : public aura::client::DragDropClient,
      public aura::WindowObserver,
      public X11WholeScreenMoveLoopDelegate {
 public:
  DesktopDragDropClientAuraX11(
      aura::RootWindow* root_window,
      views::DesktopNativeCursorManager* cursor_manager,
      Display* xdisplay,
      ::Window xwindow);
  virtual ~DesktopDragDropClientAuraX11();

  // We maintain a mapping of live DesktopDragDropClientAuraX11 objects to
  // their ::Windows. We do this so that we're able to short circuit sending
  // X11 messages to windows in our process.
  static DesktopDragDropClientAuraX11* GetForWindow(::Window window);

  // These methods handle the various X11 client messages from the platform.
  void OnXdndEnter(const XClientMessageEvent& event);
  void OnXdndLeave(const XClientMessageEvent& event);
  void OnXdndPosition(const XClientMessageEvent& event);
  void OnXdndStatus(const XClientMessageEvent& event);
  void OnXdndFinished(const XClientMessageEvent& event);
  void OnXdndDrop(const XClientMessageEvent& event);

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  // Overridden from aura::client::DragDropClient:
  virtual int StartDragAndDrop(
      const ui::OSExchangeData& data,
      aura::RootWindow* root_window,
      aura::Window* source_window,
      const gfx::Point& root_location,
      int operation,
      ui::DragDropTypes::DragEventSource source) OVERRIDE;
  virtual void DragUpdate(aura::Window* target,
                          const ui::LocatedEvent& event) OVERRIDE;
  virtual void Drop(aura::Window* target,
                    const ui::LocatedEvent& event) OVERRIDE;
  virtual void DragCancel() OVERRIDE;
  virtual bool IsDragDropInProgress() OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // Overridden from X11WholeScreenMoveLoopDelegate:
  virtual void OnMouseMovement(XMotionEvent* event) OVERRIDE;
  virtual void OnMouseReleased() OVERRIDE;
  virtual void OnMoveLoopEnded() OVERRIDE;

 private:
  typedef std::map< ::Window, std::pair<gfx::Point, unsigned long> >
      NextPositionMap;

  // When we receive an position x11 message, we need to translate that into
  // the underlying aura::Window representation, as moves internal to the X11
  // window can cause internal drag leave and enter messages.
  void DragTranslate(const gfx::Point& root_window_location,
                     scoped_ptr<ui::OSExchangeData>* data,
                     scoped_ptr<ui::DropTargetEvent>* event,
                     aura::client::DragDropDelegate** delegate);

  // Called when we need to notify the current aura::Window that we're no
  // longer dragging over it.
  void NotifyDragLeave();

  // Converts our bitfield of actions into an Atom that represents what action
  // we're most likely to take on drop.
  ::Atom DragOperationToAtom(int drag_operation);

  // Converts a single action atom to a drag operation.
  int AtomToDragOperation(::Atom atom);

  // During the blocking StartDragAndDrop() call, this converts the views-style
  // |drag_operation_| bitfield into a vector of Atoms to offer to other
  // processes.
  std::vector< ::Atom> GetOfferedDragOperations();

  // This returns a representation of the data we're offering in this
  // drag. This is done to bypass an asynchronous roundtrip with the X11
  // server.
  ui::SelectionFormatMap GetFormatMap() const;

  // Handling XdndPosition can be paused while waiting for more data; this is
  // called either synchronously from OnXdndPosition, or asynchronously after
  // we've received data requested from the other window.
  void CompleteXdndPosition(::Window source_window,
                            const gfx::Point& screen_point);

  void SendXdndEnter(::Window dest_window);
  void SendXdndLeave(::Window dest_window);
  void SendXdndPosition(::Window dest_window,
                        const gfx::Point& screen_point,
                        unsigned long time);
  void SendXdndDrop(::Window dest_window);

  // Sends |xev| to |xid|, optionally short circuiting the round trip to the X
  // server.
  void SendXClientEvent(::Window xid, XEvent* xev);

  // A nested message loop that notifies this object of events through the
  // X11WholeScreenMoveLoopDelegate interface.
  X11WholeScreenMoveLoop move_loop_;

  aura::RootWindow* root_window_;

  Display* xdisplay_;
  ::Window xwindow_;

  ui::X11AtomCache atom_cache_;

  // Target side information.
  class X11DragContext;
  scoped_ptr<X11DragContext> target_current_context_;

  // The Aura window that is currently under the cursor. We need to manually
  // keep track of this because Windows will only call our drag enter method
  // once when the user enters the associated X Window. But inside that X
  // Window there could be multiple aura windows, so we need to generate drag
  // enter events for them.
  aura::Window* target_window_;

  // Because Xdnd messages don't contain the position in messages other than
  // the XdndPosition message, we must manually keep track of the last position
  // change.
  gfx::Point target_window_location_;
  gfx::Point target_window_root_location_;

  // In the Xdnd protocol, we aren't supposed to send another XdndPosition
  // message until we have received a confirming XdndStatus message.
  std::set< ::Window> waiting_on_status_;

  // If we would send an XdndPosition message while we're waiting for an
  // XdndStatus response, we need to cache the latest details we'd send.
  NextPositionMap next_position_message_;

  // Source side information.
  ui::OSExchangeDataProviderAuraX11 const* source_provider_;
  ::Window source_current_window_;

  bool drag_drop_in_progress_;

  // The operation bitfield as requested by StartDragAndDrop.
  int drag_operation_;

  // The operation performed. Is initialized to None at the start of
  // StartDragAndDrop(), and is set only during the asynchronous XdndFinished
  // message.
  int resulting_operation_;

  // This window will be receiving a drop as soon as we receive an XdndStatus
  // from it.
  std::set< ::Window> pending_drop_;

  // We offer the other window a list of possible operations,
  // XdndActionsList. This is the requested action from the other window. This
  // is None if we haven't sent out an XdndPosition message yet, haven't yet
  // received an XdndStatus or if the other window has told us that there's no
  // action that we can agree on.
  //
  // This is a map instead of a simple variable because of the case where we
  // put an XdndLeave in the queue at roughly the same time that the other
  // window responds to an XdndStatus.
  std::map< ::Window, ::Atom> negotiated_operation_;

  // We use these cursors while dragging.
  gfx::NativeCursor grab_cursor_;
  gfx::NativeCursor copy_grab_cursor_;
  gfx::NativeCursor move_grab_cursor_;

  static std::map< ::Window, DesktopDragDropClientAuraX11*> g_live_client_map;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
