// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/page/PrintContext.h"

#include "core/dom/Document.h"
#include "core/html/HTMLElement.h"
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

    void outputLinkAndLinkedDestinations(GraphicsContext& context, Node* node, const IntRect& pageRect)
    {
        PrintContext::outputLinkAndLinkedDestinations(context, node, pageRect);
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

    virtual void drawRect(const SkRect& rect, const SkPaint& paint) override
    {
        ASSERT_TRUE(paint.getAnnotation());
        Operation operation = { DrawRect, rect };
        m_recordedOperations.append(operation);
    }

    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint) override
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
    PrintContextTest()
        : m_pageHolder(DummyPageHolder::create(IntSize(kPageWidth, kPageHeight)))
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
        GraphicsContext context(&canvas);
        printContext().begin(kPageWidth, kPageHeight);
        printContext().outputLinkAndLinkedDestinations(context, &document(), pageRect);
        printContext().end();
    }

    static String htmlForLink(int x, int y, int width, int height, const char* url)
    {
        TextStream ts;
        ts << "<a style='position: absolute; left: " << x << "px; top: " << y << "px; width: " << width << "px; height: " << height
            << "px' href='" << url << "'>" << url << "</a>";
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

} // namespace blink
