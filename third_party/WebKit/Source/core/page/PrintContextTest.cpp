// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PrintContext.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/testing/SkiaForCoreTesting.h"
#include "platform/text/TextStream.h"
#include <gtest/gtest.h>

namespace blink {

const int kPageWidth = 800;
const int kPageHeight = 600;

class MockPrintContext : public PrintContext {
public:
    MockPrintContext(LocalFrame* frame) : PrintContext(frame) { }

    void outputLinkAndLinkedDestinations(GraphicsContext& context, const IntRect& pageRect)
    {
        PrintContext::outputLinkAndLinkedDestinations(context, pageRect);
    }
};

class MockCanvas : public SkCanvas {
public:
    enum OperationType {
        DrawRect,
        DrawPoint
    };

    struct Operation {
        OperationType type;
        SkRect rect;
    };

    MockCanvas() : SkCanvas(kPageWidth, kPageHeight) { }

    virtual void onDrawRect(const SkRect& rect, const SkPaint& paint) override
    {
        ASSERT_TRUE(paint.getAnnotation());
        Operation operation = { DrawRect, rect };
        m_recordedOperations.append(operation);
    }

    virtual void onDrawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint) override
    {
        ASSERT_EQ(1u, count); // Only called from drawPoint().
        ASSERT_TRUE(paint.getAnnotation());
        Operation operation = { DrawPoint, SkRect::MakeXYWH(pts[0].x(), pts[0].y(), 0, 0) };
        m_recordedOperations.append(operation);
    }

    const Vector<Operation>& recordedOperations() const { return m_recordedOperations; }

private:
    Vector<Operation> m_recordedOperations;
};

class PrintContextTest : public testing::Test {
protected:
    PrintContextTest(PassOwnPtr<FrameLoaderClient> frameLoaderClient = PassOwnPtr<FrameLoaderClient>())
        : m_pageHolder(DummyPageHolder::create(IntSize(kPageWidth, kPageHeight), nullptr, frameLoaderClient))
        , m_printContext(adoptPtrWillBeNoop(new MockPrintContext(document().frame()))) { }

    Document& document() const { return m_pageHolder->document(); }
    MockPrintContext& printContext() { return *m_printContext.get(); }

    void setBodyInnerHTML(String bodyContent)
    {
        document().body()->setInnerHTML(bodyContent, ASSERT_NO_EXCEPTION);
    }

    void printSinglePage(SkCanvas& canvas)
    {
        IntRect pageRect(0, 0, kPageWidth, kPageHeight);
        GraphicsContext context(&canvas, nullptr);
        printContext().begin(kPageWidth, kPageHeight);
        printContext().outputLinkAndLinkedDestinations(context, pageRect);
        printContext().end();
    }

    static String htmlForLink(int x, int y, int width, int height, const char* url, const char* children = nullptr)
    {
        TextStream ts;
        ts << "<a style='position: absolute; left: " << x << "px; top: " << y << "px; width: " << width << "px; height: " << height
            << "px' href='" << url << "'>" << (children ? children : url) << "</a>";
        return ts.release();
    }

    static String htmlForAnchor(int x, int y, const char* name)
    {
        TextStream ts;
        ts << "<a name='" << name << "' style='position: absolute; left: " << x << "px; top: " << y << "px'>" << name << "</a>";
        return ts.release();
    }

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
    OwnPtrWillBePersistent<MockPrintContext> m_printContext;
};

class SingleChildFrameLoaderClient : public EmptyFrameLoaderClient {
public:
    SingleChildFrameLoaderClient() : m_child(nullptr) { }

    virtual Frame* firstChild() const override { return m_child; }
    virtual Frame* lastChild() const override { return m_child; }

    void setChild(Frame* child) { m_child = child; }

private:
    Frame* m_child;
};

class FrameLoaderClientWithParent : public EmptyFrameLoaderClient {
public:
    FrameLoaderClientWithParent(Frame* parent) : m_parent(parent) { }

    virtual Frame* parent() const override { return m_parent; }

private:
    Frame* m_parent;
};

class PrintContextFrameTest : public PrintContextTest {
public:
    PrintContextFrameTest() : PrintContextTest(adoptPtr(new SingleChildFrameLoaderClient())) { }
};

