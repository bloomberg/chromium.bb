// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_enum_util.h"

namespace ui {

const char* ToString(ax::mojom::Event event) {
  switch (event) {
    case ax::mojom::Event::kNone:
      return "none";
    case ax::mojom::Event::kActiveDescendantChanged:
      return "activedescendantchanged";
    case ax::mojom::Event::kAlert:
      return "alert";
    case ax::mojom::Event::kAriaAttributeChanged:
      return "ariaAttributeChanged";
    case ax::mojom::Event::kAutocorrectionOccured:
      return "autocorrectionOccured";
    case ax::mojom::Event::kBlur:
      return "blur";
    case ax::mojom::Event::kCheckedStateChanged:
      return "checkedStateChanged";
    case ax::mojom::Event::kChildrenChanged:
      return "childrenChanged";
    case ax::mojom::Event::kClicked:
      return "clicked";
    case ax::mojom::Event::kDocumentSelectionChanged:
      return "documentSelectionChanged";
    case ax::mojom::Event::kExpandedChanged:
      return "expandedChanged";
    case ax::mojom::Event::kFocus:
      return "focus";
    case ax::mojom::Event::kFocusContext:
      return "focusContext";
    case ax::mojom::Event::kHide:
      return "hide";
    case ax::mojom::Event::kHitTestResult:
      return "hitTestResult";
    case ax::mojom::Event::kHover:
      return "hover";
    case ax::mojom::Event::kImageFrameUpdated:
      return "imageFrameUpdated";
    case ax::mojom::Event::kInvalidStatusChanged:
      return "invalidStatusChanged";
    case ax::mojom::Event::kLayoutComplete:
      return "layoutComplete";
    case ax::mojom::Event::kLiveRegionCreated:
      return "liveRegionCreated";
    case ax::mojom::Event::kLiveRegionChanged:
      return "liveRegionChanged";
    case ax::mojom::Event::kLoadComplete:
      return "loadComplete";
    case ax::mojom::Event::kLocationChanged:
      return "locationChanged";
    case ax::mojom::Event::kMediaStartedPlaying:
      return "mediaStartedPlaying";
    case ax::mojom::Event::kMediaStoppedPlaying:
      return "mediaStoppedPlaying";
    case ax::mojom::Event::kMenuEnd:
      return "menuEnd";
    case ax::mojom::Event::kMenuListItemSelected:
      return "menuListItemSelected";
    case ax::mojom::Event::kMenuListValueChanged:
      return "menuListValueChanged";
    case ax::mojom::Event::kMenuPopupEnd:
      return "menuPopupEnd";
    case ax::mojom::Event::kMenuPopupStart:
      return "menuPopupStart";
    case ax::mojom::Event::kMenuStart:
      return "menuStart";
    case ax::mojom::Event::kMouseCanceled:
      return "mouseCanceled";
    case ax::mojom::Event::kMouseDragged:
      return "mouseDragged";
    case ax::mojom::Event::kMouseMoved:
      return "mouseMoved";
    case ax::mojom::Event::kMousePressed:
      return "mousePressed";
    case ax::mojom::Event::kMouseReleased:
      return "mouseReleased";
    case ax::mojom::Event::kRowCollapsed:
      return "rowCollapsed";
    case ax::mojom::Event::kRowCountChanged:
      return "rowCountChanged";
    case ax::mojom::Event::kRowExpanded:
      return "rowExpanded";
    case ax::mojom::Event::kScrollPositionChanged:
      return "scrollPositionChanged";
    case ax::mojom::Event::kScrolledToAnchor:
      return "scrolledToAnchor";
    case ax::mojom::Event::kSelectedChildrenChanged:
      return "selectedChildrenChanged";
    case ax::mojom::Event::kSelection:
      return "selection";
    case ax::mojom::Event::kSelectionAdd:
      return "selectionAdd";
    case ax::mojom::Event::kSelectionRemove:
      return "selectionRemove";
    case ax::mojom::Event::kShow:
      return "show";
    case ax::mojom::Event::kStateChanged:
      return "stateChanged";
    case ax::mojom::Event::kTextChanged:
      return "textChanged";
    case ax::mojom::Event::kTextSelectionChanged:
      return "textSelectionChanged";
    case ax::mojom::Event::kTreeChanged:
      return "treeChanged";
    case ax::mojom::Event::kValueChanged:
      return "valueChanged";
  }

  return "";
}

ax::mojom::Event ParseEvent(const char* event) {
  if (0 == strcmp(event, "none"))
    return ax::mojom::Event::kNone;
  if (0 == strcmp(event, "activedescendantchanged"))
    return ax::mojom::Event::kActiveDescendantChanged;
  if (0 == strcmp(event, "alert"))
    return ax::mojom::Event::kAlert;
  if (0 == strcmp(event, "ariaAttributeChanged"))
    return ax::mojom::Event::kAriaAttributeChanged;
  if (0 == strcmp(event, "autocorrectionOccured"))
    return ax::mojom::Event::kAutocorrectionOccured;
  if (0 == strcmp(event, "blur"))
    return ax::mojom::Event::kBlur;
  if (0 == strcmp(event, "checkedStateChanged"))
    return ax::mojom::Event::kCheckedStateChanged;
  if (0 == strcmp(event, "childrenChanged"))
    return ax::mojom::Event::kChildrenChanged;
  if (0 == strcmp(event, "clicked"))
    return ax::mojom::Event::kClicked;
  if (0 == strcmp(event, "documentSelectionChanged"))
    return ax::mojom::Event::kDocumentSelectionChanged;
  if (0 == strcmp(event, "expandedChanged"))
    return ax::mojom::Event::kExpandedChanged;
  if (0 == strcmp(event, "focus"))
    return ax::mojom::Event::kFocus;
  if (0 == strcmp(event, "hide"))
    return ax::mojom::Event::kHide;
  if (0 == strcmp(event, "hitTestResult"))
    return ax::mojom::Event::kHitTestResult;
  if (0 == strcmp(event, "hover"))
    return ax::mojom::Event::kHover;
  if (0 == strcmp(event, "imageFrameUpdated"))
    return ax::mojom::Event::kImageFrameUpdated;
  if (0 == strcmp(event, "invalidStatusChanged"))
    return ax::mojom::Event::kInvalidStatusChanged;
  if (0 == strcmp(event, "layoutComplete"))
    return ax::mojom::Event::kLayoutComplete;
  if (0 == strcmp(event, "liveRegionCreated"))
    return ax::mojom::Event::kLiveRegionCreated;
  if (0 == strcmp(event, "liveRegionChanged"))
    return ax::mojom::Event::kLiveRegionChanged;
  if (0 == strcmp(event, "loadComplete"))
    return ax::mojom::Event::kLoadComplete;
  if (0 == strcmp(event, "locationChanged"))
    return ax::mojom::Event::kLocationChanged;
  if (0 == strcmp(event, "mediaStartedPlaying"))
    return ax::mojom::Event::kMediaStartedPlaying;
  if (0 == strcmp(event, "mediaStoppedPlaying"))
    return ax::mojom::Event::kMediaStoppedPlaying;
  if (0 == strcmp(event, "menuEnd"))
    return ax::mojom::Event::kMenuEnd;
  if (0 == strcmp(event, "menuListItemSelected"))
    return ax::mojom::Event::kMenuListItemSelected;
  if (0 == strcmp(event, "menuListValueChanged"))
    return ax::mojom::Event::kMenuListValueChanged;
  if (0 == strcmp(event, "menuPopupEnd"))
    return ax::mojom::Event::kMenuPopupEnd;
  if (0 == strcmp(event, "menuPopupStart"))
    return ax::mojom::Event::kMenuPopupStart;
  if (0 == strcmp(event, "menuStart"))
    return ax::mojom::Event::kMenuStart;
  if (0 == strcmp(event, "mouseCanceled"))
    return ax::mojom::Event::kMouseCanceled;
  if (0 == strcmp(event, "mouseDragged"))
    return ax::mojom::Event::kMouseDragged;
  if (0 == strcmp(event, "mouseMoved"))
    return ax::mojom::Event::kMouseMoved;
  if (0 == strcmp(event, "mousePressed"))
    return ax::mojom::Event::kMousePressed;
  if (0 == strcmp(event, "mouseReleased"))
    return ax::mojom::Event::kMouseReleased;
  if (0 == strcmp(event, "rowCollapsed"))
    return ax::mojom::Event::kRowCollapsed;
  if (0 == strcmp(event, "rowCountChanged"))
    return ax::mojom::Event::kRowCountChanged;
  if (0 == strcmp(event, "rowExpanded"))
    return ax::mojom::Event::kRowExpanded;
  if (0 == strcmp(event, "scrollPositionChanged"))
    return ax::mojom::Event::kScrollPositionChanged;
  if (0 == strcmp(event, "scrolledToAnchor"))
    return ax::mojom::Event::kScrolledToAnchor;
  if (0 == strcmp(event, "selectedChildrenChanged"))
    return ax::mojom::Event::kSelectedChildrenChanged;
  if (0 == strcmp(event, "selection"))
    return ax::mojom::Event::kSelection;
  if (0 == strcmp(event, "selectionAdd"))
    return ax::mojom::Event::kSelectionAdd;
  if (0 == strcmp(event, "selectionRemove"))
    return ax::mojom::Event::kSelectionRemove;
  if (0 == strcmp(event, "show"))
    return ax::mojom::Event::kShow;
  if (0 == strcmp(event, "textChanged"))
    return ax::mojom::Event::kTextChanged;
  if (0 == strcmp(event, "textSelectionChanged"))
    return ax::mojom::Event::kTextSelectionChanged;
  if (0 == strcmp(event, "treeChanged"))
    return ax::mojom::Event::kTreeChanged;
  if (0 == strcmp(event, "valueChanged"))
    return ax::mojom::Event::kValueChanged;
  return ax::mojom::Event::kNone;
}

const char* ToString(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kNone:
      return "none";
    case ax::mojom::Role::kAbbr:
      return "abbr";
    case ax::mojom::Role::kAlertDialog:
      return "alertDialog";
    case ax::mojom::Role::kAlert:
      return "alert";
    case ax::mojom::Role::kAnchor:
      return "anchor";
    case ax::mojom::Role::kAnnotation:
      return "annotation";
    case ax::mojom::Role::kApplication:
      return "application";
    case ax::mojom::Role::kArticle:
      return "article";
    case ax::mojom::Role::kAudio:
      return "audio";
    case ax::mojom::Role::kBanner:
      return "banner";
    case ax::mojom::Role::kBlockquote:
      return "blockquote";
    case ax::mojom::Role::kButton:
      return "button";
    case ax::mojom::Role::kCanvas:
      return "canvas";
    case ax::mojom::Role::kCaption:
      return "caption";
    case ax::mojom::Role::kCaret:
      return "caret";
    case ax::mojom::Role::kCell:
      return "cell";
    case ax::mojom::Role::kCheckBox:
      return "checkBox";
    case ax::mojom::Role::kClient:
      return "client";
    case ax::mojom::Role::kColorWell:
      return "colorWell";
    case ax::mojom::Role::kColumnHeader:
      return "columnHeader";
    case ax::mojom::Role::kColumn:
      return "column";
    case ax::mojom::Role::kComboBoxGrouping:
      return "comboBoxGrouping";
    case ax::mojom::Role::kComboBoxMenuButton:
      return "comboBoxMenuButton";
    case ax::mojom::Role::kComplementary:
      return "complementary";
    case ax::mojom::Role::kContentInfo:
      return "contentInfo";
    case ax::mojom::Role::kDate:
      return "date";
    case ax::mojom::Role::kDateTime:
      return "dateTime";
    case ax::mojom::Role::kDefinition:
      return "definition";
    case ax::mojom::Role::kDescriptionListDetail:
      return "descriptionListDetail";
    case ax::mojom::Role::kDescriptionList:
      return "descriptionList";
    case ax::mojom::Role::kDescriptionListTerm:
      return "descriptionListTerm";
    case ax::mojom::Role::kDesktop:
      return "desktop";
    case ax::mojom::Role::kDetails:
      return "details";
    case ax::mojom::Role::kDialog:
      return "dialog";
    case ax::mojom::Role::kDirectory:
      return "directory";
    case ax::mojom::Role::kDisclosureTriangle:
      return "disclosureTriangle";
    case ax::mojom::Role::kDocument:
      return "document";
    case ax::mojom::Role::kEmbeddedObject:
      return "embeddedObject";
    case ax::mojom::Role::kFeed:
      return "feed";
    case ax::mojom::Role::kFigcaption:
      return "figcaption";
    case ax::mojom::Role::kFigure:
      return "figure";
    case ax::mojom::Role::kFooter:
      return "footer";
    case ax::mojom::Role::kForm:
      return "form";
    case ax::mojom::Role::kGenericContainer:
      return "genericContainer";
    case ax::mojom::Role::kGrid:
      return "grid";
    case ax::mojom::Role::kGroup:
      return "group";
    case ax::mojom::Role::kHeading:
      return "heading";
    case ax::mojom::Role::kIframe:
      return "iframe";
    case ax::mojom::Role::kIframePresentational:
      return "iframePresentational";
    case ax::mojom::Role::kIgnored:
      return "ignored";
    case ax::mojom::Role::kImageMap:
      return "imageMap";
    case ax::mojom::Role::kImage:
      return "image";
    case ax::mojom::Role::kInlineTextBox:
      return "inlineTextBox";
    case ax::mojom::Role::kInputTime:
      return "inputTime";
    case ax::mojom::Role::kLabelText:
      return "labelText";
    case ax::mojom::Role::kLayoutTable:
      return "layoutTable";
    case ax::mojom::Role::kLayoutTableCell:
      return "layoutTableCell";
    case ax::mojom::Role::kLayoutTableColumn:
      return "layoutTableColumn";
    case ax::mojom::Role::kLayoutTableRow:
      return "layoutTableRow";
    case ax::mojom::Role::kLegend:
      return "legend";
    case ax::mojom::Role::kLineBreak:
      return "lineBreak";
    case ax::mojom::Role::kLink:
      return "link";
    case ax::mojom::Role::kListBoxOption:
      return "listBoxOption";
    case ax::mojom::Role::kListBox:
      return "listBox";
    case ax::mojom::Role::kListItem:
      return "listItem";
    case ax::mojom::Role::kListMarker:
      return "listMarker";
    case ax::mojom::Role::kList:
      return "list";
    case ax::mojom::Role::kLocationBar:
      return "locationBar";
    case ax::mojom::Role::kLog:
      return "log";
    case ax::mojom::Role::kMain:
      return "main";
    case ax::mojom::Role::kMark:
      return "mark";
    case ax::mojom::Role::kMarquee:
      return "marquee";
    case ax::mojom::Role::kMath:
      return "math";
    case ax::mojom::Role::kMenu:
      return "menu";
    case ax::mojom::Role::kMenuBar:
      return "menuBar";
    case ax::mojom::Role::kMenuButton:
      return "menuButton";
    case ax::mojom::Role::kMenuItem:
      return "menuItem";
    case ax::mojom::Role::kMenuItemCheckBox:
      return "menuItemCheckBox";
    case ax::mojom::Role::kMenuItemRadio:
      return "menuItemRadio";
    case ax::mojom::Role::kMenuListOption:
      return "menuListOption";
    case ax::mojom::Role::kMenuListPopup:
      return "menuListPopup";
    case ax::mojom::Role::kMeter:
      return "meter";
    case ax::mojom::Role::kNavigation:
      return "navigation";
    case ax::mojom::Role::kNote:
      return "note";
    case ax::mojom::Role::kPane:
      return "pane";
    case ax::mojom::Role::kParagraph:
      return "paragraph";
    case ax::mojom::Role::kPopUpButton:
      return "popUpButton";
    case ax::mojom::Role::kPre:
      return "pre";
    case ax::mojom::Role::kPresentational:
      return "presentational";
    case ax::mojom::Role::kProgressIndicator:
      return "progressIndicator";
    case ax::mojom::Role::kRadioButton:
      return "radioButton";
    case ax::mojom::Role::kRadioGroup:
      return "radioGroup";
    case ax::mojom::Role::kRegion:
      return "region";
    case ax::mojom::Role::kRootWebArea:
      return "rootWebArea";
    case ax::mojom::Role::kRowHeader:
      return "rowHeader";
    case ax::mojom::Role::kRow:
      return "row";
    case ax::mojom::Role::kRuby:
      return "ruby";
    case ax::mojom::Role::kSvgRoot:
      return "svgRoot";
    case ax::mojom::Role::kScrollBar:
      return "scrollBar";
    case ax::mojom::Role::kSearch:
      return "search";
    case ax::mojom::Role::kSearchBox:
      return "searchBox";
    case ax::mojom::Role::kSlider:
      return "slider";
    case ax::mojom::Role::kSliderThumb:
      return "sliderThumb";
    case ax::mojom::Role::kSpinButtonPart:
      return "spinButtonPart";
    case ax::mojom::Role::kSpinButton:
      return "spinButton";
    case ax::mojom::Role::kSplitter:
      return "splitter";
    case ax::mojom::Role::kStaticText:
      return "staticText";
    case ax::mojom::Role::kStatus:
      return "status";
    case ax::mojom::Role::kSwitch:
      return "switch";
    case ax::mojom::Role::kTabList:
      return "tabList";
    case ax::mojom::Role::kTabPanel:
      return "tabPanel";
    case ax::mojom::Role::kTab:
      return "tab";
    case ax::mojom::Role::kTableHeaderContainer:
      return "tableHeaderContainer";
    case ax::mojom::Role::kTable:
      return "table";
    case ax::mojom::Role::kTerm:
      return "term";
    case ax::mojom::Role::kTextField:
      return "textField";
    case ax::mojom::Role::kTextFieldWithComboBox:
      return "textFieldWithComboBox";
    case ax::mojom::Role::kTime:
      return "time";
    case ax::mojom::Role::kTimer:
      return "timer";
    case ax::mojom::Role::kTitleBar:
      return "titleBar";
    case ax::mojom::Role::kToggleButton:
      return "toggleButton";
    case ax::mojom::Role::kToolbar:
      return "toolbar";
    case ax::mojom::Role::kTreeGrid:
      return "treeGrid";
    case ax::mojom::Role::kTreeItem:
      return "treeItem";
    case ax::mojom::Role::kTree:
      return "tree";
    case ax::mojom::Role::kUnknown:
      return "unknown";
    case ax::mojom::Role::kTooltip:
      return "tooltip";
    case ax::mojom::Role::kVideo:
      return "video";
    case ax::mojom::Role::kWebArea:
      return "webArea";
    case ax::mojom::Role::kWebView:
      return "webView";
    case ax::mojom::Role::kWindow:
      return "window";
  }

