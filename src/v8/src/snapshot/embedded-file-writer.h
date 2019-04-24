// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_FILE_WRITER_H_
#define V8_SNAPSHOT_EMBEDDED_FILE_WRITER_H_

#include <cstdio>
#include <cstring>

#include "src/globals.h"
#include "src/snapshot/snapshot.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {

enum DataDirective {
  kByte,
  kLong,
  kQuad,
  kOcta,
};

static constexpr char kDefaultEmbeddedVariant[] = "Default";

// The platform-dependent logic for emitting assembly code for the generated
// embedded.S file.
class EmbeddedFileWriter;
class PlatformDependentEmbeddedFileWriter final {
 public:
  void SetFile(FILE* fp) { fp_ = fp; }

  void SectionText();
  void SectionData();
  void SectionRoData();

  void AlignToCodeAlignment();
  void AlignToDataAlignment();

  void DeclareUint32(const char* name, uint32_t value);
  void DeclarePointerToSymbol(const char* name, const char* target);

  void DeclareLabel(const char* name);

  void SourceInfo(int fileid, const char* filename, int line);
  void DeclareFunctionBegin(const char* name);
  void DeclareFunctionEnd(const char* name);

  // Returns the number of printed characters.
  int HexLiteral(uint64_t value);

  void Comment(const char* string);
  void Newline() { fprintf(fp_, "\n"); }

  void FilePrologue();
  void DeclareExternalFilename(int fileid, const char* filename);
  void FileEpilogue();

  int IndentedDataDirective(DataDirective directive);

  FILE* fp() const { return fp_; }

 private:
  void DeclareSymbolGlobal(const char* name);

 private:
  FILE* fp_ = nullptr;
};

// When writing out compiled builtins to a file, we
// Detailed source-code information about builtins can only be obtained by
// registration on the isolate during compilation.
class EmbeddedFileWriterInterface {
 public:
  // We maintain a database of filenames to synthetic IDs.
  virtual int LookupOrAddExternallyCompiledFilename(const char* filename) = 0;
  virtual const char* GetExternallyCompiledFilename(int index) const = 0;
  virtual int GetExternallyCompiledFilenameCount() const = 0;

  // The isolate will call the method below just prior to replacing the
  // compiled builtin Code objects with trampolines.
  virtual void PrepareBuiltinSourcePositionMap(Builtins* builtins) = 0;
};

// Generates the embedded.S file which is later compiled into the final v8
// binary. Its contents are exported through two symbols:
//
// v8_<variant>_embedded_blob_ (intptr_t):
//     a pointer to the start of the embedded blob.
// v8_<variant>_embedded_blob_size_ (uint32_t):
//     size of the embedded blob in bytes.
//
// The variant is usually "Default" but can be modified in multisnapshot builds.
class EmbeddedFileWriter : public EmbeddedFileWriterInterface {
 public:
  int LookupOrAddExternallyCompiledFilename(const char* filename) override {
    auto result = external_filenames_.find(filename);
    if (result != external_filenames_.end()) {
      return result->second;
    }
    int new_id =
        ExternalFilenameIndexToId(static_cast<int>(external_filenames_.size()));
    external_filenames_.insert(std::make_pair(filename, new_id));
    external_filenames_by_index_.push_back(filename);
    DCHECK_EQ(external_filenames_by_index_.size(), external_filenames_.size());
    return new_id;
  }

  const char* GetExternallyCompiledFilename(int fileid) const override {
    size_t index = static_cast<size_t>(ExternalFilenameIdToIndex(fileid));
    DCHECK_GE(index, 0);
    DCHECK_LT(index, external_filenames_by_index_.size());

    return external_filenames_by_index_[index];
  }

  int GetExternallyCompiledFilenameCount() const override {
    return static_cast<int>(external_filenames_.size());
  }

  void PrepareBuiltinSourcePositionMap(Builtins* builtins) override;

  void SetEmbeddedFile(const char* embedded_src_path) {
    embedded_src_path_ = embedded_src_path;
  }

  void SetEmbeddedVariant(const char* embedded_variant) {
    embedded_variant_ = embedded_variant;
  }

