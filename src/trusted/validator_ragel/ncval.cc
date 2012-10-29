#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "native_client/src/include/elf.h"
#include "native_client/src/include/elf_constants.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

using std::vector;
using std::string;


// Hypothetically reading whole ELF file to memory can cause problems with huge
// amounts of debug info, but unless it actually happens this approach is used
// for simplicity.
void ReadImage(const char *filename, vector<uint8_t> *data) {
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to open input file: %s\n", filename);
    exit(1);
  }

  fseek(fp, 0, SEEK_END);
  size_t file_size = ftell(fp);

  data->resize(file_size);

  fseek(fp, 0, SEEK_SET);
  size_t got = fread(&(*data)[0], 1, file_size, fp);
  if (got != file_size) {
    fprintf(stderr, "Unable to read data from input file: %s\n",
            filename);
    exit(1);
  }
  fclose(fp);
}


struct Segment {
  const uint8_t *data;
  uint32_t size;
  uint32_t vaddr;
};


Segment FindTextSegment32(const vector<uint8_t> &data) {
  Segment segment = {NULL, 0, 0}; // only to suppress 'uninitialized' warning
  bool found = false;

  const Elf32_Ehdr &header = *reinterpret_cast<const Elf32_Ehdr *>(&data[0]);
  CHECK(sizeof(header) <= data.size());
  CHECK(memcmp(header.e_ident, ELFMAG, SELFMAG) == 0);

  for (int index = 0; index < header.e_phnum; ++index) {
    const Elf32_Phdr &phdr = *reinterpret_cast<const Elf32_Phdr *>(
        &data[header.e_phoff + header.e_phentsize * index]);

    // TODO(shcherbina): size of other loadable segments
    if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X)) {
      if (found) {
        printf("More than one text segment.\n");
        exit(1);
      }

      if (phdr.p_flags != (PF_R | PF_X)) {
        // Cast to support 64-bit ELF.
        printf("Text segment is expected to have flags PF_R | PF_X "
               "(has 0x%" NACL_PRIx64 " instead).\n",
               static_cast<uint64_t>(phdr.p_flags));
        exit(1);
      }

      CHECK(phdr.p_filesz <= phdr.p_memsz);
      if (phdr.p_filesz < phdr.p_memsz) {
        printf("File image is smaller than memory image size.\n");
        exit(1);
      }

      // TODO(shcherbina): find or introduce proper constant.
      if (phdr.p_filesz > 256 << 20) {
        printf("Test segment is too large.");
        exit(1);
      }

      if (phdr.p_vaddr > UINT32_MAX - phdr.p_filesz) {
        printf("Text segment does not fit in 4GB.");
        exit(1);
      }

      segment.data = &data[phdr.p_offset];
      segment.size = static_cast<uint32_t>(phdr.p_filesz);
      segment.vaddr = static_cast<uint32_t>(phdr.p_vaddr);
      found = true;
    }
  }
  if (!found) {
    printf("Text segment not found.\n");
    exit(1);
  }
  return segment;
}


struct Error {
  uint32_t offset;
  string message;
};


struct UserData {
  Segment segment;
  vector<Error> *errors;
};


Bool ProcessError(const uint8_t *begin, const uint8_t *end,
                  uint32_t validation_info, void *user_data_ptr) {
  UNREFERENCED_PARAMETER(end);
  UserData &user_data = *reinterpret_cast<UserData *>(user_data_ptr);
  Error error;
  ptrdiff_t offset = begin - user_data.segment.data;
  error.offset = user_data.segment.vaddr + static_cast<uint32_t>(offset);
  if (validation_info & UNRECOGNIZED_INSTRUCTION)
    error.message = "unrecognized instruction";
  if (validation_info & DIRECT_JUMP_OUT_OF_RANGE)
    error.message = "direct jump out of range";
  if (validation_info & CPUID_UNSUPPORTED_INSTRUCTION)
    error.message = "required CPU feature not found";
  // TODO(shcherbina): report jump sources, not destinations
  if (validation_info & BAD_JUMP_TARGET)
    error.message = "jump to this offset";
  // ...
  // TODO(shcherbina): distinguish more kinds of errors
  else
    error.message = "<placeholder error message>";
  user_data.errors->push_back(error);
  return FALSE;
}


bool Validate32(const Segment &segment, vector<Error> *errors) {
  CHECK(segment.size % kBundleSize == 0);
  CHECK(segment.vaddr % kBundleSize == 0);

  errors->clear();

  UserData user_data;
  user_data.segment = segment;
  user_data.errors = errors;

  // TODO(shcherbina): customize from command line
  const NaClCPUFeaturesX86 *cpu_features = &kFullCPUIDFeatures;
  ValidationOptions options = static_cast<ValidationOptions>(0);

  return ValidateChunkIA32(
      segment.data, segment.size,
      options, cpu_features,
      ProcessError, &user_data);
}


void Usage() {
  printf("Usage:\n");
  printf("    ncval <elf file>\n");
  exit(1);
}


const char* ParseOptions(int argc, const char * const *argv) {
  if (argc != 2)
    Usage();

  return argv[1];
}


int main(int argc, char **argv) {
  const char *input_file = ParseOptions(argc, argv);

  printf("Validating %s ...\n", input_file);

  vector<uint8_t> data;
  ReadImage(input_file, &data);
  Segment segment = FindTextSegment32(data);

  vector<Error> errors;
  bool result = Validate32(segment, &errors);

  for (size_t i = 0; i < errors.size(); i++) {
    const Error &e = errors[i];
    printf("%8" NACL_PRIx32 ": %s\n", e.offset, e.message.c_str());
  }

  if (result) {
    printf("Valid.\n");
    return 0;
  } else {
    printf("Invalid.\n");
    return 1;
  }
}
