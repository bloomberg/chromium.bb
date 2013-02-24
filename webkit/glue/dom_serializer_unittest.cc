// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/hash_tables.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeCollection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebCString;
using WebKit::WebData;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebNodeCollection;
using WebKit::WebNodeList;
using WebKit::WebPageSerializer;
using WebKit::WebPageSerializerClient;
using WebKit::WebNode;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;
using WebKit::WebVector;

namespace {

// Iterate recursively over sub-frames to find one with with a given url.
WebFrame* FindSubFrameByURL(WebView* web_view, const GURL& url) {
  if (!web_view->mainFrame())
    return NULL;

  std::vector<WebFrame*> stack;
  stack.push_back(web_view->mainFrame());

  while (!stack.empty()) {
    WebFrame* current_frame = stack.back();
    stack.pop_back();
    if (GURL(current_frame->document().url()) == url)
      return current_frame;
    WebNodeCollection all = current_frame->document().all();
    for (WebNode node = all.firstItem();
         !node.isNull(); node = all.nextItem()) {
      if (!node.isElementNode())
        continue;
      // Check frame tag and iframe tag
      WebElement element = node.to<WebElement>();
      if (!element.hasTagName("frame") && !element.hasTagName("iframe"))
        continue;
      WebFrame* sub_frame = WebFrame::fromFrameOwnerElement(element);
      if (sub_frame)
        stack.push_back(sub_frame);
    }
  }
  return NULL;
}

class DomSerializerTests : public TestShellTest,
                           public WebPageSerializerClient {
 public:
  DomSerializerTests()
    : local_directory_name_(FILE_PATH_LITERAL("./dummy_files/")) { }

  // DomSerializerDelegate.
  virtual void didSerializeDataForFrame(const WebURL& frame_web_url,
                                        const WebCString& data,
                                        PageSerializationStatus status) {

    GURL frame_url(frame_web_url);
    // If the all frames are finished saving, check all finish status
    if (status == WebPageSerializerClient::AllFramesAreFinished) {
      SerializationFinishStatusMap::iterator it =
          serialization_finish_status_.begin();
      for (; it != serialization_finish_status_.end(); ++it)
        ASSERT_TRUE(it->second);
      serialized_ = true;
      return;
    }

    // Check finish status of current frame.
    SerializationFinishStatusMap::iterator it =
        serialization_finish_status_.find(frame_url.spec());
    // New frame, set initial status as false.
    if (it == serialization_finish_status_.end())
      serialization_finish_status_[frame_url.spec()] = false;

    it = serialization_finish_status_.find(frame_url.spec());
    ASSERT_TRUE(it != serialization_finish_status_.end());
    // In process frame, finish status should be false.
    ASSERT_FALSE(it->second);

    // Add data to corresponding frame's content.
    serialized_frame_map_[frame_url.spec()] += data.data();

    // Current frame is completed saving, change the finish status.
    if (status == WebPageSerializerClient::CurrentFrameIsFinished)
      it->second = true;
  }

  bool HasSerializedFrame(const GURL& frame_url) {
    return serialized_frame_map_.find(frame_url.spec()) !=
           serialized_frame_map_.end();
  }

  const std::string& GetSerializedContentForFrame(
      const GURL& frame_url) {
    return serialized_frame_map_[frame_url.spec()];
  }

  // Load web page according to specific URL.
  void LoadPageFromURL(const GURL& page_url) {
    // Load the test file.
    test_shell_->ResetTestController();
    test_shell_->LoadURL(page_url);
    test_shell_->WaitTestFinished();
  }

  // Load web page according to input content and relative URLs within
  // the document.
  void LoadContents(const std::string& contents,
                    const GURL& base_url,
                    const WebString encoding_info) {
    test_shell_->ResetTestController();
    // If input encoding is empty, use UTF-8 as default encoding.
    if (encoding_info.isEmpty()) {
      test_shell_->webView()->mainFrame()->loadHTMLString(contents, base_url);
    } else {
      WebData data(contents.data(), contents.length());

      // Do not use WebFrame.LoadHTMLString because it assumes that input
      // html contents use UTF-8 encoding.
      // TODO(darin): This should use WebFrame::loadData.
      WebFrame* web_frame =
          test_shell_->webView()->mainFrame();

      ASSERT_TRUE(web_frame != NULL);

      web_frame->loadData(data, "text/html", encoding_info, base_url);
    }

    test_shell_->WaitTestFinished();
  }

  // Serialize page DOM according to specific page URL. The parameter
  // recursive_serialization indicates whether we will serialize all
  // sub-frames.
  void SerializeDomForURL(const GURL& page_url,
                          bool recursive_serialization) {
    // Find corresponding WebFrame according to page_url.
    WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(),
                                            page_url);
    ASSERT_TRUE(web_frame != NULL);
    // Add input file URl to links_.
    links_.assign(&page_url,1);
    // Add dummy file path to local_path_.
    WebString file_path = webkit_base::FilePathStringToWebString(
        FILE_PATH_LITERAL("c:\\dummy.htm"));
    local_paths_.assign(&file_path, 1);
    // Start serializing DOM.
    bool result = WebPageSerializer::serialize(web_frame,
       recursive_serialization,
       static_cast<WebPageSerializerClient*>(this),
       links_,
       local_paths_,
       webkit_base::FilePathToWebString(local_directory_name_));
    ASSERT_TRUE(result);
    ASSERT_TRUE(serialized_);
  }

