// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_

#define GL_SCANOUT_CHROMIUM 0x6000

struct DeleteTexturesImmediate {
  typedef DeleteTexturesImmediate ValueType;
  static const CommandId kCmdId = kDeleteTexturesImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _textures) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _textures, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _textures) {
    static_cast<ValueType*>(cmd)->Init(_n, _textures);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteTexturesImmediate) == 8,
              "size of DeleteTexturesImmediate should be 8");
static_assert(offsetof(DeleteTexturesImmediate, header) == 0,
              "offset of DeleteTexturesImmediate header should be 0");
static_assert(offsetof(DeleteTexturesImmediate, n) == 4,
              "offset of DeleteTexturesImmediate n should be 4");

struct Finish {
  typedef Finish ValueType;
  static const CommandId kCmdId = kFinish;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Finish) == 4, "size of Finish should be 4");
static_assert(offsetof(Finish, header) == 0,
              "offset of Finish header should be 0");

struct Flush {
  typedef Flush ValueType;
  static const CommandId kCmdId = kFlush;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Flush) == 4, "size of Flush should be 4");
static_assert(offsetof(Flush, header) == 0,
              "offset of Flush header should be 0");

struct GetError {
  typedef GetError ValueType;
  static const CommandId kCmdId = kGetError;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLenum Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    SetHeader();
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd, uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetError) == 12, "size of GetError should be 12");
static_assert(offsetof(GetError, header) == 0,
              "offset of GetError header should be 0");
static_assert(offsetof(GetError, result_shm_id) == 4,
              "offset of GetError result_shm_id should be 4");
static_assert(offsetof(GetError, result_shm_offset) == 8,
              "offset of GetError result_shm_offset should be 8");

struct GenQueriesEXTImmediate {
  typedef GenQueriesEXTImmediate ValueType;
  static const CommandId kCmdId = kGenQueriesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _queries) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _queries, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _queries) {
    static_cast<ValueType*>(cmd)->Init(_n, _queries);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenQueriesEXTImmediate) == 8,
              "size of GenQueriesEXTImmediate should be 8");
static_assert(offsetof(GenQueriesEXTImmediate, header) == 0,
              "offset of GenQueriesEXTImmediate header should be 0");
static_assert(offsetof(GenQueriesEXTImmediate, n) == 4,
              "offset of GenQueriesEXTImmediate n should be 4");

struct DeleteQueriesEXTImmediate {
  typedef DeleteQueriesEXTImmediate ValueType;
  static const CommandId kCmdId = kDeleteQueriesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _queries) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _queries, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _queries) {
    static_cast<ValueType*>(cmd)->Init(_n, _queries);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteQueriesEXTImmediate) == 8,
              "size of DeleteQueriesEXTImmediate should be 8");
static_assert(offsetof(DeleteQueriesEXTImmediate, header) == 0,
              "offset of DeleteQueriesEXTImmediate header should be 0");
static_assert(offsetof(DeleteQueriesEXTImmediate, n) == 4,
              "offset of DeleteQueriesEXTImmediate n should be 4");

struct BeginQueryEXT {
  typedef BeginQueryEXT ValueType;
  static const CommandId kCmdId = kBeginQueryEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLuint _id,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    SetHeader();
    target = _target;
    id = _id;
    sync_data_shm_id = _sync_data_shm_id;
    sync_data_shm_offset = _sync_data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLuint _id,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _id, _sync_data_shm_id,
                                       _sync_data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t id;
  uint32_t sync_data_shm_id;
  uint32_t sync_data_shm_offset;
};

static_assert(sizeof(BeginQueryEXT) == 20,
              "size of BeginQueryEXT should be 20");
static_assert(offsetof(BeginQueryEXT, header) == 0,
              "offset of BeginQueryEXT header should be 0");
static_assert(offsetof(BeginQueryEXT, target) == 4,
              "offset of BeginQueryEXT target should be 4");
static_assert(offsetof(BeginQueryEXT, id) == 8,
              "offset of BeginQueryEXT id should be 8");
static_assert(offsetof(BeginQueryEXT, sync_data_shm_id) == 12,
              "offset of BeginQueryEXT sync_data_shm_id should be 12");
static_assert(offsetof(BeginQueryEXT, sync_data_shm_offset) == 16,
              "offset of BeginQueryEXT sync_data_shm_offset should be 16");

