// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/pickle.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/web_io_operators.h"

using WebKit::WebData;
using WebKit::WebHistoryItem;
using WebKit::WebHTTPBody;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebUChar;
using WebKit::WebVector;

namespace {
class GlueSerializeTest : public testing::Test {
 public:
  // Makes a FormData with some random data.
  WebHTTPBody MakeFormData() {
    WebHTTPBody http_body;
    http_body.initialize();

    const char d1[] = "first data block";
    http_body.appendData(WebData(d1, sizeof(d1)-1));

    http_body.appendFile(WebString::fromUTF8("file.txt"));

    const char d2[] = "data the second";
    http_body.appendData(WebData(d2, sizeof(d2)-1));

    return http_body;
  }

  WebHTTPBody MakeFormDataWithPasswords() {
    WebHTTPBody http_body = MakeFormData();
    http_body.setContainsPasswordData(true);
    return http_body;
  }

  // Constructs a HistoryItem with some random data and an optional child.
  WebHistoryItem MakeHistoryItem(bool with_form_data, bool pregnant) {
    WebHistoryItem item;
    item.initialize();

    item.setURLString(WebString::fromUTF8("urlString"));
    item.setOriginalURLString(WebString::fromUTF8("originalURLString"));
    item.setTarget(WebString::fromUTF8("target"));
    item.setParent(WebString::fromUTF8("parent"));
    item.setTitle(WebString::fromUTF8("title"));
    item.setAlternateTitle(WebString::fromUTF8("alternateTitle"));
    item.setLastVisitedTime(13.37);
    item.setScrollOffset(WebPoint(42, -42));
    item.setIsTargetItem(true);
    item.setVisitCount(42*42);
    item.setPageScaleFactor(2.0f);
    item.setItemSequenceNumber(123);
    item.setDocumentSequenceNumber(456);

    WebVector<WebString> document_state(size_t(3));
    document_state[0] = WebString::fromUTF8("state1");
    document_state[1] = WebString::fromUTF8("state2");
    document_state[2] = WebString::fromUTF8("state AWESOME");
    item.setDocumentState(document_state);

    // Form Data
    if (with_form_data) {
      item.setHTTPBody(MakeFormData());
      item.setHTTPContentType(WebString::fromUTF8("formContentType"));
    }

    // Setting the FormInfo causes the referrer to be set, so we set the
    // referrer after setting the form info.
    item.setReferrer(WebString::fromUTF8("referrer"));

    // Children
    if (pregnant)
      item.appendToChildren(MakeHistoryItem(false, false));

    return item;
  }

  WebHistoryItem MakeHistoryItemWithPasswordData(bool pregnant) {
    WebHistoryItem item = MakeHistoryItem(false, pregnant);
    item.setHTTPBody(MakeFormDataWithPasswords());
    item.setHTTPContentType(WebString::fromUTF8("formContentType"));
    return item;
  }

  // Checks that a == b.
  void HistoryItemExpectEqual(const WebHistoryItem& a,
                              const WebHistoryItem& b,
                              int version) {
    HistoryItemExpectBaseDataEqual(a, b, version);
    HistoryItemExpectFormDataEqual(a, b);
    HistoryItemExpectChildrenEqual(a, b);
  }

  void HistoryItemExpectBaseDataEqual(const WebHistoryItem& a,
                                      const WebHistoryItem& b,
                                      int version) {
    EXPECT_EQ(string16(a.urlString()), string16(b.urlString()));
    EXPECT_EQ(string16(a.originalURLString()), string16(b.originalURLString()));
    EXPECT_EQ(string16(a.target()), string16(b.target()));
    EXPECT_EQ(string16(a.parent()), string16(b.parent()));
    EXPECT_EQ(string16(a.title()), string16(b.title()));
    EXPECT_EQ(string16(a.alternateTitle()), string16(b.alternateTitle()));
    EXPECT_EQ(a.lastVisitedTime(), b.lastVisitedTime());
    EXPECT_EQ(a.scrollOffset(), b.scrollOffset());
    EXPECT_EQ(a.isTargetItem(), b.isTargetItem());
    EXPECT_EQ(a.visitCount(), b.visitCount());
    EXPECT_EQ(string16(a.referrer()), string16(b.referrer()));
    if (version >= 11)
      EXPECT_EQ(a.pageScaleFactor(), b.pageScaleFactor());
    if (version >= 9)
      EXPECT_EQ(a.itemSequenceNumber(), b.itemSequenceNumber());
    if (version >= 6)
      EXPECT_EQ(a.documentSequenceNumber(), b.documentSequenceNumber());

    const WebVector<WebString>& a_docstate = a.documentState();
    const WebVector<WebString>& b_docstate = b.documentState();
    EXPECT_EQ(a_docstate.size(), b_docstate.size());
    for (size_t i = 0, c = a_docstate.size(); i < c; ++i)
      EXPECT_EQ(string16(a_docstate[i]), string16(b_docstate[i]));
  }

