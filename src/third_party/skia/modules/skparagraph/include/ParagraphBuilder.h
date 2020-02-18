// Copyright 2019 Google LLC.
#ifndef ParagraphBuilder_DEFINED
#define ParagraphBuilder_DEFINED

#include <memory>
#include <stack>
#include <string>
#include <tuple>
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"

namespace skia {
namespace textlayout {

class ParagraphBuilder {
public:
    ParagraphBuilder(const ParagraphStyle& style, sk_sp<FontCollection> fontCollection) { }

    virtual ~ParagraphBuilder() = default;

    // Push a style to the stack. The corresponding text added with AddText will
    // use the top-most style.
    virtual void pushStyle(const TextStyle& style) = 0;

    // Remove a style from the stack. Useful to apply different styles to chunks
    // of text such as bolding.
    // Example:
    //   builder.PushStyle(normal_style);
    //   builder.AddText("Hello this is normal. ");
    //
    //   builder.PushStyle(bold_style);
    //   builder.AddText("And this is BOLD. ");
    //
    //   builder.Pop();
    //   builder.AddText(" Back to normal again.");
    virtual void pop() = 0;

    virtual TextStyle peekStyle() = 0;

    // Adds text to the builder. Forms the proper runs to use the upper-most style
    // on the style_stack_;
    virtual void addText(const std::u16string& text) = 0;

    // Converts to u16string before adding.
    virtual void addText(const char* text) = 0;

    virtual void setParagraphStyle(const ParagraphStyle& style) = 0;

    // Constructs a SkParagraph object that can be used to layout and paint the text to a SkCanvas.
    virtual std::unique_ptr<Paragraph> Build() = 0;

    static std::unique_ptr<ParagraphBuilder> make(const ParagraphStyle& style,
                                                  sk_sp<FontCollection> fontCollection);
};
}  // namespace textlayout
}  // namespace skia

#endif  // ParagraphBuilder_DEFINED
