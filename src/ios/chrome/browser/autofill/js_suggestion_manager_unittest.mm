// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_suggestion_manager.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Test fixture to test suggestions.
class JsSuggestionManagerTest : public web::WebJsTest<ChromeWebTest> {
 protected:
  JsSuggestionManagerTest()
      : web::WebJsTest<ChromeWebTest>(@[ @"suggestion_controller" ]) {}
  // Loads the given HTML and initializes the Autofill JS scripts.
  void LoadHtml(NSString* html);
  // Helper method that initializes a form with three fields. Can be used to
  // test whether adding an attribute on the second field causes it to be
  // skipped (or not, as is appropriate) by selectNextElement.
  void SequentialNavigationSkipCheck(NSString* attribute, BOOL shouldSkip);
  // Returns the active element name from the JS side.
  NSString* GetActiveElementName() {
    return ExecuteJavaScript(@"document.activeElement.name");
  }
  JsSuggestionManager* manager_;
};

void JsSuggestionManagerTest::LoadHtml(NSString* html) {
  WebJsTest<ChromeWebTest>::LoadHtml(html);
  manager_ =
      static_cast<JsSuggestionManager*>([web_state()->GetJSInjectionReceiver()
          instanceOfClass:[JsSuggestionManager class]]);
  [manager_ inject];
}

TEST_F(JsSuggestionManagerTest, InitAndInject) {
  LoadHtml(@"<html></html>");
  EXPECT_TRUE([manager_ hasBeenInjected]);
}

TEST_F(JsSuggestionManagerTest, SelectElementInTabOrder) {
  NSString* htmlFragment =
      @"<html> <body>"
       "<input id='1 (0)' tabIndex=1 href='http://www.w3schools.com'>1 (0)</a>"
       "<input id='0 (0)' tabIndex=0 href='http://www.w3schools.com'>0 (0)</a>"
       "<input id='2' tabIndex=2 href='http://www.w3schools.com'>2</a>"
       "<input id='0 (1)' tabIndex=0 href='http://www.w3schools.com'>0 (1)</a>"
       "<input id='-2' tabIndex=-2 href='http://www.w3schools.com'>-2</a>"
       "<a href='http://www.w3schools.com'></a>"
       "<input id='-1 (0)' tabIndex=-1 href='http://www.w3schools.com'>-1</a>"
       "<input id='-2 (2)' tabIndex=-2 href='http://www.w3schools.com'>-2</a>"
       "<input id='0 (2)' tabIndex=0 href='http://www.w3schools.com'>0 - 2</a>"
       "<input id='3' tabIndex=3 href='http://www.w3schools.com'>3</a>"
       "<input id='1 (1)' tabIndex=1 href='http://www.w3schools.com'>1 (1)</a>"
       "<input id='-1 (1)' tabIndex=-1 href='http://www.w3schools.com'>-1 </a>"
       "<input id='0 (3)' tabIndex=0 href='http://www.w3schools.com'>0 (3)</a>"
       "</body></html>";
  LoadHtmlAndInject(htmlFragment);

  // clang-format off
  NSDictionary* next_expected_ids = @ {
      @"1 (0)"  : @"1 (1)",
      @"0 (0)"  : @"0 (1)",
      @"2"      : @"3",
      @"0 (1)"  : @"0 (2)",
      @"-2"     : @"0 (2)",
      @"-1 (0)" : @"0 (2)",
      @"-2 (2)" : @"0 (2)",
      @"0 (2)"  : @"0 (3)",
      @"3"      : @"0 (0)",
      @"1 (1)"  : @"2",
      @"-1 (1)" : @"0 (3)",
      @"0 (3)"  : @"null"
  };
  // clang-format on

  for (NSString* element_id : next_expected_ids) {
    NSString* expected_id = [next_expected_ids objectForKey:element_id];
    EXPECT_NSEQ(expected_id,
                ExecuteJavaScriptWithFormat(
                    @"var elements=document.getElementsByTagName('input');"
                     "var element=document.getElementById('%@');"
                     "var next = __gCrWeb.suggestion.getNextElementInTabOrder("
                     "    element, elements);"
                     "next ? next.id : 'null';",
                    element_id))
        << "Wrong when selecting next element of element with element id "
        << base::SysNSStringToUTF8(element_id);
  }
  EXPECT_NSEQ(@YES,
              ExecuteJavaScriptWithFormat(
                  @"var elements=document.getElementsByTagName('input');"
                   "var element=document.getElementsByTagName('a')[0];"
                   "var next = __gCrWeb.suggestion.getNextElementInTabOrder("
                   "    element, elements); next===null"))
      << "Wrong when selecting the next element of an element not in the "
      << "element list.";

  for (NSString* element_id : next_expected_ids) {
    NSString* expected_id = [next_expected_ids objectForKey:element_id];
    if ([expected_id isEqualToString:@"null"]) {
      // If the expected next element is null, the focus is not moved.
      expected_id = element_id;
    }
    EXPECT_NSEQ(expected_id, ExecuteJavaScriptWithFormat(
                                 @"document.getElementById('%@').focus();"
                                  "__gCrWeb.suggestion.selectNextElement();"
                                  "document.activeElement.id",
                                 element_id))
        << "Wrong when selecting next element with active element "
        << base::SysNSStringToUTF8(element_id);
  }

  for (NSString* element_id : next_expected_ids) {
    // If the expected next element is null, there is no next element.
    BOOL expected = ![next_expected_ids[element_id] isEqualToString:@"null"];
    EXPECT_NSEQ(@(expected), ExecuteJavaScriptWithFormat(
                                 @"document.getElementById('%@').focus();"
                                  "__gCrWeb.suggestion.hasNextElement()",
                                 element_id))
        << "Wrong when checking hasNextElement() for "
        << base::SysNSStringToUTF8(element_id);
  }

  // clang-format off
  NSDictionary* prev_expected_ids = @{
      @"1 (0)" : @"null",
      @"0 (0)" : @"3",
      @"2"     : @"1 (1)",
      @"0 (1)" : @"0 (0)",
      @"-2"    : @"0 (1)",
      @"-1 (0)": @"0 (1)",
      @"-2 (2)": @"0 (1)",
      @"0 (2)" : @"0 (1)",
      @"3"     : @"2",
      @"1 (1)" : @"1 (0)",
      @"-1 (1)": @"1 (1)",
      @"0 (3)" : @"0 (2)",
  };
  // clang-format on

  for (NSString* element_id : prev_expected_ids) {
    NSString* expected_id = [prev_expected_ids objectForKey:element_id];
    EXPECT_NSEQ(
        expected_id,
        ExecuteJavaScriptWithFormat(
            @"var elements=document.getElementsByTagName('input');"
             "var element=document.getElementById('%@');"
             "var prev = __gCrWeb.suggestion.getPreviousElementInTabOrder("
             "    element, elements);"
             "prev ? prev.id : 'null';",
            element_id))
        << "Wrong when selecting prev element of element with element id "
        << base::SysNSStringToUTF8(element_id);
  }
  EXPECT_NSEQ(
      @YES, ExecuteJavaScriptWithFormat(
                @"var elements=document.getElementsByTagName('input');"
                 "var element=document.getElementsByTagName('a')[0];"
                 "var prev = __gCrWeb.suggestion.getPreviousElementInTabOrder("
                 "    element, elements); prev===null"))
      << "Wrong when selecting the previous element of an element not in the "
      << "element list";

  for (NSString* element_id : prev_expected_ids) {
    NSString* expected_id = [prev_expected_ids objectForKey:element_id];
    if ([expected_id isEqualToString:@"null"]) {
      // If the expected previous element is null, the focus is not moved.
      expected_id = element_id;
    }
    EXPECT_NSEQ(expected_id, ExecuteJavaScriptWithFormat(
                                 @"document.getElementById('%@').focus();"
                                  "__gCrWeb.suggestion.selectPreviousElement();"
                                  "document.activeElement.id",
                                 element_id))
        << "Wrong when selecting previous element with active element "
        << base::SysNSStringToUTF8(element_id);
  }

  for (NSString* element_id : prev_expected_ids) {
    // If the expected next element is null, there is no next element.
    BOOL expected = ![prev_expected_ids[element_id] isEqualToString:@"null"];
    EXPECT_NSEQ(@(expected), ExecuteJavaScriptWithFormat(
                                 @"document.getElementById('%@').focus();"
                                  "__gCrWeb.suggestion.hasPreviousElement()",
                                 element_id))
        << "Wrong when checking hasPreviousElement() for "
        << base::SysNSStringToUTF8(element_id);
  }
}

