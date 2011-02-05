// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/emf_win.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/rect.h"

namespace {
const int kCustomGdiCommentSignature = 0xdeadbabe;
struct PageBreakRecord {
  int signature;
  enum PageBreakType {
    START_PAGE,
    END_PAGE,
  } type;
  explicit PageBreakRecord(PageBreakType type_in)
      : signature(kCustomGdiCommentSignature), type(type_in) {
  }
  bool IsValid() const {
    return (signature == kCustomGdiCommentSignature) &&
           (type >= START_PAGE) && (type <= END_PAGE);
  }
};
}

namespace printing {

bool DIBFormatNativelySupported(HDC dc, uint32 escape, const BYTE* bits,
                                int size) {
  BOOL supported = FALSE;
  if (ExtEscape(dc, QUERYESCSUPPORT, sizeof(escape),
                reinterpret_cast<LPCSTR>(&escape), 0, 0) > 0) {
    ExtEscape(dc, escape, size, reinterpret_cast<LPCSTR>(bits),
              sizeof(supported), reinterpret_cast<LPSTR>(&supported));
  }
  return !!supported;
}

Emf::Emf() : emf_(NULL), hdc_(NULL) {
}

Emf::~Emf() {
  CloseEmf();
  DCHECK(!emf_ && !hdc_);
}

bool Emf::Init(const void* src_buffer, uint32 src_buffer_size) {
  DCHECK(!emf_ && !hdc_);
  emf_ = SetEnhMetaFileBits(src_buffer_size,
                            reinterpret_cast<const BYTE*>(src_buffer));
  return emf_ != NULL;
}

bool Emf::CreateDc(HDC sibling, const RECT* rect) {
  DCHECK(!emf_ && !hdc_);
  hdc_ = CreateEnhMetaFile(sibling, NULL, rect, NULL);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

bool Emf::CreateFileBackedDc(HDC sibling, const RECT* rect,
                             const FilePath& path) {
  DCHECK(!emf_ && !hdc_);
  DCHECK(!path.empty());
  hdc_ = CreateEnhMetaFile(sibling, path.value().c_str(), rect, NULL);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

bool Emf::CreateFromFile(const FilePath& metafile_path) {
  DCHECK(!emf_ && !hdc_);
  emf_ = GetEnhMetaFile(metafile_path.value().c_str());
  DCHECK(emf_);
  return emf_ != NULL;
}


bool Emf::CloseDc() {
  DCHECK(!emf_ && hdc_);
  emf_ = CloseEnhMetaFile(hdc_);
  DCHECK(emf_);
  hdc_ = NULL;
  return emf_ != NULL;
}

void Emf::CloseEmf() {
  DCHECK(!hdc_);
  if (emf_) {
    DeleteEnhMetaFile(emf_);
    emf_ = NULL;
  }
}

bool Emf::Playback(HDC hdc, const RECT* rect) const {
  DCHECK(emf_ && !hdc_);
  RECT bounds;
  if (!rect) {
    // Get the natural bounds of the EMF buffer.
    bounds = GetBounds().ToRECT();
    rect = &bounds;
  }
  return PlayEnhMetaFile(hdc, emf_, rect) != 0;
}

bool Emf::SafePlayback(HDC context) const {
  DCHECK(emf_ && !hdc_);
  XFORM base_matrix;
  if (!GetWorldTransform(context, &base_matrix)) {
    NOTREACHED();
    return false;
  }
  return EnumEnhMetaFile(context,
                         emf_,
                         &Emf::SafePlaybackProc,
                         reinterpret_cast<void*>(&base_matrix),
                         &GetBounds().ToRECT()) != 0;
}

gfx::Rect Emf::GetBounds() const {
  DCHECK(emf_ && !hdc_);
  ENHMETAHEADER header;
  if (GetEnhMetaFileHeader(emf_, sizeof(header), &header) != sizeof(header)) {
    NOTREACHED();
    return gfx::Rect();
  }
  if (header.rclBounds.left == 0 &&
      header.rclBounds.top == 0 &&
      header.rclBounds.right == -1 &&
      header.rclBounds.bottom == -1) {
    // A freshly created EMF buffer that has no drawing operation has invalid
    // bounds. Instead of having an (0,0) size, it has a (-1,-1) size. Detect
    // this special case and returns an empty Rect instead of an invalid one.
    return gfx::Rect();
  }
  return gfx::Rect(header.rclBounds.left,
                   header.rclBounds.top,
                   header.rclBounds.right - header.rclBounds.left,
                   header.rclBounds.bottom - header.rclBounds.top);
}

uint32 Emf::GetDataSize() const {
  DCHECK(emf_ && !hdc_);
  return GetEnhMetaFileBits(emf_, 0, NULL);
}

bool Emf::GetData(void* buffer, uint32 size) const {
  DCHECK(emf_ && !hdc_);
  DCHECK(buffer && size);
  uint32 size2 =
      GetEnhMetaFileBits(emf_, size, reinterpret_cast<BYTE*>(buffer));
  DCHECK(size2 == size);
  return size2 == size && size2 != 0;
}

bool Emf::GetData(std::vector<uint8>* buffer) const {
  uint32 size = GetDataSize();
  if (!size)
    return false;

  buffer->resize(size);
  if (!GetData(&buffer->front(), size))
    return false;
  return true;
}

bool Emf::SaveTo(const std::wstring& filename) const {
  HANDLE file = CreateFile(filename.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           CREATE_ALWAYS, 0, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  bool success = false;
  std::vector<uint8> buffer;
  if (GetData(&buffer)) {
    DWORD written = 0;
    if (WriteFile(file, &*buffer.begin(), static_cast<DWORD>(buffer.size()),
                  &written, NULL) &&
        written == buffer.size()) {
      success = true;
    }
  }
  CloseHandle(file);
  return success;
}

int CALLBACK Emf::SafePlaybackProc(HDC hdc,
                                   HANDLETABLE* handle_table,
                                   const ENHMETARECORD* record,
                                   int objects_count,
                                   LPARAM param) {
  const XFORM* base_matrix = reinterpret_cast<const XFORM*>(param);
  EnumerationContext context;
  context.handle_table = handle_table;
  context.objects_count = objects_count;
  context.hdc = hdc;
  Record record_instance(&context, record);
  bool success = record_instance.SafePlayback(base_matrix);
  DCHECK(success);
  return 1;
}

Emf::Record::Record(const EnumerationContext* context,
                    const ENHMETARECORD* record)
    : record_(record),
      context_(context) {
  DCHECK(record_);
}

bool Emf::Record::Play() const {
  return 0 != PlayEnhMetaFileRecord(context_->hdc,
                                    context_->handle_table,
                                    record_,
                                    context_->objects_count);
}

bool Emf::Record::SafePlayback(const XFORM* base_matrix) const {
  // For EMF field description, see [MS-EMF] Enhanced Metafile Format
  // Specification.
  //
  // This is the second major EMF breakage I get; the first one being
  // SetDCBrushColor/SetDCPenColor/DC_PEN/DC_BRUSH being silently ignored.
  //
  // This function is the guts of the fix for bug 1186598. Some printer drivers
  // somehow choke on certain EMF records, but calling the corresponding
  // function directly on the printer HDC is fine. Still, playing the EMF record
  // fails. Go figure.
  //
  // The main issue is that SetLayout is totally unsupported on these printers
  // (HP 4500/4700). I used to call SetLayout and I stopped. I found out this is
  // not sufficient because GDI32!PlayEnhMetaFile internally calls SetLayout(!)
  // Damn.
  //
  // So I resorted to manually parse the EMF records and play them one by one.
  // The issue with this method compared to using PlayEnhMetaFile to play back
  // an EMF buffer is that the later silently fixes the matrix to take in
  // account the matrix currently loaded at the time of the call.
  // The matrix magic is done transparently when using PlayEnhMetaFile but since
  // I'm processing one field at a time, I need to do the fixup myself. Note
  // that PlayEnhMetaFileRecord doesn't fix the matrix correctly even when
  // called inside an EnumEnhMetaFile loop. Go figure (bis).
  //
  // So when I see a EMR_SETWORLDTRANSFORM and EMR_MODIFYWORLDTRANSFORM, I need
  // to fix the matrix according to the matrix previously loaded before playing
  // back the buffer. Otherwise, the previously loaded matrix would be ignored
  // and the EMF buffer would always be played back at its native resolution.
  // Duh.
  //
  // I also use this opportunity to skip over eventual EMR_SETLAYOUT record that
  // could remain.
  //
  // Another tweak we make is for JPEGs/PNGs in calls to StretchDIBits.
  // (Our Pepper plugin code uses a JPEG). If the printer does not support
  // JPEGs/PNGs natively we decompress the JPEG/PNG and then set it to the
  // device.
  // TODO(sanjeevr): We should also add JPEG/PNG support for SetSIBitsToDevice
  //
  // We also process any custom EMR_GDICOMMENT records which are our
  // placeholders for StartPage and EndPage.
  // Note: I should probably care about view ports and clipping, eventually.
  bool res;
  switch (record()->iType) {
    case EMR_STRETCHDIBITS: {
      const EMRSTRETCHDIBITS * sdib_record =
          reinterpret_cast<const EMRSTRETCHDIBITS*>(record());
      const BYTE* record_start = reinterpret_cast<const BYTE *>(record());
      const BITMAPINFOHEADER *bmih =
          reinterpret_cast<const BITMAPINFOHEADER *>(record_start +
                                                     sdib_record->offBmiSrc);
      const BYTE* bits = record_start + sdib_record->offBitsSrc;
      bool play_normally = true;
      res = false;
      HDC hdc = context_->hdc;
      scoped_ptr<SkBitmap> bitmap;
      if (bmih->biCompression == BI_JPEG) {
        if (!DIBFormatNativelySupported(hdc, CHECKJPEGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          base::TimeTicks start_time = base::TimeTicks::Now();
          bitmap.reset(gfx::JPEGCodec::Decode(bits, bmih->biSizeImage));
          UMA_HISTOGRAM_TIMES("Printing.JPEGDecompressTime",
                              base::TimeTicks::Now() - start_time);
        }
      } else if (bmih->biCompression == BI_PNG) {
        if (!DIBFormatNativelySupported(hdc, CHECKPNGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          bitmap.reset(new SkBitmap());
          base::TimeTicks start_time = base::TimeTicks::Now();
          gfx::PNGCodec::Decode(bits, bmih->biSizeImage, bitmap.get());
          UMA_HISTOGRAM_TIMES("Printing.PNGDecompressTime",
                              base::TimeTicks::Now() - start_time);
        }
      }
      if (!play_normally) {
        DCHECK(bitmap.get());
        if (bitmap.get()) {
          SkAutoLockPixels lock(*bitmap.get());
          DCHECK_EQ(bitmap->getConfig(), SkBitmap::kARGB_8888_Config);
          const uint32_t* pixels =
              static_cast<const uint32_t*>(bitmap->getPixels());
          if (pixels == NULL) {
            NOTREACHED();
            return false;
          }
          BITMAPINFOHEADER bmi = {0};
          gfx::CreateBitmapHeader(bitmap->width(), bitmap->height(), &bmi);
          res = (0 != StretchDIBits(hdc, sdib_record->xDest, sdib_record->yDest,
                                    sdib_record->cxDest,
                                    sdib_record->cyDest, sdib_record->xSrc,
                                    sdib_record->ySrc,
                                    sdib_record->cxSrc, sdib_record->cySrc,
                                    pixels,
                                    reinterpret_cast<const BITMAPINFO *>(&bmi),
                                    sdib_record->iUsageSrc,
                                    sdib_record->dwRop));
        }
      } else {
        res = Play();
      }
      break;
    }
    case EMR_SETWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize, sizeof(DWORD) * 2 + sizeof(XFORM));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      HDC hdc = context_->hdc;
      if (base_matrix) {
        res = 0 != SetWorldTransform(hdc, base_matrix) &&
                   ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
      } else {
        res = 0 != SetWorldTransform(hdc, xform);
      }
      break;
    }
    case EMR_MODIFYWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize,
                sizeof(DWORD) * 2 + sizeof(XFORM) + sizeof(DWORD));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      const DWORD* option = reinterpret_cast<const DWORD*>(xform + 1);
      HDC hdc = context_->hdc;
      switch (*option) {
        case MWT_IDENTITY:
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix);
          } else {
            res = 0 != ModifyWorldTransform(hdc, xform, MWT_IDENTITY);
          }
          break;
        case MWT_LEFTMULTIPLY:
        case MWT_RIGHTMULTIPLY:
          res = 0 != ModifyWorldTransform(hdc, xform, *option);
          break;
        case 4:  // MWT_SET
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix) &&
                       ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
          } else {
            res = 0 != SetWorldTransform(hdc, xform);
          }
          break;
        default:
          res = false;
          break;
      }
      break;
    }
    case EMR_SETLAYOUT:
      // Ignore it.
      res = true;
      break;
    case EMR_GDICOMMENT: {
      const EMRGDICOMMENT* comment_record =
          reinterpret_cast<const EMRGDICOMMENT*>(record());
      if (comment_record->cbData == sizeof(PageBreakRecord)) {
        const PageBreakRecord* page_break_record =
            reinterpret_cast<const PageBreakRecord*>(comment_record->Data);
        if (page_break_record && page_break_record->IsValid()) {
          if (page_break_record->type == PageBreakRecord::START_PAGE) {
            res = !!::StartPage(context_->hdc);
          } else if (page_break_record->type == PageBreakRecord::END_PAGE) {
            res = !!::EndPage(context_->hdc);
          } else {
            res = false;
            NOTREACHED();
          }
        } else {
          res = Play();
        }
      } else {
        res = true;
      }
      break;
    }
    default: {
      res = Play();
      break;
    }
  }
  return res;
}

