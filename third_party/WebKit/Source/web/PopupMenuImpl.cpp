// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/PopupMenuImpl.h"

#include "core/HTMLNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/ScopedEventQueue.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLHRElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/ChromeClient.h"
#include "core/page/PagePopup.h"
#include "platform/geometry/IntRect.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMouseEvent.h"
#include "public/web/WebColorChooser.h"

namespace blink {

namespace {

const char* FontWeightToString(FontWeight weight) {
  switch (weight) {
    case kFontWeight100:
      return "100";
    case kFontWeight200:
      return "200";
    case kFontWeight300:
      return "300";
    case kFontWeight400:
      return "400";
    case kFontWeight500:
      return "500";
    case kFontWeight600:
      return "600";
    case kFontWeight700:
      return "700";
    case kFontWeight800:
      return "800";
    case kFontWeight900:
      return "900";
  }
  NOTREACHED();
  return nullptr;
}

// TODO crbug.com/516675 Add stretch to serialization

const char* FontStyleToString(FontStyle style) {
  switch (style) {
    case kFontStyleNormal:
      return "normal";
    case kFontStyleOblique:
      return "oblique";
    case kFontStyleItalic:
      return "italic";
  }
  NOTREACHED();
  return nullptr;
}

const char* TextTransformToString(ETextTransform transform) {
  switch (transform) {
    case ETextTransform::kCapitalize:
      return "capitalize";
    case ETextTransform::kUppercase:
      return "uppercase";
    case ETextTransform::kLowercase:
      return "lowercase";
    case ETextTransform::kNone:
      return "none";
  }
  NOTREACHED();
  return "";
}

}  // anonymous namespace

class PopupMenuCSSFontSelector : public CSSFontSelector,
                                 private CSSFontSelectorClient {
  USING_GARBAGE_COLLECTED_MIXIN(PopupMenuCSSFontSelector);

 public:
  static PopupMenuCSSFontSelector* Create(
      Document* document,
      CSSFontSelector* owner_font_selector) {
    return new PopupMenuCSSFontSelector(document, owner_font_selector);
  }

  ~PopupMenuCSSFontSelector();

  // We don't override willUseFontData() for now because the old PopupListBox
  // only worked with fonts loaded when opening the popup.
  PassRefPtr<FontData> GetFontData(const FontDescription&,
                                   const AtomicString&) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  PopupMenuCSSFontSelector(Document*, CSSFontSelector*);

  void FontsNeedUpdate(CSSFontSelector*) override;

  Member<CSSFontSelector> owner_font_selector_;
};

PopupMenuCSSFontSelector::PopupMenuCSSFontSelector(
    Document* document,
    CSSFontSelector* owner_font_selector)
    : CSSFontSelector(document), owner_font_selector_(owner_font_selector) {
  owner_font_selector_->RegisterForInvalidationCallbacks(this);
}

PopupMenuCSSFontSelector::~PopupMenuCSSFontSelector() {}

PassRefPtr<FontData> PopupMenuCSSFontSelector::GetFontData(
    const FontDescription& description,
    const AtomicString& name) {
  return owner_font_selector_->GetFontData(description, name);
}

void PopupMenuCSSFontSelector::FontsNeedUpdate(CSSFontSelector* font_selector) {
  DispatchInvalidationCallbacks();
}

DEFINE_TRACE(PopupMenuCSSFontSelector) {
  visitor->Trace(owner_font_selector_);
  CSSFontSelector::Trace(visitor);
  CSSFontSelectorClient::Trace(visitor);
}

// ----------------------------------------------------------------

class PopupMenuImpl::ItemIterationContext {
  STACK_ALLOCATED();

 public:
  ItemIterationContext(const ComputedStyle& style, SharedBuffer* buffer)
      : base_style_(style),
        background_color_(
            style.VisitedDependentColor(CSSPropertyBackgroundColor)),
        list_index_(0),
        is_in_group_(false),
        buffer_(buffer) {
    DCHECK(buffer_);
#if OS(LINUX)
    // On other platforms, the <option> background color is the same as the
    // <select> background color. On Linux, that makes the <option>
    // background color very dark, so by default, try to use a lighter
    // background color for <option>s.
    if (LayoutTheme::GetTheme().SystemColor(CSSValueButtonface) ==
        background_color_)
      background_color_ = LayoutTheme::GetTheme().SystemColor(CSSValueMenu);
#endif
  }