struct EndQueryEXT {
  typedef EndQueryEXT ValueType;
  static const CommandId kCmdId = kEndQueryEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _submit_count) {
    SetHeader();
    target = _target;
    submit_count = _submit_count;
  }

  void* Set(void* cmd, GLenum _target, GLuint _submit_count) {
    static_cast<ValueType*>(cmd)->Init(_target, _submit_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t submit_count;
};

static_assert(sizeof(EndQueryEXT) == 12, "size of EndQueryEXT should be 12");
static_assert(offsetof(EndQueryEXT, header) == 0,
              "offset of EndQueryEXT header should be 0");
static_assert(offsetof(EndQueryEXT, target) == 4,
              "offset of EndQueryEXT target should be 4");
static_assert(offsetof(EndQueryEXT, submit_count) == 8,
              "offset of EndQueryEXT submit_count should be 8");

struct LoseContextCHROMIUM {
  typedef LoseContextCHROMIUM ValueType;
  static const CommandId kCmdId = kLoseContextCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _current, GLenum _other) {
    SetHeader();
    current = _current;
    other = _other;
  }

  void* Set(void* cmd, GLenum _current, GLenum _other) {
    static_cast<ValueType*>(cmd)->Init(_current, _other);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t current;
  uint32_t other;
};

static_assert(sizeof(LoseContextCHROMIUM) == 12,
              "size of LoseContextCHROMIUM should be 12");
static_assert(offsetof(LoseContextCHROMIUM, header) == 0,
              "offset of LoseContextCHROMIUM header should be 0");
static_assert(offsetof(LoseContextCHROMIUM, current) == 4,
              "offset of LoseContextCHROMIUM current should be 4");
static_assert(offsetof(LoseContextCHROMIUM, other) == 8,
              "offset of LoseContextCHROMIUM other should be 8");

struct InsertFenceSyncCHROMIUM {
  typedef InsertFenceSyncCHROMIUM ValueType;
  static const CommandId kCmdId = kInsertFenceSyncCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint64 _release_count) {
    SetHeader();
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_release_count), &release_count_0,
        &release_count_1);
  }

  void* Set(void* cmd, GLuint64 _release_count) {
    static_cast<ValueType*>(cmd)->Init(_release_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 release_count() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        release_count_0, release_count_1));
  }

  gpu::CommandHeader header;
  uint32_t release_count_0;
  uint32_t release_count_1;
};

static_assert(sizeof(InsertFenceSyncCHROMIUM) == 12,
              "size of InsertFenceSyncCHROMIUM should be 12");
static_assert(offsetof(InsertFenceSyncCHROMIUM, header) == 0,
              "offset of InsertFenceSyncCHROMIUM header should be 0");
static_assert(offsetof(InsertFenceSyncCHROMIUM, release_count_0) == 4,
              "offset of InsertFenceSyncCHROMIUM release_count_0 should be 4");
static_assert(offsetof(InsertFenceSyncCHROMIUM, release_count_1) == 8,
              "offset of InsertFenceSyncCHROMIUM release_count_1 should be 8");

struct WaitSyncTokenCHROMIUM {
  typedef WaitSyncTokenCHROMIUM ValueType;
  static const CommandId kCmdId = kWaitSyncTokenCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _namespace_id,
            GLuint64 _command_buffer_id,
            GLuint64 _release_count) {
    SetHeader();
    namespace_id = _namespace_id;
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_command_buffer_id), &command_buffer_id_0,
        &command_buffer_id_1);
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_release_count), &release_count_0,
        &release_count_1);
  }

  void* Set(void* cmd,
            GLint _namespace_id,
            GLuint64 _command_buffer_id,
            GLuint64 _release_count) {
    static_cast<ValueType*>(cmd)->Init(_namespace_id, _command_buffer_id,
                                       _release_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 command_buffer_id() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        command_buffer_id_0, command_buffer_id_1));
  }

  GLuint64 release_count() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        release_count_0, release_count_1));
  }

  gpu::CommandHeader header;
  int32_t namespace_id;
  uint32_t command_buffer_id_0;
  uint32_t command_buffer_id_1;
  uint32_t release_count_0;
  uint32_t release_count_1;
};

static_assert(sizeof(WaitSyncTokenCHROMIUM) == 24,
              "size of WaitSyncTokenCHROMIUM should be 24");
static_assert(offsetof(WaitSyncTokenCHROMIUM, header) == 0,
              "offset of WaitSyncTokenCHROMIUM header should be 0");
