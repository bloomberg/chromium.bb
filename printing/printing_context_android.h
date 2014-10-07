// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_ANDROID_H_
#define PRINTING_PRINTING_CONTEXT_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "printing/printing_context.h"

namespace printing {

// Android subclass of PrintingContext.  The implementation for this header file
// resides in Chrome for Android repository.  This class communicates with the
// Java side through JNI.
class PRINTING_EXPORT PrintingContextAndroid : public PrintingContext {
 public:
  explicit PrintingContextAndroid(Delegate* delegate);
  virtual ~PrintingContextAndroid();

  // Called when the page is successfully written to a PDF using the file
  // descriptor specified, or when the printing operation failed.
  static void PdfWritingDone(int fd, bool success);

  // Called from Java, when printing settings from the user are ready or the
  // printing operation is canceled.
  void AskUserForSettingsReply(JNIEnv* env, jobject obj, jboolean success);

  // PrintingContext implementation.
  virtual void AskUserForSettings(
      int max_pages,
      bool has_selection,
      const PrintSettingsCallback& callback) override;
  virtual Result UseDefaultSettings() override;
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() override;
  virtual Result UpdatePrinterSettings(bool external_preview,
                                       bool show_system_dialog) override;
  virtual Result InitWithSettings(const PrintSettings& settings) override;
  virtual Result NewDocument(const base::string16& document_name) override;
  virtual Result NewPage() override;
  virtual Result PageDone() override;
  virtual Result DocumentDone() override;
  virtual void Cancel() override;
  virtual void ReleaseContext() override;
  virtual gfx::NativeDrawingContext context() const override;

  // Registers JNI bindings for RegisterContext.
  static bool RegisterPrintingContext(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_printing_context_;

  // The callback from AskUserForSettings to be called when the settings are
  // ready on the Java side
  PrintSettingsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextAndroid);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_ANDROID_H_

