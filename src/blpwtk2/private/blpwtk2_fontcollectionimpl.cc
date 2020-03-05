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

#include <blpwtk2_fontcollectionimpl.h>
#include "base/debug/alias.h"
#include "base/stl_util.h"
#include <base/files/memory_mapped_file.h>
#include <base/strings/string16.h>

namespace mswr = Microsoft::WRL;

namespace blpwtk2 {

                    // ====================
                    // class FontFileStream
                    // ====================

class FontFileStream
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileStream> {
  public:
    FontFileStream();
    ~FontFileStream() override;

    // IDWriteFontFileStream methods.
    HRESULT STDMETHODCALLTYPE ReadFileFragment(
        void const **fragment_start,
        UINT64       file_offset,
        UINT64       fragment_size,
        void       **context) override;

    void STDMETHODCALLTYPE ReleaseFileFragment(void *context) override;
    HRESULT STDMETHODCALLTYPE GetFileSize(UINT64 *file_size) override;
    HRESULT STDMETHODCALLTYPE GetLastWriteTime(UINT64 *last_write_time) override;

    HRESULT RuntimeClassInitialize(const std::wstring& font_file);

  private:
    mswr::ComPtr<IDWriteFontCollectionLoader> d_font_loader;
    std::unique_ptr<base::MemoryMappedFile> d_memory;

    DISALLOW_COPY_AND_ASSIGN(FontFileStream);
};


                    // ====================
                    // class FontFileLoader
                    // ====================

class FontFileLoader final
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteLocalFontFileLoader> {
  public:
    explicit FontFileLoader(
            const std::vector<std::wstring>& font_files);

    // IDWriteFontFileLoader methods.
    HRESULT STDMETHODCALLTYPE CreateStreamFromKey(
            void const             *ref_key,
            UINT32                  ref_key_size,
            IDWriteFontFileStream **stream) override;

    // IDWriteLocalFontFileLoader methods
    HRESULT STDMETHODCALLTYPE GetFilePathFromKey(
            const void *ref_key,
            UINT32      ref_key_size,
            wchar_t    *file_path,
            UINT32      file_path_size) override;

    HRESULT STDMETHODCALLTYPE GetFilePathLengthFromKey(
            const void *ref_key,
            UINT32      ref_key_size,
            UINT32     *file_path_length) override;

    HRESULT STDMETHODCALLTYPE GetLastWriteTimeFromKey(
            const void *ref_key,
            UINT32      ref_key_size,
            FILETIME   *last_write_time) override;

  private:
    const std::wstring *Lookup(void const *ref_key,
                               UINT32      ref_key_size);

    std::vector<std::wstring> d_font_files;
    DISALLOW_COPY_AND_ASSIGN(FontFileLoader);
};

                    // ========================
                    // class FontFileEnumerator
                    // ========================

class FontFileEnumerator final
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileEnumerator> {
  public:
    explicit FontFileEnumerator(
            const mswr::ComPtr<IDWriteFactory>&        factory,
            const mswr::ComPtr<IDWriteFontFileLoader>& file_loader,
            std::vector<std::wstring>&&                font_files);

    // IDWriteFontFileEnumerator methods.
    HRESULT STDMETHODCALLTYPE MoveNext(BOOL *has_current_file) override;
    HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile **font_file) override;

  private:
    mswr::ComPtr<IDWriteFactory> d_factory;
    mswr::ComPtr<IDWriteFontFileLoader> d_file_loader;
    mswr::ComPtr<IDWriteFontFile> d_current_file;
    unsigned int d_font_idx;
    std::vector<std::wstring> d_font_files;

    DISALLOW_COPY_AND_ASSIGN(FontFileEnumerator);
};


                    // ==========================
                    // class FontCollectionLoader
                    // ==========================

class FontCollectionLoader final
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontCollectionLoader> {
  public:
    explicit FontCollectionLoader(
        const mswr::ComPtr<IDWriteFontFileLoader>& file_loader,
        const std::vector<std::wstring>&&          font_files);

    // IDWriteFontCollectionLoader methods.
    HRESULT STDMETHODCALLTYPE
    CreateEnumeratorFromKey(IDWriteFactory             *factory,
                            void const                 *key,
                            UINT32                      key_size,
                            IDWriteFontFileEnumerator **file_enumerator) override;

  private:
    mswr::ComPtr<IDWriteFontFileLoader> d_file_loader;
    std::vector<std::wstring> d_font_files;

    DISALLOW_COPY_AND_ASSIGN(FontCollectionLoader);
};

                    // =============================
                    // class CompositeFontCollection
                    // =============================