static_assert(offsetof(WaitSyncTokenCHROMIUM, namespace_id) == 4,
              "offset of WaitSyncTokenCHROMIUM namespace_id should be 4");
static_assert(
    offsetof(WaitSyncTokenCHROMIUM, command_buffer_id_0) == 8,
    "offset of WaitSyncTokenCHROMIUM command_buffer_id_0 should be 8");
static_assert(
    offsetof(WaitSyncTokenCHROMIUM, command_buffer_id_1) == 12,
    "offset of WaitSyncTokenCHROMIUM command_buffer_id_1 should be 12");
static_assert(offsetof(WaitSyncTokenCHROMIUM, release_count_0) == 16,
              "offset of WaitSyncTokenCHROMIUM release_count_0 should be 16");
static_assert(offsetof(WaitSyncTokenCHROMIUM, release_count_1) == 20,
              "offset of WaitSyncTokenCHROMIUM release_count_1 should be 20");

struct BeginRasterCHROMIUMImmediate {
  typedef BeginRasterCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kBeginRasterCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _sk_color,
            GLuint _msaa_sample_count,
            GLboolean _can_use_lcd_text,
            GLint _color_type,
            GLuint _color_space_transfer_cache_id,
            const GLbyte* _mailbox) {
    SetHeader();
    sk_color = _sk_color;
    msaa_sample_count = _msaa_sample_count;
    can_use_lcd_text = _can_use_lcd_text;
    color_type = _color_type;
    color_space_transfer_cache_id = _color_space_transfer_cache_id;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _sk_color,
            GLuint _msaa_sample_count,
            GLboolean _can_use_lcd_text,
            GLint _color_type,
            GLuint _color_space_transfer_cache_id,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(
        _sk_color, _msaa_sample_count, _can_use_lcd_text, _color_type,
        _color_space_transfer_cache_id, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t sk_color;
  uint32_t msaa_sample_count;
  uint32_t can_use_lcd_text;
  int32_t color_type;
  uint32_t color_space_transfer_cache_id;
};

static_assert(sizeof(BeginRasterCHROMIUMImmediate) == 24,
              "size of BeginRasterCHROMIUMImmediate should be 24");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, header) == 0,
              "offset of BeginRasterCHROMIUMImmediate header should be 0");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, sk_color) == 4,
              "offset of BeginRasterCHROMIUMImmediate sk_color should be 4");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, msaa_sample_count) == 8,
    "offset of BeginRasterCHROMIUMImmediate msaa_sample_count should be 8");
static_assert(
    offsetof(BeginRasterCHROMIUMImmediate, can_use_lcd_text) == 12,
    "offset of BeginRasterCHROMIUMImmediate can_use_lcd_text should be 12");
static_assert(offsetof(BeginRasterCHROMIUMImmediate, color_type) == 16,
              "offset of BeginRasterCHROMIUMImmediate color_type should be 16");
static_assert(offsetof(BeginRasterCHROMIUMImmediate,
                       color_space_transfer_cache_id) == 20,
              "offset of BeginRasterCHROMIUMImmediate "
              "color_space_transfer_cache_id should be 20");