  void HistoryItemExpectFormDataEqual(const WebHistoryItem& a,
                                      const WebHistoryItem& b) {
    const WebHTTPBody& a_body = a.httpBody();
    const WebHTTPBody& b_body = b.httpBody();
    EXPECT_EQ(!a_body.isNull(), !b_body.isNull());
    if (!a_body.isNull() && !b_body.isNull()) {
      EXPECT_EQ(a_body.containsPasswordData(), b_body.containsPasswordData());
      EXPECT_EQ(a_body.elementCount(), b_body.elementCount());
      WebHTTPBody::Element a_elem, b_elem;
      for (size_t i = 0; a_body.elementAt(i, a_elem) &&
                         b_body.elementAt(i, b_elem); ++i) {
        EXPECT_EQ(a_elem.type, b_elem.type);
        if (a_elem.type == WebHTTPBody::Element::TypeData) {
          EXPECT_EQ(std::string(a_elem.data.data(), a_elem.data.size()),
                    std::string(b_elem.data.data(), b_elem.data.size()));
        } else {
          EXPECT_EQ(string16(a_elem.filePath), string16(b_elem.filePath));
        }
      }
    }
    EXPECT_EQ(string16(a.httpContentType()), string16(b.httpContentType()));
  }

  void HistoryItemExpectChildrenEqual(const WebHistoryItem& a,
                                      const WebHistoryItem& b) {
    const WebVector<WebHistoryItem>& a_children = a.children();
    const WebVector<WebHistoryItem>& b_children = b.children();
    EXPECT_EQ(a_children.size(), b_children.size());
    for (size_t i = 0, c = a_children.size(); i < c; ++i)
      HistoryItemExpectEqual(a_children[i], b_children[i],
                             webkit_glue::HistoryItemCurrentVersion());
  }
};

// Test old versions of serialized data to ensure that newer versions of code
// can still read history items written by previous versions.
TEST_F(GlueSerializeTest, BackwardsCompatibleTest) {
  const WebHistoryItem& item = MakeHistoryItem(false, false);

  // Make sure current version can read all previous versions.
  for (int i = 1; i < webkit_glue::HistoryItemCurrentVersion(); i++) {
    std::string serialized_item;
    webkit_glue::HistoryItemToVersionedString(item, i, &serialized_item);
    const WebHistoryItem& deserialized_item =
        webkit_glue::HistoryItemFromString(serialized_item);
    ASSERT_FALSE(item.isNull());
    ASSERT_FALSE(deserialized_item.isNull());
    HistoryItemExpectEqual(item, deserialized_item, i);
  }
}

// Makes sure that a HistoryItem remains intact after being serialized and
// deserialized.
TEST_F(GlueSerializeTest, HistoryItemSerializeTest) {
  const WebHistoryItem& item = MakeHistoryItem(true, true);
  const std::string& serialized_item = webkit_glue::HistoryItemToString(item);
  const WebHistoryItem& deserialized_item =
      webkit_glue::HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item.isNull());
  ASSERT_FALSE(deserialized_item.isNull());
  HistoryItemExpectEqual(item, deserialized_item,
                         webkit_glue::HistoryItemCurrentVersion());
}

// Checks that broken messages don't take out our process.
TEST_F(GlueSerializeTest, BadMessagesTest) {
  {
    Pickle p;
    // Version 1
    p.WriteInt(1);
    // Empty strings.
    for (int i = 0; i < 6; ++i)
      p.WriteInt(-1);
    // Bad real number.
    p.WriteInt(-1);
    std::string s(static_cast<const char*>(p.data()), p.size());
    webkit_glue::HistoryItemFromString(s);
  }
  {
    double d = 0;
    Pickle p;
    // Version 1
    p.WriteInt(1);
    // Empty strings.
    for (int i = 0; i < 6; ++i)
      p.WriteInt(-1);
    // More misc fields.
    p.WriteData(reinterpret_cast<const char*>(&d), sizeof(d));
    p.WriteInt(1);
    p.WriteInt(1);
    p.WriteInt(0);
    p.WriteInt(0);
    p.WriteInt(-1);
    p.WriteInt(0);
    // WebForm
    p.WriteInt(1);
    p.WriteInt(WebHTTPBody::Element::TypeData);
    std::string s(static_cast<const char*>(p.data()), p.size());
    webkit_glue::HistoryItemFromString(s);
  }
}