  return "";
}

ax::mojom::Role ParseRole(const char* role) {
  if (0 == strcmp(role, "none"))
    return ax::mojom::Role::kNone;
  if (0 == strcmp(role, "abbr"))
    return ax::mojom::Role::kAbbr;
  if (0 == strcmp(role, "alertDialog"))
    return ax::mojom::Role::kAlertDialog;
  if (0 == strcmp(role, "alert"))
    return ax::mojom::Role::kAlert;
  if (0 == strcmp(role, "anchor"))
    return ax::mojom::Role::kAnchor;
  if (0 == strcmp(role, "annotation"))
    return ax::mojom::Role::kAnnotation;
  if (0 == strcmp(role, "application"))
    return ax::mojom::Role::kApplication;
  if (0 == strcmp(role, "article"))
    return ax::mojom::Role::kArticle;
  if (0 == strcmp(role, "audio"))
    return ax::mojom::Role::kAudio;
  if (0 == strcmp(role, "banner"))
    return ax::mojom::Role::kBanner;
  if (0 == strcmp(role, "blockquote"))
    return ax::mojom::Role::kBlockquote;
  if (0 == strcmp(role, "button"))
    return ax::mojom::Role::kButton;
  if (0 == strcmp(role, "canvas"))
    return ax::mojom::Role::kCanvas;
  if (0 == strcmp(role, "caption"))
    return ax::mojom::Role::kCaption;
  if (0 == strcmp(role, "caret"))
    return ax::mojom::Role::kCaret;
  if (0 == strcmp(role, "cell"))
    return ax::mojom::Role::kCell;
  if (0 == strcmp(role, "checkBox"))
    return ax::mojom::Role::kCheckBox;
  if (0 == strcmp(role, "client"))
    return ax::mojom::Role::kClient;
  if (0 == strcmp(role, "colorWell"))
    return ax::mojom::Role::kColorWell;
  if (0 == strcmp(role, "columnHeader"))
    return ax::mojom::Role::kColumnHeader;
  if (0 == strcmp(role, "column"))
    return ax::mojom::Role::kColumn;
  if (0 == strcmp(role, "comboBoxGrouping"))
    return ax::mojom::Role::kComboBoxGrouping;
  if (0 == strcmp(role, "comboBoxMenuButton"))
    return ax::mojom::Role::kComboBoxMenuButton;
  if (0 == strcmp(role, "complementary"))
    return ax::mojom::Role::kComplementary;
  if (0 == strcmp(role, "contentInfo"))
    return ax::mojom::Role::kContentInfo;
  if (0 == strcmp(role, "date"))
    return ax::mojom::Role::kDate;
  if (0 == strcmp(role, "dateTime"))
    return ax::mojom::Role::kDateTime;
  if (0 == strcmp(role, "definition"))
    return ax::mojom::Role::kDefinition;
  if (0 == strcmp(role, "descriptionListDetail"))
    return ax::mojom::Role::kDescriptionListDetail;
  if (0 == strcmp(role, "descriptionList"))
    return ax::mojom::Role::kDescriptionList;
  if (0 == strcmp(role, "descriptionListTerm"))
    return ax::mojom::Role::kDescriptionListTerm;
  if (0 == strcmp(role, "desktop"))
    return ax::mojom::Role::kDesktop;
  if (0 == strcmp(role, "details"))
    return ax::mojom::Role::kDetails;
  if (0 == strcmp(role, "dialog"))
    return ax::mojom::Role::kDialog;
  if (0 == strcmp(role, "directory"))
    return ax::mojom::Role::kDirectory;
  if (0 == strcmp(role, "disclosureTriangle"))
    return ax::mojom::Role::kDisclosureTriangle;
  if (0 == strcmp(role, "document"))
    return ax::mojom::Role::kDocument;
  if (0 == strcmp(role, "embeddedObject"))
    return ax::mojom::Role::kEmbeddedObject;
  if (0 == strcmp(role, "feed"))
    return ax::mojom::Role::kFeed;
  if (0 == strcmp(role, "figcaption"))
    return ax::mojom::Role::kFigcaption;
  if (0 == strcmp(role, "figure"))
    return ax::mojom::Role::kFigure;
  if (0 == strcmp(role, "footer"))
    return ax::mojom::Role::kFooter;
  if (0 == strcmp(role, "form"))
    return ax::mojom::Role::kForm;
  if (0 == strcmp(role, "genericContainer"))
    return ax::mojom::Role::kGenericContainer;
  if (0 == strcmp(role, "grid"))
    return ax::mojom::Role::kGrid;
  if (0 == strcmp(role, "group"))
    return ax::mojom::Role::kGroup;
  if (0 == strcmp(role, "heading"))
    return ax::mojom::Role::kHeading;
  if (0 == strcmp(role, "iframe"))
    return ax::mojom::Role::kIframe;
  if (0 == strcmp(role, "iframePresentational"))
    return ax::mojom::Role::kIframePresentational;
  if (0 == strcmp(role, "ignored"))
    return ax::mojom::Role::kIgnored;
  if (0 == strcmp(role, "imageMap"))
    return ax::mojom::Role::kImageMap;
  if (0 == strcmp(role, "image"))
    return ax::mojom::Role::kImage;
  if (0 == strcmp(role, "inlineTextBox"))
    return ax::mojom::Role::kInlineTextBox;
  if (0 == strcmp(role, "inputTime"))
    return ax::mojom::Role::kInputTime;
  if (0 == strcmp(role, "labelText"))
    return ax::mojom::Role::kLabelText;
  if (0 == strcmp(role, "layoutTable"))
    return ax::mojom::Role::kLayoutTable;
  if (0 == strcmp(role, "layoutTableCell"))
    return ax::mojom::Role::kLayoutTableCell;
  if (0 == strcmp(role, "layoutTableColumn"))
    return ax::mojom::Role::kLayoutTableColumn;
  if (0 == strcmp(role, "layoutTableRow"))
    return ax::mojom::Role::kLayoutTableRow;
  if (0 == strcmp(role, "legend"))
    return ax::mojom::Role::kLegend;
  if (0 == strcmp(role, "lineBreak"))
    return ax::mojom::Role::kLineBreak;
  if (0 == strcmp(role, "link"))
    return ax::mojom::Role::kLink;
  if (0 == strcmp(role, "listBoxOption"))
    return ax::mojom::Role::kListBoxOption;
  if (0 == strcmp(role, "listBox"))
    return ax::mojom::Role::kListBox;
  if (0 == strcmp(role, "listItem"))
    return ax::mojom::Role::kListItem;
  if (0 == strcmp(role, "listMarker"))
    return ax::mojom::Role::kListMarker;
  if (0 == strcmp(role, "list"))
    return ax::mojom::Role::kList;
  if (0 == strcmp(role, "locationBar"))
    return ax::mojom::Role::kLocationBar;
  if (0 == strcmp(role, "log"))
    return ax::mojom::Role::kLog;
  if (0 == strcmp(role, "main"))
    return ax::mojom::Role::kMain;
  if (0 == strcmp(role, "mark"))
    return ax::mojom::Role::kMark;
  if (0 == strcmp(role, "marquee"))
    return ax::mojom::Role::kMarquee;
  if (0 == strcmp(role, "math"))
    return ax::mojom::Role::kMath;
  if (0 == strcmp(role, "menu"))
    return ax::mojom::Role::kMenu;
  if (0 == strcmp(role, "menuBar"))
    return ax::mojom::Role::kMenuBar;
  if (0 == strcmp(role, "menuButton"))
    return ax::mojom::Role::kMenuButton;
  if (0 == strcmp(role, "menuItem"))
    return ax::mojom::Role::kMenuItem;
  if (0 == strcmp(role, "menuItemCheckBox"))
    return ax::mojom::Role::kMenuItemCheckBox;
  if (0 == strcmp(role, "menuItemRadio"))
    return ax::mojom::Role::kMenuItemRadio;
  if (0 == strcmp(role, "menuListOption"))
    return ax::mojom::Role::kMenuListOption;
  if (0 == strcmp(role, "menuListPopup"))
    return ax::mojom::Role::kMenuListPopup;
  if (0 == strcmp(role, "meter"))
    return ax::mojom::Role::kMeter;
  if (0 == strcmp(role, "navigation"))
    return ax::mojom::Role::kNavigation;
  if (0 == strcmp(role, "note"))
    return ax::mojom::Role::kNote;
  if (0 == strcmp(role, "pane"))
    return ax::mojom::Role::kPane;
  if (0 == strcmp(role, "paragraph"))
    return ax::mojom::Role::kParagraph;
  if (0 == strcmp(role, "popUpButton"))
    return ax::mojom::Role::kPopUpButton;
  if (0 == strcmp(role, "pre"))
    return ax::mojom::Role::kPre;
  if (0 == strcmp(role, "presentational"))
    return ax::mojom::Role::kPresentational;
  if (0 == strcmp(role, "progressIndicator"))
    return ax::mojom::Role::kProgressIndicator;
  if (0 == strcmp(role, "radioButton"))
    return ax::mojom::Role::kRadioButton;
  if (0 == strcmp(role, "radioGroup"))
    return ax::mojom::Role::kRadioGroup;
  if (0 == strcmp(role, "region"))
    return ax::mojom::Role::kRegion;
  if (0 == strcmp(role, "rootWebArea"))
    return ax::mojom::Role::kRootWebArea;
  if (0 == strcmp(role, "rowHeader"))
    return ax::mojom::Role::kRowHeader;
  if (0 == strcmp(role, "row"))
    return ax::mojom::Role::kRow;
  if (0 == strcmp(role, "ruby"))
    return ax::mojom::Role::kRuby;
  if (0 == strcmp(role, "svgRoot"))
    return ax::mojom::Role::kSvgRoot;
  if (0 == strcmp(role, "scrollBar"))
    return ax::mojom::Role::kScrollBar;
  if (0 == strcmp(role, "search"))
    return ax::mojom::Role::kSearch;
  if (0 == strcmp(role, "searchBox"))
    return ax::mojom::Role::kSearchBox;
  if (0 == strcmp(role, "slider"))
    return ax::mojom::Role::kSlider;
  if (0 == strcmp(role, "sliderThumb"))
    return ax::mojom::Role::kSliderThumb;
  if (0 == strcmp(role, "spinButtonPart"))
    return ax::mojom::Role::kSpinButtonPart;
  if (0 == strcmp(role, "spinButton"))
    return ax::mojom::Role::kSpinButton;
  if (0 == strcmp(role, "splitter"))
    return ax::mojom::Role::kSplitter;
  if (0 == strcmp(role, "staticText"))
    return ax::mojom::Role::kStaticText;
  if (0 == strcmp(role, "status"))
    return ax::mojom::Role::kStatus;
  if (0 == strcmp(role, "switch"))
    return ax::mojom::Role::kSwitch;
  if (0 == strcmp(role, "tabList"))
    return ax::mojom::Role::kTabList;
  if (0 == strcmp(role, "tabPanel"))
    return ax::mojom::Role::kTabPanel;
  if (0 == strcmp(role, "tab"))
    return ax::mojom::Role::kTab;
  if (0 == strcmp(role, "tableHeaderContainer"))
    return ax::mojom::Role::kTableHeaderContainer;
  if (0 == strcmp(role, "table"))
    return ax::mojom::Role::kTable;
  if (0 == strcmp(role, "term"))
    return ax::mojom::Role::kTerm;
  if (0 == strcmp(role, "textField"))
    return ax::mojom::Role::kTextField;
  if (0 == strcmp(role, "textFieldWithComboBox"))
    return ax::mojom::Role::kTextFieldWithComboBox;
  if (0 == strcmp(role, "time"))
    return ax::mojom::Role::kTime;
  if (0 == strcmp(role, "timer"))
    return ax::mojom::Role::kTimer;
  if (0 == strcmp(role, "titleBar"))
    return ax::mojom::Role::kTitleBar;
  if (0 == strcmp(role, "toggleButton"))
    return ax::mojom::Role::kToggleButton;
  if (0 == strcmp(role, "toolbar"))
    return ax::mojom::Role::kToolbar;
  if (0 == strcmp(role, "treeGrid"))
    return ax::mojom::Role::kTreeGrid;
  if (0 == strcmp(role, "treeItem"))
    return ax::mojom::Role::kTreeItem;
  if (0 == strcmp(role, "tree"))
    return ax::mojom::Role::kTree;
  if (0 == strcmp(role, "unknown"))
    return ax::mojom::Role::kUnknown;
  if (0 == strcmp(role, "tooltip"))
    return ax::mojom::Role::kTooltip;
  if (0 == strcmp(role, "video"))
    return ax::mojom::Role::kVideo;
  if (0 == strcmp(role, "webArea"))
    return ax::mojom::Role::kWebArea;
  if (0 == strcmp(role, "webView"))
    return ax::mojom::Role::kWebView;
  if (0 == strcmp(role, "window"))
    return ax::mojom::Role::kWindow;
  return ax::mojom::Role::kNone;
}

