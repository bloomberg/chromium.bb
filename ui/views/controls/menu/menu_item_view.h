// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_ITEM_VIEW_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_ITEM_VIEW_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/view.h"

#if defined(OS_WIN)
#include <windows.h>

#include "ui/native_theme/native_theme.h"
#endif

namespace gfx {
class FontList;
struct VectorIcon;
}

namespace views {

namespace internal {
class MenuRunnerImpl;
}

namespace test {
class TestMenuItemViewShown;
}

class MenuController;
class MenuDelegate;
class TestMenuItemView;
class SubmenuView;

// MenuItemView --------------------------------------------------------------

// MenuItemView represents a single menu item with a label and optional icon.
// Each MenuItemView may also contain a submenu, which in turn may contain
// any number of child MenuItemViews.
//
// To use a menu create an initial MenuItemView using the constructor that
// takes a MenuDelegate, then create any number of child menu items by way
// of the various AddXXX methods.
//
// MenuItemView is itself a View, which means you can add Views to each
// MenuItemView. This is normally NOT want you want, rather add other child
// Views to the submenu of the MenuItemView. Any child views of the MenuItemView
// that are focusable can be navigated to by way of the up/down arrow and can be
// activated by way of space/return keys. Activating a focusable child results
// in |AcceleratorPressed| being invoked. Note, that as menus try not to steal
// focus from the hosting window child views do not actually get focus. Instead
// |SetHotTracked| is used as the user navigates around.
//
// To show the menu use MenuRunner. See MenuRunner for details on how to run
// (show) the menu as well as for details on the life time of the menu.

class VIEWS_EXPORT MenuItemView : public View {
 public:
  friend class MenuController;

  // The menu item view's class name.
  static const char kViewClassName[];

  // ID used to identify menu items.
  static const int kMenuItemViewID;

  // ID used to identify empty menu items.
  static const int kEmptyMenuItemViewID;

  // Different types of menu items.  EMPTY is a special type for empty
  // menus that is only used internally.
  enum Type {
    NORMAL,
    SUBMENU,
    CHECKBOX,
    RADIO,
    SEPARATOR,
    EMPTY
  };

  // Where the menu should be drawn, above or below the bounds (when
  // the bounds is non-empty).  POSITION_BEST_FIT (default) positions
  // the menu below the bounds unless the menu does not fit on the
  // screen and the re is more space above.
  enum MenuPosition {
    POSITION_BEST_FIT,
    POSITION_ABOVE_BOUNDS,
    POSITION_BELOW_BOUNDS
  };

  // The data structure which is used for the menu size
  struct MenuItemDimensions {
    MenuItemDimensions()
        : standard_width(0),
          children_width(0),
          minor_text_width(0),
          height(0) {}

    // Width of everything except the accelerator and children views.
    int standard_width;
    // The width of all contained views of the item.
    int children_width;
    // The amount of space needed to accommodate the subtext.
    int minor_text_width;
    // The height of the menu item.
    int height;
  };

  // Constructor for use with the top level menu item. This menu is never
  // shown to the user, rather its use as the parent for all menu items.
  explicit MenuItemView(MenuDelegate* delegate);

  // Overridden from View:
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Returns the preferred height of menu items. This is only valid when the
  // menu is about to be shown.
  static int pref_menu_height() { return pref_menu_height_; }

  // X-coordinate of where the label starts.
  static int label_start() { return label_start_; }

  // Returns if a given |anchor| is a bubble or not.
  static bool IsBubble(MenuAnchorPosition anchor);

  // Returns the accessible name to be used with screen readers. Mnemonics are
  // removed and the menu item accelerator text is appended.
  static base::string16 GetAccessibleNameForMenuItem(
      const base::string16& item_text, const base::string16& accelerator_text);

  // Hides and cancels the menu. This does nothing if the menu is not open.
  void Cancel();

  // Add an item to the menu at a specified index.  ChildrenChanged() should
  // called after adding menu items if the menu may be active.
  MenuItemView* AddMenuItemAt(int index,
                              int item_id,
                              const base::string16& label,
                              const base::string16& sublabel,
                              const base::string16& minor_text,
                              const gfx::VectorIcon* minor_icon,
                              const gfx::ImageSkia& icon,
                              Type type,
                              ui::MenuSeparatorType separator_style);

  // Remove an item from the menu at a specified index. The removed MenuItemView
  // is deleted when ChildrenChanged() is invoked.
  void RemoveMenuItemAt(int index);