  void SerializeBaseStyle() {
    DCHECK(!is_in_group_);
    PagePopupClient::AddString("baseStyle: {", buffer_);
    AddProperty("backgroundColor", background_color_.Serialized(), buffer_);
    AddProperty(
        "color",
        BaseStyle().VisitedDependentColor(CSSPropertyColor).Serialized(),
        buffer_);
    AddProperty("textTransform",
                String(TextTransformToString(BaseStyle().TextTransform())),
                buffer_);
    AddProperty("fontSize", BaseFont().ComputedPixelSize(), buffer_);
    AddProperty("fontStyle", String(FontStyleToString(BaseFont().Style())),
                buffer_);
    AddProperty("fontVariant",
                BaseFont().VariantCaps() == FontDescription::kSmallCaps
                    ? String("small-caps")
                    : String(),
                buffer_);

    PagePopupClient::AddString("fontFamily: [", buffer_);
    for (const FontFamily* f = &BaseFont().Family(); f; f = f->Next()) {
      AddJavaScriptString(f->Family().GetString(), buffer_);
      if (f->Next())
        PagePopupClient::AddString(",", buffer_);
    }
    PagePopupClient::AddString("]", buffer_);
    PagePopupClient::AddString("},\n", buffer_);
  }

  Color BackgroundColor() const {
    return is_in_group_
               ? group_style_->VisitedDependentColor(CSSPropertyBackgroundColor)
               : background_color_;
  }
  // Do not use baseStyle() for background-color, use backgroundColor()
  // instead.
  const ComputedStyle& BaseStyle() {
    return is_in_group_ ? *group_style_ : base_style_;
  }
  const FontDescription& BaseFont() {
    return is_in_group_ ? group_style_->GetFontDescription()
                        : base_style_.GetFontDescription();
  }
  void StartGroupChildren(const ComputedStyle& group_style) {
    DCHECK(!is_in_group_);
    PagePopupClient::AddString("children: [", buffer_);
    is_in_group_ = true;
    group_style_ = &group_style;
  }
  void FinishGroupIfNecessary() {
    if (!is_in_group_)
      return;
    PagePopupClient::AddString("],},\n", buffer_);
    is_in_group_ = false;
    group_style_ = nullptr;
  }

  const ComputedStyle& base_style_;
  Color background_color_;
  const ComputedStyle* group_style_;