 private:
  // Map frame_url to corresponding serialized_content.
  typedef base::hash_map<std::string, std::string> SerializedFrameContentMap;
  SerializedFrameContentMap serialized_frame_map_;
  // Map frame_url to corresponding status of serialization finish.
  typedef base::hash_map<std::string, bool> SerializationFinishStatusMap;
  SerializationFinishStatusMap serialization_finish_status_;
  // Flag indicates whether the process of serializing DOM is finished or not.
  bool serialized_;
  // The links_ contain dummy original URLs of all saved links.
  WebVector<WebURL> links_;
  // The local_paths_ contain dummy corresponding local file paths of all saved
  // links, which matched links_ one by one.
  WebVector<WebString> local_paths_;
  // The local_directory_name_ is dummy relative path of directory which
  // contain all saved auxiliary files included all sub frames and resources.
  const base::FilePath local_directory_name_;

 protected:
  // testing::Test
  virtual void SetUp() {
    TestShellTest::SetUp();
    serialized_ = false;
  }

  virtual void TearDown() {
    TestShellTest::TearDown();
  }
};

// Helper function that test whether the first node in the doc is a doc type
// node.
bool HasDocType(const WebDocument& doc) {
  WebNode node = doc.firstChild();
  if (node.isNull())
    return false;
  return node.nodeType() == WebNode::DocumentTypeNode;
}

// Helper function for checking whether input node is META tag. Return true
// means it is META element, otherwise return false. The parameter charset_info
// return actual charset info if the META tag has charset declaration.
bool IsMetaElement(const WebNode& node, std::string& charset_info) {
  if (!node.isElementNode())
    return false;
  const WebElement meta = node.toConst<WebElement>();
  if (!meta.hasTagName("meta"))
    return false;
  charset_info.erase(0, charset_info.length());
  // Check the META charset declaration.
  WebString httpEquiv = meta.getAttribute("http-equiv");
  if (LowerCaseEqualsASCII(httpEquiv, "content-type")) {
    std::string content = meta.getAttribute("content").utf8();
    int pos = content.find("charset", 0);
    if (pos > -1) {
      // Add a dummy charset declaration to charset_info, which indicates this
      // META tag has charset declaration although we do not get correct value
      // yet.
      charset_info.append("has-charset-declaration");
      int remaining_length = content.length() - pos - 7;
      if (!remaining_length)
        return true;
      int start_pos = pos + 7;
      // Find "=" symbol.
      while (remaining_length--)
        if (content[start_pos++] == L'=')
          break;
      // Skip beginning space.
      while (remaining_length) {
        if (content[start_pos] > 0x0020)
          break;
        ++start_pos;
        --remaining_length;
      }
      if (!remaining_length)
        return true;
      int end_pos = start_pos;
      // Now we find out the start point of charset info. Search the end point.
      while (remaining_length--) {
        if (content[end_pos] <= 0x0020 || content[end_pos] == L';')
          break;
        ++end_pos;
      }
      // Get actual charset info.
      charset_info = content.substr(start_pos, end_pos - start_pos);
      return true;
    }
  }
  return true;
}

// If original contents have document type, the serialized contents also have
// document type.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithDocType) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("youtube_1.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Make sure original contents have document type.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(HasDocType(doc));
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->document().encoding());
  // Make sure serialized contents still have document type.
  web_frame = test_shell_->webView()->mainFrame();
  doc = web_frame->document();
  ASSERT_TRUE(HasDocType(doc));
}

