// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LABEL_H_
#define UI_VIEWS_CONTROLS_LABEL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/selection_controller_delegate.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/word_lookup_client.h"

namespace views {
class LabelSelectionTest;
class MenuRunner;
class SelectionController;

// A view subclass that can display a string.
class VIEWS_EXPORT Label : public View,
                           public ContextMenuController,
                           public WordLookupClient,
                           public SelectionControllerDelegate,
                           public ui::SimpleMenuModel::Delegate {
 public:
  METADATA_HEADER(Label);

  enum MenuCommands {
    kCopy = 1,
    kSelectAll,
    kLastCommandId = kSelectAll,
  };

  // Helper to construct a Label that doesn't use the views typography spec.
  // Using this causes Label to obtain colors from ui::NativeTheme and line
  // spacing from gfx::FontList::GetHeight().
  // TODO(tapted): Audit users of this class when MD is default. Then add
  // foreground/background colors, line spacing and everything else that
  // views::TextContext abstracts away so the separate setters can be removed.
  struct CustomFont {
    // TODO(tapted): Change this to a size delta and font weight since that's
    // typically all the callers really care about, and would allow Label to
    // guarantee caching of the FontList in ResourceBundle.
    const gfx::FontList& font_list;
  };

  // Create Labels with style::CONTEXT_CONTROL_LABEL and style::STYLE_PRIMARY.
  // TODO(tapted): Remove these. Callers must specify a context or use the
  // constructor taking a CustomFont.
  Label();
  explicit Label(const base::string16& text);

  // Construct a Label in the given |text_context|. The |text_style| can change
  // later, so provide a default. The |text_context| is fixed.
  // By default text directionality will be derived from the label text, however
  // it can be overriden with |directionality_mode|.
  Label(const base::string16& text,
        int text_context,
        int text_style = style::STYLE_PRIMARY,
        gfx::DirectionalityMode directionality_mode =
            gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);

  // Construct a Label with the given |font| description.
  Label(const base::string16& text, const CustomFont& font);

  ~Label() override;

  static const gfx::FontList& GetDefaultFontList();

  // Gets or sets the fonts used by this label.
  const gfx::FontList& font_list() const { return full_text_->font_list(); }

  // TODO(tapted): Replace this with a private method, e.g., OnFontChanged().
  virtual void SetFontList(const gfx::FontList& font_list);

  // Get or set the label text.
  const base::string16& GetText() const;
  virtual void SetText(const base::string16& text);

  // Where the label appears in the UI. Passed in from the constructor. This is
  // a value from views::style::TextContext or an enum that extends it.
  int GetTextContext() const;

  // Enables or disables auto-color-readability (enabled by default).  If this
  // is enabled, then calls to set any foreground or background color will
  // trigger an automatic mapper that uses color_utils::BlendForMinContrast()
  // to ensure that the foreground colors are readable over the background
  // color.
  bool GetAutoColorReadabilityEnabled() const;
  void SetAutoColorReadabilityEnabled(bool enabled);

  // Gets/Sets the color.  This will automatically force the color to be
  // readable over the current background color, if auto color readability is
  // enabled.
  SkColor GetEnabledColor() const;
  virtual void SetEnabledColor(SkColor color);

  // Gets/Sets the background color. This won't be explicitly drawn, but the
  // label will force the text color to be readable over it.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);

  // Gets/Sets the selection text color. This will automatically force the color
  // to be readable over the selection background color, if auto color
  // readability is enabled. Initialized with system default.
  SkColor GetSelectionTextColor() const;
  void SetSelectionTextColor(SkColor color);

  // Gets/Sets the selection background color. Initialized with system default.
  SkColor GetSelectionBackgroundColor() const;
  void SetSelectionBackgroundColor(SkColor color);

  // Get/Set drop shadows underneath the text.
  const gfx::ShadowValues& GetShadows() const;
  void SetShadows(const gfx::ShadowValues& shadows);

  // Gets/Sets whether subpixel rendering is used; the default is true, but this
  // feature also requires an opaque background color.
  // TODO(mukai): rename this as SetSubpixelRenderingSuppressed() to keep the
  // consistency with RenderText field name.
  bool GetSubpixelRenderingEnabled() const;
  void SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled);

  // Gets/Sets the horizontal alignment; the argument value is mirrored in RTL
  // UI.
  gfx::HorizontalAlignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Gets/Sets the vertical alignment. Affects how whitespace is distributed
  // vertically around the label text, or if the label is not tall enough to
  // render all of the text, what gets cut off. ALIGN_MIDDLE is default and is
  // strongly suggested for single-line labels because it produces a consistent
  // baseline even when rendering with mixed fonts.
  gfx::VerticalAlignment GetVerticalAlignment() const;
  void SetVerticalAlignment(gfx::VerticalAlignment alignment);