const char* ToString(ax::mojom::State state) {
  switch (state) {
    case ax::mojom::State::kNone:
      return "none";
    case ax::mojom::State::kCollapsed:
      return "collapsed";
    case ax::mojom::State::kDefault:
      return "default";
    case ax::mojom::State::kEditable:
      return "editable";
    case ax::mojom::State::kExpanded:
      return "expanded";
    case ax::mojom::State::kFocusable:
      return "focusable";
    case ax::mojom::State::kHaspopup:
      return "haspopup";
    case ax::mojom::State::kHorizontal:
      return "horizontal";
    case ax::mojom::State::kHovered:
      return "hovered";
    case ax::mojom::State::kIgnored:
      return "ignored";
    case ax::mojom::State::kInvisible:
      return "invisible";
    case ax::mojom::State::kLinked:
      return "linked";
    case ax::mojom::State::kMultiline:
      return "multiline";
    case ax::mojom::State::kMultiselectable:
      return "multiselectable";
    case ax::mojom::State::kProtected:
      return "protected";
    case ax::mojom::State::kRequired:
      return "required";
    case ax::mojom::State::kRichlyEditable:
      return "richlyEditable";
    case ax::mojom::State::kVertical:
      return "vertical";
    case ax::mojom::State::kVisited:
      return "visited";
  }

  return "";
}

ax::mojom::State ParseState(const char* state) {
  if (0 == strcmp(state, "none"))
    return ax::mojom::State::kNone;
  if (0 == strcmp(state, "collapsed"))
    return ax::mojom::State::kCollapsed;
  if (0 == strcmp(state, "default"))
    return ax::mojom::State::kDefault;
  if (0 == strcmp(state, "editable"))
    return ax::mojom::State::kEditable;
  if (0 == strcmp(state, "expanded"))
    return ax::mojom::State::kExpanded;
  if (0 == strcmp(state, "focusable"))
    return ax::mojom::State::kFocusable;
  if (0 == strcmp(state, "haspopup"))
    return ax::mojom::State::kHaspopup;
  if (0 == strcmp(state, "horizontal"))
    return ax::mojom::State::kHorizontal;
  if (0 == strcmp(state, "hovered"))
    return ax::mojom::State::kHovered;
  if (0 == strcmp(state, "ignored"))
    return ax::mojom::State::kIgnored;
  if (0 == strcmp(state, "invisible"))
    return ax::mojom::State::kInvisible;
  if (0 == strcmp(state, "linked"))
    return ax::mojom::State::kLinked;
  if (0 == strcmp(state, "multiline"))
    return ax::mojom::State::kMultiline;
  if (0 == strcmp(state, "multiselectable"))
    return ax::mojom::State::kMultiselectable;
  if (0 == strcmp(state, "protected"))
    return ax::mojom::State::kProtected;
  if (0 == strcmp(state, "required"))
    return ax::mojom::State::kRequired;
  if (0 == strcmp(state, "richlyEditable"))
    return ax::mojom::State::kRichlyEditable;
  if (0 == strcmp(state, "vertical"))
    return ax::mojom::State::kVertical;
  if (0 == strcmp(state, "visited"))
    return ax::mojom::State::kVisited;
  return ax::mojom::State::kNone;
}

const char* ToString(ax::mojom::Action action) {
  switch (action) {
    case ax::mojom::Action::kNone:
      return "none";
    case ax::mojom::Action::kBlur:
      return "blur";
    case ax::mojom::Action::kCustomAction:
      return "customAction";
    case ax::mojom::Action::kDecrement:
      return "decrement";
    case ax::mojom::Action::kDoDefault:
      return "doDefault";
    case ax::mojom::Action::kFocus:
      return "focus";
    case ax::mojom::Action::kGetImageData:
      return "getImageData";
    case ax::mojom::Action::kHitTest:
      return "hitTest";
    case ax::mojom::Action::kIncrement:
      return "increment";
    case ax::mojom::Action::kLoadInlineTextBoxes:
      return "loadInlineTextBoxes";
    case ax::mojom::Action::kReplaceSelectedText:
      return "replaceSelectedText";
    case ax::mojom::Action::kScrollBackward:
      return "scrollBackward";
    case ax::mojom::Action::kScrollForward:
      return "scrollForward";
    case ax::mojom::Action::kScrollUp:
      return "scrollUp";
    case ax::mojom::Action::kScrollDown:
      return "scrollDown";
    case ax::mojom::Action::kScrollLeft:
      return "scrollLeft";
    case ax::mojom::Action::kScrollRight:
      return "scrollRight";
    case ax::mojom::Action::kScrollToMakeVisible:
      return "scrollToMakeVisible";
    case ax::mojom::Action::kScrollToPoint:
      return "scrollToPoint";
    case ax::mojom::Action::kSetScrollOffset:
      return "setScrollOffset";
    case ax::mojom::Action::kSetSelection:
      return "setSelection";
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      return "setSequentialFocusNavigationStartingPoint";
    case ax::mojom::Action::kSetValue:
      return "setValue";
    case ax::mojom::Action::kShowContextMenu:
      return "showContextMenu";
  }

  return "";
}

