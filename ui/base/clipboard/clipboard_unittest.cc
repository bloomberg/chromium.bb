// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "base/message_loop.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#endif

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
#include "base/pickle.h"
#endif

namespace ui {

#if defined(OS_WIN)
class ClipboardTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    message_loop_.reset(new MessageLoopForUI());
  }
  virtual void TearDown() {
  }

 private:
  scoped_ptr<MessageLoop> message_loop_;
};
#elif defined(OS_POSIX)
typedef PlatformTest ClipboardTest;
#endif  // defined(OS_WIN)

namespace {

bool ClipboardContentsIsExpected(const string16& copied_markup,
                                 const string16& pasted_markup) {
#if defined(OS_POSIX)
  return pasted_markup.find(copied_markup) != string16::npos;
#else
  return copied_markup == pasted_markup;
#endif
}

}  // namespace

TEST_F(ClipboardTest, ClearTest) {
  Clipboard clipboard;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteText(ASCIIToUTF16("clear me"));
  }

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteHTML(ASCIIToUTF16("<b>broom</b>"), "");
  }

  EXPECT_FALSE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), Clipboard::BUFFER_STANDARD));
  EXPECT_FALSE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextFormatType(), Clipboard::BUFFER_STANDARD));
}

TEST_F(ClipboardTest, TextTest) {
  Clipboard clipboard;

  string16 text(ASCIIToUTF16("This is a string16!#$")), text_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteText(text);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), Clipboard::BUFFER_STANDARD));
  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetPlainTextFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  clipboard.ReadText(Clipboard::BUFFER_STANDARD, &text_result);

  EXPECT_EQ(text, text_result);
  clipboard.ReadAsciiText(Clipboard::BUFFER_STANDARD, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TEST_F(ClipboardTest, HTMLTest) {
  Clipboard clipboard;

  string16 markup(ASCIIToUTF16("<string>Hi!</string>")), markup_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  clipboard.ReadHTML(Clipboard::BUFFER_STANDARD, &markup_result, &url_result);
  EXPECT_TRUE(ClipboardContentsIsExpected(markup, markup_result));
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
}

TEST_F(ClipboardTest, TrickyHTMLTest) {
  Clipboard clipboard;

  string16 markup(ASCIIToUTF16("<em>Bye!<!--EndFragment --></em>")),
      markup_result;
  std::string url, url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  clipboard.ReadHTML(Clipboard::BUFFER_STANDARD, &markup_result, &url_result);
  EXPECT_TRUE(ClipboardContentsIsExpected(markup, markup_result));
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
}

#if defined(OS_LINUX)
// Regression test for crbug.com/56298 (pasting empty HTML crashes Linux).
TEST_F(ClipboardTest, EmptyHTMLTest) {
  Clipboard clipboard;
  // ScopedClipboardWriter doesn't let us write empty data to the clipboard.
  clipboard.clipboard_data_ = new Clipboard::TargetMap();
  // The 1 is so the compiler doesn't warn about allocating an empty array.
  char* empty = new char[1];
  clipboard.InsertMapping("text/html", empty, 0U);
  clipboard.SetGtkClipboard();

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  string16 markup_result;
  std::string url_result;
  clipboard.ReadHTML(Clipboard::BUFFER_STANDARD, &markup_result, &url_result);
  EXPECT_TRUE(ClipboardContentsIsExpected(string16(), markup_result));
}
#endif

// TODO(estade): Port the following test (decide what target we use for urls)
#if !defined(OS_POSIX) || defined(OS_MACOSX)
TEST_F(ClipboardTest, BookmarkTest) {
  Clipboard clipboard;

  string16 title(ASCIIToUTF16("The Example Company")), title_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteBookmark(title, url);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetUrlWFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  clipboard.ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
}
#endif  // defined(OS_WIN)

TEST_F(ClipboardTest, MultiFormatTest) {
  Clipboard clipboard;

  string16 text(ASCIIToUTF16("Hi!")), text_result;
  string16 markup(ASCIIToUTF16("<strong>Hi!</string>")), markup_result;
  std::string url("http://www.example.com/"), url_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteHTML(markup, url);
    clipboard_writer.WriteText(text);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  EXPECT_TRUE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), Clipboard::BUFFER_STANDARD));
  EXPECT_TRUE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextFormatType(), Clipboard::BUFFER_STANDARD));
  clipboard.ReadHTML(Clipboard::BUFFER_STANDARD, &markup_result, &url_result);
  EXPECT_TRUE(ClipboardContentsIsExpected(markup, markup_result));
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
  clipboard.ReadText(Clipboard::BUFFER_STANDARD, &text_result);
  EXPECT_EQ(text, text_result);
  clipboard.ReadAsciiText(Clipboard::BUFFER_STANDARD, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TEST_F(ClipboardTest, URLTest) {
  Clipboard clipboard;

  string16 url(ASCIIToUTF16("http://www.google.com/"));

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteURL(url);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(
      Clipboard::GetPlainTextWFormatType(), Clipboard::BUFFER_STANDARD));
  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetPlainTextFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  string16 text_result;
  clipboard.ReadText(Clipboard::BUFFER_STANDARD, &text_result);

  EXPECT_EQ(text_result, url);

  std::string ascii_text;
  clipboard.ReadAsciiText(Clipboard::BUFFER_STANDARD, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);

#if defined(OS_LINUX)
  ascii_text.clear();
  clipboard.ReadAsciiText(Clipboard::BUFFER_SELECTION, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);
#endif  // defined(OS_LINUX)
}

