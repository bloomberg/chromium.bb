// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ERROR_H_
#define EXTENSIONS_BROWSER_EXTENSION_ERROR_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "extensions/common/stack_frame.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionError {
 public:
  enum Type {
    MANIFEST_ERROR = 0,
    RUNTIME_ERROR,
    INTERNAL_ERROR,
    NUM_ERROR_TYPES,  // Put new values above this.
  };

  virtual ~ExtensionError();

  virtual std::string GetDebugString() const;

  // Return true if this error and |rhs| are considered equal, and should be
  // grouped together.
  bool IsEqual(const ExtensionError* rhs) const;

  Type type() const { return type_; }
  const std::string& extension_id() const { return extension_id_; }
  int id() const { return id_; }
  void set_id(int id) { id_ = id; }
  bool from_incognito() const { return from_incognito_; }
  logging::LogSeverity level() const { return level_; }
  const base::string16& source() const { return source_; }
  const base::string16& message() const { return message_; }
  size_t occurrences() const { return occurrences_; }
  void set_occurrences(size_t occurrences) { occurrences_ = occurrences; }

 protected:
  ExtensionError(Type type,
                 const std::string& extension_id,
                 bool from_incognito,
                 logging::LogSeverity level,
                 const base::string16& source,
                 const base::string16& message);

  virtual bool IsEqualImpl(const ExtensionError* rhs) const = 0;

  // Which type of error this is.
  Type type_;
  // The ID of the extension which caused the error.
  std::string extension_id_;
  // The id of this particular error. This can be zero if the id is never set.
  int id_;
  // Whether or not the error was caused while incognito.
  bool from_incognito_;
  // The severity level of the error.
  logging::LogSeverity level_;
  // The source for the error; this can be a script, web page, or manifest file.
  // This is stored as a string (rather than a url) since it can be a Chrome
  // script file (e.g., event_bindings.js).
  base::string16 source_;
  // The error message itself.
  base::string16 message_;
  // The number of times this error has occurred.
  size_t occurrences_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionError);
};

class ManifestError : public ExtensionError {
 public:
  ManifestError(const std::string& extension_id,
                const base::string16& message,
                const base::string16& manifest_key,
                const base::string16& manifest_specific);
  ~ManifestError() override;

  std::string GetDebugString() const override;

  const base::string16& manifest_key() const { return manifest_key_; }
  const base::string16& manifest_specific() const { return manifest_specific_; }

 private:
  bool IsEqualImpl(const ExtensionError* rhs) const override;

  // If present, this indicates the feature in the manifest which caused the
  // error.
  base::string16 manifest_key_;
  // If present, this is a more-specific location of the error - for instance,
  // a specific permission which is incorrect, rather than simply "permissions".
  base::string16 manifest_specific_;

  DISALLOW_COPY_AND_ASSIGN(ManifestError);
};

class RuntimeError : public ExtensionError {
 public:
  RuntimeError(const std::string& extension_id,  // optional, sometimes unknown.
               bool from_incognito,
               const base::string16& source,
               const base::string16& message,
               const StackTrace& stack_trace,
               const GURL& context_url,
               logging::LogSeverity level,
               int render_view_id,
               int render_process_id);
  ~RuntimeError() override;

  std::string GetDebugString() const override;

  const GURL& context_url() const { return context_url_; }
  const StackTrace& stack_trace() const { return stack_trace_; }
  int render_frame_id() const { return render_frame_id_; }
  int render_process_id() const { return render_process_id_; }

 private:
  bool IsEqualImpl(const ExtensionError* rhs) const override;

  // Since we piggy-back onto other error reporting systems (like V8 and
  // WebKit), the reported information may need to be cleaned up in order to be
  // in a consistent format.
  void CleanUpInit();

  GURL context_url_;
  StackTrace stack_trace_;

  // Keep track of the render process which caused the error in order to
  // inspect the frame later, if possible.
  int render_frame_id_;
  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeError);
};

class InternalError : public ExtensionError {
 public:
  InternalError(const std::string& extension_id,
                const base::string16& message,
                logging::LogSeverity level);
  ~InternalError() override;

  std::string GetDebugString() const override;

 private:
  bool IsEqualImpl(const ExtensionError* rhs) const override;

  DISALLOW_COPY_AND_ASSIGN(InternalError);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ERROR_H_