TEST_F(GlueSerializeTest, RemoveFormData) {
  const WebHistoryItem& item1 = MakeHistoryItem(true, true);
  std::string serialized_item = webkit_glue::HistoryItemToString(item1);
  serialized_item =
      webkit_glue::RemoveFormDataFromHistoryState(serialized_item);
  const WebHistoryItem& item2 =
      webkit_glue::HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item1.isNull());
  ASSERT_FALSE(item2.isNull());

  HistoryItemExpectBaseDataEqual(item1, item2,
                                 webkit_glue::HistoryItemCurrentVersion());
  HistoryItemExpectChildrenEqual(item1, item2);

  // Form data was removed, but the identifier was kept.
  const WebHTTPBody& body1 = item1.httpBody();
  const WebHTTPBody& body2 = item2.httpBody();
  EXPECT_FALSE(body1.isNull());
  EXPECT_FALSE(body2.isNull());
  EXPECT_GT(body1.elementCount(), 0U);
  EXPECT_EQ(0U, body2.elementCount());
  EXPECT_EQ(body1.identifier(), body2.identifier());
}

TEST_F(GlueSerializeTest, FilePathsFromHistoryState) {
  WebHistoryItem item = MakeHistoryItem(false, true);

  // Append file paths to item.
  FilePath file_path1(FILE_PATH_LITERAL("file.txt"));
  FilePath file_path2(FILE_PATH_LITERAL("another_file"));
  WebHTTPBody http_body;
  http_body.initialize();
  http_body.appendFile(webkit_base::FilePathToWebString(file_path1));
  http_body.appendFile(webkit_base::FilePathToWebString(file_path2));
  item.setHTTPBody(http_body);

  std::string serialized_item = webkit_glue::HistoryItemToString(item);
  const std::vector<FilePath>& file_paths =
      webkit_glue::FilePathsFromHistoryState(serialized_item);
  ASSERT_EQ(2U, file_paths.size());
  EXPECT_EQ(file_path1, file_paths[0]);
  EXPECT_EQ(file_path2, file_paths[1]);
}

// Makes sure that a HistoryItem containing password data remains intact after
// being serialized and deserialized.
TEST_F(GlueSerializeTest, HistoryItemWithPasswordsSerializeTest) {
  const WebHistoryItem& item = MakeHistoryItemWithPasswordData(true);
  const std::string& serialized_item = webkit_glue::HistoryItemToString(item);
  const WebHistoryItem& deserialized_item =
      webkit_glue::HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item.isNull());
  ASSERT_FALSE(deserialized_item.isNull());
  HistoryItemExpectEqual(item, deserialized_item,
                         webkit_glue::HistoryItemCurrentVersion());
}

TEST_F(GlueSerializeTest, RemovePasswordData) {
  const WebHistoryItem& item1 = MakeHistoryItemWithPasswordData(true);
  std::string serialized_item = webkit_glue::HistoryItemToString(item1);
  serialized_item =
      webkit_glue::RemovePasswordDataFromHistoryState(serialized_item);
  const WebHistoryItem& item2 =
      webkit_glue::HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item1.isNull());
  ASSERT_FALSE(item2.isNull());

  HistoryItemExpectBaseDataEqual(item1, item2,
                                 webkit_glue::HistoryItemCurrentVersion());
  HistoryItemExpectChildrenEqual(item1, item2);

  // Form data was removed, but the identifier was kept.
  const WebHTTPBody& body1 = item1.httpBody();
  const WebHTTPBody& body2 = item2.httpBody();
  EXPECT_FALSE(body1.isNull());
  EXPECT_FALSE(body2.isNull());
  EXPECT_GT(body1.elementCount(), 0U);
  EXPECT_EQ(0U, body2.elementCount());
  EXPECT_EQ(body1.identifier(), body2.identifier());
}

TEST_F(GlueSerializeTest, RemovePasswordDataWithNoPasswordData) {
  const WebHistoryItem& item1 = MakeHistoryItem(true, true);
  std::string serialized_item = webkit_glue::HistoryItemToString(item1);
  serialized_item =
      webkit_glue::RemovePasswordDataFromHistoryState(serialized_item);
  const WebHistoryItem& item2 =
      webkit_glue::HistoryItemFromString(serialized_item);

  ASSERT_FALSE(item1.isNull());
  ASSERT_FALSE(item2.isNull());

  // Form data was not removed.
  HistoryItemExpectEqual(item1, item2,
                         webkit_glue::HistoryItemCurrentVersion());
}

}  // namespace
