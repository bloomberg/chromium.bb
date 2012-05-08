/* libs/graphics/ports/SkFontHost_fontconfig.cpp
**
** Copyright 2008, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

// -----------------------------------------------------------------------------
// This file provides implementations of the font resolution members of
// SkFontHost by using the fontconfig[1] library. Fontconfig is usually found
// on Linux systems and handles configuration, parsing and caching issues
// involved with enumerating and matching fonts.
//
// [1] http://fontconfig.org
// -----------------------------------------------------------------------------

#include <map>
#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "SkFontHost.h"
#include "SkStream.h"
#include "SkFontHost_fontconfig_control.h"
#include "SkFontHost_fontconfig_impl.h"
#include "SkFontHost_fontconfig_direct.h"

static FontConfigInterface* global_fc_impl = NULL;

void SkiaFontConfigUseDirectImplementation() {
    if (global_fc_impl)
        delete global_fc_impl;
    global_fc_impl = new FontConfigDirect;
}

void SkiaFontConfigSetImplementation(FontConfigInterface* font_config) {
    if (global_fc_impl)
        delete global_fc_impl;
    global_fc_impl = font_config;
}

static FontConfigInterface* GetFcImpl() {
  if (!global_fc_impl)
      global_fc_impl = new FontConfigDirect;
  return global_fc_impl;
}

SK_DECLARE_STATIC_MUTEX(global_remote_font_map_lock);
static std::map<uint32_t, std::pair<uint8_t*, size_t> >* global_remote_fonts;

// Initialize the map declared above. Note that its corresponding mutex must be
// locked before calling this function.
static void AllocateGlobalRemoteFontsMapOnce() {
    if (!global_remote_fonts) {
        global_remote_fonts =
            new std::map<uint32_t, std::pair<uint8_t*, size_t> >();
    }
}

static unsigned global_next_remote_font_id;

// This is the maximum size of the font cache.
static const unsigned kFontCacheMemoryBudget = 2 * 1024 * 1024;  // 2MB

// UniqueIds are encoded as (filefaceid << 8) | style
// For system fonts, filefaceid = (fileid << 4) | face_index.
// For remote fonts, filefaceid = fileid.

static unsigned UniqueIdToFileFaceId(unsigned uniqueid)
{
    return uniqueid >> 8;
}

static SkTypeface::Style UniqueIdToStyle(unsigned uniqueid)
{
    return static_cast<SkTypeface::Style>(uniqueid & 0xff);
}

static unsigned FileFaceIdAndStyleToUniqueId(unsigned filefaceid,
                                             SkTypeface::Style style)
{
    SkASSERT((style & 0xff) == style);
    return (filefaceid << 8) | static_cast<int>(style);
}

static const unsigned kRemoteFontMask = 0x00800000u;

static bool IsRemoteFont(unsigned filefaceid)
{
    return filefaceid & kRemoteFontMask;
}

class FontConfigTypeface : public SkTypeface {
public:
    FontConfigTypeface(Style style, uint32_t id)
        : SkTypeface(style, id)
    { }

    ~FontConfigTypeface()
    {
        const uint32_t id = uniqueID();
        if (IsRemoteFont(UniqueIdToFileFaceId(id))) {
            SkAutoMutexAcquire ac(global_remote_font_map_lock);
            AllocateGlobalRemoteFontsMapOnce();
            std::map<uint32_t, std::pair<uint8_t*, size_t> >::iterator iter
                = global_remote_fonts->find(id);
            if (iter != global_remote_fonts->end()) {
                sk_free(iter->second.first);  // remove the font on memory.
                global_remote_fonts->erase(iter);
            }
        }
    }
};

// static
SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       SkTypeface::Style style)
{
    std::string resolved_family_name;

    if (familyFace) {
        // Given the fileid we can ask fontconfig for the familyname of the
        // font.
        const unsigned filefaceid = UniqueIdToFileFaceId(familyFace->uniqueID());
        if (!GetFcImpl()->Match(&resolved_family_name, NULL,
          true /* filefaceid valid */, filefaceid, "",
          NULL, 0, NULL, NULL)) {
            return NULL;
        }
    } else if (familyName) {
        resolved_family_name = familyName;
    }

    bool bold = style & SkTypeface::kBold;
    bool italic = style & SkTypeface::kItalic;
    unsigned filefaceid;
    if (!GetFcImpl()->Match(NULL, &filefaceid,
                            false, -1, /* no filefaceid */
                            resolved_family_name, NULL, 0,
                            &bold, &italic)) {
        return NULL;
    }
    const SkTypeface::Style resulting_style = static_cast<SkTypeface::Style>(
        (bold ? SkTypeface::kBold : 0) |
        (italic ? SkTypeface::kItalic : 0));

    const unsigned id = FileFaceIdAndStyleToUniqueId(filefaceid,
                                                     resulting_style);
    SkTypeface* typeface = SkNEW_ARGS(FontConfigTypeface, (resulting_style, id));
    return typeface;
}

