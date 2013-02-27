// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

// Important note:
// The Android clipboard system only supports text format. So we use the
// Android system when some text is added or retrieved from the system. For
// anything else, we currently store the value in some process wide static
// variable protected by a lock. So the (non-text) clipboard will only work
// within the same process.

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::GetClass;
using base::android::MethodID;
using base::android::ScopedJavaLocalRef;

namespace ui {

namespace {
// Various formats we support.
const char kPlainTextFormat[] = "text";
const char kHTMLFormat[] = "html";
const char kRTFFormat[] = "rtf";
const char kBitmapFormat[] = "bitmap";
const char kWebKitSmartPasteFormat[] = "webkit_smart";
const char kBookmarkFormat[] = "bookmark";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";
const char kMimeTypeWebCustomData[] = "chromium/x-web-custom-data";
const char kSourceTagFormat[] = "source_tag";

class ClipboardMap {
 public:
  ClipboardMap();
  std::string Get(const std::string& format);
  bool HasFormat(const std::string& format);
  void Set(const std::string& format, const std::string& data);
  void Clear();

 private:
  void SyncWithAndroidClipboard();
  std::map<std::string, std::string> map_;
  base::Lock lock_;

  // Java class and methods for the Android ClipboardManager.
  base::android::ScopedJavaGlobalRef<jobject> clipboard_manager_;
  jmethodID set_text_;
  jmethodID get_text_;
  jmethodID has_text_;
  jmethodID to_string_;
};
base::LazyInstance<ClipboardMap>::Leaky g_map = LAZY_INSTANCE_INITIALIZER;

ClipboardMap::ClipboardMap() {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Get the context.
  jobject context = base::android::GetApplicationContext();
  DCHECK(context);

  // Get the context class.
  ScopedJavaLocalRef<jclass> context_class =
      GetClass(env, "android/content/Context");

  // Get the system service method.
  jmethodID get_system_service = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, context_class.obj(), "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;");

  // Retrieve the system service.
  ScopedJavaLocalRef<jstring> service_name(env, env->NewStringUTF("clipboard"));
  clipboard_manager_.Reset(env, env->CallObjectMethod(context,
      get_system_service, service_name.obj()));
  DCHECK(clipboard_manager_.obj() && !ClearException(env));

  // Retain a few methods we'll keep using.
  ScopedJavaLocalRef<jclass> clipboard_class =
      GetClass(env, "android/text/ClipboardManager");
  set_text_ = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, clipboard_class.obj(), "setText", "(Ljava/lang/CharSequence;)V");
  get_text_ = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, clipboard_class.obj(), "getText", "()Ljava/lang/CharSequence;");
  has_text_ = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, clipboard_class.obj(), "hasText", "()Z");

  // Will need to call toString as CharSequence is not always a String.
  ScopedJavaLocalRef<jclass> charsequence_class =
      GetClass(env, "java/lang/CharSequence");
  to_string_ = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, charsequence_class.obj(), "toString", "()Ljava/lang/String;");
}

std::string ClipboardMap::Get(const std::string& format) {
  base::AutoLock lock(lock_);
  SyncWithAndroidClipboard();
  std::map<std::string, std::string>::const_iterator it = map_.find(format);
  return it == map_.end() ? std::string() : it->second;
}

bool ClipboardMap::HasFormat(const std::string& format) {
  base::AutoLock lock(lock_);
  SyncWithAndroidClipboard();
  return ContainsKey(map_, format);
}

void ClipboardMap::Set(const std::string& format, const std::string& data) {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  SyncWithAndroidClipboard();

  map_[format] = data;
  if (format == kPlainTextFormat) {
    ScopedJavaLocalRef<jstring> str(
        env, env->NewStringUTF(data.c_str()));
    DCHECK(str.obj() && !ClearException(env));
    env->CallVoidMethod(clipboard_manager_.obj(), set_text_, str.obj());
  }
}

void ClipboardMap::Clear() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  map_.clear();
  env->CallVoidMethod(clipboard_manager_.obj(), set_text_, NULL);
}

