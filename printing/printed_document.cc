// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/page_number.h"
#include "printing/print_settings_conversion.h"
#include "printing/printed_page.h"
#include "printing/units.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_elider.h"

namespace printing {

namespace {

base::LazyInstance<base::FilePath>::Leaky g_debug_dump_info =
    LAZY_INSTANCE_INITIALIZER;

void DebugDumpPageTask(const base::string16& doc_name,
                       const PrintedPage* page) {
  base::AssertBlockingAllowed();

  if (g_debug_dump_info.Get().empty())
    return;

  base::string16 filename = doc_name;
  filename +=
      base::ASCIIToUTF16(base::StringPrintf("_%04d", page->page_number()));
  base::FilePath file_path =
#if defined(OS_WIN)
      PrintedDocument::CreateDebugDumpPath(filename, FILE_PATH_LITERAL(".emf"));
#else   // OS_WIN
      PrintedDocument::CreateDebugDumpPath(filename, FILE_PATH_LITERAL(".pdf"));
#endif  // OS_WIN
  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  page->metafile()->SaveTo(&file);
}

void DebugDumpDataTask(const base::string16& doc_name,
                       const base::FilePath::StringType& extension,
                       const base::RefCountedMemory* data) {
  base::AssertBlockingAllowed();

  base::FilePath path =
      PrintedDocument::CreateDebugDumpPath(doc_name, extension);
  if (path.empty())
    return;
  base::WriteFile(path,
                  reinterpret_cast<const char*>(data->front()),
                  base::checked_cast<int>(data->size()));
}

void DebugDumpSettings(const base::string16& doc_name,
                       const PrintSettings& settings) {
  base::DictionaryValue job_settings;
  PrintSettingsToJobSettingsDebug(settings, &job_settings);
  std::string settings_str;
  base::JSONWriter::WriteWithOptions(
      job_settings, base::JSONWriter::OPTIONS_PRETTY_PRINT, &settings_str);
  scoped_refptr<base::RefCountedMemory> data =
      base::RefCountedString::TakeString(&settings_str);
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&DebugDumpDataTask, doc_name, FILE_PATH_LITERAL(".json"),
                     base::RetainedRef(data)));
}

}  // namespace

PrintedDocument::PrintedDocument(const PrintSettings& settings,
                                 const base::string16& name,
                                 int cookie)
    : immutable_(settings, name, cookie) {
  // Records the expected page count if a range is setup.
  if (!settings.ranges().empty()) {
    // If there is a range, set the number of page
    for (unsigned i = 0; i < settings.ranges().size(); ++i) {
      const PageRange& range = settings.ranges()[i];
      mutable_.expected_page_count_ += range.to - range.from + 1;
    }
  }

  if (!g_debug_dump_info.Get().empty())
    DebugDumpSettings(name, settings);
}

PrintedDocument::~PrintedDocument() {
}

void PrintedDocument::SetPage(int page_number,
                              std::unique_ptr<MetafilePlayer> metafile,
#if defined(OS_WIN)
                              float shrink,
#endif  // OS_WIN
                              const gfx::Size& paper_size,
                              const gfx::Rect& page_rect) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  scoped_refptr<PrintedPage> page(new PrintedPage(
      page_number + 1, std::move(metafile), paper_size, page_rect));
#if defined(OS_WIN)
  page->set_shrink_factor(shrink);
#endif  // OS_WIN
  {
    base::AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
    if (page_number < mutable_.first_page)
      mutable_.first_page = page_number;
#endif
  }

  if (!g_debug_dump_info.Get().empty()) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
        base::BindOnce(&DebugDumpPageTask, name(), base::RetainedRef(page)));
  }
}

scoped_refptr<PrintedPage> PrintedDocument::GetPage(int page_number) {
  scoped_refptr<PrintedPage> page;
  {
    base::AutoLock lock(lock_);
    PrintedPages::const_iterator itr = mutable_.pages_.find(page_number);
    if (itr != mutable_.pages_.end())
      page = itr->second;
  }
  return page;
}

bool PrintedDocument::IsComplete() const {
  base::AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
  PageNumber page(immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;

  for (; page != PageNumber::npos(); ++page) {
#if defined(OS_WIN) || defined(OS_MACOSX)
    const bool metafile_must_be_valid = true;
#elif defined(OS_POSIX)
    const bool metafile_must_be_valid = (page.ToInt() == mutable_.first_page);
#endif
    PrintedPages::const_iterator itr = mutable_.pages_.find(page.ToInt());
    if (itr == mutable_.pages_.end() || !itr->second.get())
      return false;
    if (metafile_must_be_valid && !itr->second->metafile())
      return false;
  }
  return true;
}

void PrintedDocument::set_page_count(int max_page) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_.ranges().empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

void PrintedDocument::set_debug_dump_path(
    const base::FilePath& debug_dump_path) {
  g_debug_dump_info.Get() = debug_dump_path;
}

base::FilePath PrintedDocument::CreateDebugDumpPath(
    const base::string16& document_name,
    const base::FilePath::StringType& extension) {
  if (g_debug_dump_info.Get().empty())
    return base::FilePath();
  // Create a filename.
  base::string16 filename;
  base::Time now(base::Time::Now());
  filename = base::TimeFormatShortDateAndTime(now);
  filename += base::ASCIIToUTF16("_");
  filename += document_name;
  base::FilePath::StringType system_filename;
#if defined(OS_WIN)
  system_filename = filename;
#else   // OS_WIN
  system_filename = base::UTF16ToUTF8(filename);
#endif  // OS_WIN
  base::i18n::ReplaceIllegalCharactersInPath(&system_filename, '_');
  return g_debug_dump_info.Get().Append(system_filename).AddExtension(
      extension);
}

void PrintedDocument::DebugDumpData(
    const base::RefCountedMemory* data,
    const base::FilePath::StringType& extension) {
  if (g_debug_dump_info.Get().empty())
    return;
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::BACKGROUND, base::MayBlock()},
                           base::BindOnce(&DebugDumpDataTask, name(), extension,
                                          base::RetainedRef(data)));
}

PrintedDocument::Mutable::Mutable() : expected_page_count_(0), page_count_(0) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  first_page = INT_MAX;
#endif
}

PrintedDocument::Mutable::~Mutable() {
}

PrintedDocument::Immutable::Immutable(const PrintSettings& settings,
                                      const base::string16& name,
                                      int cookie)
    : settings_(settings), name_(name), cookie_(cookie) {}

PrintedDocument::Immutable::~Immutable() {}

#if defined(OS_ANDROID)
// This function is not used on android.
void PrintedDocument::RenderPrintedPage(const PrintedPage& page,
                                        PrintingContext* context) const {
  NOTREACHED();
}
#endif

}  // namespace printing
