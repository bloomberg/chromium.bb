/*
 * Copyright (C) 2017 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_FONTCOLLECTION_H
#define INCLUDED_BLPWTK2_FONTCOLLECTION_H

#include <content/public/browser/browser_message_filter.h>
#include <dwrite.h>
#include <wrl.h>

namespace blpwtk2 {

                    // ========================
                    // class FontCollectionImpl
                    // ========================

class FontCollectionImpl final : public content::FontCollection {
  public:
    static FontCollectionImpl *GetCurrent();

    void SetCustomFonts(const std::vector<std::wstring>&& font_files);

    // content::FontCollection methods
    int GetFontCollection(
            IDWriteFactory         *factory,
            IDWriteFontCollection **font_collection) override;

  private:
    FontCollectionImpl();
    ~FontCollectionImpl();

    HRESULT GetPrivateFontLoader(
            const Microsoft::WRL::ComPtr<IDWriteFactory>&        factory,
            const std::vector<std::wstring>&&                    font_files,
            Microsoft::WRL::ComPtr<IDWriteFontCollectionLoader> *font_loader);

    HRESULT GetPrivateFontCollection(
            const Microsoft::WRL::ComPtr<IDWriteFactory>&  factory,
            const std::vector<std::wstring>&&              font_files,
            Microsoft::WRL::ComPtr<IDWriteFontCollection> *font_collection);

    Microsoft::WRL::ComPtr<IDWriteFontCollection> d_font_collection;
    std::vector<std::wstring> d_font_files;

    DISALLOW_COPY_AND_ASSIGN(FontCollectionImpl);
};
} // namespace blpwtk2

#endif