struct RasterCHROMIUM {
  typedef RasterCHROMIUM ValueType;
  static const CommandId kCmdId = kRasterCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _raster_shm_id,
            GLuint _raster_shm_offset,
            GLsizeiptr _raster_shm_size,
            GLuint _font_shm_id,
            GLuint _font_shm_offset,
            GLsizeiptr _font_shm_size) {
    SetHeader();
    raster_shm_id = _raster_shm_id;
    raster_shm_offset = _raster_shm_offset;
    raster_shm_size = _raster_shm_size;
    font_shm_id = _font_shm_id;
    font_shm_offset = _font_shm_offset;
    font_shm_size = _font_shm_size;
  }

  void* Set(void* cmd,
            GLuint _raster_shm_id,
            GLuint _raster_shm_offset,
            GLsizeiptr _raster_shm_size,
            GLuint _font_shm_id,
            GLuint _font_shm_offset,
            GLsizeiptr _font_shm_size) {
    static_cast<ValueType*>(cmd)->Init(_raster_shm_id, _raster_shm_offset,
                                       _raster_shm_size, _font_shm_id,
                                       _font_shm_offset, _font_shm_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t raster_shm_id;
  uint32_t raster_shm_offset;
  int32_t raster_shm_size;
  uint32_t font_shm_id;
  uint32_t font_shm_offset;
  int32_t font_shm_size;
};

static_assert(sizeof(RasterCHROMIUM) == 28,
              "size of RasterCHROMIUM should be 28");
static_assert(offsetof(RasterCHROMIUM, header) == 0,
              "offset of RasterCHROMIUM header should be 0");
static_assert(offsetof(RasterCHROMIUM, raster_shm_id) == 4,
              "offset of RasterCHROMIUM raster_shm_id should be 4");
static_assert(offsetof(RasterCHROMIUM, raster_shm_offset) == 8,
              "offset of RasterCHROMIUM raster_shm_offset should be 8");
static_assert(offsetof(RasterCHROMIUM, raster_shm_size) == 12,
              "offset of RasterCHROMIUM raster_shm_size should be 12");
static_assert(offsetof(RasterCHROMIUM, font_shm_id) == 16,
              "offset of RasterCHROMIUM font_shm_id should be 16");
static_assert(offsetof(RasterCHROMIUM, font_shm_offset) == 20,
              "offset of RasterCHROMIUM font_shm_offset should be 20");
static_assert(offsetof(RasterCHROMIUM, font_shm_size) == 24,
              "offset of RasterCHROMIUM font_shm_size should be 24");

struct EndRasterCHROMIUM {
  typedef EndRasterCHROMIUM ValueType;
  static const CommandId kCmdId = kEndRasterCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(EndRasterCHROMIUM) == 4,
              "size of EndRasterCHROMIUM should be 4");
static_assert(offsetof(EndRasterCHROMIUM, header) == 0,
              "offset of EndRasterCHROMIUM header should be 0");

struct CreateTransferCacheEntryINTERNAL {
  typedef CreateTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kCreateTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type,
            GLuint _entry_id,
            GLuint _handle_shm_id,
            GLuint _handle_shm_offset,
            GLuint _data_shm_id,
            GLuint _data_shm_offset,
            GLuint _data_size) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
    handle_shm_id = _handle_shm_id;
    handle_shm_offset = _handle_shm_offset;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
    data_size = _data_size;
  }

  void* Set(void* cmd,
            GLuint _entry_type,
            GLuint _entry_id,
            GLuint _handle_shm_id,
            GLuint _handle_shm_offset,
            GLuint _data_shm_id,
            GLuint _data_shm_offset,
            GLuint _data_size) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id, _handle_shm_id,
                                       _handle_shm_offset, _data_shm_id,
                                       _data_shm_offset, _data_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
  uint32_t handle_shm_id;
  uint32_t handle_shm_offset;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  uint32_t data_size;
};

static_assert(sizeof(CreateTransferCacheEntryINTERNAL) == 32,
              "size of CreateTransferCacheEntryINTERNAL should be 32");
static_assert(offsetof(CreateTransferCacheEntryINTERNAL, header) == 0,
              "offset of CreateTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of CreateTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of CreateTransferCacheEntryINTERNAL entry_id should be 8");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, handle_shm_id) == 12,
    "offset of CreateTransferCacheEntryINTERNAL handle_shm_id should be 12");
static_assert(offsetof(CreateTransferCacheEntryINTERNAL, handle_shm_offset) ==
                  16,
              "offset of CreateTransferCacheEntryINTERNAL handle_shm_offset "
              "should be 16");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_shm_id) == 20,
    "offset of CreateTransferCacheEntryINTERNAL data_shm_id should be 20");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_shm_offset) == 24,
    "offset of CreateTransferCacheEntryINTERNAL data_shm_offset should be 24");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_size) == 28,
    "offset of CreateTransferCacheEntryINTERNAL data_size should be 28");

struct DeleteTransferCacheEntryINTERNAL {
  typedef DeleteTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kDeleteTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type, GLuint _entry_id) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
  }

  void* Set(void* cmd, GLuint _entry_type, GLuint _entry_id) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
};

static_assert(sizeof(DeleteTransferCacheEntryINTERNAL) == 12,
              "size of DeleteTransferCacheEntryINTERNAL should be 12");
static_assert(offsetof(DeleteTransferCacheEntryINTERNAL, header) == 0,
              "offset of DeleteTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(DeleteTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of DeleteTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(DeleteTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of DeleteTransferCacheEntryINTERNAL entry_id should be 8");