  unsigned list_index_;
  bool is_in_group_;
  SharedBuffer* buffer_;
};

// ----------------------------------------------------------------

PopupMenuImpl* PopupMenuImpl::Create(ChromeClient* chrome_client,
                                     HTMLSelectElement& owner_element) {
  return new PopupMenuImpl(chrome_client, owner_element);
}

PopupMenuImpl::PopupMenuImpl(ChromeClient* chrome_client,
                             HTMLSelectElement& owner_element)
    : chrome_client_(chrome_client),
      owner_element_(owner_element),
      popup_(nullptr),
      needs_update_(false) {}

PopupMenuImpl::~PopupMenuImpl() {
  DCHECK(!popup_);
}

DEFINE_TRACE(PopupMenuImpl) {
  visitor->Trace(chrome_client_);
  visitor->Trace(owner_element_);
  PopupMenu::Trace(visitor);
}

void PopupMenuImpl::WriteDocument(SharedBuffer* data) {
  HTMLSelectElement& owner_element = *owner_element_;
  IntRect anchor_rect_in_screen = chrome_client_->ViewportToScreen(
      owner_element.VisibleBoundsInVisualViewport(),
      owner_element.GetDocument().View());

  float scale_factor = chrome_client_->WindowToViewportScalar(1.f);
  PagePopupClient::AddString(
      "<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", data);
  data->Append(Platform::Current()->LoadResource("pickerCommon.css"));
  data->Append(Platform::Current()->LoadResource("listPicker.css"));
  if (!RuntimeEnabledFeatures::forceTallerSelectPopupEnabled())
    PagePopupClient::AddString("@media (any-pointer:coarse) {", data);
  int padding = static_cast<int>(roundf(4 * scale_factor));
  int min_height = static_cast<int>(roundf(24 * scale_factor));
  PagePopupClient::AddString(String::Format("option, optgroup {"
                                            "padding-top: %dpx;"
                                            "}\n"
                                            "option {"
                                            "padding-bottom: %dpx;"
                                            "min-height: %dpx;"
                                            "display: flex;"
                                            "align-items: center;"
                                            "}",
                                            padding, padding, min_height),
                             data);
  if (!RuntimeEnabledFeatures::forceTallerSelectPopupEnabled()) {
    // Closes @media.
    PagePopupClient::AddString("}", data);
  }

  PagePopupClient::AddString(
      "</style></head><body><div id=main>Loading...</div><script>\n"
      "window.dialogArguments = {\n",
      data);
  AddProperty("selectedIndex", owner_element.SelectedListIndex(), data);
  const ComputedStyle* owner_style = owner_element.GetComputedStyle();
  ItemIterationContext context(*owner_style, data);
  context.SerializeBaseStyle();
  PagePopupClient::AddString("children: [\n", data);
  const HeapVector<Member<HTMLElement>>& items = owner_element.GetListItems();
  for (; context.list_index_ < items.size(); ++context.list_index_) {
    Element& child = *items[context.list_index_];
    if (!isHTMLOptGroupElement(child.parentNode()))
      context.FinishGroupIfNecessary();
    if (isHTMLOptionElement(child))
      AddOption(context, toHTMLOptionElement(child));
    else if (isHTMLOptGroupElement(child))
      AddOptGroup(context, toHTMLOptGroupElement(child));
    else if (isHTMLHRElement(child))
      AddSeparator(context, toHTMLHRElement(child));
  }
  context.FinishGroupIfNecessary();
  PagePopupClient::AddString("],\n", data);

  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data);
  AddProperty("zoomFactor", 1, data);
  AddProperty("scaleFactor", scale_factor, data);
  bool is_rtl = !owner_style->IsLeftToRightDirection();
  AddProperty("isRTL", is_rtl, data);
  AddProperty("paddingStart",
              is_rtl ? owner_element.ClientPaddingRight().ToDouble()
                     : owner_element.ClientPaddingLeft().ToDouble(),
              data);
  PagePopupClient::AddString("};\n", data);
  data->Append(Platform::Current()->LoadResource("pickerCommon.js"));
  data->Append(Platform::Current()->LoadResource("listPicker.js"));
  PagePopupClient::AddString("</script></body>\n", data);
}

void PopupMenuImpl::AddElementStyle(ItemIterationContext& context,
                                    HTMLElement& element) {
  const ComputedStyle* style = owner_element_->ItemComputedStyle(element);
  DCHECK(style);
  SharedBuffer* data = context.buffer_;
  // TODO(tkent): We generate unnecessary "style: {\n},\n" even if no
  // additional style.
  PagePopupClient::AddString("style: {\n", data);
  if (style->Visibility() == EVisibility::kHidden)
    AddProperty("visibility", String("hidden"), data);
  if (style->Display() == EDisplay::kNone)
    AddProperty("display", String("none"), data);
  const ComputedStyle& base_style = context.BaseStyle();
  if (base_style.Direction() != style->Direction()) {
    AddProperty(
        "direction",
        String(style->Direction() == TextDirection::kRtl ? "rtl" : "ltr"),
        data);
  }
  if (IsOverride(style->GetUnicodeBidi()))
    AddProperty("unicodeBidi", String("bidi-override"), data);
  Color foreground_color = style->VisitedDependentColor(CSSPropertyColor);
  if (base_style.VisitedDependentColor(CSSPropertyColor) != foreground_color)
    AddProperty("color", foreground_color.Serialized(), data);
  Color background_color =
      style->VisitedDependentColor(CSSPropertyBackgroundColor);
  if (context.BackgroundColor() != background_color &&
      background_color != Color::kTransparent)
    AddProperty("backgroundColor", background_color.Serialized(), data);
  const FontDescription& base_font = context.BaseFont();
  const FontDescription& font_description =
      style->GetFont().GetFontDescription();
  if (base_font.ComputedPixelSize() != font_description.ComputedPixelSize()) {
    // We don't use FontDescription::specifiedSize() because this element
    // might have its own zoom level.
    AddProperty("fontSize", font_description.ComputedPixelSize(), data);
  }
  // Our UA stylesheet has font-weight:normal for OPTION.
  if (kFontWeightNormal != font_description.Weight())
    AddProperty("fontWeight",
                String(FontWeightToString(font_description.Weight())), data);
  if (base_font.Family() != font_description.Family()) {
    PagePopupClient::AddString("fontFamily: [\n", data);
    for (const FontFamily* f = &font_description.Family(); f; f = f->Next()) {
      AddJavaScriptString(f->Family().GetString(), data);
      if (f->Next())
        PagePopupClient::AddString(",\n", data);
    }
    PagePopupClient::AddString("],\n", data);
  }
  if (base_font.Style() != font_description.Style())
    AddProperty("fontStyle",
                String(FontStyleToString(font_description.Style())), data);

  if (base_font.VariantCaps() != font_description.VariantCaps() &&
      font_description.VariantCaps() == FontDescription::kSmallCaps)
    AddProperty("fontVariant", String("small-caps"), data);

  if (base_style.TextTransform() != style->TextTransform())
    AddProperty("textTransform",
                String(TextTransformToString(style->TextTransform())), data);

  PagePopupClient::AddString("},\n", data);
}