  void WriteEmbedded(const i::EmbeddedData* blob) const {
    MaybeWriteEmbeddedFile(blob);
  }

 private:
  void MaybeWriteEmbeddedFile(const i::EmbeddedData* blob) const {
    if (embedded_src_path_ == nullptr) return;

    FILE* fp = GetFileDescriptorOrDie(embedded_src_path_);

    PlatformDependentEmbeddedFileWriter writer;
    writer.SetFile(fp);

    WriteFilePrologue(&writer);
    WriteExternalFilenames(&writer);
    WriteMetadataSection(&writer, blob);
    WriteInstructionStreams(&writer, blob);
    WriteFileEpilogue(&writer, blob);

    fclose(fp);
  }

  static FILE* GetFileDescriptorOrDie(const char* filename) {
    FILE* fp = v8::base::OS::FOpen(filename, "wb");
    if (fp == nullptr) {
      i::PrintF("Unable to open file \"%s\" for writing.\n", filename);
      exit(1);
    }
    return fp;
  }

  void WriteFilePrologue(PlatformDependentEmbeddedFileWriter* w) const {
    w->Comment("Autogenerated file. Do not edit.");
    w->Newline();
    w->FilePrologue();
  }

  void WriteExternalFilenames(PlatformDependentEmbeddedFileWriter* w) const {
    w->Comment(
        "Source positions in the embedded blob refer to filenames by id.");
    w->Comment("Assembly directives here map the id to a filename.");
    w->Newline();

    // Write external filenames.
    int size = static_cast<int>(external_filenames_by_index_.size());
    for (int i = 0; i < size; i++) {
      w->DeclareExternalFilename(ExternalFilenameIndexToId(i),
                                 external_filenames_by_index_[i]);
    }
  }

  // Fairly arbitrary but should fit all symbol names.
  static constexpr int kTemporaryStringLength = 256;

  void WriteMetadataSection(PlatformDependentEmbeddedFileWriter* w,
                            const i::EmbeddedData* blob) const {
    char embedded_blob_data_symbol[kTemporaryStringLength];
    i::SNPrintF(i::Vector<char>(embedded_blob_data_symbol),
                "v8_%s_embedded_blob_data_", embedded_variant_);

    w->Comment("The embedded blob starts here. Metadata comes first, followed");
    w->Comment("by builtin instruction streams.");
    w->SectionText();
    w->AlignToCodeAlignment();
    w->DeclareLabel(embedded_blob_data_symbol);

    WriteBinaryContentsAsInlineAssembly(w, blob->data(),
                                        i::EmbeddedData::RawDataOffset());
  }

  void WriteBuiltin(PlatformDependentEmbeddedFileWriter* w,
                    const i::EmbeddedData* blob, const int builtin_id) const {
    const bool is_default_variant =
        std::strcmp(embedded_variant_, kDefaultEmbeddedVariant) == 0;

    char builtin_symbol[kTemporaryStringLength];
    if (is_default_variant) {
      // Create nicer symbol names for the default mode.
      i::SNPrintF(i::Vector<char>(builtin_symbol), "Builtins_%s",
                  i::Builtins::name(builtin_id));
    } else {
      i::SNPrintF(i::Vector<char>(builtin_symbol), "%s_Builtins_%s",
                  embedded_variant_, i::Builtins::name(builtin_id));
    }

    // Labels created here will show up in backtraces. We check in
    // Isolate::SetEmbeddedBlob that the blob layout remains unchanged, i.e.
    // that labels do not insert bytes into the middle of the blob byte
    // stream.
    w->DeclareFunctionBegin(builtin_symbol);
    const std::vector<byte>& current_positions = source_positions_[builtin_id];

    // The code below interleaves bytes of assembly code for the builtin
    // function with source positions at the appropriate offsets.
    Vector<const byte> vpos(current_positions.data(), current_positions.size());
    v8::internal::SourcePositionTableIterator positions(
        vpos, SourcePositionTableIterator::kExternalOnly);

    const uint8_t* data = reinterpret_cast<const uint8_t*>(
        blob->InstructionStartOfBuiltin(builtin_id));
    uint32_t size = blob->PaddedInstructionSizeOfBuiltin(builtin_id);
    uint32_t i = 0;
    uint32_t next_offset = static_cast<uint32_t>(
        positions.done() ? size : positions.code_offset());
    while (i < size) {
      if (i == next_offset) {
        // Write source directive.
        w->SourceInfo(positions.source_position().ExternalFileId(),
                      GetExternallyCompiledFilename(
                          positions.source_position().ExternalFileId()),
                      positions.source_position().ExternalLine());
        positions.Advance();
        next_offset = static_cast<uint32_t>(
            positions.done() ? size : positions.code_offset());
      }
      CHECK_GE(next_offset, i);
      WriteBinaryContentsAsInlineAssembly(w, data + i, next_offset - i);
      i = next_offset;
    }

    w->DeclareFunctionEnd(builtin_symbol);
  }