ax::mojom::Action ParseAction(const char* action) {
  if (0 == strcmp(action, "none"))
    return ax::mojom::Action::kNone;
  if (0 == strcmp(action, "blur"))
    return ax::mojom::Action::kBlur;
  if (0 == strcmp(action, "customAction"))
    return ax::mojom::Action::kCustomAction;
  if (0 == strcmp(action, "decrement"))
    return ax::mojom::Action::kDecrement;
  if (0 == strcmp(action, "doDefault"))
    return ax::mojom::Action::kDoDefault;
  if (0 == strcmp(action, "focus"))
    return ax::mojom::Action::kFocus;
  if (0 == strcmp(action, "getImageData"))
    return ax::mojom::Action::kGetImageData;
  if (0 == strcmp(action, "hitTest"))
    return ax::mojom::Action::kHitTest;
  if (0 == strcmp(action, "increment"))
    return ax::mojom::Action::kIncrement;
  if (0 == strcmp(action, "loadInlineTextBoxes"))
    return ax::mojom::Action::kLoadInlineTextBoxes;
  if (0 == strcmp(action, "replaceSelectedText"))
    return ax::mojom::Action::kReplaceSelectedText;
  if (0 == strcmp(action, "scrollBackward"))
    return ax::mojom::Action::kScrollBackward;
  if (0 == strcmp(action, "scrollForward"))
    return ax::mojom::Action::kScrollForward;
  if (0 == strcmp(action, "scrollUp"))
    return ax::mojom::Action::kScrollUp;
  if (0 == strcmp(action, "scrollDown"))
    return ax::mojom::Action::kScrollDown;
  if (0 == strcmp(action, "scrollLeft"))
    return ax::mojom::Action::kScrollLeft;
  if (0 == strcmp(action, "scrollRight"))
    return ax::mojom::Action::kScrollRight;
  if (0 == strcmp(action, "scrollToMakeVisible"))
    return ax::mojom::Action::kScrollToMakeVisible;
  if (0 == strcmp(action, "scrollToPoint"))
    return ax::mojom::Action::kScrollToPoint;
  if (0 == strcmp(action, "setScrollOffset"))
    return ax::mojom::Action::kSetScrollOffset;
  if (0 == strcmp(action, "setSelection"))
    return ax::mojom::Action::kSetSelection;
  if (0 == strcmp(action, "setSequentialFocusNavigationStartingPoint"))
    return ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
  if (0 == strcmp(action, "setValue"))
    return ax::mojom::Action::kSetValue;
  if (0 == strcmp(action, "showContextMenu"))
    return ax::mojom::Action::kShowContextMenu;
  return ax::mojom::Action::kNone;
}

const char* ToString(ax::mojom::ActionFlags action_flags) {
  switch (action_flags) {
    case ax::mojom::ActionFlags::kNone:
      return "none";
    case ax::mojom::ActionFlags::kRequestImages:
      return "requestImages";
    case ax::mojom::ActionFlags::kRequestInlineTextBoxes:
      return "requestInlineTextBoxes";
  }

  return "";
}

ax::mojom::ActionFlags ParseActionFlags(const char* action_flags) {
  if (0 == strcmp(action_flags, "none"))
    return ax::mojom::ActionFlags::kNone;
  if (0 == strcmp(action_flags, "requestImages"))
    return ax::mojom::ActionFlags::kRequestImages;
  if (0 == strcmp(action_flags, "requestInlineTextBoxes"))
    return ax::mojom::ActionFlags::kRequestInlineTextBoxes;
  return ax::mojom::ActionFlags::kNone;
}

const char* ToString(ax::mojom::DefaultActionVerb default_action_verb) {
  switch (default_action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return "none";
    case ax::mojom::DefaultActionVerb::kActivate:
      return "activate";
    case ax::mojom::DefaultActionVerb::kCheck:
      return "check";
    case ax::mojom::DefaultActionVerb::kClick:
      return "click";
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return "clickAncestor";
    case ax::mojom::DefaultActionVerb::kJump:
      return "jump";
    case ax::mojom::DefaultActionVerb::kOpen:
      return "open";
    case ax::mojom::DefaultActionVerb::kPress:
      return "press";
    case ax::mojom::DefaultActionVerb::kSelect:
      return "select";
    case ax::mojom::DefaultActionVerb::kUncheck:
      return "uncheck";
  }

  return "";
}

ax::mojom::DefaultActionVerb ParseDefaultActionVerb(
    const char* default_action_verb) {
  if (0 == strcmp(default_action_verb, "none"))
    return ax::mojom::DefaultActionVerb::kNone;
  if (0 == strcmp(default_action_verb, "activate"))
    return ax::mojom::DefaultActionVerb::kActivate;
  if (0 == strcmp(default_action_verb, "check"))
    return ax::mojom::DefaultActionVerb::kCheck;
  if (0 == strcmp(default_action_verb, "click"))
    return ax::mojom::DefaultActionVerb::kClick;
  if (0 == strcmp(default_action_verb, "clickAncestor"))
    return ax::mojom::DefaultActionVerb::kClickAncestor;
  if (0 == strcmp(default_action_verb, "jump"))
    return ax::mojom::DefaultActionVerb::kJump;
  if (0 == strcmp(default_action_verb, "open"))
    return ax::mojom::DefaultActionVerb::kOpen;
  if (0 == strcmp(default_action_verb, "press"))
    return ax::mojom::DefaultActionVerb::kPress;
  if (0 == strcmp(default_action_verb, "select"))
    return ax::mojom::DefaultActionVerb::kSelect;
  if (0 == strcmp(default_action_verb, "uncheck"))
    return ax::mojom::DefaultActionVerb::kUncheck;
  return ax::mojom::DefaultActionVerb::kNone;
}

const char* ToString(ax::mojom::Mutation mutation) {
  switch (mutation) {
    case ax::mojom::Mutation::kNone:
      return "none";
    case ax::mojom::Mutation::kNodeCreated:
      return "nodeCreated";
    case ax::mojom::Mutation::kSubtreeCreated:
      return "subtreeCreated";
    case ax::mojom::Mutation::kNodeChanged:
      return "nodeChanged";
    case ax::mojom::Mutation::kNodeRemoved:
      return "nodeRemoved";
  }

  return "";
}

ax::mojom::Mutation ParseMutation(const char* mutation) {
  if (0 == strcmp(mutation, "none"))
    return ax::mojom::Mutation::kNone;
  if (0 == strcmp(mutation, "nodeCreated"))
    return ax::mojom::Mutation::kNodeCreated;
  if (0 == strcmp(mutation, "subtreeCreated"))
    return ax::mojom::Mutation::kSubtreeCreated;
  if (0 == strcmp(mutation, "nodeChanged"))
    return ax::mojom::Mutation::kNodeChanged;
  if (0 == strcmp(mutation, "nodeRemoved"))
    return ax::mojom::Mutation::kNodeRemoved;
  return ax::mojom::Mutation::kNone;
}

const char* ToString(ax::mojom::StringAttribute string_attribute) {
  switch (string_attribute) {
    case ax::mojom::StringAttribute::kNone:
      return "none";
    case ax::mojom::StringAttribute::kAccessKey:
      return "accessKey";
    case ax::mojom::StringAttribute::kAriaInvalidValue:
      return "ariaInvalidValue";
    case ax::mojom::StringAttribute::kAutoComplete:
      return "autoComplete";
    case ax::mojom::StringAttribute::kChromeChannel:
      return "chromeChannel";
    case ax::mojom::StringAttribute::kClassName:
      return "className";
    case ax::mojom::StringAttribute::kContainerLiveRelevant:
      return "containerLiveRelevant";
    case ax::mojom::StringAttribute::kContainerLiveStatus:
      return "containerLiveStatus";
    case ax::mojom::StringAttribute::kDescription:
      return "description";
    case ax::mojom::StringAttribute::kDisplay:
      return "display";
    case ax::mojom::StringAttribute::kFontFamily:
      return "fontFamily";
    case ax::mojom::StringAttribute::kHtmlTag:
      return "htmlTag";
    case ax::mojom::StringAttribute::kImageDataUrl:
      return "imageDataUrl";
    case ax::mojom::StringAttribute::kInnerHtml:
      return "innerHtml";
    case ax::mojom::StringAttribute::kKeyShortcuts:
      return "keyShortcuts";
    case ax::mojom::StringAttribute::kLanguage:
      return "language";
    case ax::mojom::StringAttribute::kName:
      return "name";
    case ax::mojom::StringAttribute::kLiveRelevant:
      return "liveRelevant";
    case ax::mojom::StringAttribute::kLiveStatus:
      return "liveStatus";
    case ax::mojom::StringAttribute::kPlaceholder:
      return "placeholder";
    case ax::mojom::StringAttribute::kRole:
      return "role";
    case ax::mojom::StringAttribute::kRoleDescription:
      return "roleDescription";
    case ax::mojom::StringAttribute::kUrl:
      return "url";
    case ax::mojom::StringAttribute::kValue:
      return "value";
  }

  return "";
}

ax::mojom::StringAttribute ParseStringAttribute(const char* string_attribute) {
  if (0 == strcmp(string_attribute, "none"))
    return ax::mojom::StringAttribute::kNone;
  if (0 == strcmp(string_attribute, "accessKey"))
    return ax::mojom::StringAttribute::kAccessKey;
  if (0 == strcmp(string_attribute, "ariaInvalidValue"))
    return ax::mojom::StringAttribute::kAriaInvalidValue;
  if (0 == strcmp(string_attribute, "autoComplete"))
    return ax::mojom::StringAttribute::kAutoComplete;
  if (0 == strcmp(string_attribute, "chromeChannel"))
    return ax::mojom::StringAttribute::kChromeChannel;
  if (0 == strcmp(string_attribute, "className"))
    return ax::mojom::StringAttribute::kClassName;
  if (0 == strcmp(string_attribute, "containerLiveRelevant"))
    return ax::mojom::StringAttribute::kContainerLiveRelevant;
  if (0 == strcmp(string_attribute, "containerLiveStatus"))
    return ax::mojom::StringAttribute::kContainerLiveStatus;
  if (0 == strcmp(string_attribute, "description"))
    return ax::mojom::StringAttribute::kDescription;
  if (0 == strcmp(string_attribute, "display"))
    return ax::mojom::StringAttribute::kDisplay;
  if (0 == strcmp(string_attribute, "fontFamily"))
    return ax::mojom::StringAttribute::kFontFamily;
  if (0 == strcmp(string_attribute, "htmlTag"))
    return ax::mojom::StringAttribute::kHtmlTag;
  if (0 == strcmp(string_attribute, "imageDataUrl"))
    return ax::mojom::StringAttribute::kImageDataUrl;
  if (0 == strcmp(string_attribute, "innerHtml"))
    return ax::mojom::StringAttribute::kInnerHtml;
  if (0 == strcmp(string_attribute, "keyShortcuts"))
    return ax::mojom::StringAttribute::kKeyShortcuts;
  if (0 == strcmp(string_attribute, "language"))
    return ax::mojom::StringAttribute::kLanguage;
  if (0 == strcmp(string_attribute, "name"))
    return ax::mojom::StringAttribute::kName;
  if (0 == strcmp(string_attribute, "liveRelevant"))
    return ax::mojom::StringAttribute::kLiveRelevant;
  if (0 == strcmp(string_attribute, "liveStatus"))
    return ax::mojom::StringAttribute::kLiveStatus;
  if (0 == strcmp(string_attribute, "placeholder"))
    return ax::mojom::StringAttribute::kPlaceholder;
  if (0 == strcmp(string_attribute, "role"))
    return ax::mojom::StringAttribute::kRole;
  if (0 == strcmp(string_attribute, "roleDescription"))
    return ax::mojom::StringAttribute::kRoleDescription;
  if (0 == strcmp(string_attribute, "url"))
    return ax::mojom::StringAttribute::kUrl;
  if (0 == strcmp(string_attribute, "value"))
    return ax::mojom::StringAttribute::kValue;
  return ax::mojom::StringAttribute::kNone;
}