void PopupMenuImpl::AddOption(ItemIterationContext& context,
                              HTMLOptionElement& element) {
  SharedBuffer* data = context.buffer_;
  PagePopupClient::AddString("{", data);
  AddProperty("label", element.DisplayLabel(), data);
  AddProperty("value", context.list_index_, data);
  if (!element.title().IsEmpty())
    AddProperty("title", element.title(), data);
  const AtomicString& aria_label =
      element.FastGetAttribute(HTMLNames::aria_labelAttr);
  if (!aria_label.IsEmpty())
    AddProperty("ariaLabel", aria_label, data);
  if (element.IsDisabledFormControl())
    AddProperty("disabled", true, data);
  AddElementStyle(context, element);
  PagePopupClient::AddString("},", data);
}

void PopupMenuImpl::AddOptGroup(ItemIterationContext& context,
                                HTMLOptGroupElement& element) {
  SharedBuffer* data = context.buffer_;
  PagePopupClient::AddString("{\n", data);
  PagePopupClient::AddString("type: \"optgroup\",\n", data);
  AddProperty("label", element.GroupLabelText(), data);
  AddProperty("title", element.title(), data);
  AddProperty("ariaLabel", element.FastGetAttribute(HTMLNames::aria_labelAttr),
              data);
  AddProperty("disabled", element.IsDisabledFormControl(), data);
  AddElementStyle(context, element);
  context.StartGroupChildren(*owner_element_->ItemComputedStyle(element));
  // We should call ItemIterationContext::finishGroupIfNecessary() later.
}

void PopupMenuImpl::AddSeparator(ItemIterationContext& context,
                                 HTMLHRElement& element) {
  SharedBuffer* data = context.buffer_;
  PagePopupClient::AddString("{\n", data);
  PagePopupClient::AddString("type: \"separator\",\n", data);
  AddProperty("title", element.title(), data);
  AddProperty("ariaLabel", element.FastGetAttribute(HTMLNames::aria_labelAttr),
              data);
  AddProperty("disabled", element.IsDisabledFormControl(), data);
  AddElementStyle(context, element);
  PagePopupClient::AddString("},\n", data);
}

void PopupMenuImpl::SelectFontsFromOwnerDocument(Document& document) {
  Document& owner_document = OwnerElement().GetDocument();
  document.GetStyleEngine().SetFontSelector(PopupMenuCSSFontSelector::Create(
      &document, owner_document.GetStyleEngine().FontSelector()));
}

void PopupMenuImpl::SetValueAndClosePopup(int num_value,
                                          const String& string_value) {
  DCHECK(popup_);
  DCHECK(owner_element_);
  if (!string_value.IsEmpty()) {
    bool success;
    int list_index = string_value.ToInt(&success);
    DCHECK(success);

    EventQueueScope scope;
    owner_element_->SelectOptionByPopup(list_index);
    if (popup_)
      chrome_client_->ClosePagePopup(popup_);
    // 'change' event is dispatched here.  For compatbility with
    // Angular 1.2, we need to dispatch a change event before
    // mouseup/click events.
  } else {
    if (popup_)
      chrome_client_->ClosePagePopup(popup_);
  }
  // We dispatch events on the owner element to match the legacy behavior.
  // Other browsers dispatch click events before and after showing the popup.
  if (owner_element_) {
    // TODO(dtapuska): Why is this event positionless?
    WebMouseEvent event;
    event.SetFrameScale(1);
    Element* owner = &OwnerElement();
    owner->DispatchMouseEvent(event, EventTypeNames::mouseup);
    owner->DispatchMouseEvent(event, EventTypeNames::click);
  }
}