// If original contents do not have document type, the serialized contents
// also do not have document type.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithoutDocType) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("youtube_2.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Make sure original contents do not have document type.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(!HasDocType(doc));
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->document().encoding());
  // Make sure serialized contents do not have document type.
  web_frame = test_shell_->webView()->mainFrame();
  doc = web_frame->document();
  ASSERT_TRUE(!HasDocType(doc));
}

// Serialize XML document which has all 5 built-in entities. After
// finishing serialization, the serialized contents should be same
// with original XML document.
TEST_F(DomSerializerTests, SerializeXMLDocWithBuiltInEntities) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("note.xml");
  // Read original contents for later comparison.
  std::string original_contents;
  ASSERT_TRUE(file_util::ReadFileToString(page_file_path, &original_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Compare the serialized contents with original contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  ASSERT_EQ(original_contents, serialized_contents);
}

// When serializing DOM, we add MOTW declaration before html tag.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithAddingMOTW) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("youtube_2.htm");
  // Read original contents for later comparison .
  std::string original_contents;
  ASSERT_TRUE(file_util::ReadFileToString(page_file_path, &original_contents));
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Make sure original contents does not have MOTW;
  std::string motw_declaration =
     WebPageSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
  ASSERT_FALSE(motw_declaration.empty());
  // The encoding of original contents is ISO-8859-1, so we convert the MOTW
  // declaration to ASCII and search whether original contents has it or not.
  ASSERT_TRUE(std::string::npos ==
      original_contents.find(motw_declaration));
  // Load the test file.
  LoadPageFromURL(file_url);
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Make sure the serialized contents have MOTW ;
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  ASSERT_FALSE(std::string::npos ==
      serialized_contents.find(motw_declaration));
}

// When serializing DOM, we will add the META which have correct charset
// declaration as first child of HEAD element for resolving WebKit bug:
// http://bugs.webkit.org/show_bug.cgi?id=16621 even the original document
// does not have META charset declaration.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithNoMetaCharsetInOriginalDoc) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("youtube_1.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);

  // Make sure there is no META charset declaration in original document.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  WebElement head_element = doc.head();
  ASSERT_TRUE(!head_element.isNull());
  // Go through all children of HEAD element.
  for (WebNode child = head_element.firstChild(); !child.isNull();
       child = child.nextSibling()) {
    std::string charset_info;
    if (IsMetaElement(child, charset_info))
      ASSERT_TRUE(charset_info.empty());
  }
  // Do serialization.
  SerializeDomForURL(file_url, false);

  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->document().encoding());
  // Make sure the first child of HEAD element is META which has charset
  // declaration in serialized contents.
  web_frame = test_shell_->webView()->mainFrame();
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  head_element = doc.head();
  ASSERT_TRUE(!head_element.isNull());
  WebNode meta_node = head_element.firstChild();
  ASSERT_TRUE(!meta_node.isNull());
  // Get meta charset info.
  std::string charset_info2;
  ASSERT_TRUE(IsMetaElement(meta_node, charset_info2));
  ASSERT_TRUE(!charset_info2.empty());
  ASSERT_EQ(charset_info2,
            std::string(web_frame->document().encoding().utf8()));

  // Make sure no more additional META tags which have charset declaration.
  for (WebNode child = meta_node.nextSibling(); !child.isNull();
       child = child.nextSibling()) {
    std::string charset_info;
    if (IsMetaElement(child, charset_info))
      ASSERT_TRUE(charset_info.empty());
  }
}