const char* ToString(ax::mojom::IntAttribute int_attribute) {
  switch (int_attribute) {
    case ax::mojom::IntAttribute::kNone:
      return "none";
    case ax::mojom::IntAttribute::kDefaultActionVerb:
      return "defaultActionVerb";
    case ax::mojom::IntAttribute::kScrollX:
      return "scrollX";
    case ax::mojom::IntAttribute::kScrollXMin:
      return "scrollXMin";
    case ax::mojom::IntAttribute::kScrollXMax:
      return "scrollXMax";
    case ax::mojom::IntAttribute::kScrollY:
      return "scrollY";
    case ax::mojom::IntAttribute::kScrollYMin:
      return "scrollYMin";
    case ax::mojom::IntAttribute::kScrollYMax:
      return "scrollYMax";
    case ax::mojom::IntAttribute::kTextSelStart:
      return "textSelStart";
    case ax::mojom::IntAttribute::kTextSelEnd:
      return "textSelEnd";
    case ax::mojom::IntAttribute::kAriaColumnCount:
      return "ariaColumnCount";
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
      return "ariaCellColumnIndex";
    case ax::mojom::IntAttribute::kAriaRowCount:
      return "ariaRowCount";
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
      return "ariaCellRowIndex";
    case ax::mojom::IntAttribute::kTableRowCount:
      return "tableRowCount";
    case ax::mojom::IntAttribute::kTableColumnCount:
      return "tableColumnCount";
    case ax::mojom::IntAttribute::kTableHeaderId:
      return "tableHeaderId";
    case ax::mojom::IntAttribute::kTableRowIndex:
      return "tableRowIndex";
    case ax::mojom::IntAttribute::kTableRowHeaderId:
      return "tableRowHeaderId";
    case ax::mojom::IntAttribute::kTableColumnIndex:
      return "tableColumnIndex";
    case ax::mojom::IntAttribute::kTableColumnHeaderId:
      return "tableColumnHeaderId";
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
      return "tableCellColumnIndex";
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
      return "tableCellColumnSpan";
    case ax::mojom::IntAttribute::kTableCellRowIndex:
      return "tableCellRowIndex";
    case ax::mojom::IntAttribute::kTableCellRowSpan:
      return "tableCellRowSpan";
    case ax::mojom::IntAttribute::kSortDirection:
      return "sortDirection";
    case ax::mojom::IntAttribute::kHierarchicalLevel:
      return "hierarchicalLevel";
    case ax::mojom::IntAttribute::kNameFrom:
      return "nameFrom";
    case ax::mojom::IntAttribute::kDescriptionFrom:
      return "descriptionFrom";
    case ax::mojom::IntAttribute::kActivedescendantId:
      return "activedescendantId";
    case ax::mojom::IntAttribute::kDetailsId:
      return "detailsId";
    case ax::mojom::IntAttribute::kErrormessageId:
      return "errormessageId";
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
      return "inPageLinkTargetId";
    case ax::mojom::IntAttribute::kMemberOfId:
      return "memberOfId";
    case ax::mojom::IntAttribute::kNextOnLineId:
      return "nextOnLineId";
    case ax::mojom::IntAttribute::kPreviousOnLineId:
      return "previousOnLineId";
    case ax::mojom::IntAttribute::kChildTreeId:
      return "childTreeId";
    case ax::mojom::IntAttribute::kRestriction:
      return "restriction";
    case ax::mojom::IntAttribute::kSetSize:
      return "setSize";
    case ax::mojom::IntAttribute::kPosInSet:
      return "posInSet";
    case ax::mojom::IntAttribute::kColorValue:
      return "colorValue";
    case ax::mojom::IntAttribute::kAriaCurrentState:
      return "ariaCurrentState";
    case ax::mojom::IntAttribute::kBackgroundColor:
      return "backgroundColor";
    case ax::mojom::IntAttribute::kColor:
      return "color";
    case ax::mojom::IntAttribute::kInvalidState:
      return "invalidState";
    case ax::mojom::IntAttribute::kCheckedState:
      return "checkedState";
    case ax::mojom::IntAttribute::kTextDirection:
      return "textDirection";
    case ax::mojom::IntAttribute::kTextStyle:
      return "textStyle";
    case ax::mojom::IntAttribute::kPreviousFocusId:
      return "previousFocusId";
    case ax::mojom::IntAttribute::kNextFocusId:
      return "nextFocusId";
  }

  return "";
}

ax::mojom::IntAttribute ParseIntAttribute(const char* int_attribute) {
  if (0 == strcmp(int_attribute, "none"))
    return ax::mojom::IntAttribute::kNone;
  if (0 == strcmp(int_attribute, "defaultActionVerb"))
    return ax::mojom::IntAttribute::kDefaultActionVerb;
  if (0 == strcmp(int_attribute, "scrollX"))
    return ax::mojom::IntAttribute::kScrollX;
  if (0 == strcmp(int_attribute, "scrollXMin"))
    return ax::mojom::IntAttribute::kScrollXMin;
  if (0 == strcmp(int_attribute, "scrollXMax"))
    return ax::mojom::IntAttribute::kScrollXMax;
  if (0 == strcmp(int_attribute, "scrollY"))
    return ax::mojom::IntAttribute::kScrollY;
  if (0 == strcmp(int_attribute, "scrollYMin"))
    return ax::mojom::IntAttribute::kScrollYMin;
  if (0 == strcmp(int_attribute, "scrollYMax"))
    return ax::mojom::IntAttribute::kScrollYMax;
  if (0 == strcmp(int_attribute, "textSelStart"))
    return ax::mojom::IntAttribute::kTextSelStart;
  if (0 == strcmp(int_attribute, "textSelEnd"))
    return ax::mojom::IntAttribute::kTextSelEnd;
  if (0 == strcmp(int_attribute, "ariaColumnCount"))
    return ax::mojom::IntAttribute::kAriaColumnCount;
  if (0 == strcmp(int_attribute, "ariaCellColumnIndex"))
    return ax::mojom::IntAttribute::kAriaCellColumnIndex;
  if (0 == strcmp(int_attribute, "ariaRowCount"))
    return ax::mojom::IntAttribute::kAriaRowCount;
  if (0 == strcmp(int_attribute, "ariaCellRowIndex"))
    return ax::mojom::IntAttribute::kAriaCellRowIndex;
  if (0 == strcmp(int_attribute, "tableRowCount"))
    return ax::mojom::IntAttribute::kTableRowCount;
  if (0 == strcmp(int_attribute, "tableColumnCount"))
    return ax::mojom::IntAttribute::kTableColumnCount;
  if (0 == strcmp(int_attribute, "tableHeaderId"))
    return ax::mojom::IntAttribute::kTableHeaderId;
  if (0 == strcmp(int_attribute, "tableRowIndex"))
    return ax::mojom::IntAttribute::kTableRowIndex;
  if (0 == strcmp(int_attribute, "tableRowHeaderId"))
    return ax::mojom::IntAttribute::kTableRowHeaderId;
  if (0 == strcmp(int_attribute, "tableColumnIndex"))
    return ax::mojom::IntAttribute::kTableColumnIndex;
  if (0 == strcmp(int_attribute, "tableColumnHeaderId"))
    return ax::mojom::IntAttribute::kTableColumnHeaderId;
  if (0 == strcmp(int_attribute, "tableCellColumnIndex"))
    return ax::mojom::IntAttribute::kTableCellColumnIndex;
  if (0 == strcmp(int_attribute, "tableCellColumnSpan"))
    return ax::mojom::IntAttribute::kTableCellColumnSpan;
  if (0 == strcmp(int_attribute, "tableCellRowIndex"))
    return ax::mojom::IntAttribute::kTableCellRowIndex;
  if (0 == strcmp(int_attribute, "tableCellRowSpan"))
    return ax::mojom::IntAttribute::kTableCellRowSpan;
  if (0 == strcmp(int_attribute, "sortDirection"))
    return ax::mojom::IntAttribute::kSortDirection;
  if (0 == strcmp(int_attribute, "hierarchicalLevel"))
    return ax::mojom::IntAttribute::kHierarchicalLevel;
  if (0 == strcmp(int_attribute, "nameFrom"))
    return ax::mojom::IntAttribute::kNameFrom;
  if (0 == strcmp(int_attribute, "descriptionFrom"))
    return ax::mojom::IntAttribute::kDescriptionFrom;
  if (0 == strcmp(int_attribute, "activedescendantId"))
    return ax::mojom::IntAttribute::kActivedescendantId;
  if (0 == strcmp(int_attribute, "detailsId"))
    return ax::mojom::IntAttribute::kDetailsId;
  if (0 == strcmp(int_attribute, "errormessageId"))
    return ax::mojom::IntAttribute::kErrormessageId;
  if (0 == strcmp(int_attribute, "inPageLinkTargetId"))
    return ax::mojom::IntAttribute::kInPageLinkTargetId;
  if (0 == strcmp(int_attribute, "memberOfId"))
    return ax::mojom::IntAttribute::kMemberOfId;
  if (0 == strcmp(int_attribute, "nextOnLineId"))
    return ax::mojom::IntAttribute::kNextOnLineId;
  if (0 == strcmp(int_attribute, "previousOnLineId"))
    return ax::mojom::IntAttribute::kPreviousOnLineId;
  if (0 == strcmp(int_attribute, "childTreeId"))
    return ax::mojom::IntAttribute::kChildTreeId;
  if (0 == strcmp(int_attribute, "restriction"))
    return ax::mojom::IntAttribute::kRestriction;
  if (0 == strcmp(int_attribute, "setSize"))
    return ax::mojom::IntAttribute::kSetSize;
  if (0 == strcmp(int_attribute, "posInSet"))
    return ax::mojom::IntAttribute::kPosInSet;
  if (0 == strcmp(int_attribute, "colorValue"))
    return ax::mojom::IntAttribute::kColorValue;
  if (0 == strcmp(int_attribute, "ariaCurrentState"))
    return ax::mojom::IntAttribute::kAriaCurrentState;
  if (0 == strcmp(int_attribute, "backgroundColor"))
    return ax::mojom::IntAttribute::kBackgroundColor;
  if (0 == strcmp(int_attribute, "color"))
    return ax::mojom::IntAttribute::kColor;
  if (0 == strcmp(int_attribute, "invalidState"))
    return ax::mojom::IntAttribute::kInvalidState;
  if (0 == strcmp(int_attribute, "checkedState"))
    return ax::mojom::IntAttribute::kCheckedState;
  if (0 == strcmp(int_attribute, "textDirection"))
    return ax::mojom::IntAttribute::kTextDirection;
  if (0 == strcmp(int_attribute, "textStyle"))
    return ax::mojom::IntAttribute::kTextStyle;
  if (0 == strcmp(int_attribute, "previousFocusId"))
    return ax::mojom::IntAttribute::kPreviousFocusId;
  if (0 == strcmp(int_attribute, "nextFocusId"))
    return ax::mojom::IntAttribute::kNextFocusId;
  return ax::mojom::IntAttribute::kNone;
}

const char* ToString(ax::mojom::FloatAttribute float_attribute) {
  switch (float_attribute) {
    case ax::mojom::FloatAttribute::kNone:
      return "none";
    case ax::mojom::FloatAttribute::kValueForRange:
      return "valueForRange";
    case ax::mojom::FloatAttribute::kMinValueForRange:
      return "minValueForRange";
    case ax::mojom::FloatAttribute::kMaxValueForRange:
      return "maxValueForRange";
    case ax::mojom::FloatAttribute::kStepValueForRange:
      return "stepValueForRange";
    case ax::mojom::FloatAttribute::kFontSize:
      return "fontSize";
  }

  return "";
}

ax::mojom::FloatAttribute ParseFloatAttribute(const char* float_attribute) {
  if (0 == strcmp(float_attribute, "none"))
    return ax::mojom::FloatAttribute::kNone;
  if (0 == strcmp(float_attribute, "valueForRange"))
    return ax::mojom::FloatAttribute::kValueForRange;
  if (0 == strcmp(float_attribute, "minValueForRange"))
    return ax::mojom::FloatAttribute::kMinValueForRange;
  if (0 == strcmp(float_attribute, "maxValueForRange"))
    return ax::mojom::FloatAttribute::kMaxValueForRange;
  if (0 == strcmp(float_attribute, "stepValueForRange"))
    return ax::mojom::FloatAttribute::kStepValueForRange;
  if (0 == strcmp(float_attribute, "fontSize"))
    return ax::mojom::FloatAttribute::kFontSize;
  return ax::mojom::FloatAttribute::kNone;
}

const char* ToString(ax::mojom::BoolAttribute bool_attribute) {
  switch (bool_attribute) {
    case ax::mojom::BoolAttribute::kNone:
      return "none";
    case ax::mojom::BoolAttribute::kBusy:
      return "busy";
    case ax::mojom::BoolAttribute::kEditableRoot:
      return "editableRoot";
    case ax::mojom::BoolAttribute::kContainerLiveAtomic:
      return "containerLiveAtomic";
    case ax::mojom::BoolAttribute::kContainerLiveBusy:
      return "containerLiveBusy";
    case ax::mojom::BoolAttribute::kLiveAtomic:
      return "liveAtomic";
    case ax::mojom::BoolAttribute::kModal:
      return "modal";
    case ax::mojom::BoolAttribute::kUpdateLocationOnly:
      return "updateLocationOnly";
    case ax::mojom::BoolAttribute::kCanvasHasFallback:
      return "canvasHasFallback";
    case ax::mojom::BoolAttribute::kScrollable:
      return "scrollable";
    case ax::mojom::BoolAttribute::kClickable:
      return "clickable";
    case ax::mojom::BoolAttribute::kClipsChildren:
      return "clipsChildren";
    case ax::mojom::BoolAttribute::kSelected:
      return "selected";
  }

  return "";
}