  // Appends an item to this menu.
  // item_id    The id of the item, used to identify it in delegate callbacks
  //            or (if delegate is NULL) to identify the command associated
  //            with this item with the controller specified in the ctor. Note
  //            that this value should not be 0 as this has a special meaning
  //            ("NULL command, no item selected")
  // label      The text label shown.
  // type       The type of item.
  MenuItemView* AppendMenuItem(int item_id,
                               const base::string16& label,
                               Type type);

  // Append a submenu to this menu.
  // The returned pointer is owned by this menu.
  MenuItemView* AppendSubMenu(int item_id,
                              const base::string16& label);

  // Append a submenu with an icon to this menu.
  // The returned pointer is owned by this menu.
  MenuItemView* AppendSubMenuWithIcon(int item_id,
                                      const base::string16& label,
                                      const gfx::ImageSkia& icon);

  // This is a convenience for standard text label menu items where the label
  // is provided with this call.
  MenuItemView* AppendMenuItemWithLabel(int item_id,
                                        const base::string16& label);

  // This is a convenience for text label menu items where the label is
  // provided by the delegate.
  MenuItemView* AppendDelegateMenuItem(int item_id);

  // Adds a separator to this menu
  void AppendSeparator();

  // Appends a menu item with an icon. This is for the menu item which
  // needs an icon. Calling this function forces the Menu class to draw
  // the menu, instead of relying on Windows.
  MenuItemView* AppendMenuItemWithIcon(int item_id,
                                       const base::string16& label,
                                       const gfx::ImageSkia& icon);

  // All the AppendXXX methods funnel into this.
  MenuItemView* AppendMenuItemImpl(int item_id,
                                   const base::string16& label,
                                   const base::string16& sublabel,
                                   const base::string16& minor_text,
                                   const gfx::VectorIcon* minor_icon,
                                   const gfx::ImageSkia& icon,
                                   Type type,
                                   ui::MenuSeparatorType separator_style);

  // Returns the view that contains child menu items. If the submenu has
  // not been creates, this creates it.
  virtual SubmenuView* CreateSubmenu();

  // Returns true if this menu item has a submenu.
  virtual bool HasSubmenu() const;

  // Returns the view containing child menu items.
  virtual SubmenuView* GetSubmenu() const;

  // Returns true if this menu item has a submenu and it is showing
  virtual bool SubmenuIsShowing() const;

  // Returns the parent menu item.
  MenuItemView* GetParentMenuItem() { return parent_menu_item_; }
  const MenuItemView* GetParentMenuItem() const { return parent_menu_item_; }

  // Sets/Gets the title.
  void SetTitle(const base::string16& title);
  const base::string16& title() const { return title_; }

  // Sets the subtitle.
  void SetSubtitle(const base::string16& subtitle);

  // Sets the minor text.
  void SetMinorText(const base::string16& minor_text);

  // Sets the minor icon.
  void SetMinorIcon(const gfx::VectorIcon* minor_icon);

  // Returns the type of this menu.
  const Type& GetType() const { return type_; }

  // Sets whether this item is selected. This is invoked as the user moves
  // the mouse around the menu while open.
  void SetSelected(bool selected);

  // Returns true if the item is selected.
  bool IsSelected() const { return selected_; }

  // Sets the |tooltip| for a menu item view with |item_id| identifier.
  void SetTooltip(const base::string16& tooltip, int item_id);

  // Sets the icon for the descendant identified by item_id.
  void SetIcon(const gfx::ImageSkia& icon, int item_id);

  // Sets the icon of this menu item.
  void SetIcon(const gfx::ImageSkia& icon);

  // Sets the view used to render the icon. This clobbers any icon set via
  // SetIcon(). MenuItemView takes ownership of |icon_view|.
  void SetIconView(View* icon_view);
  View* icon_view() { return icon_view_; }

  // Sets the command id of this menu item.
  void SetCommand(int command) { command_ = command; }

  // Returns the command id of this item.
  int GetCommand() const { return command_; }

  // Paints the menu item.
  void OnPaint(gfx::Canvas* canvas) override;

  // Returns the preferred size of this item.
  gfx::Size CalculatePreferredSize() const override;

  // Gets the preferred height for the given |width|. This is only different
  // from GetPreferredSize().width() if the item has a child view with flexible
  // dimensions.
  int GetHeightForWidth(int width) const override;