#define EXPECT_SKRECT_EQ(expectedX, expectedY, expectedWidth, expectedHeight, actualRect) \
    EXPECT_EQ(expectedX, actualRect.x()); \
    EXPECT_EQ(expectedY, actualRect.y()); \
    EXPECT_EQ(expectedWidth, actualRect.width()); \
    EXPECT_EQ(expectedHeight, actualRect.height());

TEST_F(PrintContextTest, LinkTarget)
{
    MockCanvas canvas;
    setBodyInnerHTML(htmlForLink(50, 60, 70, 80, "http://www.google.com")
        + htmlForLink(150, 160, 170, 180, "http://www.google.com#fragment"));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());

    // The items in the result can be in any sequence.
    size_t firstIndex = operations[0].rect.x() == 50 ? 0 : 1;
    EXPECT_EQ(MockCanvas::DrawRect, operations[firstIndex].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[firstIndex].rect);
    // We should also check if the annotation is correct but Skia doesn't export
    // SkAnnotation API.

    size_t secondIndex = firstIndex == 0 ? 1 : 0;
    EXPECT_EQ(MockCanvas::DrawRect, operations[secondIndex].type);
    EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[secondIndex].rect);
}

TEST_F(PrintContextTest, LinkedTarget)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML(htmlForLink(50, 60, 70, 80, "#fragment") // Generates a Link_Named_Dest_Key annotation
        + htmlForLink(150, 160, 170, 180, "#not-found") // Generates no annotation
        + htmlForAnchor(250, 260, "fragment") // Generates a Define_Named_Dest_Key annotation
        + htmlForAnchor(350, 360, "fragment-not-used")); // Generates no annotation
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());

    size_t firstIndex = operations[0].rect.x() == 50 ? 0 : 1;
    EXPECT_EQ(MockCanvas::DrawRect, operations[firstIndex].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[firstIndex].rect);

    size_t secondIndex = firstIndex == 0 ? 1 : 0;
    EXPECT_EQ(MockCanvas::DrawPoint, operations[secondIndex].type);
    EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[secondIndex].rect);
}

TEST_F(PrintContextTest, LinkTargetBoundingBox)
{
    MockCanvas canvas;
    setBodyInnerHTML(htmlForLink(50, 60, 70, 20, "http://www.google.com", "<img style='width: 200px; height: 100px'>"));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(1u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 200, 100, operations[0].rect);
}

TEST_F(PrintContextFrameTest, WithSubframe)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML("<iframe id='frame' src='http://b.com/' width='500' height='500'></iframe>");

    HTMLIFrameElement& iframe = *toHTMLIFrameElement(document().getElementById("frame"));
    OwnPtr<FrameLoaderClient> frameLoaderClient = adoptPtr(new FrameLoaderClientWithParent(document().frame()));
    RefPtrWillBePersistent<LocalFrame> subframe = LocalFrame::create(frameLoaderClient.get(), document().frame()->host(), &iframe);
    subframe->setView(FrameView::create(subframe.get(), IntSize(500, 500)));
    subframe->init();
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(subframe.get());
    document().frame()->host()->incrementSubframeCount();

    Document& frameDocument = *iframe.contentDocument();
    frameDocument.setBaseURLOverride(KURL(ParsedURLString, "http://b.com/"));
    frameDocument.body()->setInnerHTML(htmlForLink(50, 60, 70, 80, "#fragment")
        + htmlForLink(150, 160, 170, 180, "http://www.google.com")
        + htmlForLink(250, 260, 270, 280, "http://www.google.com#fragment"),
        ASSERT_NO_EXCEPTION);
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());

    size_t firstIndex = operations[0].rect.x() == 150 ? 0 : 1;
    EXPECT_EQ(MockCanvas::DrawRect, operations[firstIndex].type);
    EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[firstIndex].rect);

    size_t secondIndex = firstIndex == 0 ? 1 : 0;
    EXPECT_EQ(MockCanvas::DrawRect, operations[secondIndex].type);
    EXPECT_SKRECT_EQ(250, 260, 270, 280, operations[secondIndex].rect);

    subframe->detach();
    static_cast<SingleChildFrameLoaderClient*>(document().frame()->client())->setChild(nullptr);
    document().frame()->host()->decrementSubframeCount();
}

} // namespace blink
