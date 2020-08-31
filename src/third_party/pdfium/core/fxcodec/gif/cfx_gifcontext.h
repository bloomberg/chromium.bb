// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_GIF_CFX_GIFCONTEXT_H_
#define CORE_FXCODEC_GIF_CFX_GIFCONTEXT_H_

#include <memory>
#include <vector>

#include "core/fxcodec/gif/cfx_gif.h"
#include "core/fxcodec/gif/cfx_lzwdecompressor.h"
#include "core/fxcodec/gif/gifmodule.h"
#include "core/fxcrt/unowned_ptr.h"

class CFX_CodecMemory;

namespace fxcodec {

class CFX_GifContext : public ModuleIface::Context {
 public:
  CFX_GifContext(GifModule* gif_module, GifModule::Delegate* delegate);
  ~CFX_GifContext() override;

  void RecordCurrentPosition(uint32_t* cur_pos);
  void ReadScanline(int32_t row_num, uint8_t* row_buf);
  bool GetRecordPosition(uint32_t cur_pos,
                         int32_t left,
                         int32_t top,
                         int32_t width,
                         int32_t height,
                         int32_t pal_num,
                         CFX_GifPalette* pal,
                         int32_t delay_time,
                         bool user_input,
                         int32_t trans_index,
                         int32_t disposal_method,
                         bool interlace);
  CFX_GifDecodeStatus ReadHeader();
  CFX_GifDecodeStatus GetFrame();
  CFX_GifDecodeStatus LoadFrame(int32_t frame_num);
  void SetInputBuffer(RetainPtr<CFX_CodecMemory> codec_memory);
  uint32_t GetAvailInput() const;
  size_t GetFrameNum() const { return images_.size(); }

  UnownedPtr<GifModule> const gif_module_;
  UnownedPtr<GifModule::Delegate> const delegate_;
  std::vector<CFX_GifPalette> global_palette_;
  uint8_t global_pal_exp_ = 0;
  uint32_t img_row_offset_ = 0;
  uint32_t img_row_avail_size_ = 0;
  int32_t decode_status_ = GIF_D_STATUS_SIG;
  std::unique_ptr<CFX_GifGraphicControlExtension> graphic_control_extension_;
  std::vector<std::unique_ptr<CFX_GifImage>> images_;
  std::unique_ptr<CFX_LZWDecompressor> lzw_decompressor_;
  int width_ = 0;
  int height_ = 0;
  uint8_t bc_index_ = 0;
  uint8_t global_sort_flag_ = 0;
  uint8_t global_color_resolution_ = 0;
  uint8_t img_pass_num_ = 0;

 protected:
  bool ReadAllOrNone(uint8_t* dest, uint32_t size);
  CFX_GifDecodeStatus ReadGifSignature();
  CFX_GifDecodeStatus ReadLogicalScreenDescriptor();

  RetainPtr<CFX_CodecMemory> input_buffer_;

 private:
  void SaveDecodingStatus(int32_t status);
  CFX_GifDecodeStatus DecodeExtension();
  CFX_GifDecodeStatus DecodeImageInfo();
  void DecodingFailureAtTailCleanup(CFX_GifImage* gif_image);
  bool ScanForTerminalMarker();
};

}  // namespace fxcodec

#endif  // CORE_FXCODEC_GIF_CFX_GIFCONTEXT_H_