// When serializing DOM, if the original document has multiple META charset
// declaration, we will add the META which have correct charset declaration
// as first child of HEAD element and remove all original META charset
// declarations.
TEST_F(DomSerializerTests,
       SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDoc) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("youtube_2.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);

  // Make sure there are multiple META charset declarations in original
  // document.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  WebElement head_ele = doc.head();
  ASSERT_TRUE(!head_ele.isNull());
  // Go through all children of HEAD element.
  int charset_declaration_count = 0;
  for (WebNode child = head_ele.firstChild(); !child.isNull();
       child = child.nextSibling()) {
    std::string charset_info;
    if (IsMetaElement(child, charset_info) && !charset_info.empty())
      charset_declaration_count++;
  }
  // The original doc has more than META tags which have charset declaration.
  ASSERT_TRUE(charset_declaration_count > 1);

  // Do serialization.
  SerializeDomForURL(file_url, false);

  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->document().encoding());
  // Make sure only first child of HEAD element is META which has charset
  // declaration in serialized contents.
  web_frame = test_shell_->webView()->mainFrame();
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  head_ele = doc.head();
  ASSERT_TRUE(!head_ele.isNull());
  WebNode meta_node = head_ele.firstChild();
  ASSERT_TRUE(!meta_node.isNull());
  // Get meta charset info.
  std::string charset_info2;
  ASSERT_TRUE(IsMetaElement(meta_node, charset_info2));
  ASSERT_TRUE(!charset_info2.empty());
  ASSERT_EQ(charset_info2,
            std::string(web_frame->document().encoding().utf8()));

  // Make sure no more additional META tags which have charset declaration.
  for (WebNode child = meta_node.nextSibling(); !child.isNull();
       child = child.nextSibling()) {
    std::string charset_info;
    if (IsMetaElement(child, charset_info))
      ASSERT_TRUE(charset_info.empty());
  }
}

// Test situation of html entities in text when serializing HTML DOM.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithEntitiesInText) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII(
      "dom_serializer/htmlentities_in_text.htm");
  // Get file URL. The URL is dummy URL to identify the following loading
  // actions. The test content is in constant:original_contents.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Test contents.
  static const char* const original_contents =
      "<html><body>&amp;&lt;&gt;\"\'</body></html>";
  // Load the test contents.
  LoadContents(original_contents, file_url, WebString());

  // Get BODY's text content in DOM.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  WebElement body_ele = doc.body();
  ASSERT_TRUE(!body_ele.isNull());
  WebNode text_node = body_ele.firstChild();
  ASSERT_TRUE(text_node.isTextNode());
  ASSERT_TRUE(std::string(text_node.createMarkup().utf8()) ==
              "&amp;&lt;&gt;\"\'");
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Compare the serialized contents with original contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  // Compare the serialized contents with original contents to make sure
  // they are same.
  // Because we add MOTW when serializing DOM, so before comparison, we also
  // need to add MOTW to original_contents.
  std::string original_str =
    WebPageSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
  original_str += original_contents;
  // Since WebCore now inserts a new HEAD element if there is no HEAD element
  // when creating BODY element. (Please see HTMLParser::bodyCreateErrorCheck.)
  // We need to append the HEAD content and corresponding META content if we
  // find WebCore-generated HEAD element.
  if (!doc.head().isNull()) {
    WebString encoding = web_frame->document().encoding();
    std::string htmlTag("<html>");
    std::string::size_type pos = original_str.find(htmlTag);
    ASSERT_NE(std::string::npos, pos);
    pos += htmlTag.length();
    std::string head_part("<head>");
    head_part +=
        WebPageSerializer::generateMetaCharsetDeclaration(encoding).utf8();
    head_part += "</head>";
    original_str.insert(pos, head_part);
  }
  ASSERT_EQ(original_str, serialized_contents);
}

// Test situation of html entities in attribute value when serializing
// HTML DOM.
// This test started to fail at WebKit r65388. See http://crbug.com/52279.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithEntitiesInAttributeValue) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII(
      "dom_serializer/htmlentities_in_attribute_value.htm");
  // Get file URL. The URL is dummy URL to identify the following loading
  // actions. The test content is in constant:original_contents.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Test contents.
  static const char* const original_contents =
      "<html><body title=\"&amp;&lt;&gt;&quot;&#39;\"></body></html>";
  // Load the test contents.
  LoadContents(original_contents, file_url, WebString());
  // Get value of BODY's title attribute in DOM.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  WebElement body_ele = doc.body();
  ASSERT_TRUE(!body_ele.isNull());
  WebString value = body_ele.getAttribute("title");
  ASSERT_TRUE(std::string(value.utf8()) == "&<>\"\'");
  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Compare the serialized contents with original contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  // Compare the serialized contents with original contents to make sure
  // they are same.
  std::string original_str =
      WebPageSerializer::generateMarkOfTheWebDeclaration(file_url).utf8();
  original_str += original_contents;
  if (!doc.isNull()) {
    WebString encoding = web_frame->document().encoding();
    std::string htmlTag("<html>");
    std::string::size_type pos = original_str.find(htmlTag);
    ASSERT_NE(std::string::npos, pos);
    pos += htmlTag.length();
    std::string head_part("<head>");
    head_part +=
        WebPageSerializer::generateMetaCharsetDeclaration(encoding).utf8();
    head_part += "</head>";
    original_str.insert(pos, head_part);
  }
  ASSERT_EQ(original_str, serialized_contents);
}