class CompositeFontCollection final
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontCollection> {
  public:
    CompositeFontCollection(
            const std::vector<mswr::ComPtr<IDWriteFontCollection>>&& collections);

    // IDWriteFontCollection
    HRESULT STDMETHODCALLTYPE FindFamilyName(const wchar_t *family_name,
                                             UINT32        *index,
                                             BOOL          *exists) override;

    HRESULT STDMETHODCALLTYPE GetFontFamily(UINT32              index,
                                            IDWriteFontFamily **font_family) override;

    UINT32 STDMETHODCALLTYPE GetFontFamilyCount() override;

    HRESULT STDMETHODCALLTYPE GetFontFromFontFace(IDWriteFontFace  *font_face,
                                                  IDWriteFont     **font) override;

  private:
    std::vector<mswr::ComPtr<IDWriteFontCollection>> d_collections;

    DISALLOW_COPY_AND_ASSIGN(CompositeFontCollection);
};

                    // --------------------
                    // class FontFileStream
                    // --------------------

FontFileStream::FontFileStream()
{
}

FontFileStream::~FontFileStream()
{
}

// IDWriteFontFileStream methods.
HRESULT STDMETHODCALLTYPE FontFileStream::ReadFileFragment(
        void const **fragment_start,
        UINT64       file_offset,
        UINT64       fragment_size,
        void       **context)
{
    if (d_memory.get() && d_memory->IsValid() &&
        file_offset < d_memory->length() &&
        (file_offset + fragment_size) <= d_memory->length()) {

        *fragment_start = static_cast<BYTE const*>(d_memory->data()) + static_cast<size_t>(file_offset);
        *context = nullptr;
        return S_OK;
    }

    return E_FAIL;
}

void STDMETHODCALLTYPE FontFileStream::ReleaseFileFragment(void *context)
{
}

HRESULT STDMETHODCALLTYPE FontFileStream::GetFileSize(UINT64 *file_size)
{
    if (d_memory.get() && d_memory->IsValid()) {
        *file_size = d_memory->length();
        return S_OK;
    }

    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FontFileStream::GetLastWriteTime(UINT64 *last_write_time)
{
    if (d_memory.get() && d_memory->IsValid()) {
        // According to MSDN article http://goo.gl/rrSYzi the "last modified time"
        // is used by DirectWrite font selection algorithms to determine whether
        // one font resource is more up to date than another one.
        // So by returning 0 we are assuming that it will treat all fonts to be
        // equally up to date.
        *last_write_time = 0;
        return S_OK;
    }

    return E_FAIL;
}

HRESULT FontFileStream::RuntimeClassInitialize(const std::wstring& font_file)
{
    base::string16 font_file_name(font_file);
    base::FilePath path = base::FilePath(font_file_name);
    d_memory.reset(new base::MemoryMappedFile());

    // Put some debug information on stack.
    WCHAR font_name[MAX_PATH];
    path.value().copy(font_name, base::size(font_name));
    base::debug::Alias(font_name);

    if (d_memory->Initialize(path)) {
        return S_OK;
    }

    d_memory.reset();
    return E_FAIL;
}

                    // --------------------
                    // class FontFileLoader
                    // --------------------

FontFileLoader::FontFileLoader(
        const std::vector<std::wstring>& font_files)
    : d_font_files(font_files)
{
}

const std::wstring *FontFileLoader::Lookup(void const *ref_key,
                                           UINT32      ref_key_size)
{
    DCHECK(ref_key_size == sizeof(unsigned int));
    if (ref_key_size == sizeof(unsigned int)) {
        unsigned int font_key = *static_cast<const unsigned int*>(ref_key);

        DCHECK(font_key < d_font_files.size());
        if (font_key < d_font_files.size()) {
            return &d_font_files[font_key];
        }
    }

    return nullptr;
}


// IDWriteFontFileLoader methods.
HRESULT STDMETHODCALLTYPE FontFileLoader::CreateStreamFromKey(
        void const             *ref_key,
        UINT32                  ref_key_size,
        IDWriteFontFileStream **stream)
{
    const std::wstring *value = Lookup(ref_key, ref_key_size);
    DCHECK(value);

    if (value) {
        mswr::ComPtr<FontFileStream> font_stream;
        HRESULT hr = mswr::MakeAndInitialize<FontFileStream>(
                &font_stream,
                *value);

        DCHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) {
            *stream = font_stream.Detach();
            return S_OK;
        }
    }

    return E_FAIL;
}

