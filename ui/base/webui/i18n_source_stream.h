// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WEBUI_I18N_SOURCE_STREAM_H_
#define UI_BASE_WEBUI_I18N_SOURCE_STREAM_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/filter/filter_source_stream.h"
#include "ui/base/template_expressions.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT I18nSourceStream : public net::FilterSourceStream {
 public:
  ~I18nSourceStream() override;

  // Factory function to create an I18nSourceStream.
  // |replacements| is a dictionary of i18n replacements.
  static std::unique_ptr<I18nSourceStream> Create(
      std::unique_ptr<SourceStream> previous,
      SourceStream::SourceType type,
      const ui::TemplateReplacements* replacements);

 private:
  I18nSourceStream(std::unique_ptr<SourceStream> previous,
                   SourceStream::SourceType type,
                   const TemplateReplacements* replacements);

  // SourceStream implementation.
  std::string GetTypeAsString() const override;
  int FilterData(net::IOBuffer* output_buffer,
                 int output_buffer_size,
                 net::IOBuffer* input_buffer,
                 int input_buffer_size,
                 int* consumed_bytes,
                 bool upstream_end_reached) override;

  // Keep split $i18n tags (wait for the whole tag). This is expected to vary
  // in size from 0 to a few KB and should never be larger than the input file
  // (in the worst case).
  std::string input_;

  // Keep excess that didn't fit in the output buffer. This is expected to vary
  // in size from 0 to a few KB and should never get much larger than the input
  // file (in the worst case).
  std::string output_;

  // A map of i18n replacement keys and translations.
  const TemplateReplacements* replacements_;

  DISALLOW_COPY_AND_ASSIGN(I18nSourceStream);
};

}  // namespace ui

#endif  // UI_BASE_WEBUI_I18N_SOURCE_STREAM_H_