// static
SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream)
{
    if (!stream)
        return NULL;

    const size_t length = stream->read(0, 0);
    if (!length)
        return NULL;
    if (length >= 1024 * 1024 * 1024)
        return NULL;  // don't accept too large fonts (>= 1GB) for safety.

    uint8_t* font = (uint8_t*)sk_malloc_throw(length);
    if (stream->read(font, length) != length) {
        sk_free(font);
        return NULL;
    }

    SkTypeface::Style style = static_cast<SkTypeface::Style>(0);
    unsigned id = 0;
    {
        SkAutoMutexAcquire ac(global_remote_font_map_lock);
        AllocateGlobalRemoteFontsMapOnce();
        id = FileFaceIdAndStyleToUniqueId(
            global_next_remote_font_id | kRemoteFontMask, style);

        if (++global_next_remote_font_id >= kRemoteFontMask)
            global_next_remote_font_id = 0;

        if (!global_remote_fonts->insert(
                std::make_pair(id, std::make_pair(font, length))).second) {
            sk_free(font);
            return NULL;
        }
    }

    SkTypeface* typeface = SkNEW_ARGS(FontConfigTypeface, (style, id));
    return typeface;
}

// static
SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[])
{
    SkASSERT(!"SkFontHost::CreateTypefaceFromFile unimplemented");
    return NULL;
}

void SkFontHost::Serialize(const SkTypeface*, SkWStream*) {
    SkASSERT(!"SkFontHost::Serialize unimplemented");
}

SkTypeface* SkFontHost::Deserialize(SkStream* stream) {
    SkASSERT(!"SkFontHost::Deserialize unimplemented");
    return NULL;
}

// static
uint32_t SkFontHost::NextLogicalFont(SkFontID curr, SkFontID orig) {
    // We don't handle font fallback, WebKit does.
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

class SkFileDescriptorStream : public SkStream {
  public:
    SkFileDescriptorStream(int fd) {
        memory_ = NULL;
        offset_ = 0;

        // this ensures that if we fail in the constructor, we will safely
        // ignore all subsequent calls to read() because we will always trim
        // the requested size down to 0
        length_ = 0;

        struct stat st;
        if (fstat(fd, &st))
            return;

        void* memory = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if (memory == MAP_FAILED)
            return;

        memory_ = reinterpret_cast<uint8_t*>(memory);
        length_ = st.st_size;
    }

    ~SkFileDescriptorStream() {
        munmap(const_cast<uint8_t*>(memory_), length_);
    }

    virtual bool rewind() {
        offset_ = 0;
        return true;
    }

    // SkStream implementation.
    virtual size_t read(void* buffer, size_t size) {
        if (!buffer && !size) {
            // This is request for the length of the stream.
            return length_;
        }

        size_t remaining = length_ - offset_;
        if (size > remaining)
            size = remaining;
        if (buffer)
            memcpy(buffer, memory_ + offset_, size);

        offset_ += size;
        return size;
    }

    virtual const void* getMemoryBase() {
        return memory_;
    }

  private:
    const uint8_t* memory_;
    size_t offset_, length_;
};

///////////////////////////////////////////////////////////////////////////////

// static
SkStream* SkFontHost::OpenStream(uint32_t id)
{
    const unsigned filefaceid = UniqueIdToFileFaceId(id);

    if (IsRemoteFont(filefaceid)) {
      // remote font
      SkAutoMutexAcquire ac(global_remote_font_map_lock);
      AllocateGlobalRemoteFontsMapOnce();
      std::map<uint32_t, std::pair<uint8_t*, size_t> >::const_iterator iter
          = global_remote_fonts->find(id);
      if (iter == global_remote_fonts->end())
          return NULL;
      return SkNEW_ARGS(
          SkMemoryStream, (iter->second.first, iter->second.second));
    }

    // system font
    const int fd = GetFcImpl()->Open(filefaceid);
    if (fd < 0)
        return NULL;

    return SkNEW_ARGS(SkFileDescriptorStream, (fd));
}

// static
size_t SkFontHost::GetFileName(SkFontID fontID, char path[], size_t length,
                               int32_t* index) {
    const unsigned filefaceid = UniqueIdToFileFaceId(fontID);

    if (IsRemoteFont(filefaceid))
        return 0;

    if (index) {
        *index = filefaceid & 0xfu;
        // 1 is a bogus return value.
        // We had better change the signature of this function in Skia
        // to return bool to indicate success/failure and have another
        // out param for fileName length.
        if (!path)
          return 1;
    }

    if (path)
        SkASSERT(!"SkFontHost::GetFileName does not support the font path "
                  "retrieval.");

    return 0;
}
