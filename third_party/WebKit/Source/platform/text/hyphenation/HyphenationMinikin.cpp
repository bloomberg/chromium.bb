// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "public/platform/modules/hyphenation/hyphenation.mojom-blink.h"

namespace blink {

class HyphenationMinikin : public Hyphenation {
public:
    bool openDictionary(const AtomicString& locale);

    size_t lastHyphenLocation(const StringView& text, size_t beforeIndex) const override;

private:
    static base::PlatformFile openDictionaryFile(const AtomicString& locale);

    base::MemoryMappedFile m_file;
};

static mojom::blink::HyphenationPtr connectToRemoteService()
{
    mojom::blink::HyphenationPtr service;
    Platform::current()->serviceRegistry()->connectToRemoteService(
        mojo::GetProxy(&service));
    return service;
}

static const mojom::blink::HyphenationPtr& getService()
{
    DEFINE_STATIC_LOCAL(mojom::blink::HyphenationPtr, service,
        (connectToRemoteService()));
    return service;
}

base::PlatformFile HyphenationMinikin::openDictionaryFile(const AtomicString& locale)
{
    const mojom::blink::HyphenationPtr& service = getService();
    mojo::ScopedHandle handle;
    base::ElapsedTimer timer;
    service->OpenDictionary(locale, &handle);
    UMA_HISTOGRAM_TIMES("Hyphenation.Open", timer.Elapsed());
    if (!handle.is_valid())
        return base::kInvalidPlatformFile;

    base::PlatformFile file;
    MojoResult result = mojo::UnwrapPlatformFile(std::move(handle), &file);
    if (result != MOJO_RESULT_OK) {
        DLOG(ERROR) << "UnwrapPlatformFile failed";
        return base::kInvalidPlatformFile;
    }
    return file;
}

bool HyphenationMinikin::openDictionary(const AtomicString& locale)
{
    base::PlatformFile file = openDictionaryFile(locale);
    if (file == base::kInvalidPlatformFile)
        return false;
    if (!m_file.Initialize(base::File(file))) {
        DLOG(ERROR) << "mmap failed";
        return false;
    }

    // TODO(kojii): Create dictionary from m_file when Minikin is ready.

    return true;
}

size_t HyphenationMinikin::lastHyphenLocation(const StringView& text, size_t beforeIndex) const
{
    // TODO(kojii): Call minikin using the dictionary when Minikin is ready.
    return 0;
}

PassRefPtr<Hyphenation> Hyphenation::platformGetHyphenation(const AtomicString& locale)
{
    RefPtr<HyphenationMinikin> hyphenation(adoptRef(new HyphenationMinikin));
    if (!hyphenation->openDictionary(locale))
        return nullptr;
    return hyphenation.release();
}

} // namespace blink
