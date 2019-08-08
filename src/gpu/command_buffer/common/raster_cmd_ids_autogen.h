// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_

#define RASTER_COMMAND_LIST(OP)                            \
  OP(Finish)                                     /* 256 */ \
  OP(Flush)                                      /* 257 */ \
  OP(GetError)                                   /* 258 */ \
  OP(GenQueriesEXTImmediate)                     /* 259 */ \
  OP(DeleteQueriesEXTImmediate)                  /* 260 */ \
  OP(BeginQueryEXT)                              /* 261 */ \
  OP(EndQueryEXT)                                /* 262 */ \
  OP(LoseContextCHROMIUM)                        /* 263 */ \
  OP(BeginRasterCHROMIUMImmediate)               /* 264 */ \
  OP(RasterCHROMIUM)                             /* 265 */ \
  OP(EndRasterCHROMIUM)                          /* 266 */ \
  OP(CreateTransferCacheEntryINTERNAL)           /* 267 */ \
  OP(DeleteTransferCacheEntryINTERNAL)           /* 268 */ \
  OP(UnlockTransferCacheEntryINTERNAL)           /* 269 */ \
  OP(DeletePaintCacheTextBlobsINTERNALImmediate) /* 270 */ \
  OP(DeletePaintCachePathsINTERNALImmediate)     /* 271 */ \
  OP(ClearPaintCacheINTERNAL)                    /* 272 */ \
  OP(CopySubTextureINTERNALImmediate)            /* 273 */ \
  OP(TraceBeginCHROMIUM)                         /* 274 */ \
  OP(TraceEndCHROMIUM)                           /* 275 */ \
  OP(SetActiveURLCHROMIUM)                       /* 276 */

enum CommandId {
  kOneBeforeStartPoint =
      cmd::kLastCommonId,  // All Raster commands start after this.
#define RASTER_CMD_OP(name) k##name,
  RASTER_COMMAND_LIST(RASTER_CMD_OP)
#undef RASTER_CMD_OP
      kNumCommands,
  kFirstRasterCommand = kOneBeforeStartPoint + 1
};

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_IDS_AUTOGEN_H_