// Test situation of non-standard HTML entities when serializing HTML DOM.
// This test started to fail at WebKit r65351. See http://crbug.com/52279.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithNonStandardEntities) {
  // Make a test file URL and load it.
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("nonstandard_htmlentities.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  LoadPageFromURL(file_url);

  // Get value of BODY's title attribute in DOM.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  WebElement body_element = doc.body();
  // Unescaped string for "&percnt;&nsup;&sup1;&apos;".
  static const wchar_t parsed_value[] = {
    '%', 0x2285, 0x00b9, '\'', 0
  };
  WebString value = body_element.getAttribute("title");
  ASSERT_TRUE(UTF16ToWide(value) == parsed_value);
  ASSERT_TRUE(UTF16ToWide(body_element.innerText()) == parsed_value);

  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Check the serialized string.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  // Confirm that the serialized string has no non-standard HTML entities.
  ASSERT_EQ(std::string::npos, serialized_contents.find("&percnt;"));
  ASSERT_EQ(std::string::npos, serialized_contents.find("&nsup;"));
  ASSERT_EQ(std::string::npos, serialized_contents.find("&sup1;"));
  ASSERT_EQ(std::string::npos, serialized_contents.find("&apos;"));
}

// Test situation of BASE tag in original document when serializing HTML DOM.
// When serializing, we should comment the BASE tag, append a new BASE tag.
// rewrite all the savable URLs to relative local path, and change other URLs
// to absolute URLs.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithBaseTag) {
  // There are total 2 available base tags in this test file.
  const int kTotalBaseTagCountInTestFile = 2;

  base::FilePath page_file_path = data_dir_.AppendASCII("dom_serializer");
  file_util::EnsureEndsWithSeparator(&page_file_path);

  // Get page dir URL which is base URL of this file.
  GURL path_dir_url = net::FilePathToFileURL(page_file_path);
  // Get file path.
  page_file_path =
      page_file_path.AppendASCII("html_doc_has_base_tag.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Since for this test, we assume there is no savable sub-resource links for
  // this test file, also all links are relative URLs in this test file, so we
  // need to check those relative URLs and make sure document has BASE tag.
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  // Go through all descent nodes.
  WebNodeCollection all = doc.all();
  int original_base_tag_count = 0;
  for (WebNode node = all.firstItem(); !node.isNull();
       node = all.nextItem()) {
    if (!node.isElementNode())
      continue;
    WebElement element = node.to<WebElement>();
    if (element.hasTagName("base")) {
      original_base_tag_count++;
    } else {
      // Get link.
      WebString value =
          webkit_glue::GetSubResourceLinkFromElement(element);
      if (value.isNull() && element.hasTagName("a")) {
        value = element.getAttribute("href");
        if (value.isEmpty())
          value = WebString();
      }
      // Each link is relative link.
      if (!value.isNull()) {
        GURL link(value.utf8());
        ASSERT_TRUE(link.scheme().empty());
      }
    }
  }
  ASSERT_EQ(original_base_tag_count, kTotalBaseTagCountInTestFile);
  // Make sure in original document, the base URL is not equal with the
  // |path_dir_url|.
  GURL original_base_url(doc.baseURL());
  ASSERT_NE(original_base_url, path_dir_url);

  // Do serialization.
  SerializeDomForURL(file_url, false);

  // Load the serialized contents.
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);
  LoadContents(serialized_contents, file_url,
               web_frame->document().encoding());

  // Make sure all links are absolute URLs and doc there are some number of
  // BASE tags in serialized HTML data. Each of those BASE tags have same base
  // URL which is as same as URL of current test file.
  web_frame = test_shell_->webView()->mainFrame();
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  // Go through all descent nodes.
  all = doc.all();
  int new_base_tag_count = 0;
  for (WebNode node = all.firstItem(); !node.isNull();
       node = all.nextItem()) {
    if (!node.isElementNode())
      continue;
    WebElement element = node.to<WebElement>();
    if (element.hasTagName("base")) {
      new_base_tag_count++;
    } else {
      // Get link.
      WebString value =
          webkit_glue::GetSubResourceLinkFromElement(element);
      if (value.isNull() && element.hasTagName("a")) {
        value = element.getAttribute("href");
        if (value.isEmpty())
          value = WebString();
      }
      // Each link is absolute link.
      if (!value.isNull()) {
        GURL link(std::string(value.utf8()));
        ASSERT_FALSE(link.scheme().empty());
      }
    }
  }
  // We have one more added BASE tag which is generated by JavaScript.
  ASSERT_EQ(new_base_tag_count, original_base_tag_count + 1);
  // Make sure in new document, the base URL is equal with the |path_dir_url|.
  GURL new_base_url(doc.baseURL());
  ASSERT_EQ(new_base_url, path_dir_url);
}