TEST_F(ClipboardTest, SharedBitmapTest) {
  unsigned int fake_bitmap[] = {
    0x46155189, 0xF6A55C8D, 0x79845674, 0xFA57BD89,
    0x78FD46AE, 0x87C64F5A, 0x36EDC5AF, 0x4378F568,
    0x91E9F63A, 0xC31EA14F, 0x69AB32DF, 0x643A3FD1,
  };
  gfx::Size fake_bitmap_size(3, 4);
  uint32 bytes = sizeof(fake_bitmap);

  // Create shared memory region.
  base::SharedMemory shared_buf;
  ASSERT_TRUE(shared_buf.CreateAndMapAnonymous(bytes));
  memcpy(shared_buf.memory(), fake_bitmap, bytes);
  base::SharedMemoryHandle handle_to_share;
  base::ProcessHandle current_process = base::kNullProcessHandle;
#if defined(OS_WIN)
  current_process = GetCurrentProcess();
#endif
  shared_buf.ShareToProcess(current_process, &handle_to_share);
  ASSERT_TRUE(shared_buf.Unmap());

  // Setup data for clipboard.
  Clipboard::ObjectMapParam placeholder_param;
  Clipboard::ObjectMapParam size_param;
  const char* size_data = reinterpret_cast<const char*>(&fake_bitmap_size);
  for (size_t i = 0; i < sizeof(fake_bitmap_size); ++i)
    size_param.push_back(size_data[i]);

  Clipboard::ObjectMapParams params;
  params.push_back(placeholder_param);
  params.push_back(size_param);

  Clipboard::ObjectMap objects;
  objects[Clipboard::CBF_SMBITMAP] = params;
  Clipboard::ReplaceSharedMemHandle(&objects, handle_to_share, current_process);

  Clipboard clipboard;
  clipboard.WriteObjects(objects);

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                          Clipboard::BUFFER_STANDARD));
}

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
TEST_F(ClipboardTest, DataTest) {
  Clipboard clipboard;
  const char* kFormat = "chromium/x-test-format";
  std::string payload("test string");
  Pickle write_pickle;
  write_pickle.WriteString(payload);

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WritePickledData(write_pickle, kFormat);
  }

  ASSERT_TRUE(clipboard.IsFormatAvailableByString(
      kFormat, Clipboard::BUFFER_STANDARD));
  std::string output;
  clipboard.ReadData(kFormat, &output);
  ASSERT_FALSE(output.empty());

  Pickle read_pickle(output.data(), output.size());
  void* iter = NULL;
  std::string unpickled_string;
  ASSERT_TRUE(read_pickle.ReadString(&iter, &unpickled_string));
  EXPECT_EQ(payload, unpickled_string);
}
#endif