  // Return the preferred dimensions of the item in pixel.
  const MenuItemDimensions& GetDimensions() const;

  // Returns the object responsible for controlling showing the menu.
  MenuController* GetMenuController();
  const MenuController* GetMenuController() const;

  // Returns the delegate. This returns the delegate of the root menu item.
  MenuDelegate* GetDelegate();
  const MenuDelegate* GetDelegate() const;
  void set_delegate(MenuDelegate* delegate) { delegate_ = delegate; }

  // Returns the root parent, or this if this has no parent.
  MenuItemView* GetRootMenuItem();
  const MenuItemView* GetRootMenuItem() const;

  // Returns the mnemonic for this MenuItemView, or 0 if this MenuItemView
  // doesn't have a mnemonic.
  base::char16 GetMnemonic();

  // Do we have icons? This only has effect on the top menu. Turning this on
  // makes the menus slightly wider and taller.
  void set_has_icons(bool has_icons) {
    has_icons_ = has_icons;
  }
  bool has_icons() const { return has_icons_; }

  // Returns the descendant with the specified command.
  MenuItemView* GetMenuItemByID(int id);

  // Invoke if you remove/add children to the menu while it's showing. This
  // recalculates the bounds.
  void ChildrenChanged();

  // Sizes any child views.
  void Layout() override;

  // Returns true if the menu has mnemonics. This only useful on the root menu
  // item.
  bool has_mnemonics() const { return has_mnemonics_; }

  // Set top and bottom margins in pixels.  If no margin is set or a
  // negative margin is specified then MenuConfig values are used.
  void SetMargins(int top_margin, int bottom_margin);

  // Suppress the right margin if this is set to false.
  void set_use_right_margin(bool use_right_margin) {
    use_right_margin_ = use_right_margin;
  }

 protected:
  // Creates a MenuItemView. This is used by the various AddXXX methods.
  MenuItemView(MenuItemView* parent, int command, Type type);

  // MenuRunner owns MenuItemView and should be the only one deleting it.
  ~MenuItemView() override;

  void ChildPreferredSizeChanged(View* child) override;

  const char* GetClassName() const override;

  // Returns the preferred size (and padding) of any children.
  virtual gfx::Size GetChildPreferredSize() const;

  // Returns the various margins.
  int GetTopMargin() const;
  int GetBottomMargin() const;

 private:
  friend class internal::MenuRunnerImpl;  // For access to ~MenuItemView.
  friend class test::TestMenuItemViewShown;  // for access to |submenu_|;
  friend class TestMenuItemView;             // For access to AddEmptyMenus();

  enum PaintButtonMode { PB_NORMAL, PB_FOR_DRAG };

  // Calculates all sizes that we can from the OS.
  //
  // This is invoked prior to Running a menu.
  void UpdateMenuPartSizes();

  // Called by the two constructors to initialize this menu item.
  void Init(MenuItemView* parent,
            int command,
            MenuItemView::Type type,
            MenuDelegate* delegate);

  // The RunXXX methods call into this to set up the necessary state before
  // running. |is_first_menu| is true if no menus are currently showing.
  void PrepareForRun(bool is_first_menu,
                     bool has_mnemonics,
                     bool show_mnemonics);

  // Returns the flags passed to DrawStringRect.
  int GetDrawStringFlags();

  // Returns the font list to use for menu text.
  const gfx::FontList& GetFontList() const;

  // If this menu item has no children a child is added showing it has no
  // children. Otherwise AddEmtpyMenus is recursively invoked on child menu
  // items that have children.
  void AddEmptyMenus();

  // Undoes the work of AddEmptyMenus.
  void RemoveEmptyMenus();

  // Given bounds within our View, this helper routine mirrors the bounds if
  // necessary.
  void AdjustBoundsForRTLUI(gfx::Rect* rect) const;

  // Actual paint implementation. If mode is PB_FOR_DRAG, portions of the menu
  // are not rendered.
  void PaintButton(gfx::Canvas* canvas, PaintButtonMode mode);

  // Paints the right-side icon and text.
  void PaintMinorIconAndText(gfx::Canvas* canvas, SkColor color);

  // Destroys the window used to display this menu and recursively destroys
  // the windows used to display all descendants.
  void DestroyAllMenuHosts();

  // Returns the text that should be displayed on the end (right) of the menu
  // item. This will be the accelerator (if one exists), otherwise |subtitle_|.
  base::string16 GetMinorText() const;

