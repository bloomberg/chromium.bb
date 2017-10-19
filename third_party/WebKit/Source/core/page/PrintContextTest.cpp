// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/PrintContext.h"

#include <memory>

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/TextStream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace blink {

const int kPageWidth = 800;
const int kPageHeight = 600;

class MockPageContextCanvas : public SkCanvas {
 public:
  enum OperationType { kDrawRect, kDrawPoint };

  struct Operation {
    OperationType type;
    SkRect rect;
  };

  MockPageContextCanvas() : SkCanvas(kPageWidth, kPageHeight) {}
  ~MockPageContextCanvas() override {}

  void onDrawAnnotation(const SkRect& rect,
                        const char key[],
                        SkData* value) override {
    if (rect.width() == 0 && rect.height() == 0) {
      SkPoint point = getTotalMatrix().mapXY(rect.x(), rect.y());
      Operation operation = {kDrawPoint,
                             SkRect::MakeXYWH(point.x(), point.y(), 0, 0)};
      recorded_operations_.push_back(operation);
    } else {
      Operation operation = {kDrawRect, rect};
      getTotalMatrix().mapRect(&operation.rect);
      recorded_operations_.push_back(operation);
    }
  }

  const Vector<Operation>& RecordedOperations() const {
    return recorded_operations_;
  }

 private:
  Vector<Operation> recorded_operations_;
};

class PrintContextTest : public RenderingTest {
 protected:
  explicit PrintContextTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client) {}
  ~PrintContextTest() override {}

  void SetUp() override {
    RenderingTest::SetUp();
    print_context_ = new PrintContext(GetDocument().GetFrame());
  }

  PrintContext& GetPrintContext() { return *print_context_.Get(); }

  void SetBodyInnerHTML(String body_content) {
    GetDocument().body()->setAttribute(HTMLNames::styleAttr, "margin: 0");
    GetDocument().body()->SetInnerHTMLFromString(body_content);
  }

  void PrintSinglePage(MockPageContextCanvas& canvas) {
    IntRect page_rect(0, 0, kPageWidth, kPageHeight);
    GetPrintContext().BeginPrintMode(page_rect.Width(), page_rect.Height());
    GetDocument().View()->UpdateAllLifecyclePhases();
    PaintRecordBuilder builder(page_rect);
    GraphicsContext& context = builder.Context();
    context.SetPrinting(true);
    GetDocument().View()->PaintContents(context, kGlobalPaintPrinting,
                                        page_rect);
    {
      DrawingRecorder recorder(context, *GetDocument().GetLayoutView(),
                               DisplayItem::kPrintedContentDestinationLocations,
                               page_rect);
      GetPrintContext().OutputLinkedDestinations(context, page_rect);
    }
    builder.EndRecording()->Playback(&canvas);
    GetPrintContext().EndPrintMode();
  }

  static String AbsoluteBlockHtmlForLink(int x,
                                         int y,
                                         int width,
                                         int height,
                                         const char* url,
                                         const char* children = nullptr) {
    TextStream ts;
    ts << "<a style='position: absolute; left: " << x << "px; top: " << y
       << "px; width: " << width << "px; height: " << height << "px' href='"
       << url << "'>" << (children ? children : url) << "</a>";
    return ts.Release();
  }

  static String InlineHtmlForLink(const char* url,
                                  const char* children = nullptr) {
    TextStream ts;
    ts << "<a href='" << url << "'>" << (children ? children : url) << "</a>";
    return ts.Release();
  }

  static String HtmlForAnchor(int x,
                              int y,
                              const char* name,
                              const char* text_content) {
    TextStream ts;
    ts << "<a name='" << name << "' style='position: absolute; left: " << x
       << "px; top: " << y << "px'>" << text_content << "</a>";
    return ts.Release();
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<PrintContext> print_context_;
};

class PrintContextFrameTest : public PrintContextTest {
 public:
  PrintContextFrameTest()
      : PrintContextTest(SingleChildLocalFrameClient::Create()) {}
};

#define EXPECT_SKRECT_EQ(expectedX, expectedY, expectedWidth, expectedHeight, \
                         actualRect)                                          \
  EXPECT_EQ(expectedX, actualRect.x());                                       \
  EXPECT_EQ(expectedY, actualRect.y());                                       \
  EXPECT_EQ(expectedWidth, actualRect.width());                               \
  EXPECT_EQ(expectedHeight, actualRect.height());

TEST_F(PrintContextTest, LinkTarget) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(150, 160, 170, 180,
                               "http://www.google.com#fragment"));
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetUnderAnonymousBlockBeforeBlock) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML("<div style='padding-top: 50px'>" +
                   InlineHtmlForLink("http://www.google.com",
                                     "<img style='width: 111; height: 10'>") +
                   "<div> " +
                   InlineHtmlForLink("http://www.google1.com",
                                     "<img style='width: 122; height: 20'>") +
                   "</div>" + "</div>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 50, 111, 10, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(0, 60, 122, 20, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetContainingABlock) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<div style='padding-top: 50px'>" +
      InlineHtmlForLink("http://www.google2.com",
                        "<div style='width:133; height: 30'>BLOCK</div>") +
      "</div>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 50, 133, 30, operations[0].rect);
}