ax::mojom::BoolAttribute ParseBoolAttribute(const char* bool_attribute) {
  if (0 == strcmp(bool_attribute, "none"))
    return ax::mojom::BoolAttribute::kNone;
  if (0 == strcmp(bool_attribute, "busy"))
    return ax::mojom::BoolAttribute::kBusy;
  if (0 == strcmp(bool_attribute, "editableRoot"))
    return ax::mojom::BoolAttribute::kEditableRoot;
  if (0 == strcmp(bool_attribute, "containerLiveAtomic"))
    return ax::mojom::BoolAttribute::kContainerLiveAtomic;
  if (0 == strcmp(bool_attribute, "containerLiveBusy"))
    return ax::mojom::BoolAttribute::kContainerLiveBusy;
  if (0 == strcmp(bool_attribute, "liveAtomic"))
    return ax::mojom::BoolAttribute::kLiveAtomic;
  if (0 == strcmp(bool_attribute, "modal"))
    return ax::mojom::BoolAttribute::kModal;
  if (0 == strcmp(bool_attribute, "updateLocationOnly"))
    return ax::mojom::BoolAttribute::kUpdateLocationOnly;
  if (0 == strcmp(bool_attribute, "canvasHasFallback"))
    return ax::mojom::BoolAttribute::kCanvasHasFallback;
  if (0 == strcmp(bool_attribute, "scrollable"))
    return ax::mojom::BoolAttribute::kScrollable;
  if (0 == strcmp(bool_attribute, "clickable"))
    return ax::mojom::BoolAttribute::kClickable;
  if (0 == strcmp(bool_attribute, "clipsChildren"))
    return ax::mojom::BoolAttribute::kClipsChildren;
  if (0 == strcmp(bool_attribute, "selected"))
    return ax::mojom::BoolAttribute::kSelected;
  return ax::mojom::BoolAttribute::kNone;
}

const char* ToString(ax::mojom::IntListAttribute int_list_attribute) {
  switch (int_list_attribute) {
    case ax::mojom::IntListAttribute::kNone:
      return "none";
    case ax::mojom::IntListAttribute::kIndirectChildIds:
      return "indirectChildIds";
    case ax::mojom::IntListAttribute::kControlsIds:
      return "controlsIds";
    case ax::mojom::IntListAttribute::kDescribedbyIds:
      return "describedbyIds";
    case ax::mojom::IntListAttribute::kFlowtoIds:
      return "flowtoIds";
    case ax::mojom::IntListAttribute::kLabelledbyIds:
      return "labelledbyIds";
    case ax::mojom::IntListAttribute::kRadioGroupIds:
      return "radioGroupIds";
    case ax::mojom::IntListAttribute::kLineBreaks:
      return "lineBreaks";
    case ax::mojom::IntListAttribute::kMarkerTypes:
      return "markerTypes";
    case ax::mojom::IntListAttribute::kMarkerStarts:
      return "markerStarts";
    case ax::mojom::IntListAttribute::kMarkerEnds:
      return "markerEnds";
    case ax::mojom::IntListAttribute::kCellIds:
      return "cellIds";
    case ax::mojom::IntListAttribute::kUniqueCellIds:
      return "uniqueCellIds";
    case ax::mojom::IntListAttribute::kCharacterOffsets:
      return "characterOffsets";
    case ax::mojom::IntListAttribute::kCachedLineStarts:
      return "cachedLineStarts";
    case ax::mojom::IntListAttribute::kWordStarts:
      return "wordStarts";
    case ax::mojom::IntListAttribute::kWordEnds:
      return "wordEnds";
    case ax::mojom::IntListAttribute::kCustomActionIds:
      return "customActionIds";
  }

  return "";
}

ax::mojom::IntListAttribute ParseIntListAttribute(
    const char* int_list_attribute) {
  if (0 == strcmp(int_list_attribute, "none"))
    return ax::mojom::IntListAttribute::kNone;
  if (0 == strcmp(int_list_attribute, "indirectChildIds"))
    return ax::mojom::IntListAttribute::kIndirectChildIds;
  if (0 == strcmp(int_list_attribute, "controlsIds"))
    return ax::mojom::IntListAttribute::kControlsIds;
  if (0 == strcmp(int_list_attribute, "describedbyIds"))
    return ax::mojom::IntListAttribute::kDescribedbyIds;
  if (0 == strcmp(int_list_attribute, "flowtoIds"))
    return ax::mojom::IntListAttribute::kFlowtoIds;
  if (0 == strcmp(int_list_attribute, "labelledbyIds"))
    return ax::mojom::IntListAttribute::kLabelledbyIds;
  if (0 == strcmp(int_list_attribute, "radioGroupIds"))
    return ax::mojom::IntListAttribute::kRadioGroupIds;
  if (0 == strcmp(int_list_attribute, "lineBreaks"))
    return ax::mojom::IntListAttribute::kLineBreaks;
  if (0 == strcmp(int_list_attribute, "markerTypes"))
    return ax::mojom::IntListAttribute::kMarkerTypes;
  if (0 == strcmp(int_list_attribute, "markerStarts"))
    return ax::mojom::IntListAttribute::kMarkerStarts;
  if (0 == strcmp(int_list_attribute, "markerEnds"))
    return ax::mojom::IntListAttribute::kMarkerEnds;
  if (0 == strcmp(int_list_attribute, "cellIds"))
    return ax::mojom::IntListAttribute::kCellIds;
  if (0 == strcmp(int_list_attribute, "uniqueCellIds"))
    return ax::mojom::IntListAttribute::kUniqueCellIds;
  if (0 == strcmp(int_list_attribute, "characterOffsets"))
    return ax::mojom::IntListAttribute::kCharacterOffsets;
  if (0 == strcmp(int_list_attribute, "cachedLineStarts"))
    return ax::mojom::IntListAttribute::kCachedLineStarts;
  if (0 == strcmp(int_list_attribute, "wordStarts"))
    return ax::mojom::IntListAttribute::kWordStarts;
  if (0 == strcmp(int_list_attribute, "wordEnds"))
    return ax::mojom::IntListAttribute::kWordEnds;
  if (0 == strcmp(int_list_attribute, "customActionIds"))
    return ax::mojom::IntListAttribute::kCustomActionIds;
  return ax::mojom::IntListAttribute::kNone;
}

const char* ToString(ax::mojom::StringListAttribute string_list_attribute) {
  switch (string_list_attribute) {
    case ax::mojom::StringListAttribute::kNone:
      return "none";
    case ax::mojom::StringListAttribute::kCustomActionDescriptions:
      return "customActionDescriptions";
  }

  return "";
}

ax::mojom::StringListAttribute ParseStringListAttribute(
    const char* string_list_attribute) {
  if (0 == strcmp(string_list_attribute, "none"))
    return ax::mojom::StringListAttribute::kNone;
  if (0 == strcmp(string_list_attribute, "customActionDescriptions"))
    return ax::mojom::StringListAttribute::kCustomActionDescriptions;
  return ax::mojom::StringListAttribute::kNone;
}

const char* ToString(ax::mojom::MarkerType marker_type) {
  switch (marker_type) {
    case ax::mojom::MarkerType::kNone:
      return "none";
    case ax::mojom::MarkerType::kSpelling:
      return "spelling";
    case ax::mojom::MarkerType::kGrammar:
      return "grammar";
    case ax::mojom::MarkerType::kSpellingGrammar:
      return "spellingGrammar";
    case ax::mojom::MarkerType::kTextMatch:
      return "textMatch";
    case ax::mojom::MarkerType::kSpellingTextMatch:
      return "spellingTextMatch";
    case ax::mojom::MarkerType::kGrammarTextMatch:
      return "grammarTextMatch";
    case ax::mojom::MarkerType::kSpellingGrammarTextMatch:
      return "spellingGrammarTextMatch";
    case ax::mojom::MarkerType::kActiveSuggestion:
      return "activeSuggestion";
    case ax::mojom::MarkerType::kSpellingActiveSuggestion:
      return "spellingActiveSuggestion";
    case ax::mojom::MarkerType::kGrammarActiveSuggestion:
      return "grammarActiveSuggestion";
    case ax::mojom::MarkerType::kSpellingGrammarActiveSuggestion:
      return "spellingGrammarActiveSuggestion";
    case ax::mojom::MarkerType::kTextMatchActiveSuggestion:
      return "textMatchActiveSuggestion";
    case ax::mojom::MarkerType::kSpellingTextMatchActiveSuggestion:
      return "spellingTextMatchActiveSuggestion";
    case ax::mojom::MarkerType::kGrammarTextMatchActiveSuggestion:
      return "grammarTextMatchActiveSuggestion";
    case ax::mojom::MarkerType::kSpellingGrammarTextMatchActiveSuggestion:
      return "spellingGrammarTextMatchActiveSuggestion";
    case ax::mojom::MarkerType::kSuggestion:
      return "suggestion";
    case ax::mojom::MarkerType::kSpellingSuggestion:
      return "spellingSuggestion";
    case ax::mojom::MarkerType::kGrammarSuggestion:
      return "grammarSuggestion";
    case ax::mojom::MarkerType::kSpellingGrammarSuggestion:
      return "spellingGrammarSuggestion";
    case ax::mojom::MarkerType::kTextMatchSuggestion:
      return "textMatchSuggestion";
    case ax::mojom::MarkerType::kSpellingTextMatchSuggestion:
      return "spellingTextMatchSuggestion";
    case ax::mojom::MarkerType::kGrammarTextMatchSuggestion:
      return "grammarTextMatchSuggestion";
    case ax::mojom::MarkerType::kSpellingGrammarTextMatchSuggestion:
      return "spellingGrammarTextMatchSuggestion";
    case ax::mojom::MarkerType::kActiveSuggestionSuggestion:
      return "activeSuggestionSuggestion";
    case ax::mojom::MarkerType::kSpellingActiveSuggestionSuggestion:
      return "spellingActiveSuggestionSuggestion";
    case ax::mojom::MarkerType::kGrammarActiveSuggestionSuggestion:
      return "grammarActiveSuggestionSuggestion";
    case ax::mojom::MarkerType::kSpellingGrammarActiveSuggestionSuggestion:
      return "spellingGrammarActiveSuggestionSuggestion";
    case ax::mojom::MarkerType::kTextMatchActiveSuggestionSuggestion:
      return "textMatchActiveSuggestionSuggestion";
    case ax::mojom::MarkerType::kSpellingTextMatchActiveSuggestionSuggestion:
      return "spellingTextMatchActiveSuggestionSuggestion";
    case ax::mojom::MarkerType::kGrammarTextMatchActiveSuggestionSuggestion:
      return "grammarTextMatchActiveSuggestionSuggestion";
    case ax::mojom::MarkerType::
        kSpellingGrammarTextMatchActiveSuggestionSuggestion:
      return "spellingGrammarTextMatchActiveSuggestionSuggestion";
  }

  return "";
}

