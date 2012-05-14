// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_H_
#define UI_AURA_WINDOW_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/window_types.h"
#include "ui/base/events.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace ui {
class Layer;
class Texture;
class Transform;
}

namespace aura {

class EventFilter;
class LayoutManager;
class RootWindow;
class WindowDelegate;
class WindowObserver;

namespace internal {
class FocusManager;
}

// Defined in window_property.h (which we do not include)
template<typename T>
struct WindowProperty;

// Aura window implementation. Interesting events are sent to the
// WindowDelegate.
// TODO(beng): resolve ownership.
class AURA_EXPORT Window : public ui::LayerDelegate,
                           public ui::GestureConsumer {
 public:
  typedef std::vector<Window*> Windows;

  class AURA_EXPORT TestApi {
   public:
    explicit TestApi(Window* window);

    bool OwnsLayer() const;

   private:
    TestApi();

    Window* window_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit Window(WindowDelegate* delegate);
  virtual ~Window();

  // Initializes the window. This creates the window's layer.
  void Init(ui::LayerType layer_type);

  void set_owned_by_parent(bool owned_by_parent) {
    owned_by_parent_ = owned_by_parent;
  }

  // A type is used to identify a class of Windows and customize behavior such
  // as event handling and parenting.  This field should only be consumed by the
  // shell -- Aura itself shouldn't contain type-specific logic.
  client::WindowType type() const { return type_; }
  void SetType(client::WindowType type);

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  const std::string& name() const { return name_; }
  void SetName(const std::string& name);

  const string16 title() const { return title_; }
  void set_title(const string16& title) { title_ = title; }

  bool transparent() const { return transparent_; }
  void SetTransparent(bool transparent);

  ui::Layer* layer() { return layer_; }
  const ui::Layer* layer() const { return layer_; }

  // Releases the Window's owning reference to its layer, and returns it.
  // This is used when you need to animate the presentation of the Window just
  // prior to destroying it. The window can be destroyed soon after calling this
  // function, and the caller is then responsible for disposing of the layer
  // once any animation completes. Note that layer() will remain valid until the
  // end of ~Window().
  ui::Layer* AcquireLayer();

  WindowDelegate* delegate() { return delegate_; }

  const gfx::Rect& bounds() const;

  Window* parent() { return parent_; }
  const Window* parent() const { return parent_; }

  // Returns the RootWindow that contains this Window or NULL if the Window is
  // not contained by a RootWindow.
  virtual RootWindow* GetRootWindow();
  virtual const RootWindow* GetRootWindow() const;

  // The Window does not own this object.
  void set_user_data(void* user_data) { user_data_ = user_data; }
  void* user_data() const { return user_data_; }

  // Changes the visibility of the window.
  void Show();
  void Hide();
  // Returns true if this window and all its ancestors are visible.
  bool IsVisible() const;
  // Returns the visibility requested by this window. IsVisible() takes into
  // account the visibility of the layer and ancestors, where as this tracks
  // whether Show() without a Hide() has been invoked.
  bool TargetVisibility() const { return visible_; }

  // Returns the window's bounds in screen coordinates. In ash, this is
  // effectively screen bounds.
  //
  // TODO(oshima): Fix this to return screen's coordinate for multi-monitor
  // support.
  gfx::Rect GetBoundsInRootWindow() const;

  virtual void SetTransform(const ui::Transform& transform);

  // Assigns a LayoutManager to size and place child windows.
  // The Window takes ownership of the LayoutManager.
  void SetLayoutManager(LayoutManager* layout_manager);
  LayoutManager* layout_manager() { return layout_manager_.get(); }

  // Changes the bounds of the window. If present, the window's parent's
  // LayoutManager may adjust the bounds.
  void SetBounds(const gfx::Rect& new_bounds);

  // Returns the target bounds of the window. If the window's layer is
  // not animating, it simply returns the current bounds.
  gfx::Rect GetTargetBounds() const;

  // Marks the a portion of window as needing to be painted.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Assigns a new external texture to the window's layer.
  void SetExternalTexture(ui::Texture* texture);

  // Sets the parent window of the window. If NULL, the window is parented to
  // the root window.
  void SetParent(Window* parent);

  // Stacks the specified child of this Window at the front of the z-order.
  void StackChildAtTop(Window* child);

  // Stacks |child| above |target|.  Does nothing if |child| is already above
  // |target|.  Does not stack on top of windows with NULL layer delegates,
  // see WindowTest.StackingMadrigal for details.
  void StackChildAbove(Window* child, Window* target);

  // Stacks |child| below |target|. Does nothing if |child| is already below
  // |target|.
  void StackChildBelow(Window* child, Window* target);

  // Tree operations.
  void AddChild(Window* child);
  void RemoveChild(Window* child);

  const Windows& children() const { return children_; }

  // Returns true if this Window contains |other| somewhere in its children.
  bool Contains(const Window* other) const;

  // Adds or removes |child| as a transient child of this window. Transient
  // children get the following behavior:
  // . The transient parent destroys any transient children when it is
  //   destroyed. This means a transient child is destroyed if either its parent
  //   or transient parent is destroyed.
  // . If a transient child and its transient parent share the same parent, then
  //   transient children are always ordered above the transient parent.
  // Transient windows are typically used for popups and menus.
  void AddTransientChild(Window* child);
  void RemoveTransientChild(Window* child);

  const Windows& transient_children() const { return transient_children_; }

  Window* transient_parent() { return transient_parent_; }
  const Window* transient_parent() const { return transient_parent_; }

  // Retrieves the first-level child with the specified id, or NULL if no first-
  // level child is found matching |id|.
  Window* GetChildById(int id);
  const Window* GetChildById(int id) const;

  // Converts |point| from |source|'s coordinates to |target|'s. If |source| is
  // NULL, the function returns without modifying |point|. |target| cannot be
  // NULL.
  static void ConvertPointToWindow(const Window* source,
                                   const Window* target,
                                   gfx::Point* point);

  // Returns the cursor for the specified point, in window coordinates.
  gfx::NativeCursor GetCursor(const gfx::Point& point) const;

  // Window takes ownership of the EventFilter.
  void SetEventFilter(EventFilter* event_filter);
  EventFilter* event_filter() { return event_filter_.get(); }

  // Add/remove observer.
  void AddObserver(WindowObserver* observer);
  void RemoveObserver(WindowObserver* observer);

  void set_ignore_events(bool ignore_events) { ignore_events_ = ignore_events; }

  // Sets the window to grab hits for an area extending -|insets| pixels outside
  // its bounds. This can be used to create an invisible non- client area, for
  // example if your windows have no visible frames but still need to have
  // resize edges.
  void set_hit_test_bounds_override_outer(const gfx::Insets& insets) {
    hit_test_bounds_override_outer_ = insets;
  }
  gfx::Insets hit_test_bounds_override_outer() const {
    return hit_test_bounds_override_outer_;
  }

  // Sets the window to grab hits for an area extending |insets| pixels inside
  // its bounds (even if that inner region overlaps a child window). This can be
  // used to create an invisible non-client area that overlaps the client area.
  void set_hit_test_bounds_override_inner(const gfx::Insets& insets) {
    hit_test_bounds_override_inner_ = insets;
  }
  gfx::Insets hit_test_bounds_override_inner() const {
    return hit_test_bounds_override_inner_;
  }

  // Returns true if the |point_in_root| in root window's coordinate falls
  // within this window's bounds. Returns false if the window is detached
  // from root window.
  bool ContainsPointInRoot(const gfx::Point& point_in_root);

  // Returns true if relative-to-this-Window's-origin |local_point| falls
  // within this Window's bounds.
  bool ContainsPoint(const gfx::Point& local_point);

  // Returns true if the mouse pointer at relative-to-this-Window's-origin
  // |local_point| can trigger an event for this Window.
  // TODO(beng): A Window can supply a hit-test mask to cause some portions of
  // itself to not trigger events, causing the events to fall through to the
  // Window behind.
  bool HitTest(const gfx::Point& local_point);

  // Returns the Window that most closely encloses |local_point| for the
  // purposes of event targeting.
  Window* GetEventHandlerForPoint(const gfx::Point& local_point);

  // Returns the topmost Window with a delegate containing |local_point|.
  Window* GetTopWindowContainingPoint(const gfx::Point& local_point);

  // Returns this window's toplevel window (the highest-up-the-tree anscestor
  // that has a delegate set).  The toplevel window may be |this|.
  Window* GetToplevelWindow();

  // Claims or relinquishes the claim to focus.
  void Focus();
  void Blur();

  // Returns true if the Window is currently the focused window.
  bool HasFocus() const;

  // Returns true if the Window can be focused.
  virtual bool CanFocus() const;

  // Returns true if the Window can receive events.
  virtual bool CanReceiveEvents() const;

  // Returns the FocusManager for the Window, which may be attached to a parent
  // Window. Can return NULL if the Window has no FocusManager.
  virtual internal::FocusManager* GetFocusManager();
  virtual const internal::FocusManager* GetFocusManager() const;

  // Sets the capture window to |window|, for events specified in
  // |flags|. |flags| is ui::CaptureEventFlags. This does nothing if
  // the window isn't showing (VISIBILITY_SHOWN), or isn't contained
  // in a valid window hierarchy.
  void SetCapture(unsigned int flags);

  // Stop capturing all events (mouse and touch).
  void ReleaseCapture();

  // Returns true if there is a window capturing all event types
  // specified by |flags|. |flags| is ui::CaptureEventFlags.
  bool HasCapture(unsigned int flags);

  // Suppresses painting window content by disgarding damaged rect and ignoring
  // new paint requests.
  void SuppressPaint();

  // Sets the |value| of the given window |property|. Setting to the default
  // value (e.g., NULL) removes the property. The caller is responsible for the
  // lifetime of any object set as a property on the Window.
  template<typename T>
  void SetProperty(const WindowProperty<T>* property, T value);

  // Returns the value of the given window |property|.  Returns the
  // property-specific default value if the property was not previously set.
  template<typename T>
  T GetProperty(const WindowProperty<T>* property) const;

  // Sets the |property| to its default value. Useful for avoiding a cast when
  // setting to NULL.
  template<typename T>
  void ClearProperty(const WindowProperty<T>* property);

  // NativeWidget::[GS]etNativeWindowProperty use strings as keys, and this is
  // difficult to change while retaining compatibility with other platforms.
  // TODO(benrg): Find a better solution.
  void SetNativeWindowProperty(const char* key, void* value);
  void* GetNativeWindowProperty(const char* key) const;

  // Type of a function to delete a property that this window owns.
  typedef void (*PropertyDeallocator)(intptr_t value);

 private:
  friend class LayoutManager;

  // Used when stacking windows.
  enum StackDirection {
    STACK_ABOVE,
    STACK_BELOW
  };

  // Called by the public {Set,Get,Clear}Property functions.
  intptr_t SetPropertyInternal(const void* key,
                               const char* name,
                               PropertyDeallocator deallocator,
                               intptr_t value,
                               intptr_t default_value);
  intptr_t GetPropertyInternal(const void* key, intptr_t default_value) const;

  // Changes the bounds of the window without condition.
  void SetBoundsInternal(const gfx::Rect& new_bounds);

  // Updates the visible state of the layer, but does not make visible-state
  // specific changes. Called from Show()/Hide().
  void SetVisible(bool visible);

  // Schedules a paint for the Window's entire bounds.
  void SchedulePaint();

  // Gets a Window (either this one or a subwindow) containing |local_point|.
  // If |return_tightest| is true, returns the tightest-containing (i.e.
  // furthest down the hierarchy) Window containing the point; otherwise,
  // returns the loosest.  If |for_event_handling| is true, then hit-test masks
  // are honored; otherwise, only bounds checks are performed.
  Window* GetWindowForPoint(const gfx::Point& local_point,
                            bool return_tightest,
                            bool for_event_handling);

  // Called when this window's parent has changed.
  void OnParentChanged();

  // Determines the real location for stacking |child| and invokes
  // StackChildRelativeToImpl().
  void StackChildRelativeTo(Window* child,
                            Window* target,
                            StackDirection direction);

  // Implementation of StackChildRelativeTo().
  void StackChildRelativeToImpl(Window* child,
                                Window* target,
                                StackDirection direction);

  // Called when this window's stacking order among its siblings is changed.
  void OnStackingChanged();

  // Notifies observers registered with this Window (and its subtree) when the
  // Window has been added or is about to be removed from a RootWindow.
  void NotifyAddedToRootWindow();
  void NotifyRemovingFromRootWindow();

  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;

  // Updates the layer name with a name based on the window's name and id.
  void UpdateLayerName(const std::string& name);

#ifndef NDEBUG
  // These methods are useful when debugging.
  std::string GetDebugInfo() const;
  void PrintWindowHierarchy(int depth) const;
#endif

  client::WindowType type_;

  // True if the Window is owned by its parent - i.e. it will be deleted by its
  // parent during its parents destruction. True is the default.
  bool owned_by_parent_;

  WindowDelegate* delegate_;

  // The Window will own its layer unless ownership is relinquished via a call
  // to AcquireLayer(). After that moment |layer_| will still be valid but
  // |layer_owner_| will be NULL. The reason for releasing ownership is that
  // the client may wish to animate the window's layer beyond the lifetime of
  // the window, e.g. fading it out when it is destroyed.
  scoped_ptr<ui::Layer> layer_owner_;
  ui::Layer* layer_;

  // The Window's parent.
  Window* parent_;

  // Child windows. Topmost is last.
  Windows children_;

  // Transient windows.
  Windows transient_children_;

  Window* transient_parent_;

  // The visibility state of the window as set by Show()/Hide(). This may differ
  // from the visibility of the underlying layer, which may remain visible after
  // the window is hidden (e.g. to animate its disappearance).
  bool visible_;

  int id_;
  std::string name_;

  string16 title_;

  // Whether layer is initialized as non-opaque.
  bool transparent_;

  scoped_ptr<EventFilter> event_filter_;
  scoped_ptr<LayoutManager> layout_manager_;

  void* user_data_;

  // Makes the window pass all events through to any windows behind it.
  bool ignore_events_;

  // See set_hit_test_outer_override().
  gfx::Insets hit_test_bounds_override_outer_;
  gfx::Insets hit_test_bounds_override_inner_;

  ObserverList<WindowObserver> observers_;

  // Value struct to keep the name and deallocator for this property.
  // Key cannot be used for this purpose because it can be char* or
  // WindowProperty<>.
  struct Value {
    const char* name;
    intptr_t value;
    PropertyDeallocator deallocator;
  };

  std::map<const void*, Value> prop_map_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_H_