// Serializing page which has an empty HEAD tag.
TEST_F(DomSerializerTests, SerializeHTMLDOMWithEmptyHead) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("empty_head.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());

  // Load the test html content.
  static const char* const empty_head_contents =
    "<html><head></head><body>hello world</body></html>";
  LoadContents(empty_head_contents, file_url, WebString());

  // Make sure the head tag is empty.
  WebFrame* web_frame = test_shell_->webView()->mainFrame();
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  WebElement head_element = doc.head();
  ASSERT_TRUE(!head_element.isNull());
  ASSERT_TRUE(!head_element.hasChildNodes());
  ASSERT_TRUE(head_element.childNodes().length() == 0);

  // Do serialization.
  SerializeDomForURL(file_url, false);
  // Make sure the serialized contents have META ;
  ASSERT_TRUE(HasSerializedFrame(file_url));
  const std::string& serialized_contents =
      GetSerializedContentForFrame(file_url);

  // Reload serialized contents and make sure there is only one META tag.
  LoadContents(serialized_contents, file_url, web_frame->document().encoding());
  web_frame = test_shell_->webView()->mainFrame();
  ASSERT_TRUE(web_frame != NULL);
  doc = web_frame->document();
  ASSERT_TRUE(doc.isHTMLDocument());
  head_element = doc.head();
  ASSERT_TRUE(!head_element.isNull());
  ASSERT_TRUE(head_element.hasChildNodes());
  ASSERT_TRUE(head_element.childNodes().length() == 1);
  WebNode meta_node = head_element.firstChild();
  ASSERT_TRUE(!meta_node.isNull());
  // Get meta charset info.
  std::string charset_info;
  ASSERT_TRUE(IsMetaElement(meta_node, charset_info));
  ASSERT_TRUE(!charset_info.empty());
  ASSERT_EQ(charset_info,
            std::string(web_frame->document().encoding().utf8()));

  // Check the body's first node is text node and its contents are
  // "hello world"
  WebElement body_element = doc.body();
  ASSERT_TRUE(!body_element.isNull());
  WebNode text_node = body_element.firstChild();
  ASSERT_TRUE(text_node.isTextNode());
  WebString text_node_contents = text_node.nodeValue();
  ASSERT_TRUE(std::string(text_node_contents.utf8()) == "hello world");
}

// Test that we don't crash when the page contains an iframe that
// was handled as a download (http://crbug.com/42212).
TEST_F(DomSerializerTests, SerializeDocumentWithDownloadedIFrame) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("iframe-src-is-exe.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  LoadPageFromURL(file_url);
  // Do a recursive serialization. We pass if we don't crash.
  SerializeDomForURL(file_url, true);
}

TEST_F(DomSerializerTests, SubResourceForElementsInNonHTMLNamespace) {
  base::FilePath page_file_path = data_dir_;
  page_file_path = page_file_path.AppendASCII("dom_serializer");
  page_file_path = page_file_path.AppendASCII("non_html_namespace.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  LoadPageFromURL(file_url);
  WebFrame* web_frame = FindSubFrameByURL(test_shell_->webView(), file_url);
  ASSERT_TRUE(web_frame != NULL);
  WebDocument doc = web_frame->document();
  WebNode lastNodeInBody = doc.body().lastChild();
  ASSERT_EQ(WebNode::ElementNode, lastNodeInBody.nodeType());
  WebString uri = webkit_glue::GetSubResourceLinkFromElement(
      lastNodeInBody.to<WebElement>());
  EXPECT_TRUE(uri.isNull());
}

}  // namespace