TEST_F(JsSuggestionManagerTest, SequentialNavigation) {
  LoadHtml(@"<html><body><form name='testform' method='post'>"
            "<input type='text' name='firstname'/>"
            "<input type='text' name='lastname'/>"
            "<input type='email' name='email'/>"
            "</form></body></html>");

  [manager_
      executeJavaScript:@"document.getElementsByName('firstname')[0].focus()"
      completionHandler:nil];
  [manager_ selectNextElement];
  EXPECT_NSEQ(@"lastname", GetActiveElementName());
  __block BOOL block_was_called = NO;
  [manager_ fetchPreviousAndNextElementsPresenceWithCompletionHandler:^void(
                BOOL has_previous_element, BOOL has_next_element) {
    block_was_called = YES;
    EXPECT_TRUE(has_previous_element);
    EXPECT_TRUE(has_next_element);
  }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });
  [manager_ selectNextElement];
  EXPECT_NSEQ(@"email", GetActiveElementName());
  [manager_ selectPreviousElement];
  EXPECT_NSEQ(@"lastname", GetActiveElementName());
}

void JsSuggestionManagerTest::SequentialNavigationSkipCheck(NSString* attribute,
                                                            BOOL shouldSkip) {
  LoadHtml([NSString stringWithFormat:@"<html><body>"
                                       "<form name='testform' method='post'>"
                                       "<input type='text' name='firstname'/>"
                                       "<%@ name='middlename'/>"
                                       "<input type='text' name='lastname'/>"
                                       "</form></body></html>",
                                      attribute]);
  [manager_
      executeJavaScript:@"document.getElementsByName('firstname')[0].focus()"
      completionHandler:nil];
  NSString* const kActiveElementNameJS = @"document.activeElement.name";
  EXPECT_NSEQ(@"firstname",
              web::test::ExecuteJavaScript(manager_, kActiveElementNameJS));
  [manager_ selectNextElement];
  NSString* activeElementNameJS = GetActiveElementName();
  if (shouldSkip)
    EXPECT_NSEQ(@"lastname", activeElementNameJS);
  else
    EXPECT_NSEQ(@"middlename", activeElementNameJS);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationNoSkipText) {
  SequentialNavigationSkipCheck(@"input type='text'", NO);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationNoSkipTextArea) {
  SequentialNavigationSkipCheck(@"input type='textarea'", NO);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationOverInvisibleElement) {
  SequentialNavigationSkipCheck(@"input type='text' style='display:none'", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationOverHiddenElement) {
  SequentialNavigationSkipCheck(@"input type='text' style='visibility:hidden'",
                                YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationOverDisabledElement) {
  SequentialNavigationSkipCheck(@"type='text' disabled", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationNoSkipPassword) {
  SequentialNavigationSkipCheck(@"input type='password'", NO);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationSkipSubmit) {
  SequentialNavigationSkipCheck(@"input type='submit'", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationSkipImage) {
  SequentialNavigationSkipCheck(@"input type='image'", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationSkipButton) {
  SequentialNavigationSkipCheck(@"input type='button'", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationSkipRange) {
  SequentialNavigationSkipCheck(@"input type='range'", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationSkipRadio) {
  SequentialNavigationSkipCheck(@"type='radio'", YES);
}

TEST_F(JsSuggestionManagerTest, SequentialNavigationSkipCheckbox) {
  SequentialNavigationSkipCheck(@"type='checkbox'", YES);
}

// Special test for a condition where the closeKeyboard script would cause an
// illegal JS recursion if a blur event results in an event that triggers a
// crwebinvoke:// back, such as a page change.
TEST_F(JsSuggestionManagerTest, CloseKeyboardSafetyTest) {
  LoadHtml(@"<select id='select'>Select</select>");
  ExecuteJavaScript(
      @"select.onblur = function(){window.location.href = '#test'}");
  ExecuteJavaScript(@"select.focus()");
  // In the failure condition the app will crash during the next line.
  [manager_ closeKeyboard];
  // TODO(crbug.com/661624): add a check for the keyboard actually being
  // dismissed; unfortunately it is not known how to adapt
  // WaitForBackgroundTasks to yield for events wrapped with window.setTimeout()
  // or other deferred events.
}

// Test fixture to test
// |fetchPreviousAndNextElementsPresenceWithCompletionHandler|.
class FetchPreviousAndNextExceptionTest : public JsSuggestionManagerTest {
 public:
  void SetUp() override {
    JsSuggestionManagerTest::SetUp();
    LoadHtml(@"<html></html>");
  }

 protected:
  // Evaluates JS and tests that the completion handler passed to
  // |fetchPreviousAndNextElementsPresenceWithCompletionHandler| is called with
  // (NO, NO) indicating no previous and next element.
  void EvaluateJavaScriptAndExpectNoPreviousAndNextElement(NSString* js) {
    ExecuteJavaScript(js);
    __block BOOL block_was_called = NO;
    id completionHandler = ^(BOOL hasPreviousElement, BOOL hasNextElement) {
      EXPECT_FALSE(hasPreviousElement);
      EXPECT_FALSE(hasNextElement);
      block_was_called = YES;
    };
    [manager_ fetchPreviousAndNextElementsPresenceWithCompletionHandler:
                  completionHandler];
    base::test::ios::WaitUntilCondition(^bool() {
      return block_was_called;
    });
  }
};

// Tests that |fetchPreviousAndNextElementsPresenceWithCompletionHandler| works
// when |__gCrWeb.suggestion.hasPreviousElement| throws an exception.
TEST_F(FetchPreviousAndNextExceptionTest, HasPreviousElementException) {
  EvaluateJavaScriptAndExpectNoPreviousAndNextElement(
      @"__gCrWeb.suggestion.hasPreviousElement = function() { bar.foo1; }");
}

// Tests that |fetchPreviousAndNextElementsPresenceWithCompletionHandler| works
// when |__gCrWeb.suggestion.hasNextElement| throws an exception.
TEST_F(FetchPreviousAndNextExceptionTest, HasNextElementException) {
  EvaluateJavaScriptAndExpectNoPreviousAndNextElement(
      @"__gCrWeb.suggestion.hasNextElement = function() { bar.foo1; }");
}

// Tests that |fetchPreviousAndNextElementsPresenceWithCompletionHandler| works
// when |Array.toString| has been overridden to return a malformed string
// without a ",".
TEST_F(FetchPreviousAndNextExceptionTest, HasPreviousElementNull) {
  EvaluateJavaScriptAndExpectNoPreviousAndNextElement(
      @"Array.prototype.toString = function() { return 'Hello'; }");
}

}  // namespace