ax::mojom::MarkerType ParseMarkerType(const char* marker_type) {
  if (0 == strcmp(marker_type, "none"))
    return ax::mojom::MarkerType::kNone;
  if (0 == strcmp(marker_type, "spelling"))
    return ax::mojom::MarkerType::kSpelling;
  if (0 == strcmp(marker_type, "grammar"))
    return ax::mojom::MarkerType::kGrammar;
  if (0 == strcmp(marker_type, "spellingGrammar"))
    return ax::mojom::MarkerType::kSpellingGrammar;
  if (0 == strcmp(marker_type, "textMatch"))
    return ax::mojom::MarkerType::kTextMatch;
  if (0 == strcmp(marker_type, "spellingTextMatch"))
    return ax::mojom::MarkerType::kSpellingTextMatch;
  if (0 == strcmp(marker_type, "grammarTextMatch"))
    return ax::mojom::MarkerType::kGrammarTextMatch;
  if (0 == strcmp(marker_type, "spellingGrammarTextMatch"))
    return ax::mojom::MarkerType::kSpellingGrammarTextMatch;
  if (0 == strcmp(marker_type, "activeSuggestion"))
    return ax::mojom::MarkerType::kActiveSuggestion;
  if (0 == strcmp(marker_type, "spellingActiveSuggestion"))
    return ax::mojom::MarkerType::kSpellingActiveSuggestion;
  if (0 == strcmp(marker_type, "grammarActiveSuggestion"))
    return ax::mojom::MarkerType::kGrammarActiveSuggestion;
  if (0 == strcmp(marker_type, "spellingGrammarActiveSuggestion"))
    return ax::mojom::MarkerType::kSpellingGrammarActiveSuggestion;
  if (0 == strcmp(marker_type, "textMatchActiveSuggestion"))
    return ax::mojom::MarkerType::kTextMatchActiveSuggestion;
  if (0 == strcmp(marker_type, "spellingTextMatchActiveSuggestion"))
    return ax::mojom::MarkerType::kSpellingTextMatchActiveSuggestion;
  if (0 == strcmp(marker_type, "grammarTextMatchActiveSuggestion"))
    return ax::mojom::MarkerType::kGrammarTextMatchActiveSuggestion;
  if (0 == strcmp(marker_type, "spellingGrammarTextMatchActiveSuggestion"))
    return ax::mojom::MarkerType::kSpellingGrammarTextMatchActiveSuggestion;
  if (0 == strcmp(marker_type, "suggestion"))
    return ax::mojom::MarkerType::kSuggestion;
  if (0 == strcmp(marker_type, "spellingSuggestion"))
    return ax::mojom::MarkerType::kSpellingSuggestion;
  if (0 == strcmp(marker_type, "grammarSuggestion"))
    return ax::mojom::MarkerType::kGrammarSuggestion;
  if (0 == strcmp(marker_type, "spellingGrammarSuggestion"))
    return ax::mojom::MarkerType::kSpellingGrammarSuggestion;
  if (0 == strcmp(marker_type, "textMatchSuggestion"))
    return ax::mojom::MarkerType::kTextMatchSuggestion;
  if (0 == strcmp(marker_type, "spellingTextMatchSuggestion"))
    return ax::mojom::MarkerType::kSpellingTextMatchSuggestion;
  if (0 == strcmp(marker_type, "grammarTextMatchSuggestion"))
    return ax::mojom::MarkerType::kGrammarTextMatchSuggestion;
  if (0 == strcmp(marker_type, "spellingGrammarTextMatchSuggestion"))
    return ax::mojom::MarkerType::kSpellingGrammarTextMatchSuggestion;
  if (0 == strcmp(marker_type, "activeSuggestionSuggestion"))
    return ax::mojom::MarkerType::kActiveSuggestionSuggestion;
  if (0 == strcmp(marker_type, "spellingActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::kSpellingActiveSuggestionSuggestion;
  if (0 == strcmp(marker_type, "grammarActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::kGrammarActiveSuggestionSuggestion;
  if (0 == strcmp(marker_type, "spellingGrammarActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::kSpellingGrammarActiveSuggestionSuggestion;
  if (0 == strcmp(marker_type, "textMatchActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::kTextMatchActiveSuggestionSuggestion;
  if (0 == strcmp(marker_type, "spellingTextMatchActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::kSpellingTextMatchActiveSuggestionSuggestion;
  if (0 == strcmp(marker_type, "grammarTextMatchActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::kGrammarTextMatchActiveSuggestionSuggestion;
  if (0 ==
      strcmp(marker_type, "spellingGrammarTextMatchActiveSuggestionSuggestion"))
    return ax::mojom::MarkerType::
        kSpellingGrammarTextMatchActiveSuggestionSuggestion;
  return ax::mojom::MarkerType::kNone;
}

const char* ToString(ax::mojom::TextDirection text_direction) {
  switch (text_direction) {
    case ax::mojom::TextDirection::kNone:
      return "none";
    case ax::mojom::TextDirection::kLtr:
      return "ltr";
    case ax::mojom::TextDirection::kRtl:
      return "rtl";
    case ax::mojom::TextDirection::kTtb:
      return "ttb";
    case ax::mojom::TextDirection::kBtt:
      return "btt";
  }

  return "";
}

ax::mojom::TextDirection ParseTextDirection(const char* text_direction) {
  if (0 == strcmp(text_direction, "none"))
    return ax::mojom::TextDirection::kNone;
  if (0 == strcmp(text_direction, "ltr"))
    return ax::mojom::TextDirection::kLtr;
  if (0 == strcmp(text_direction, "rtl"))
    return ax::mojom::TextDirection::kRtl;
  if (0 == strcmp(text_direction, "ttb"))
    return ax::mojom::TextDirection::kTtb;
  if (0 == strcmp(text_direction, "btt"))
    return ax::mojom::TextDirection::kBtt;
  return ax::mojom::TextDirection::kNone;
}

const char* ToString(ax::mojom::TextStyle text_style) {
  switch (text_style) {
    case ax::mojom::TextStyle::kNone:
      return "none";
    case ax::mojom::TextStyle::kTextStyleBold:
      return "textStyleBold";
    case ax::mojom::TextStyle::kTextStyleItalic:
      return "textStyleItalic";
    case ax::mojom::TextStyle::kTextStyleBoldItalic:
      return "textStyleBoldItalic";
    case ax::mojom::TextStyle::kTextStyleUnderline:
      return "textStyleUnderline";
    case ax::mojom::TextStyle::kTextStyleBoldUnderline:
      return "textStyleBoldUnderline";
    case ax::mojom::TextStyle::kTextStyleItalicUnderline:
      return "textStyleItalicUnderline";
    case ax::mojom::TextStyle::kTextStyleBoldItalicUnderline:
      return "textStyleBoldItalicUnderline";
    case ax::mojom::TextStyle::kTextStyleLineThrough:
      return "textStyleLineThrough";
    case ax::mojom::TextStyle::kTextStyleBoldLineThrough:
      return "textStyleBoldLineThrough";
    case ax::mojom::TextStyle::kTextStyleItalicLineThrough:
      return "textStyleItalicLineThrough";
    case ax::mojom::TextStyle::kTextStyleBoldItalicLineThrough:
      return "textStyleBoldItalicLineThrough";
    case ax::mojom::TextStyle::kTextStyleUnderlineLineThrough:
      return "textStyleUnderlineLineThrough";
    case ax::mojom::TextStyle::kTextStyleBoldUnderlineLineThrough:
      return "textStyleBoldUnderlineLineThrough";
    case ax::mojom::TextStyle::kTextStyleItalicUnderlineLineThrough:
      return "textStyleItalicUnderlineLineThrough";
    case ax::mojom::TextStyle::kTextStyleBoldItalicUnderlineLineThrough:
      return "textStyleBoldItalicUnderlineLineThrough";
  }

  return "";
}

ax::mojom::TextStyle ParseTextStyle(const char* text_style) {
  if (0 == strcmp(text_style, "none"))
    return ax::mojom::TextStyle::kNone;
  if (0 == strcmp(text_style, "textStyleBold"))
    return ax::mojom::TextStyle::kTextStyleBold;
  if (0 == strcmp(text_style, "textStyleItalic"))
    return ax::mojom::TextStyle::kTextStyleItalic;
  if (0 == strcmp(text_style, "textStyleBoldItalic"))
    return ax::mojom::TextStyle::kTextStyleBoldItalic;
  if (0 == strcmp(text_style, "textStyleUnderline"))
    return ax::mojom::TextStyle::kTextStyleUnderline;
  if (0 == strcmp(text_style, "textStyleBoldUnderline"))
    return ax::mojom::TextStyle::kTextStyleBoldUnderline;
  if (0 == strcmp(text_style, "textStyleItalicUnderline"))
    return ax::mojom::TextStyle::kTextStyleItalicUnderline;
  if (0 == strcmp(text_style, "textStyleBoldItalicUnderline"))
    return ax::mojom::TextStyle::kTextStyleBoldItalicUnderline;
  if (0 == strcmp(text_style, "textStyleLineThrough"))
    return ax::mojom::TextStyle::kTextStyleLineThrough;
  if (0 == strcmp(text_style, "textStyleBoldLineThrough"))
    return ax::mojom::TextStyle::kTextStyleBoldLineThrough;
  if (0 == strcmp(text_style, "textStyleItalicLineThrough"))
    return ax::mojom::TextStyle::kTextStyleItalicLineThrough;
  if (0 == strcmp(text_style, "textStyleBoldItalicLineThrough"))
    return ax::mojom::TextStyle::kTextStyleBoldItalicLineThrough;
  if (0 == strcmp(text_style, "textStyleUnderlineLineThrough"))
    return ax::mojom::TextStyle::kTextStyleUnderlineLineThrough;
  if (0 == strcmp(text_style, "textStyleBoldUnderlineLineThrough"))
    return ax::mojom::TextStyle::kTextStyleBoldUnderlineLineThrough;
  if (0 == strcmp(text_style, "textStyleItalicUnderlineLineThrough"))
    return ax::mojom::TextStyle::kTextStyleItalicUnderlineLineThrough;
  if (0 == strcmp(text_style, "textStyleBoldItalicUnderlineLineThrough"))
    return ax::mojom::TextStyle::kTextStyleBoldItalicUnderlineLineThrough;
  return ax::mojom::TextStyle::kNone;
}

const char* ToString(ax::mojom::AriaCurrentState aria_current_state) {
  switch (aria_current_state) {
    case ax::mojom::AriaCurrentState::kNone:
      return "none";
    case ax::mojom::AriaCurrentState::kFalse:
      return "false";
    case ax::mojom::AriaCurrentState::kTrue:
      return "true";
    case ax::mojom::AriaCurrentState::kPage:
      return "page";
    case ax::mojom::AriaCurrentState::kStep:
      return "step";
    case ax::mojom::AriaCurrentState::kLocation:
      return "location";
    case ax::mojom::AriaCurrentState::kUnclippedLocation:
      return "unclippedLocation";
    case ax::mojom::AriaCurrentState::kDate:
      return "date";
    case ax::mojom::AriaCurrentState::kTime:
      return "time";
  }

  return "";
}

ax::mojom::AriaCurrentState ParseAriaCurrentState(
    const char* aria_current_state) {
  if (0 == strcmp(aria_current_state, "none"))
    return ax::mojom::AriaCurrentState::kNone;
  if (0 == strcmp(aria_current_state, "false"))
    return ax::mojom::AriaCurrentState::kFalse;
  if (0 == strcmp(aria_current_state, "true"))
    return ax::mojom::AriaCurrentState::kTrue;
  if (0 == strcmp(aria_current_state, "page"))
    return ax::mojom::AriaCurrentState::kPage;
  if (0 == strcmp(aria_current_state, "step"))
    return ax::mojom::AriaCurrentState::kStep;
  if (0 == strcmp(aria_current_state, "location"))
    return ax::mojom::AriaCurrentState::kLocation;
  if (0 == strcmp(aria_current_state, "unclippedLocation"))
    return ax::mojom::AriaCurrentState::kUnclippedLocation;
  if (0 == strcmp(aria_current_state, "date"))
    return ax::mojom::AriaCurrentState::kDate;
  if (0 == strcmp(aria_current_state, "time"))
    return ax::mojom::AriaCurrentState::kTime;
  return ax::mojom::AriaCurrentState::kNone;
}

const char* ToString(ax::mojom::InvalidState invalid_state) {
  switch (invalid_state) {
    case ax::mojom::InvalidState::kNone:
      return "none";
    case ax::mojom::InvalidState::kFalse:
      return "false";
    case ax::mojom::InvalidState::kTrue:
      return "true";
    case ax::mojom::InvalidState::kSpelling:
      return "spelling";
    case ax::mojom::InvalidState::kGrammar:
      return "grammar";
    case ax::mojom::InvalidState::kOther:
      return "other";
  }

  return "";
}

ax::mojom::InvalidState ParseInvalidState(const char* invalid_state) {
  if (0 == strcmp(invalid_state, "none"))
    return ax::mojom::InvalidState::kNone;
  if (0 == strcmp(invalid_state, "false"))
    return ax::mojom::InvalidState::kFalse;
  if (0 == strcmp(invalid_state, "true"))
    return ax::mojom::InvalidState::kTrue;
  if (0 == strcmp(invalid_state, "spelling"))
    return ax::mojom::InvalidState::kSpelling;
  if (0 == strcmp(invalid_state, "grammar"))
    return ax::mojom::InvalidState::kGrammar;
  if (0 == strcmp(invalid_state, "other"))
    return ax::mojom::InvalidState::kOther;
  return ax::mojom::InvalidState::kNone;
}

const char* ToString(ax::mojom::Restriction restriction) {
  switch (restriction) {
    case ax::mojom::Restriction::kNone:
      return "none";
    case ax::mojom::Restriction::kReadOnly:
      return "readOnly";
    case ax::mojom::Restriction::kDisabled:
      return "disabled";
  }

  return "";
}

ax::mojom::Restriction ParseRestriction(const char* restriction) {
  if (0 == strcmp(restriction, "none"))
    return ax::mojom::Restriction::kNone;
  if (0 == strcmp(restriction, "readOnly"))
    return ax::mojom::Restriction::kReadOnly;
  if (0 == strcmp(restriction, "disabled"))
    return ax::mojom::Restriction::kDisabled;
  return ax::mojom::Restriction::kNone;
}

const char* ToString(ax::mojom::CheckedState checked_state) {
  switch (checked_state) {
    case ax::mojom::CheckedState::kNone:
      return "none";
    case ax::mojom::CheckedState::kFalse:
      return "false";
    case ax::mojom::CheckedState::kTrue:
      return "true";
    case ax::mojom::CheckedState::kMixed:
      return "mixed";
  }

  return "";
}

ax::mojom::CheckedState ParseCheckedState(const char* checked_state) {
  if (0 == strcmp(checked_state, "none"))
    return ax::mojom::CheckedState::kNone;
  if (0 == strcmp(checked_state, "false"))
    return ax::mojom::CheckedState::kFalse;
  if (0 == strcmp(checked_state, "true"))
    return ax::mojom::CheckedState::kTrue;
  if (0 == strcmp(checked_state, "mixed"))
    return ax::mojom::CheckedState::kMixed;
  return ax::mojom::CheckedState::kNone;
}

const char* ToString(ax::mojom::SortDirection sort_direction) {
  switch (sort_direction) {
    case ax::mojom::SortDirection::kNone:
      return "none";
    case ax::mojom::SortDirection::kUnsorted:
      return "unsorted";
    case ax::mojom::SortDirection::kAscending:
      return "ascending";
    case ax::mojom::SortDirection::kDescending:
      return "descending";
    case ax::mojom::SortDirection::kOther:
      return "other";
  }

  return "";
}

ax::mojom::SortDirection ParseSortDirection(const char* sort_direction) {
  if (0 == strcmp(sort_direction, "none"))
    return ax::mojom::SortDirection::kNone;
  if (0 == strcmp(sort_direction, "unsorted"))
    return ax::mojom::SortDirection::kUnsorted;
  if (0 == strcmp(sort_direction, "ascending"))
    return ax::mojom::SortDirection::kAscending;
  if (0 == strcmp(sort_direction, "descending"))
    return ax::mojom::SortDirection::kDescending;
  if (0 == strcmp(sort_direction, "other"))
    return ax::mojom::SortDirection::kOther;
  return ax::mojom::SortDirection::kNone;
}

const char* ToString(ax::mojom::NameFrom name_from) {
  switch (name_from) {
    case ax::mojom::NameFrom::kNone:
      return "none";
    case ax::mojom::NameFrom::kUninitialized:
      return "uninitialized";
    case ax::mojom::NameFrom::kAttribute:
      return "attribute";
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
      return "attributeExplicitlyEmpty";
    case ax::mojom::NameFrom::kContents:
      return "contents";
    case ax::mojom::NameFrom::kPlaceholder:
      return "placeholder";
    case ax::mojom::NameFrom::kRelatedElement:
      return "relatedElement";
    case ax::mojom::NameFrom::kValue:
      return "value";
  }

  return "";
}

ax::mojom::NameFrom ParseNameFrom(const char* name_from) {
  if (0 == strcmp(name_from, "none"))
    return ax::mojom::NameFrom::kNone;
  if (0 == strcmp(name_from, "uninitialized"))
    return ax::mojom::NameFrom::kUninitialized;
  if (0 == strcmp(name_from, "attribute"))
    return ax::mojom::NameFrom::kAttribute;
  if (0 == strcmp(name_from, "attributeExplicitlyEmpty"))
    return ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
  if (0 == strcmp(name_from, "contents"))
    return ax::mojom::NameFrom::kContents;
  if (0 == strcmp(name_from, "placeholder"))
    return ax::mojom::NameFrom::kPlaceholder;
  if (0 == strcmp(name_from, "relatedElement"))
    return ax::mojom::NameFrom::kRelatedElement;
  if (0 == strcmp(name_from, "value"))
    return ax::mojom::NameFrom::kValue;
  return ax::mojom::NameFrom::kNone;
}

const char* ToString(ax::mojom::DescriptionFrom description_from) {
  switch (description_from) {
    case ax::mojom::DescriptionFrom::kNone:
      return "none";
    case ax::mojom::DescriptionFrom::kUninitialized:
      return "uninitialized";
    case ax::mojom::DescriptionFrom::kAttribute:
      return "attribute";
    case ax::mojom::DescriptionFrom::kContents:
      return "contents";
    case ax::mojom::DescriptionFrom::kPlaceholder:
      return "placeholder";
    case ax::mojom::DescriptionFrom::kRelatedElement:
      return "relatedElement";
  }

  return "";
}

ax::mojom::DescriptionFrom ParseDescriptionFrom(const char* description_from) {
  if (0 == strcmp(description_from, "none"))
    return ax::mojom::DescriptionFrom::kNone;
  if (0 == strcmp(description_from, "uninitialized"))
    return ax::mojom::DescriptionFrom::kUninitialized;
  if (0 == strcmp(description_from, "attribute"))
    return ax::mojom::DescriptionFrom::kAttribute;
  if (0 == strcmp(description_from, "contents"))
    return ax::mojom::DescriptionFrom::kContents;
  if (0 == strcmp(description_from, "placeholder"))
    return ax::mojom::DescriptionFrom::kPlaceholder;
  if (0 == strcmp(description_from, "relatedElement"))
    return ax::mojom::DescriptionFrom::kRelatedElement;
  return ax::mojom::DescriptionFrom::kNone;
}

const char* ToString(ax::mojom::EventFrom event_from) {
  switch (event_from) {
    case ax::mojom::EventFrom::kNone:
      return "none";
    case ax::mojom::EventFrom::kUser:
      return "user";
    case ax::mojom::EventFrom::kPage:
      return "page";
    case ax::mojom::EventFrom::kAction:
      return "action";
  }

  return "";
}

ax::mojom::EventFrom ParseEventFrom(const char* event_from) {
  if (0 == strcmp(event_from, "none"))
    return ax::mojom::EventFrom::kNone;
  if (0 == strcmp(event_from, "user"))
    return ax::mojom::EventFrom::kUser;
  if (0 == strcmp(event_from, "page"))
    return ax::mojom::EventFrom::kPage;
  if (0 == strcmp(event_from, "action"))
    return ax::mojom::EventFrom::kAction;
  return ax::mojom::EventFrom::kNone;
}

const char* ToString(ax::mojom::Gesture gesture) {
  switch (gesture) {
    case ax::mojom::Gesture::kNone:
      return "none";
    case ax::mojom::Gesture::kClick:
      return "click";
    case ax::mojom::Gesture::kSwipeLeft1:
      return "swipeLeft1";
    case ax::mojom::Gesture::kSwipeUp1:
      return "swipeUp1";
    case ax::mojom::Gesture::kSwipeRight1:
      return "swipeRight1";
    case ax::mojom::Gesture::kSwipeDown1:
      return "swipeDown1";
    case ax::mojom::Gesture::kSwipeLeft2:
      return "swipeLeft2";
    case ax::mojom::Gesture::kSwipeUp2:
      return "swipeUp2";
    case ax::mojom::Gesture::kSwipeRight2:
      return "swipeRight2";
    case ax::mojom::Gesture::kSwipeDown2:
      return "swipeDown2";
    case ax::mojom::Gesture::kSwipeLeft3:
      return "swipeLeft3";
    case ax::mojom::Gesture::kSwipeUp3:
      return "swipeUp3";
    case ax::mojom::Gesture::kSwipeRight3:
      return "swipeRight3";
    case ax::mojom::Gesture::kSwipeDown3:
      return "swipeDown3";
    case ax::mojom::Gesture::kSwipeLeft4:
      return "swipeLeft4";
    case ax::mojom::Gesture::kSwipeUp4:
      return "swipeUp4";
    case ax::mojom::Gesture::kSwipeRight4:
      return "swipeRight4";
    case ax::mojom::Gesture::kSwipeDown4:
      return "swipeDown4";
    case ax::mojom::Gesture::kTap2:
      return "tap2";
  }

  return "";
}

ax::mojom::Gesture ParseGesture(const char* gesture) {
  if (0 == strcmp(gesture, "none"))
    return ax::mojom::Gesture::kNone;
  if (0 == strcmp(gesture, "click"))
    return ax::mojom::Gesture::kClick;
  if (0 == strcmp(gesture, "swipeLeft1"))
    return ax::mojom::Gesture::kSwipeLeft1;
  if (0 == strcmp(gesture, "swipeUp1"))
    return ax::mojom::Gesture::kSwipeUp1;
  if (0 == strcmp(gesture, "swipeRight1"))
    return ax::mojom::Gesture::kSwipeRight1;
  if (0 == strcmp(gesture, "swipeDown1"))
    return ax::mojom::Gesture::kSwipeDown1;
  if (0 == strcmp(gesture, "swipeLeft2"))
    return ax::mojom::Gesture::kSwipeLeft2;
  if (0 == strcmp(gesture, "swipeUp2"))
    return ax::mojom::Gesture::kSwipeUp2;
  if (0 == strcmp(gesture, "swipeRight2"))
    return ax::mojom::Gesture::kSwipeRight2;
  if (0 == strcmp(gesture, "swipeDown2"))
    return ax::mojom::Gesture::kSwipeDown2;
  if (0 == strcmp(gesture, "swipeLeft3"))
    return ax::mojom::Gesture::kSwipeLeft3;
  if (0 == strcmp(gesture, "swipeUp3"))
    return ax::mojom::Gesture::kSwipeUp3;
  if (0 == strcmp(gesture, "swipeRight3"))
    return ax::mojom::Gesture::kSwipeRight3;
  if (0 == strcmp(gesture, "swipeDown3"))
    return ax::mojom::Gesture::kSwipeDown3;
  if (0 == strcmp(gesture, "swipeLeft4"))
    return ax::mojom::Gesture::kSwipeLeft4;
  if (0 == strcmp(gesture, "swipeUp4"))
    return ax::mojom::Gesture::kSwipeUp4;
  if (0 == strcmp(gesture, "swipeRight4"))
    return ax::mojom::Gesture::kSwipeRight4;
  if (0 == strcmp(gesture, "swipeDown4"))
    return ax::mojom::Gesture::kSwipeDown4;
  if (0 == strcmp(gesture, "tap2"))
    return ax::mojom::Gesture::kTap2;
  return ax::mojom::Gesture::kNone;
}

const char* ToString(ax::mojom::TextAffinity text_affinity) {
  switch (text_affinity) {
    case ax::mojom::TextAffinity::kNone:
      return "none";
    case ax::mojom::TextAffinity::kDownstream:
      return "downstream";
    case ax::mojom::TextAffinity::kUpstream:
      return "upstream";
  }

  return "";
}

ax::mojom::TextAffinity ParseTextAffinity(const char* text_affinity) {
  if (0 == strcmp(text_affinity, "none"))
    return ax::mojom::TextAffinity::kNone;
  if (0 == strcmp(text_affinity, "downstream"))
    return ax::mojom::TextAffinity::kDownstream;
  if (0 == strcmp(text_affinity, "upstream"))
    return ax::mojom::TextAffinity::kUpstream;
  return ax::mojom::TextAffinity::kNone;
}

const char* ToString(ax::mojom::TreeOrder tree_order) {
  switch (tree_order) {
    case ax::mojom::TreeOrder::kNone:
      return "none";
    case ax::mojom::TreeOrder::kUndefined:
      return "undefined";
    case ax::mojom::TreeOrder::kBefore:
      return "before";
    case ax::mojom::TreeOrder::kEqual:
      return "equal";
    case ax::mojom::TreeOrder::kAfter:
      return "after";
  }

  return "";
}

ax::mojom::TreeOrder ParseTreeOrder(const char* tree_order) {
  if (0 == strcmp(tree_order, "none"))
    return ax::mojom::TreeOrder::kNone;
  if (0 == strcmp(tree_order, "undefined"))
    return ax::mojom::TreeOrder::kUndefined;
  if (0 == strcmp(tree_order, "before"))
    return ax::mojom::TreeOrder::kBefore;
  if (0 == strcmp(tree_order, "equal"))
    return ax::mojom::TreeOrder::kEqual;
  if (0 == strcmp(tree_order, "after"))
    return ax::mojom::TreeOrder::kAfter;
  return ax::mojom::TreeOrder::kNone;
}

}  // namespace ui