  // Get or set the distance in pixels between baselines of multi-line text.
  // Default is 0, indicating the distance between lines should be the standard
  // one for the label's text, font list, and platform.
  int GetLineHeight() const;
  void SetLineHeight(int height);

  // Get or set if the label text can wrap on multiple lines; default is false.
  bool GetMultiLine() const;
  void SetMultiLine(bool multi_line);

  // If multi-line, a non-zero value will cap the number of lines rendered, and
  // elide the rest (currently only ELIDE_TAIL supported). See gfx::RenderText.
  int GetMaxLines() const;
  void SetMaxLines(int max_lines);

  // Returns the number of lines required to render all text. The actual number
  // of rendered lines might be limited by |max_lines_| which elides the rest.
  size_t GetRequiredLines() const;

  // Get or set if the label text should be obscured before rendering (e.g.
  // should "Password!" display as "*********"); default is false.
  bool GetObscured() const;
  void SetObscured(bool obscured);

  // Returns true if some portion of the text is not displayed, either because
  // of eliding or clipping.
  bool IsDisplayTextTruncated() const;

  // Gets/Sets whether multi-line text can wrap mid-word; the default is false.
  // TODO(mukai): allow specifying WordWrapBehavior.
  bool GetAllowCharacterBreak() const;
  void SetAllowCharacterBreak(bool allow_character_break);

  // Gets/Sets the eliding or fading behavior, applied as necessary. The default
  // is to elide at the end. Eliding is not well-supported for multi-line
  // labels.
  gfx::ElideBehavior GetElideBehavior() const;
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // Gets/Sets the tooltip text.  Default behavior for a label (single-line) is
  // to show the full text if it is wider than its bounds.  Calling this
  // overrides the default behavior and lets you set a custom tooltip.  To
  // revert to default behavior, call this with an empty string.
  base::string16 GetTooltipText() const;
  void SetTooltipText(const base::string16& tooltip_text);

  // Get or set whether this label can act as a tooltip handler; the default is
  // true.  Set to false whenever an ancestor view should handle tooltips
  // instead.
  bool GetHandlesTooltips() const;
  void SetHandlesTooltips(bool enabled);

  // Resizes the label so its width is set to the fixed width and its height
  // deduced accordingly. Even if all widths of the lines are shorter than
  // |fixed_width|, the given value is applied to the element's width.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |fixed_width| is the fixed width that will be used (longer lines will be
  // wrapped).  If 0, no fixed width is enforced.
  void SizeToFit(int fixed_width);

  // Like SizeToFit, but uses a smaller width if possible.
  int GetMaximumWidth() const;
  void SetMaximumWidth(int max_width);

  // Gets/Sets whether the preferred size is empty when the label is not
  // visible.
  bool GetCollapseWhenHidden() const;
  void SetCollapseWhenHidden(bool value);

  // Get the text as displayed to the user, respecting the obscured flag.
  base::string16 GetDisplayTextForTesting();

  // Get the text direction, as displayed to the user.
  base::i18n::TextDirection GetTextDirectionForTesting();

  // Returns true if the label can be made selectable. For example, links do not
  // support text selection.
  // Subclasses should override this function in case they want to selectively
  // support text selection. If a subclass stops supporting text selection, it
  // should call SetSelectable(false).
  virtual bool IsSelectionSupported() const;

  // Returns true if the label is selectable. Default is false.
  bool GetSelectable() const;

  // Sets whether the label is selectable. False is returned if the call fails,
  // i.e. when selection is not supported but |selectable| is true. For example,
  // obscured labels do not support text selection.
  bool SetSelectable(bool selectable);

  // Returns true if the label has a selection.
  bool HasSelection() const;

  // Selects the entire text. NO-OP if the label is not selectable.
  void SelectAll();

  // Clears any active selection.
  void ClearSelection();

  // Selects the given text range. NO-OP if the label is not selectable or the
  // |range| endpoints don't lie on grapheme boundaries.
  void SelectRange(const gfx::Range& range);

  views::PropertyChangedSubscription AddTextChangedCallback(
      views::PropertyChangedCallback callback);

  // View:
  int GetBaseline() const override;
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  int GetHeightForWidth(int w) const override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool CanProcessEventsWithinSubtree() const override;
  WordLookupClient* GetWordLookupClient() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  void OnHandlePropertyChangeEffects(PropertyEffects property_effects) override;

 protected:
  // Create a single RenderText instance to actually be painted.
  virtual std::unique_ptr<gfx::RenderText> CreateRenderText() const;