void PopupMenuImpl::SetValue(const String& value) {
  DCHECK(owner_element_);
  bool success;
  int list_index = value.ToInt(&success);
  DCHECK(success);
  owner_element_->ProvisionalSelectionChanged(list_index);
}

void PopupMenuImpl::DidClosePopup() {
  // Clearing m_popup first to prevent from trying to close the popup again.
  popup_ = nullptr;
  if (owner_element_)
    owner_element_->PopupDidHide();
}

Element& PopupMenuImpl::OwnerElement() {
  return *owner_element_;
}

Locale& PopupMenuImpl::GetLocale() {
  return Locale::DefaultLocale();
}

void PopupMenuImpl::ClosePopup() {
  if (popup_)
    chrome_client_->ClosePagePopup(popup_);
  if (owner_element_)
    owner_element_->PopupDidCancel();
}

void PopupMenuImpl::Dispose() {
  if (popup_)
    chrome_client_->ClosePagePopup(popup_);
}

void PopupMenuImpl::Show() {
  DCHECK(!popup_);
  popup_ = chrome_client_->OpenPagePopup(this);
}

void PopupMenuImpl::Hide() {
  ClosePopup();
}

void PopupMenuImpl::UpdateFromElement(UpdateReason) {
  if (needs_update_)
    return;
  needs_update_ = true;
  TaskRunnerHelper::Get(TaskType::kUserInteraction,
                        &OwnerElement().GetDocument())
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&PopupMenuImpl::Update, WrapPersistent(this)));
}

void PopupMenuImpl::Update() {
  if (!popup_ || !owner_element_)
    return;
  OwnerElement().GetDocument().UpdateStyleAndLayoutTree();
  // disconnectClient() might have been called.
  if (!owner_element_)
    return;
  needs_update_ = false;

  if (!OwnerElement()
           .GetDocument()
           .GetFrame()
           ->View()
           ->VisibleContentRect()
           .Intersects(OwnerElement().PixelSnappedBoundingBox())) {
    Hide();
    return;
  }

  RefPtr<SharedBuffer> data = SharedBuffer::Create();
  PagePopupClient::AddString("window.updateData = {\n", data.Get());
  PagePopupClient::AddString("type: \"update\",\n", data.Get());
  ItemIterationContext context(*owner_element_->GetComputedStyle(), data.Get());
  context.SerializeBaseStyle();
  PagePopupClient::AddString("children: [", data.Get());
  const HeapVector<Member<HTMLElement>>& items = owner_element_->GetListItems();
  for (; context.list_index_ < items.size(); ++context.list_index_) {
    Element& child = *items[context.list_index_];
    if (!isHTMLOptGroupElement(child.parentNode()))
      context.FinishGroupIfNecessary();
    if (isHTMLOptionElement(child))
      AddOption(context, toHTMLOptionElement(child));
    else if (isHTMLOptGroupElement(child))
      AddOptGroup(context, toHTMLOptGroupElement(child));
    else if (isHTMLHRElement(child))
      AddSeparator(context, toHTMLHRElement(child));
  }
  context.FinishGroupIfNecessary();
  PagePopupClient::AddString("],\n", data.Get());
  IntRect anchor_rect_in_screen = chrome_client_->ViewportToScreen(
      owner_element_->VisibleBoundsInVisualViewport(),
      OwnerElement().GetDocument().View());
  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data.Get());
  PagePopupClient::AddString("}\n", data.Get());
  popup_->PostMessage(String::FromUTF8(data->Data(), data->size()));
}

void PopupMenuImpl::DisconnectClient() {
  owner_element_ = nullptr;
  // Cannot be done during finalization, so instead done when the
  // layout object is destroyed and disconnected.
  Dispose();
}

}  // namespace blink