struct UnlockTransferCacheEntryINTERNAL {
  typedef UnlockTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kUnlockTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type, GLuint _entry_id) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
  }

  void* Set(void* cmd, GLuint _entry_type, GLuint _entry_id) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
};

static_assert(sizeof(UnlockTransferCacheEntryINTERNAL) == 12,
              "size of UnlockTransferCacheEntryINTERNAL should be 12");
static_assert(offsetof(UnlockTransferCacheEntryINTERNAL, header) == 0,
              "offset of UnlockTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(UnlockTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of UnlockTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(UnlockTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of UnlockTransferCacheEntryINTERNAL entry_id should be 8");

struct DeletePaintCacheTextBlobsINTERNALImmediate {
  typedef DeletePaintCacheTextBlobsINTERNALImmediate ValueType;
  static const CommandId kCmdId = kDeletePaintCacheTextBlobsINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _ids) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _ids, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _ids) {
    static_cast<ValueType*>(cmd)->Init(_n, _ids);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeletePaintCacheTextBlobsINTERNALImmediate) == 8,
              "size of DeletePaintCacheTextBlobsINTERNALImmediate should be 8");
static_assert(
    offsetof(DeletePaintCacheTextBlobsINTERNALImmediate, header) == 0,
    "offset of DeletePaintCacheTextBlobsINTERNALImmediate header should be 0");
static_assert(
    offsetof(DeletePaintCacheTextBlobsINTERNALImmediate, n) == 4,
    "offset of DeletePaintCacheTextBlobsINTERNALImmediate n should be 4");

struct DeletePaintCachePathsINTERNALImmediate {
  typedef DeletePaintCachePathsINTERNALImmediate ValueType;
  static const CommandId kCmdId = kDeletePaintCachePathsINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _ids) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _ids, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _ids) {
    static_cast<ValueType*>(cmd)->Init(_n, _ids);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeletePaintCachePathsINTERNALImmediate) == 8,
              "size of DeletePaintCachePathsINTERNALImmediate should be 8");
static_assert(
    offsetof(DeletePaintCachePathsINTERNALImmediate, header) == 0,
    "offset of DeletePaintCachePathsINTERNALImmediate header should be 0");
static_assert(offsetof(DeletePaintCachePathsINTERNALImmediate, n) == 4,
              "offset of DeletePaintCachePathsINTERNALImmediate n should be 4");

struct ClearPaintCacheINTERNAL {
  typedef ClearPaintCacheINTERNAL ValueType;
  static const CommandId kCmdId = kClearPaintCacheINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(ClearPaintCacheINTERNAL) == 4,
              "size of ClearPaintCacheINTERNAL should be 4");
static_assert(offsetof(ClearPaintCacheINTERNAL, header) == 0,
              "offset of ClearPaintCacheINTERNAL header should be 0");

struct CreateAndConsumeTextureINTERNALImmediate {
  typedef CreateAndConsumeTextureINTERNALImmediate ValueType;
  static const CommandId kCmdId = kCreateAndConsumeTextureINTERNALImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _texture_id,
            bool _use_buffer,
            gfx::BufferUsage _buffer_usage,
            viz::ResourceFormat _format,
            const GLbyte* _mailbox) {
    SetHeader();
    texture_id = _texture_id;
    use_buffer = _use_buffer;
    buffer_usage = static_cast<uint32_t>(_buffer_usage);
    format = static_cast<uint32_t>(_format);
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _texture_id,
            bool _use_buffer,
            gfx::BufferUsage _buffer_usage,
            viz::ResourceFormat _format,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _use_buffer, _buffer_usage,
                                       _format, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t use_buffer;
  uint32_t buffer_usage;
  uint32_t format;
};

static_assert(sizeof(CreateAndConsumeTextureINTERNALImmediate) == 20,
              "size of CreateAndConsumeTextureINTERNALImmediate should be 20");
static_assert(
    offsetof(CreateAndConsumeTextureINTERNALImmediate, header) == 0,
    "offset of CreateAndConsumeTextureINTERNALImmediate header should be 0");
static_assert(offsetof(CreateAndConsumeTextureINTERNALImmediate, texture_id) ==
                  4,
              "offset of CreateAndConsumeTextureINTERNALImmediate texture_id "
              "should be 4");
static_assert(offsetof(CreateAndConsumeTextureINTERNALImmediate, use_buffer) ==
                  8,
              "offset of CreateAndConsumeTextureINTERNALImmediate use_buffer "
              "should be 8");