  // Returns the preferred size and position of the text in local coordinates,
  // which may exceed the local bounds of the label.
  gfx::Rect GetTextBounds() const;

  void PaintText(gfx::Canvas* canvas);

  // View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnThemeChanged() override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelTest, ResetRenderTextData);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, MultilineSupportedRenderText);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, TextChangeWithoutLayout);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, EmptyLabel);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, FocusBounds);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, MultiLineSizingWithElide);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, IsDisplayTextTruncated);
  friend class LabelSelectionTest;

  // ContextMenuController overrides:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // WordLookupClient overrides:
  bool GetWordLookupDataAtPoint(const gfx::Point& point,
                                gfx::DecoratedText* decorated_word,
                                gfx::Point* baseline_point) override;

  bool GetWordLookupDataFromSelection(gfx::DecoratedText* decorated_text,
                                      gfx::Point* baseline_point) override;

  // SelectionControllerDelegate overrides:
  gfx::RenderText* GetRenderTextForSelectionController() override;
  bool IsReadOnly() const override;
  bool SupportsDrag() const override;
  bool HasTextBeingDragged() const override;
  void SetTextBeingDragged(bool value) override;
  int GetViewHeight() const override;
  int GetViewWidth() const override;
  int GetDragSelectionDelay() const override;
  void OnBeforePointerAction() override;
  void OnAfterPointerAction(bool text_changed, bool selection_changed) override;
  bool PasteSelectionClipboard() override;
  void UpdateSelectionClipboard() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  const gfx::RenderText* GetRenderTextForSelectionController() const;

  void Init(const base::string16& text,
            const gfx::FontList& font_list,
            gfx::DirectionalityMode directionality_mode);

  void ResetLayout();

  // Set up |display_text_| to actually be painted.
  void MaybeBuildDisplayText() const;

  // Get the text size for the current layout.
  gfx::Size GetTextSize() const;

  // Returns the appropriate foreground color to use given the proposed
  // |foreground| and |background| colors.
  SkColor GetForegroundColor(SkColor foreground, SkColor background) const;

  // Updates text and selection colors from requested colors.
  void RecalculateColors();

  // Applies the foreground color to |display_text_|.
  void ApplyTextColors() const;

  // Updates any colors that have not been explicitly set from the theme.
  void UpdateColorsFromTheme();

  bool ShouldShowDefaultTooltip() const;

  // Clears |display_text_| and updates |stored_selection_range_|.
  void ClearDisplayText() const;

  // Returns the currently selected text.
  base::string16 GetSelectedText() const;

  // Updates the clipboard with the currently selected text.
  void CopyToClipboard();

  // Builds |context_menu_contents_|.
  void BuildContextMenuContents();

  const int text_context_;
  const int text_style_;

  // An un-elided and single-line RenderText object used for preferred sizing.
  std::unique_ptr<gfx::RenderText> full_text_;

  // The RenderText instance used for drawing.
  mutable std::unique_ptr<gfx::RenderText> display_text_;

  // Persists the current selection range between the calls to
  // ClearDisplayText() and MaybeBuildDisplayText(). Holds an InvalidRange when
  // not in use.
  mutable gfx::Range stored_selection_range_ = gfx::Range::InvalidRange();

  SkColor requested_enabled_color_ = gfx::kPlaceholderColor;
  SkColor actual_enabled_color_ = gfx::kPlaceholderColor;
  SkColor background_color_ = gfx::kPlaceholderColor;
  SkColor requested_selection_text_color_ = gfx::kPlaceholderColor;
  SkColor actual_selection_text_color_ = gfx::kPlaceholderColor;
  SkColor selection_background_color_ = gfx::kPlaceholderColor;

  // Set to true once the corresponding setter is invoked.
  bool enabled_color_set_ = false;
  bool background_color_set_ = false;
  bool selection_text_color_set_ = false;
  bool selection_background_color_set_ = false;

  gfx::ElideBehavior elide_behavior_ = gfx::ELIDE_TAIL;

  bool subpixel_rendering_enabled_ = true;
  bool auto_color_readability_enabled_ = true;
  // TODO(mukai): remove |multi_line_| when all RenderText can render multiline.
  bool multi_line_ = false;
  int max_lines_ = 0;
  base::string16 tooltip_text_;
  bool handles_tooltips_ = true;
  // Whether to collapse the label when it's not visible.
  bool collapse_when_hidden_ = false;
  int fixed_width_ = 0;
  int max_width_ = 0;

  std::unique_ptr<SelectionController> selection_controller_;

  // Context menu related members.
  ui::SimpleMenuModel context_menu_contents_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(Label);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LABEL_H_
