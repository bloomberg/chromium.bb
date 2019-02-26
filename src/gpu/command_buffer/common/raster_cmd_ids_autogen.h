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
  OP(DeleteTexturesImmediate)                    /* 256 */ \
  OP(Finish)                                     /* 257 */ \
  OP(Flush)                                      /* 258 */ \
  OP(GetError)                                   /* 259 */ \
  OP(GenQueriesEXTImmediate)                     /* 260 */ \
  OP(DeleteQueriesEXTImmediate)                  /* 261 */ \
  OP(BeginQueryEXT)                              /* 262 */ \
  OP(EndQueryEXT)                                /* 263 */ \
  OP(LoseContextCHROMIUM)                        /* 264 */ \
  OP(InsertFenceSyncCHROMIUM)                    /* 265 */ \
  OP(WaitSyncTokenCHROMIUM)                      /* 266 */ \
  OP(BeginRasterCHROMIUMImmediate)               /* 267 */ \
  OP(RasterCHROMIUM)                             /* 268 */ \
  OP(EndRasterCHROMIUM)                          /* 269 */ \
  OP(CreateTransferCacheEntryINTERNAL)           /* 270 */ \
  OP(DeleteTransferCacheEntryINTERNAL)           /* 271 */ \
  OP(UnlockTransferCacheEntryINTERNAL)           /* 272 */ \
  OP(DeletePaintCacheTextBlobsINTERNALImmediate) /* 273 */ \
  OP(DeletePaintCachePathsINTERNALImmediate)     /* 274 */ \
  OP(ClearPaintCacheINTERNAL)                    /* 275 */ \
  OP(CreateAndConsumeTextureINTERNALImmediate)   /* 276 */ \
  OP(CopySubTexture)                             /* 277 */ \
  OP(TraceBeginCHROMIUM)                         /* 278 */ \
  OP(TraceEndCHROMIUM)                           /* 279 */ \
  OP(SetActiveURLCHROMIUM)                       /* 280 */

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