  // Returns the icon that should be displayed to the left of the minor text.
  const gfx::VectorIcon* GetMinorIcon() const;

  // Returns the text color for the current state.  |minor| specifies if the
  // minor text or the normal text is desired.
  SkColor GetTextColor(bool minor,
                       bool render_selection,
                       bool emphasized) const;

  // Calculates and returns the MenuItemDimensions.
  MenuItemDimensions CalculateDimensions() const;

  // Get the horizontal position at which to draw the menu item's label.
  int GetLabelStartForThisItem() const;

  // Used by MenuController to cache the menu position in use by the
  // active menu.
  MenuPosition actual_menu_position() const { return actual_menu_position_; }
  void set_actual_menu_position(MenuPosition actual_menu_position) {
    actual_menu_position_ = actual_menu_position;
  }

  void set_controller(MenuController* controller) {
    if (controller)
      controller_ = controller->AsWeakPtr();
    else
      controller_.reset();
  }

  // Returns true if this MenuItemView contains a single child
  // that is responsible for rendering the content.
  bool IsContainer() const;

  // Returns number of child views excluding icon_view.
  int NonIconChildViewsCount() const;

  // Returns the max icon width; recurses over submenus.
  int GetMaxIconViewWidth() const;

  // Returns true if the menu has items with a checkbox or a radio button.
  bool HasChecksOrRadioButtons() const;

  void invalidate_dimensions() { dimensions_.height = 0; }
  bool is_dimensions_valid() const { return dimensions_.height > 0; }

  // The delegate. This is only valid for the root menu item. You shouldn't
  // use this directly, instead use GetDelegate() which walks the tree as
  // as necessary.
  MenuDelegate* delegate_;

  // The controller for the run operation, or NULL if the menu isn't showing.
  base::WeakPtr<MenuController> controller_;

  // Used to detect when Cancel was invoked.
  bool canceled_;

  // Our parent.
  MenuItemView* parent_menu_item_;

  // Type of menu. NOTE: MenuItemView doesn't itself represent SEPARATOR,
  // that is handled by an entirely different view class.
  Type type_;

  // Whether we're selected.
  bool selected_;

  // Command id.
  int command_;

  // Submenu, created via CreateSubmenu.
  SubmenuView* submenu_;

  // Title.
  base::string16 title_;

  // Subtitle/sublabel.
  base::string16 subtitle_;

  // Minor text.
  base::string16 minor_text_;

  // Minor icon.
  const gfx::VectorIcon* minor_icon_ = nullptr;

  // Does the title have a mnemonic? Only useful on the root menu item.
  bool has_mnemonics_;

  // Should we show the mnemonic? Mnemonics are shown if this is true or
  // MenuConfig says mnemonics should be shown. Only used on the root menu item.
  bool show_mnemonics_;

  // Set if menu has icons or icon_views (applies to root menu item only).
  bool has_icons_;

  // Pointer to a view with a menu icon.
  View* icon_view_;

  // The tooltip to show on hover for this menu item.
  base::string16 tooltip_;

  // Width of a menu icon area.
  static int icon_area_width_;

  // X-coordinate of where the label starts.
  static int label_start_;

  // Margins between the right of the item and the label.
  static int item_right_margin_;

  // Preferred height of menu items. Reset every time a menu is run.
  static int pref_menu_height_;

  // Cached dimensions. This is cached as text sizing calculations are quite
  // costly.
  mutable MenuItemDimensions dimensions_;

  // Removed items to be deleted in ChildrenChanged().
  std::vector<View*> removed_items_;

  // Margins in pixels.
  int top_margin_;
  int bottom_margin_;

  // Horizontal icon margins in pixels, which can differ between MenuItems.
  // These values will be set in the layout process.
  mutable int left_icon_margin_;
  mutable int right_icon_margin_;

  // |menu_position_| is the requested position with respect to the bounds.
  // |actual_menu_position_| is used by the controller to cache the
  // position of the menu being shown.
  MenuPosition requested_menu_position_;
  MenuPosition actual_menu_position_;

  // If set to false, the right margin will be removed for menu lines
  // containing other elements.
  bool use_right_margin_;

  // Contains an image for the checkbox or radio icon.
  ImageView* radio_check_image_view_;

  // The submenu indicator arrow icon in case the menu item has a Submenu.
  ImageView* submenu_arrow_image_view_;

  DISALLOW_COPY_AND_ASSIGN(MenuItemView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_ITEM_VIEW_H_