// If the text in the internal map does not match that in the Android clipboard,
// clear the map and insert the Android text into it.
void ClipboardMap::SyncWithAndroidClipboard() {
  lock_.AssertAcquired();
  JNIEnv* env = AttachCurrentThread();

  std::map<std::string, std::string>::const_iterator it =
    map_.find(kPlainTextFormat);

  if (!env->CallBooleanMethod(clipboard_manager_.obj(), has_text_)) {
    if (it != map_.end())
      map_.clear();
    return;
  }

  ScopedJavaLocalRef<jobject> char_seq_text(
      env, env->CallObjectMethod(clipboard_manager_.obj(), get_text_));
  ScopedJavaLocalRef<jstring> tmp_string(env,
      static_cast<jstring>(env->CallObjectMethod(char_seq_text.obj(),
                                                 to_string_)));
  std::string android_string = ConvertJavaStringToUTF8(tmp_string);

  if (it == map_.end() || it->second != android_string) {
    map_.clear();
    map_[kPlainTextFormat] = android_string;
  }
}

}  // namespace

Clipboard::FormatType::FormatType() {
}

Clipboard::FormatType::FormatType(const std::string& native_format)
    : data_(native_format) {
}

Clipboard::FormatType::~FormatType() {
}

std::string Clipboard::FormatType::Serialize() const {
  return data_;
}

// static
Clipboard::FormatType Clipboard::FormatType::Deserialize(
    const std::string& serialization) {
  return FormatType(serialization);
}

bool Clipboard::FormatType::Equals(const FormatType& other) const {
  return data_ == other.data_;
}

Clipboard::Clipboard() {
}

Clipboard::~Clipboard() {
}

// Main entry point used to write several values in the clipboard.
void Clipboard::WriteObjectsImpl(Buffer buffer,
                                 const ObjectMap& objects,
                                 SourceTag tag) {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  g_map.Get().Clear();
  for (ObjectMap::const_iterator iter = objects.begin();
       iter != objects.end(); ++iter) {
    DispatchObject(static_cast<ObjectType>(iter->first), iter->second);
  }
  WriteSourceTag(tag);
}

uint64 Clipboard::GetSequenceNumber(Clipboard::Buffer /* buffer */) {
  // TODO: implement this. For now this interface will advertise
  // that the clipboard never changes. That's fine as long as we
  // don't rely on this signal.
  return 0;
}

bool Clipboard::IsFormatAvailable(const Clipboard::FormatType& format,
                                  Clipboard::Buffer buffer) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  return g_map.Get().HasFormat(format.data());
}

void Clipboard::Clear(Buffer buffer) {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  g_map.Get().Clear();
}

void Clipboard::ReadAvailableTypes(Buffer buffer, std::vector<string16>* types,
                                   bool* contains_filenames) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);

  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  NOTIMPLEMENTED();

  types->clear();
  *contains_filenames = false;
}

void Clipboard::ReadText(Clipboard::Buffer buffer, string16* result) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  std::string utf8;
  ReadAsciiText(buffer, &utf8);
  *result = UTF8ToUTF16(utf8);
}

void Clipboard::ReadAsciiText(Clipboard::Buffer buffer,
                              std::string* result) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  ReportAction(buffer, READ_TEXT);
  *result = g_map.Get().Get(kPlainTextFormat);
}

// Note: |src_url| isn't really used. It is only implemented in Windows
void Clipboard::ReadHTML(Clipboard::Buffer buffer,
                         string16* markup,
                         std::string* src_url,
                         uint32* fragment_start,
                         uint32* fragment_end) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  if (src_url)
    src_url->clear();

  std::string input = g_map.Get().Get(kHTMLFormat);
  *markup = UTF8ToUTF16(input);

  *fragment_start = 0;
  *fragment_end = static_cast<uint32>(markup->length());
}

void Clipboard::ReadRTF(Buffer buffer, std::string* result) const {
  NOTIMPLEMENTED();
}