static_assert(offsetof(CreateAndConsumeTextureINTERNALImmediate,
                       buffer_usage) == 12,
              "offset of CreateAndConsumeTextureINTERNALImmediate buffer_usage "
              "should be 12");
static_assert(
    offsetof(CreateAndConsumeTextureINTERNALImmediate, format) == 16,
    "offset of CreateAndConsumeTextureINTERNALImmediate format should be 16");

struct CopySubTexture {
  typedef CopySubTexture ValueType;
  static const CommandId kCmdId = kCopySubTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id,
            GLuint _dest_id,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    source_id = _source_id;
    dest_id = _dest_id;
    xoffset = _xoffset;
    yoffset = _yoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLuint _source_id,
            GLuint _dest_id,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_source_id, _dest_id, _xoffset, _yoffset,
                                       _x, _y, _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  uint32_t dest_id;
  int32_t xoffset;
  int32_t yoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(CopySubTexture) == 36,
              "size of CopySubTexture should be 36");
static_assert(offsetof(CopySubTexture, header) == 0,
              "offset of CopySubTexture header should be 0");
static_assert(offsetof(CopySubTexture, source_id) == 4,
              "offset of CopySubTexture source_id should be 4");
static_assert(offsetof(CopySubTexture, dest_id) == 8,
              "offset of CopySubTexture dest_id should be 8");
static_assert(offsetof(CopySubTexture, xoffset) == 12,
              "offset of CopySubTexture xoffset should be 12");
static_assert(offsetof(CopySubTexture, yoffset) == 16,
              "offset of CopySubTexture yoffset should be 16");
static_assert(offsetof(CopySubTexture, x) == 20,
              "offset of CopySubTexture x should be 20");
static_assert(offsetof(CopySubTexture, y) == 24,
              "offset of CopySubTexture y should be 24");
static_assert(offsetof(CopySubTexture, width) == 28,
              "offset of CopySubTexture width should be 28");
static_assert(offsetof(CopySubTexture, height) == 32,
              "offset of CopySubTexture height should be 32");

struct TraceBeginCHROMIUM {
  typedef TraceBeginCHROMIUM ValueType;
  static const CommandId kCmdId = kTraceBeginCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _category_bucket_id, GLuint _name_bucket_id) {
    SetHeader();
    category_bucket_id = _category_bucket_id;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd, GLuint _category_bucket_id, GLuint _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_category_bucket_id, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t category_bucket_id;
  uint32_t name_bucket_id;
};

static_assert(sizeof(TraceBeginCHROMIUM) == 12,
              "size of TraceBeginCHROMIUM should be 12");
static_assert(offsetof(TraceBeginCHROMIUM, header) == 0,
              "offset of TraceBeginCHROMIUM header should be 0");
static_assert(offsetof(TraceBeginCHROMIUM, category_bucket_id) == 4,
              "offset of TraceBeginCHROMIUM category_bucket_id should be 4");
static_assert(offsetof(TraceBeginCHROMIUM, name_bucket_id) == 8,
              "offset of TraceBeginCHROMIUM name_bucket_id should be 8");

struct TraceEndCHROMIUM {
  typedef TraceEndCHROMIUM ValueType;
  static const CommandId kCmdId = kTraceEndCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(TraceEndCHROMIUM) == 4,
              "size of TraceEndCHROMIUM should be 4");
static_assert(offsetof(TraceEndCHROMIUM, header) == 0,
              "offset of TraceEndCHROMIUM header should be 0");

struct SetActiveURLCHROMIUM {
  typedef SetActiveURLCHROMIUM ValueType;
  static const CommandId kCmdId = kSetActiveURLCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _url_bucket_id) {
    SetHeader();
    url_bucket_id = _url_bucket_id;
  }

  void* Set(void* cmd, GLuint _url_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_url_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t url_bucket_id;
};

static_assert(sizeof(SetActiveURLCHROMIUM) == 8,
              "size of SetActiveURLCHROMIUM should be 8");
static_assert(offsetof(SetActiveURLCHROMIUM, header) == 0,
              "offset of SetActiveURLCHROMIUM header should be 0");
static_assert(offsetof(SetActiveURLCHROMIUM, url_bucket_id) == 4,
              "offset of SetActiveURLCHROMIUM url_bucket_id should be 4");

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