  void WriteInstructionStreams(PlatformDependentEmbeddedFileWriter* w,
                               const i::EmbeddedData* blob) const {
    for (int i = 0; i < i::Builtins::builtin_count; i++) {
      if (!blob->ContainsBuiltin(i)) continue;

      WriteBuiltin(w, blob, i);
    }
    w->Newline();
  }

  void WriteFileEpilogue(PlatformDependentEmbeddedFileWriter* w,
                         const i::EmbeddedData* blob) const {
    {
      char embedded_blob_data_symbol[kTemporaryStringLength];
      i::SNPrintF(i::Vector<char>(embedded_blob_data_symbol),
                  "v8_%s_embedded_blob_data_", embedded_variant_);

      char embedded_blob_symbol[kTemporaryStringLength];
      i::SNPrintF(i::Vector<char>(embedded_blob_symbol), "v8_%s_embedded_blob_",
                  embedded_variant_);

      w->Comment("Pointer to the beginning of the embedded blob.");
      w->SectionData();
      w->AlignToDataAlignment();
      w->DeclarePointerToSymbol(embedded_blob_symbol,
                                embedded_blob_data_symbol);
      w->Newline();
    }

    {
      char embedded_blob_size_symbol[kTemporaryStringLength];
      i::SNPrintF(i::Vector<char>(embedded_blob_size_symbol),
                  "v8_%s_embedded_blob_size_", embedded_variant_);

      w->Comment("The size of the embedded blob in bytes.");
      w->SectionRoData();
      w->AlignToDataAlignment();
      w->DeclareUint32(embedded_blob_size_symbol, blob->size());
      w->Newline();
    }

    w->FileEpilogue();
  }

#if defined(_MSC_VER) && !defined(__clang__)
#define V8_COMPILER_IS_MSVC
#endif

#if defined(V8_COMPILER_IS_MSVC)
  // Windows MASM doesn't have an .octa directive, use QWORDs instead.
  // Note: MASM *really* does not like large data streams. It takes over 5
  // minutes to assemble the ~350K lines of embedded.S produced when using
  // BYTE directives in a debug build. QWORD produces roughly 120KLOC and
  // reduces assembly time to ~40 seconds. Still terrible, but much better
  // than before. See also: https://crbug.com/v8/8475.
  static constexpr DataDirective kByteChunkDirective = kQuad;
  static constexpr int kByteChunkSize = 8;

  static int WriteByteChunk(PlatformDependentEmbeddedFileWriter* w,
                            int current_line_length, const uint8_t* data) {
    const uint64_t* quad_ptr = reinterpret_cast<const uint64_t*>(data);
    return current_line_length + w->HexLiteral(*quad_ptr);
  }

#elif defined(V8_OS_AIX)
  // PPC uses a fixed 4 byte instruction set, using .long
  // to prevent any unnecessary padding.
  static constexpr DataDirective kByteChunkDirective = kLong;
  static constexpr int kByteChunkSize = 4;