SkBitmap Clipboard::ReadImage(Buffer buffer) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  std::string input = g_map.Get().Get(kBitmapFormat);

  SkBitmap bmp;
  if (!input.empty()) {
    DCHECK_LE(sizeof(gfx::Size), input.size());
    const gfx::Size* size = reinterpret_cast<const gfx::Size*>(input.data());

    bmp.setConfig(
        SkBitmap::kARGB_8888_Config, size->width(), size->height(), 0);
    bmp.allocPixels();

    int bm_size = size->width() * size->height() * 4;
    DCHECK_EQ(sizeof(gfx::Size) + bm_size, input.size());

    memcpy(bmp.getPixels(), input.data() + sizeof(gfx::Size), bm_size);
  }
  return bmp;
}

void Clipboard::ReadCustomData(Buffer buffer,
                               const string16& type,
                               string16* result) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadBookmark(string16* title, std::string* url) const {
  NOTIMPLEMENTED();
}

void Clipboard::ReadData(const Clipboard::FormatType& format,
                         std::string* result) const {
  *result = g_map.Get().Get(format.data());
}

Clipboard::SourceTag Clipboard::ReadSourceTag(Buffer buffer) const {
  DCHECK_EQ(buffer, BUFFER_STANDARD);
  std::string result;
  ReadData(GetSourceTagFormatType(), &result);
  return Binary2SourceTag(result);
}

// static
Clipboard::FormatType Clipboard::GetFormatType(
    const std::string& format_string) {
  return FormatType::Deserialize(format_string);
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPlainTextFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPlainTextWFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kPlainTextFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebKitSmartPasteFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kWebKitSmartPasteFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetHtmlFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kHTMLFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetRtfFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kRTFFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetBitmapFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kBitmapFormat));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetWebCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypeWebCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetPepperCustomDataFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kMimeTypePepperCustomData));
  return type;
}

// static
const Clipboard::FormatType& Clipboard::GetSourceTagFormatType() {
  CR_DEFINE_STATIC_LOCAL(FormatType, type, (kSourceTagFormat));
  return type;
}

void Clipboard::WriteText(const char* text_data, size_t text_len) {
  g_map.Get().Set(kPlainTextFormat, std::string(text_data, text_len));
}

void Clipboard::WriteHTML(const char* markup_data,
                          size_t markup_len,
                          const char* url_data,
                          size_t url_len) {
  g_map.Get().Set(kHTMLFormat, std::string(markup_data, markup_len));
}

void Clipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  NOTIMPLEMENTED();
}

// Note: according to other platforms implementations, this really writes the
// URL spec.
void Clipboard::WriteBookmark(const char* title_data, size_t title_len,
                              const char* url_data, size_t url_len) {
  g_map.Get().Set(kBookmarkFormat, std::string(url_data, url_len));
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void Clipboard::WriteWebSmartPaste() {
  g_map.Get().Set(kWebKitSmartPasteFormat, std::string());
}

// All platforms use gfx::Size for size data but it is passed as a const char*
// Further, pixel_data is expected to be 32 bits per pixel
// Note: we implement this to pass all unit tests but it is currently unclear
// how some code would consume this.
void Clipboard::WriteBitmap(const char* pixel_data, const char* size_data) {
  const gfx::Size* size = reinterpret_cast<const gfx::Size*>(size_data);
  int bm_size = size->width() * size->height() * 4;

  std::string packed(size_data, sizeof(gfx::Size));
  packed += std::string(pixel_data, bm_size);
  g_map.Get().Set(kBitmapFormat, packed);
}

void Clipboard::WriteData(const Clipboard::FormatType& format,
                          const char* data_data, size_t data_len) {
  g_map.Get().Set(format.data(), std::string(data_data, data_len));
}

void Clipboard::WriteSourceTag(SourceTag tag) {
  if (tag != SourceTag()) {
    ObjectMapParam binary = SourceTag2Binary(tag);
    WriteData(GetSourceTagFormatType(), &binary[0], binary.size());
  }
}

} // namespace ui
