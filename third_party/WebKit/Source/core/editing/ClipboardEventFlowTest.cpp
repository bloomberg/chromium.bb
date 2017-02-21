// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLButtonElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/layout/LayoutObject.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

namespace {
class MockEventListener : public EventListener {
 public:
  MockEventListener() : EventListener(EventListener::CPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext*, Event*));
};
}  // namespace

class ClipboardEventFlowTest : public EditingTestBase {
 private:
  void makeDocumentEmpty() {
    while (document().firstChild())
      document().removeChild(document().firstChild());
  }

  void setElementText(Element& element, const std::string& text) {
    element.setInnerHTML(String::fromUTF8(text.c_str()), ASSERT_NO_EXCEPTION);
    updateAllLifecyclePhases();
  }

  void setElementTextAndSelectIt(Element& element,
                                 const std::string& text,
                                 bool selectionEditable) {
    setElementText(element, text);

    frame().selection().setSelection(
        SelectionInDOMTree::Builder()
            .collapse(Position(element.firstChild(), 0))
            .extend(Position(element.firstChild(), text.size()))
            .build());

    element.setAttribute(HTMLNames::contenteditableAttr,
                         selectionEditable ? "true" : "false");
  }

 protected:
  void clipboardEventTargetDependsOnSelectionEditabilityTest(
      const char* command,
      bool selectionEditable) {
    using testing::_;

    auto* html = HTMLHtmlElement::create(document());
    auto* body = HTMLBodyElement::create(document());
    auto* focusableElement = HTMLButtonElement::create(document());
    auto* elementWithSelection = HTMLDivElement::create(document());

    auto* eventListenerInstalledOnFocusedElement = new MockEventListener;
    auto* eventListenerInstalledOnElementWithSelection = new MockEventListener;

    focusableElement->addEventListener(command,
                                       eventListenerInstalledOnFocusedElement);
    elementWithSelection->addEventListener(
        command, eventListenerInstalledOnElementWithSelection);

    makeDocumentEmpty();
    document().setDesignMode("on");

    body->appendChild(focusableElement);
    body->appendChild(elementWithSelection);
    html->appendChild(body);
    document().appendChild(html);

    focusableElement->focus();

    setElementTextAndSelectIt(*elementWithSelection, "some dummy text",
                              selectionEditable);

    // allow |document.execCommand| to access clipboard
    frame().settings()->setJavaScriptCanAccessClipboard(true);

    // test expectations
    EXPECT_CALL(*eventListenerInstalledOnFocusedElement, handleEvent(_, _))
        .Times(selectionEditable ? 0 : 1);

    EXPECT_CALL(*eventListenerInstalledOnElementWithSelection,
                handleEvent(_, _))
        .Times(selectionEditable ? 1 : 0);

    // execute command
    NonThrowableExceptionState exceptionState;
    document().execCommand(command, false, "", exceptionState);
  }
};

TEST_F(ClipboardEventFlowTest,
       copySetsClipboardEventTargetToActiveElementWhenSelectionIsNotEditable) {
  clipboardEventTargetDependsOnSelectionEditabilityTest("copy", false);
}

TEST_F(
    ClipboardEventFlowTest,
    copySetsClipboardEventTargetToElementWithSelectionWhenSelectionIsEditable) {
  clipboardEventTargetDependsOnSelectionEditabilityTest("copy", true);
}

TEST_F(ClipboardEventFlowTest,
       cutSetsClipboardEventTargetToActiveElementWhenSelectionIsNotEditable) {
  clipboardEventTargetDependsOnSelectionEditabilityTest("cut", false);
}

TEST_F(
    ClipboardEventFlowTest,
    cutSetsClipboardEventTargetToElementWithSelectionWhenSelectionIsEditable) {
  clipboardEventTargetDependsOnSelectionEditabilityTest("cut", true);
}

TEST_F(ClipboardEventFlowTest,
       pasteSetsClipboardEventTargetToActiveElementWhenSelectionIsNotEditable) {
  // allow |document.execCommand| to execute 'paste' command
  frame().settings()->setDOMPasteAllowed(true);

  clipboardEventTargetDependsOnSelectionEditabilityTest("paste", false);
}

TEST_F(
    ClipboardEventFlowTest,
    pasteSetsClipboardEventTargetToElementWithSelectionWhenSelectionIsEditable) {
  // allow |document.execCommand| to execute 'paste'
  frame().settings()->setDOMPasteAllowed(true);

  clipboardEventTargetDependsOnSelectionEditabilityTest("paste", true);
}
}  // namespace blink