// IDWriteLocalFontFileLoader methods
HRESULT STDMETHODCALLTYPE FontFileLoader::GetFilePathFromKey(
        const void *ref_key,
        UINT32      ref_key_size,
        wchar_t    *file_path,
        UINT32      file_path_size)
{
    const std::wstring *value = Lookup(ref_key, ref_key_size);
    DCHECK(value);

    if (value && value->size() <= file_path_size) {
        memcpy(file_path, value->data(), value->size() * sizeof(wchar_t));
        return S_OK;
    }

    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FontFileLoader::GetFilePathLengthFromKey(
        const void *ref_key,
        UINT32      ref_key_size,
        UINT32     *file_path_length)
{
    const std::wstring *value = Lookup(ref_key, ref_key_size);
    DCHECK(value);

    if (value) {
        *file_path_length = value->size() * sizeof(wchar_t);
        return S_OK;
    }

    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FontFileLoader::GetLastWriteTimeFromKey(
        const void *ref_key,
        UINT32      ref_key_size,
        FILETIME   *last_write_time)
{
    // According to MSDN article http://goo.gl/rrSYzi the "last modified time"
    // is used by DirectWrite font selection algorithms to determine whether
    // one font resource is more up to date than another one.
    // So by returning 0 we are assuming that it will treat all fonts to be
    // equally up to date.
    memset(last_write_time, 0, sizeof(FILETIME));
    return S_OK;
}

                    // ------------------------
                    // class FontFileEnumerator
                    // ------------------------

FontFileEnumerator::FontFileEnumerator(
        const mswr::ComPtr<IDWriteFactory>&        factory,
        const mswr::ComPtr<IDWriteFontFileLoader>& file_loader,
        std::vector<std::wstring>&&                font_files)
    : d_factory(factory)
    , d_file_loader(file_loader)
    , d_font_idx(0)
    , d_font_files(std::move(font_files))
{
}

HRESULT STDMETHODCALLTYPE FontFileEnumerator::MoveNext(BOOL *has_current_file)
{
    *has_current_file = FALSE;

    if (d_current_file) {
        d_current_file.ReleaseAndGetAddressOf();
    }

    if (d_font_idx < d_font_files.size()) {
        HRESULT hr;

        if (!d_file_loader.Get()) {
            FILETIME file_time = {};
            hr =
                d_factory->CreateFontFileReference(d_font_files[d_font_idx].c_str(),
                                                   &file_time,
                                                   d_current_file.GetAddressOf());
        }
        else {
            hr =
                d_factory->CreateCustomFontFileReference(&d_font_idx,
                                                         sizeof(unsigned int),
                                                         d_file_loader.Get(),
                                                         d_current_file.GetAddressOf());
        }

        DCHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) {
            *has_current_file = TRUE;
            ++d_font_idx;
            return S_OK;
        }

        return E_FAIL;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FontFileEnumerator::GetCurrentFontFile(IDWriteFontFile **font_file)
{
    if (d_current_file) {
        *font_file = d_current_file.Detach();
        return S_OK;
    }

    *font_file = nullptr;
    return E_FAIL;
}

                    // --------------------------
                    // class FontCollectionLoader
                    // --------------------------

FontCollectionLoader::FontCollectionLoader(
        const mswr::ComPtr<IDWriteFontFileLoader>& file_loader,
        const std::vector<std::wstring>&&          font_files)
    : d_file_loader(file_loader)
    , d_font_files(std::move(font_files))
{
}

// IDWriteFontCollectionLoader methods.
HRESULT STDMETHODCALLTYPE FontCollectionLoader::CreateEnumeratorFromKey(
        IDWriteFactory             *factory,
        void const                 *key,
        UINT32                      key_size,
        IDWriteFontFileEnumerator **file_enumerator)
{
    *file_enumerator = mswr::Make<FontFileEnumerator>(
            factory,
            d_file_loader,
            std::move(d_font_files)).Detach();

    return S_OK;
}

                    // -----------------------------
                    // class CompositeFontCollection
                    // -----------------------------

CompositeFontCollection::CompositeFontCollection(
        const std::vector<mswr::ComPtr<IDWriteFontCollection>>&& collections)
    : d_collections(std::move(collections))
{
}

// IDWriteFontCollection
HRESULT STDMETHODCALLTYPE CompositeFontCollection::FindFamilyName(
        const wchar_t *family_name, UINT32 *index, BOOL *exists)
{
    HRESULT hr = E_FAIL;
    UINT32 offset = 0;

    for (auto collection : d_collections) {
        hr = collection->FindFamilyName(family_name, index, exists);

        if (*exists) {
            *index += offset;
            break;
        }

        offset += collection->GetFontFamilyCount();
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE CompositeFontCollection::GetFontFamily(
        UINT32 index, IDWriteFontFamily **font_family)
{
    HRESULT hr = E_FAIL;

    for (auto collection : d_collections) {
        unsigned int font_family_count = collection->GetFontFamilyCount();

        if (index < font_family_count) {
            hr = collection->GetFontFamily(index, font_family);
            break;
        }

        index -= font_family_count;
    }

    return hr;
}

UINT32 STDMETHODCALLTYPE CompositeFontCollection::GetFontFamilyCount()
{
    UINT32 count = 0;

    for (auto collection : d_collections) {
        count += collection->GetFontFamilyCount();
    }

    return count;
}

HRESULT STDMETHODCALLTYPE CompositeFontCollection::GetFontFromFontFace(
        IDWriteFontFace *font_face, IDWriteFont **font)
{
    HRESULT hr = E_FAIL;

    for (auto collection : d_collections) {
        hr = collection->GetFontFromFontFace(font_face, font);
        if (SUCCEEDED(hr)) {
            break;
        }
    }

    return hr;
}


                    // ------------------------
                    // class FontCollectionImpl
                    // ------------------------

// static
FontCollectionImpl *FontCollectionImpl::GetCurrent()
{
    static FontCollectionImpl s_font_collection;
    return &s_font_collection;
}

void FontCollectionImpl::SetCustomFonts(
    const std::vector<std::wstring>&& font_files)
{
    // Make sure GetFontCollection() isn't called before SetCustomFonts()
    DCHECK(!d_font_collection.Get());
    d_font_files = std::move(font_files);
}

int FontCollectionImpl::GetFontCollection(
        IDWriteFactory         *factory,
        IDWriteFontCollection **font_collection)
{
    if (d_font_collection.Get() != nullptr) {
        d_font_collection.CopyTo(font_collection);
        return S_OK;
    }

    Microsoft::WRL::ComPtr<IDWriteFontCollection> custom_font_collection;
    HRESULT hr = S_OK;

    if (!d_font_files.empty()) {
        hr = GetPrivateFontCollection(factory,
                                      std::move(d_font_files),
                                      &custom_font_collection);
    }

    DCHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
        std::vector<mswr::ComPtr<IDWriteFontCollection>> collections;

        // Add the custom fonts (if available)
        if (custom_font_collection.Get()) {
            collections.push_back(custom_font_collection);
            custom_font_collection.ReleaseAndGetAddressOf();
        }

        // Add the the system fonts to the collection
        mswr::ComPtr<IDWriteFontCollection> systemFonts;
        factory->GetSystemFontCollection(systemFonts.GetAddressOf(), FALSE);
        collections.push_back(systemFonts);

        // Create a composite font collection
        d_font_collection =
            mswr::Make<CompositeFontCollection>(std::move(collections));

        d_font_collection.CopyTo(font_collection);
        return S_OK;
    }

    return hr;
}

FontCollectionImpl::FontCollectionImpl()
{
}

FontCollectionImpl::~FontCollectionImpl()
{
}

HRESULT FontCollectionImpl::GetPrivateFontLoader(
        const mswr::ComPtr<IDWriteFactory>&        factory,
        const std::vector<std::wstring>&&          font_files,
        mswr::ComPtr<IDWriteFontCollectionLoader> *font_loader)
{
    mswr::ComPtr<IDWriteFontFileLoader> file_loader;

#if !defined(USE_BUILTIN_LOADER)
    file_loader = mswr::Make<FontFileLoader>(font_files);

    if (file_loader.Get()) {
        factory->RegisterFontFileLoader(file_loader.Get());
    }
#endif

    *font_loader = mswr::Make<FontCollectionLoader>(file_loader,
                                                    std::move(font_files));

    if (font_loader->Get()) {
        factory->RegisterFontCollectionLoader(font_loader->Get());
        return S_OK;
    }

    DCHECK(false);
    return E_FAIL;
}

HRESULT FontCollectionImpl::GetPrivateFontCollection(
        const mswr::ComPtr<IDWriteFactory>&  factory,
        const std::vector<std::wstring>&&    font_files,
        mswr::ComPtr<IDWriteFontCollection> *font_collection)
{
    TRACE_EVENT0("startup", "blpwtk2::GetPrivateFontCollection");

    mswr::ComPtr<IDWriteFontCollectionLoader> font_loader;
    HRESULT hr = GetPrivateFontLoader(factory,
                                      std::move(font_files),
                                      &font_loader);

    if (SUCCEEDED(hr)) {
        hr = factory->CreateCustomFontCollection(
                font_loader.Get(),
                nullptr,
                0,
                font_collection->GetAddressOf());

        if (SUCCEEDED(hr)) {
            return S_OK;
        }
    }

    DCHECK(false);
    return E_FAIL;
}



}