#if defined(OS_WIN)  // Windows only tests.
TEST_F(ClipboardTest, HyperlinkTest) {
  Clipboard clipboard;

  const std::string kTitle("The Example Company");
  const std::string kUrl("http://www.example.com/");
  const std::string kExpectedHtml("<a href=\"http://www.example.com/\">"
                                  "The Example Company</a>");
  std::string url_result;
  string16 html_result;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteHyperlink(ASCIIToUTF16(kTitle), kUrl);
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetHtmlFormatType(),
                                          Clipboard::BUFFER_STANDARD));
  clipboard.ReadHTML(Clipboard::BUFFER_STANDARD, &html_result, &url_result);
  EXPECT_EQ(ASCIIToUTF16(kExpectedHtml), html_result);
}

TEST_F(ClipboardTest, WebSmartPasteTest) {
  Clipboard clipboard;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteWebSmartPaste();
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(
      Clipboard::GetWebKitSmartPasteFormatType(), Clipboard::BUFFER_STANDARD));
}

TEST_F(ClipboardTest, BitmapTest) {
  unsigned int fake_bitmap[] = {
    0x46155189, 0xF6A55C8D, 0x79845674, 0xFA57BD89,
    0x78FD46AE, 0x87C64F5A, 0x36EDC5AF, 0x4378F568,
    0x91E9F63A, 0xC31EA14F, 0x69AB32DF, 0x643A3FD1,
  };

  Clipboard clipboard;

  {
    ScopedClipboardWriter clipboard_writer(&clipboard);
    clipboard_writer.WriteBitmapFromPixels(fake_bitmap, gfx::Size(3, 4));
  }

  EXPECT_TRUE(clipboard.IsFormatAvailable(Clipboard::GetBitmapFormatType(),
                                          Clipboard::BUFFER_STANDARD));
}

void HtmlTestHelper(const std::string& cf_html,
                    const std::string& expected_html) {
  std::string html;
  ClipboardUtil::CFHtmlToHtml(cf_html, &html, NULL);
  EXPECT_EQ(html, expected_html);
}

TEST_F(ClipboardTest, HtmlTest) {
  // Test converting from CF_HTML format data with <!--StartFragment--> and
  // <!--EndFragment--> comments, like from MS Word.
  HtmlTestHelper("Version:1.0\r\n"
                 "StartHTML:0000000105\r\n"
                 "EndHTML:0000000199\r\n"
                 "StartFragment:0000000123\r\n"
                 "EndFragment:0000000161\r\n"
                 "\r\n"
                 "<html>\r\n"
                 "<body>\r\n"
                 "<!--StartFragment-->\r\n"
                 "\r\n"
                 "<p>Foo</p>\r\n"
                 "\r\n"
                 "<!--EndFragment-->\r\n"
                 "</body>\r\n"
                 "</html>\r\n\r\n",
                 "<p>Foo</p>");

  // Test converting from CF_HTML format data without <!--StartFragment--> and
  // <!--EndFragment--> comments, like from OpenOffice Writer.
  HtmlTestHelper("Version:1.0\r\n"
                 "StartHTML:0000000105\r\n"
                 "EndHTML:0000000151\r\n"
                 "StartFragment:0000000121\r\n"
                 "EndFragment:0000000131\r\n"
                 "<html>\r\n"
                 "<body>\r\n"
                 "<p>Foo</p>\r\n"
                 "</body>\r\n"
                 "</html>\r\n\r\n",
                 "<p>Foo</p>");
}
#endif  // defined(OS_WIN)

// Test writing all formats we have simultaneously.
TEST_F(ClipboardTest, WriteEverything) {
  Clipboard clipboard;

  {
    ScopedClipboardWriter writer(&clipboard);
    writer.WriteText(UTF8ToUTF16("foo"));
    writer.WriteURL(UTF8ToUTF16("foo"));
    writer.WriteHTML(UTF8ToUTF16("foo"), "bar");
    writer.WriteBookmark(UTF8ToUTF16("foo"), "bar");
    writer.WriteHyperlink(ASCIIToUTF16("foo"), "bar");
    writer.WriteWebSmartPaste();
    // Left out: WriteFile, WriteFiles, WriteBitmapFromPixels, WritePickledData.
  }

  // Passes if we don't crash.
}

}  // namespace ui
