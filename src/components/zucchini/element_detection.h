// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ELEMENT_DETECTION_H_
#define COMPONENTS_ZUCCHINI_ELEMENT_DETECTION_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

class Disassembler;

// Attempts to detect an executable located at start of |image|. If found,
// returns the corresponding disassembler. Otherwise returns null.
std::unique_ptr<Disassembler> MakeDisassemblerWithoutFallback(
    ConstBufferView image);

// Attempts to create a disassembler corresponding to |exe_type| and initialize
// it with |image|, On failure, returns null.
std::unique_ptr<Disassembler> MakeDisassemblerOfType(ConstBufferView image,
                                                     ExecutableType exe_type);

// Attempts to detect an element associated with |image| and returns it, or
// returns nullopt if no element is detected.
using ElementDetector =
    base::RepeatingCallback<base::Optional<Element>(ConstBufferView image)>;

// Implementation of ElementDetector using disassemblers.
base::Optional<Element> DetectElementFromDisassembler(ConstBufferView image);

// A class to scan through an image and iteratively detect elements.
class ElementFinder {
 public:
  ElementFinder(ConstBufferView image, ElementDetector&& detector);
  ~ElementFinder();

  // Scans for the next executable using |detector|. Returns the next element
  // found, or nullopt if no more element can be found.
  base::Optional<Element> GetNext();

 private:
  ConstBufferView image_;
  ElementDetector detector_;
  offset_t pos_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ElementFinder);
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ELEMENT_DETECTION_H_