  static int WriteByteChunk(PlatformDependentEmbeddedFileWriter* w,
                            int current_line_length, const uint8_t* data) {
    const uint32_t* long_ptr = reinterpret_cast<const uint32_t*>(data);
    return current_line_length + w->HexLiteral(*long_ptr);
  }

#else  // defined(V8_COMPILER_IS_MSVC) || defined(V8_OS_AIX)
  static constexpr DataDirective kByteChunkDirective = kOcta;
  static constexpr int kByteChunkSize = 16;

  static int WriteByteChunk(PlatformDependentEmbeddedFileWriter* w,
                            int current_line_length, const uint8_t* data) {
    const size_t size = kInt64Size;

    uint64_t part1, part2;
    // Use memcpy for the reads since {data} is not guaranteed to be aligned.
#ifdef V8_TARGET_BIG_ENDIAN
    memcpy(&part1, data, size);
    memcpy(&part2, data + size, size);
#else
    memcpy(&part1, data + size, size);
    memcpy(&part2, data, size);
#endif  // V8_TARGET_BIG_ENDIAN

    if (part1 != 0) {
      current_line_length +=
          fprintf(w->fp(), "0x%" PRIx64 "%016" PRIx64, part1, part2);
    } else {
      current_line_length += fprintf(w->fp(), "0x%" PRIx64, part2);
    }
    return current_line_length;
  }
#endif  // defined(V8_COMPILER_IS_MSVC) || defined(V8_OS_AIX)
#undef V8_COMPILER_IS_MSVC

  static int WriteDirectiveOrSeparator(PlatformDependentEmbeddedFileWriter* w,
                                       int current_line_length,
                                       DataDirective directive) {
    int printed_chars;
    if (current_line_length == 0) {
      printed_chars = w->IndentedDataDirective(directive);
      DCHECK_LT(0, printed_chars);
    } else {
      printed_chars = fprintf(w->fp(), ",");
      DCHECK_EQ(1, printed_chars);
    }
    return current_line_length + printed_chars;
  }

  static int WriteLineEndIfNeeded(PlatformDependentEmbeddedFileWriter* w,
                                  int current_line_length, int write_size) {
    static const int kTextWidth = 100;
    // Check if adding ',0xFF...FF\n"' would force a line wrap. This doesn't use
    // the actual size of the string to be written to determine this so it's
    // more conservative than strictly needed.
    if (current_line_length + strlen(",0x") + write_size * 2 > kTextWidth) {
      fprintf(w->fp(), "\n");
      return 0;
    } else {
      return current_line_length;
    }
  }

  static void WriteBinaryContentsAsInlineAssembly(
      PlatformDependentEmbeddedFileWriter* w, const uint8_t* data,
      uint32_t size) {
    int current_line_length = 0;
    uint32_t i = 0;

    // Begin by writing out byte chunks.
    for (; i + kByteChunkSize < size; i += kByteChunkSize) {
      current_line_length = WriteDirectiveOrSeparator(w, current_line_length,
                                                      kByteChunkDirective);
      current_line_length = WriteByteChunk(w, current_line_length, data + i);
      current_line_length =
          WriteLineEndIfNeeded(w, current_line_length, kByteChunkSize);
    }
    if (current_line_length != 0) w->Newline();
    current_line_length = 0;

    // Write any trailing bytes one-by-one.
    for (; i < size; i++) {
      current_line_length =
          WriteDirectiveOrSeparator(w, current_line_length, kByte);
      current_line_length += w->HexLiteral(data[i]);
      current_line_length = WriteLineEndIfNeeded(w, current_line_length, 1);
    }

    if (current_line_length != 0) w->Newline();
  }

  static int ExternalFilenameIndexToId(int index) {
    return kFirstExternalFilenameId + index;
  }

  static int ExternalFilenameIdToIndex(int id) {
    return id - kFirstExternalFilenameId;
  }

  std::vector<byte> source_positions_[Builtins::builtin_count];

  // In assembly directives, filename ids need to begin with 1.
  static const int kFirstExternalFilenameId = 1;
  std::map<const char*, int> external_filenames_;
  std::vector<const char*> external_filenames_by_index_;

  const char* embedded_src_path_ = nullptr;
  const char* embedded_variant_ = kDefaultEmbeddedVariant;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_FILE_WRITER_H_