bool Emf::StartPage() {
  DCHECK(hdc_);
  if (!hdc_)
    return false;
  PageBreakRecord record(PageBreakRecord::START_PAGE);
  return !!GdiComment(hdc_, sizeof(record),
                      reinterpret_cast<const BYTE *>(&record));
}

bool Emf::EndPage() {
  DCHECK(hdc_);
  if (!hdc_)
    return false;
  PageBreakRecord record(PageBreakRecord::END_PAGE);
  return !!GdiComment(hdc_, sizeof(record),
                      reinterpret_cast<const BYTE *>(&record));
}


Emf::Enumerator::Enumerator(const Emf& emf, HDC context, const RECT* rect) {
  context_.handle_table = NULL;
  context_.objects_count = 0;
  context_.hdc = NULL;
  items_.clear();
  if (!EnumEnhMetaFile(context,
                       emf.emf(),
                       &Emf::Enumerator::EnhMetaFileProc,
                       reinterpret_cast<void*>(this),
                       rect)) {
    NOTREACHED();
    items_.clear();
  }
  DCHECK_EQ(context_.hdc, context);
}

Emf::Enumerator::const_iterator Emf::Enumerator::begin() const {
  return items_.begin();
}

Emf::Enumerator::const_iterator Emf::Enumerator::end() const {
  return items_.end();
}

int CALLBACK Emf::Enumerator::EnhMetaFileProc(HDC hdc,
                                              HANDLETABLE* handle_table,
                                              const ENHMETARECORD* record,
                                              int objects_count,
                                              LPARAM param) {
  Enumerator& emf = *reinterpret_cast<Enumerator*>(param);
  if (!emf.context_.handle_table) {
    DCHECK(!emf.context_.handle_table);
    DCHECK(!emf.context_.objects_count);
    emf.context_.handle_table = handle_table;
    emf.context_.objects_count = objects_count;
    emf.context_.hdc = hdc;
  } else {
    DCHECK_EQ(emf.context_.handle_table, handle_table);
    DCHECK_EQ(emf.context_.objects_count, objects_count);
    DCHECK_EQ(emf.context_.hdc, hdc);
  }
  emf.items_.push_back(Record(&emf.context_, record));
  return 1;
}

}  // namespace printing