TEST_F(PrintContextTest, LinkTargetUnderInInlines) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<span><b><i><img style='width: 40px; height: 40px'><br>" +
      InlineHtmlForLink("http://www.google3.com",
                        "<img style='width: 144px; height: 40px'>") +
      "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(0, 40, 144, 40, operations[0].rect);
}

TEST_F(PrintContextTest, LinkTargetUnderRelativelyPositionedInline) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
        + "<span style='position: relative; top: 50px; left: 50px'><b><i><img style='width: 1px; height: 40px'><br>"
        + InlineHtmlForLink("http://www.google3.com", "<img style='width: 155px; height: 50px'>")
        + "</i></b></span>");
  PrintSinglePage(canvas);
  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 90, 155, 50, operations[0].rect);
}

TEST_F(PrintContextTest, LinkTargetSvg) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      "<svg width='100' height='100'>"
      "<a xlink:href='http://www.w3.org'><rect x='20' y='20' width='50' "
      "height='50'/></a>"
      "<text x='10' y='90'><a "
      "xlink:href='http://www.google.com'><tspan>google</tspan></a></text>"
      "</svg>");
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(20, 20, 50, 50, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_EQ(10, operations[1].rect.x());
  EXPECT_GE(90, operations[1].rect.y());
}

TEST_F(PrintContextTest, LinkedTarget) {
  MockPageContextCanvas canvas;
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(
      AbsoluteBlockHtmlForLink(
          50, 60, 70, 80,
          "#fragment")  // Generates a Link_Named_Dest_Key annotation
      + AbsoluteBlockHtmlForLink(150, 160, 170, 180,
                                 "#not-found")  // Generates no annotation
      +
      HtmlForAnchor(250, 260, "fragment",
                    "fragment")  // Generates a Define_Named_Dest_Key annotation
      + HtmlForAnchor(350, 360, "fragment-not-used",
                      "fragment-not-used"));  // Generates no annotation
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[1].type);
  EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_F(PrintContextTest, EmptyLinkedTarget) {
  MockPageContextCanvas canvas;
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(AbsoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment") +
                   HtmlForAnchor(250, 260, "fragment", ""));
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawPoint, operations[1].type);
  EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetBoundingBox) {
  MockPageContextCanvas canvas;
  SetBodyInnerHTML(
      AbsoluteBlockHtmlForLink(50, 60, 70, 20, "http://www.google.com",
                               "<img style='width: 200px; height: 100px'>"));
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(1u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 200, 100, operations[0].rect);
}

TEST_F(PrintContextFrameTest, WithSubframe) {
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(
      "<style>::-webkit-scrollbar { display: none }</style>"
      "<iframe src='http://b.com/' width='500' height='500'"
      " style='border-width: 5px; margin: 5px; position: absolute; top: 90px; "
      "left: 90px'></iframe>");
  SetChildFrameHTML(
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment") +
      AbsoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(250, 260, 270, 280,
                               "http://www.google.com#fragment"));

  MockPageContextCanvas canvas;
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(2u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(250, 260, 170, 180, operations[0].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(350, 360, 270, 280, operations[1].rect);
}

TEST_F(PrintContextFrameTest, WithScrolledSubframe) {
  GetDocument().SetBaseURLOverride(KURL("http://a.com/"));
  SetBodyInnerHTML(
      "<style>::-webkit-scrollbar { display: none }</style>"
      "<iframe src='http://b.com/' width='500' height='500'"
      " style='border-width: 5px; margin: 5px; position: absolute; top: 90px; "
      "left: 90px'></iframe>");
  SetChildFrameHTML(
      AbsoluteBlockHtmlForLink(10, 10, 20, 20, "http://invisible.com") +
      AbsoluteBlockHtmlForLink(50, 60, 70, 80, "http://partly.visible.com") +
      AbsoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com") +
      AbsoluteBlockHtmlForLink(250, 260, 270, 280,
                               "http://www.google.com#fragment") +
      AbsoluteBlockHtmlForLink(850, 860, 70, 80,
                               "http://another.invisible.com"));

  ChildDocument().domWindow()->scrollTo(100, 100);

  MockPageContextCanvas canvas;
  PrintSinglePage(canvas);

  const Vector<MockPageContextCanvas::Operation>& operations =
      canvas.RecordedOperations();
  ASSERT_EQ(3u, operations.size());
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[0].type);
  EXPECT_SKRECT_EQ(50, 60, 70, 80,
                   operations[0].rect);  // FIXME: the rect should be clipped.
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[1].type);
  EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
  EXPECT_EQ(MockPageContextCanvas::kDrawRect, operations[2].type);
  EXPECT_SKRECT_EQ(250, 260, 270, 280, operations[2].rect);
}

}  // namespace blink
