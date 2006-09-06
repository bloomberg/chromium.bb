/* Copyright (C) 2006 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

/* minidump_format.h: A cross-platform reimplementation of minidump-related
 * portions of DbgHelp.h from the Windows Platform SDK.
 *
 * (This is C99 source, please don't corrupt it with C++.)
 *
 * This file contains the necessary definitions to read minidump files
 * produced on win32/x86.  These files may be read on any platform provided
 * that the alignments of these structures on the processing system are
 * identical to the alignments of these structures on the producing system.
 * For this reason, precise-sized types are used.  The structures defined by
 * by this file have been laid out to minimize alignment problems by ensuring
 * ensuring that all members are aligned on their natural boundaries.  In
 * In some cases, tail-padding may be significant when different ABIs specify
 * different tail-padding behaviors.  To avoid problems when reading or
 * writing affected structures, MD_*_SIZE macros are provided where needed,
 * containing the useful size of the structures without padding.
 *
 * These structures are also sufficient to populate minidump files.
 *
 * These definitions may be extended to support handling minidump files
 * for other CPUs and other operating systems.
 *
 * Because precise data type sizes are crucial for this implementation to
 * function properly and portably in terms of interoperability with minidumps
 * produced by DbgHelp on Windows, a set of primitive types with known sizes
 * are used as the basis of each structure defined by this file.  DbgHelp
 * on Windows is assumed to be the reference implementation; this file
 * seeks to provide a cross-platform compatible implementation.  To avoid
 * collisions with the types and values defined and used by DbgHelp in the
 * event that this implementation is used on Windows, each type and value
 * defined here is given a new name, beginning with "MD".  Names of the
 * equivlaent types and values in the Windows Platform SDK are given in
 * comments.
 *
 * Author: Mark Mentovai */
 

#ifndef PROCESSOR_MINIDUMP_FORMAT_H__
#define PROCESSOR_MINIDUMP_FORMAT_H__


#include "google/airbag_types.h"


/*
 * guiddef.h
 */


typedef struct {
  u_int32_t data1;
  u_int16_t data2;
  u_int16_t data3;
  u_int8_t  data4[8];
} MDGUID; /* GUID */


/*
 * WinNT.h
 */


#define MD_FLOATINGSAVEAREA_SIZEOF_REGISTERAREA_X86 80
     /* SIZE_OF_80387_REGISTERS */

typedef struct {
  u_int32_t control_word;
  u_int32_t status_word;
  u_int32_t tag_word;
  u_int32_t error_offset;
  u_int32_t error_selector;
  u_int32_t data_offset;
  u_int32_t data_selector;
  u_int8_t  register_area[MD_FLOATINGSAVEAREA_SIZEOF_REGISTERAREA_X86];
  u_int32_t cr0_npx_state;
} MDFloatingSaveAreaX86; /* FLOATING_SAVE_AREA */


#define MD_CONTEXT_SIZEOF_EXTENDED_REGISTERS_X86 512
     /* MAXIMUM_SUPPORTED_EXTENSION */

typedef struct {
  /* The next field determines the layout of the structure, and what parts
   * of it are populated */
  u_int32_t             context_flags;

  /* The next 6 registers are included with MD_CONTEXT_X86_DEBUG_REGISTERS */
  u_int32_t             dr0;
  u_int32_t             dr1;
  u_int32_t             dr2;
  u_int32_t             dr3;
  u_int32_t             dr6;
  u_int32_t             dr7;

  /* The next field is included with MD_CONTEXT_X86_FLOATING_POINT */
  MDFloatingSaveAreaX86 float_save;

  /* The next 4 registers are included with MD_CONTEXT_X86_SEGMENTS */
  u_int32_t             gs; 
  u_int32_t             fs;
  u_int32_t             es;
  u_int32_t             ds;
  /* The next 6 registers are included with MD_CONTEXT_X86_INTEGER */
  u_int32_t             edi;
  u_int32_t             esi;
  u_int32_t             ebx;
  u_int32_t             edx;
  u_int32_t             ecx;
  u_int32_t             eax;

  /* The next 6 registers are included with MD_CONTEXT_X86_CONTROL */
  u_int32_t             ebp;
  u_int32_t             eip;
  u_int32_t             cs;     /* WinNT.h says "must be sanitized" */
  u_int32_t             eflags; /* WinNT.h says "must be sanitized" */
  u_int32_t             esp;
  u_int32_t             ss;

  /* The next field is included with MD_CONTEXT_X86_EXTENDED_REGISTERS */
  u_int8_t              extended_registers[
                         MD_CONTEXT_SIZEOF_EXTENDED_REGISTERS_X86];
} MDRawContextX86; /* CONTEXT */

/* For (MDRawContextX86).context_flags.  These values indicate the type of
 * context stored in the structure. */
#define MD_CONTEXT_X86_X86               0x00010000
     /* CONTEXT_i386, CONTEXT_i486: identifies CPU */
#define MD_CONTEXT_X86_CONTROL           (MD_CONTEXT_X86_X86 | 0x00000001)
     /* CONTEXT_CONTROL */
#define MD_CONTEXT_X86_INTEGER           (MD_CONTEXT_X86_X86 | 0x00000002)
     /* CONTEXT_INTEGER */
#define MD_CONTEXT_X86_SEGMENTS          (MD_CONTEXT_X86_X86 | 0x00000004)
     /* CONTEXT_SEGMENTS */
#define MD_CONTEXT_X86_FLOATING_POINT    (MD_CONTEXT_X86_X86 | 0x00000008)
     /* CONTEXT_FLOATING_POINT */
#define MD_CONTEXT_X86_DEBUG_REGISTERS   (MD_CONTEXT_X86_X86 | 0x00000010)
     /* CONTEXT_DEBUG_REGISTERS */
#define MD_CONTEXT_X86_EXTENED_REGISTERS (MD_CONTEXT_X86_X86 | 0x00000020)
     /* CONTEXT_EXTENDED_REGISTERS */

#define MD_CONTEXT_X86_FULL              (MD_CONTEXT_X86_CONTROL | \
                                          MD_CONTEXT_X86_INTEGER | \
                                          MD_CONTEXT_X86_SEGMENTS)
     /* CONTEXT_FULL */

#define MD_CONTEXT_X86_ALL               (MD_CONTEXT_X86_FULL | \
                                          MD_CONTEXT_X86_FLOATING_POINT | \
                                          MD_CONTEXT_X86_DEBUG_REGISTERS | \
                                          MD_CONTEXT_X86_EXTENDED_REGISTERS)
     /* CONTEXT_ALL */


/*
 * WinVer.h
 */


typedef struct {
  u_int32_t signature;
  u_int32_t struct_version;
  u_int32_t file_version_hi;
  u_int32_t file_version_lo;
  u_int32_t product_version_hi;
  u_int32_t product_version_lo;
  u_int32_t file_flags_mask;    /* Identifies valid bits in fileFlags */
  u_int32_t file_flags;
  u_int32_t file_os;
  u_int32_t file_type;
  u_int32_t file_subtype;
  u_int32_t file_date_hi;
  u_int32_t file_date_lo;
} MDVSFixedFileInfo; /* VS_FIXEDFILEINFO */

/* For (MDVSFixedFileInfo).signature */
#define MD_VSFIXEDFILEINFO_SIGNATURE 0xfeef04bd
     /* VS_FFI_SIGNATURE */

/* For (MDVSFixedFileInfo).version */
#define MD_VSFIXEDFILEINFO_VERSION 0x00010000
     /* VS_FFI_STRUCVERSION */

/* For (MDVSFixedFileInfo).file_flags_mask and
 * (MDVSFixedFileInfo).file_flags */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_DEBUG        0x00000001
     /* VS_FF_DEBUG */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PRERELEASE   0x00000002
     /* VS_FF_PRERELEASE */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PATCHED      0x00000004
     /* VS_FF_PATCHED */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PRIVATEBUILD 0x00000008
     /* VS_FF_PRIVATEBUILD */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_INFOINFERRED 0x00000010
     /* VS_FF_INFOINFERRED */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_SPECIALBUILD 0x00000020
     /* VS_FF_SPECIALBUILD */

/* For (MDVSFixedFileInfo).file_os: high 16 bits */
#define MD_VSFIXEDFILEINFO_FILE_OS_UNKNOWN    0         /* VOS_UNKNOWN */
#define MD_VSFIXEDFILEINFO_FILE_OS_DOS        (1 << 16) /* VOS_DOS */
#define MD_VSFIXEDFILEINFO_FILE_OS_OS216      (2 << 16) /* VOS_OS216 */
#define MD_VSFIXEDFILEINFO_FILE_OS_OS232      (3 << 16) /* VOS_OS232 */
#define MD_VSFIXEDFILEINFO_FILE_OS_NT         (4 << 16) /* VOS_NT */
#define MD_VSFIXEDFILEINFO_FILE_OS_WINCE      (5 << 16) /* VOS_WINCE */
/* Low 16 bits */
#define MD_VSFIXEDFILEINFO_FILE_OS__BASE      0         /* VOS__BASE */
#define MD_VSFIXEDFILEINFO_FILE_OS__WINDOWS16 1         /* VOS__WINDOWS16 */
#define MD_VSFIXEDFILEINFO_FILE_OS__PM16      2         /* VOS__PM16 */
#define MD_VSFIXEDFILEINFO_FILE_OS__PM32      3         /* VOS__PM32 */
#define MD_VSFIXEDFILEINFO_FILE_OS__WINDOWS32 4         /* VOS__WINDOWS32 */

/* For (MDVSFixedFileInfo).file_type */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_UNKNOWN    0 /* VFT_UNKNOWN */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_APP        1 /* VFT_APP */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_DLL        2 /* VFT_DLL */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_DRV        3 /* VFT_DLL */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_FONT       4 /* VFT_FONT */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_VXD        5 /* VFT_VXD */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_STATIC_LIB 7 /* VFT_STATIC_LIB */

/* For (MDVSFixedFileInfo).file_subtype */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_UNKNOWN                0
     /* VFT2_UNKNOWN */
/* with file_type = MD_VSFIXEDFILEINFO_FILETYPE_DRV */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_PRINTER            1
     /* VFT2_DRV_PRINTER */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_KEYBOARD           2
     /* VFT2_DRV_KEYBOARD */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_LANGUAGE           3
     /* VFT2_DRV_LANGUAGE */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_DISPLAY            4
     /* VFT2_DRV_DISPLAY */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_MOUSE              5
     /* VFT2_DRV_MOUSE */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_NETWORK            6
     /* VFT2_DRV_NETWORK */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_SYSTEM             7
     /* VFT2_DRV_SYSTEM */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_INSTALLABLE        8
     /* VFT2_DRV_INSTALLABLE */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_SOUND              9
     /* VFT2_DRV_SOUND */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_COMM              10
     /* VFT2_DRV_COMM */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_INPUTMETHOD       11
     /* VFT2_DRV_INPUTMETHOD */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_VERSIONED_PRINTER 12
     /* VFT2_DRV_VERSIONED_PRINTER */
/* with file_type = MD_VSFIXEDFILEINFO_FILETYPE_FONT */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_RASTER            1
     /* VFT2_FONT_RASTER */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_VECTOR            2
     /* VFT2_FONT_VECTOR */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_TRUETYPE          3
     /* VFT2_FONT_TRUETYPE */


/*
 * DbgHelp.h
 */


typedef u_int32_t MDRVA; /* RVA */


typedef struct {
  u_int32_t data_size;
  MDRVA     rva;
} MDLocationDescriptor; /* MINIDUMP_LOCATION_DESCRIPTOR */


typedef struct {
  u_int64_t            start_of_memory_range;
  MDLocationDescriptor memory;
} MDMemoryDescriptor; /* MINIDUMP_MEMORY_DESCRIPTOR */


typedef struct {
  u_int32_t signature;
  u_int32_t version;
  u_int32_t stream_count;
  MDRVA     stream_directory_rva;
  u_int32_t checksum;
  u_int32_t time_date_stamp;      /* time_t */
  u_int64_t flags;
} MDRawHeader; /* MINIDUMP_HEADER */

/* For (MDRawHeader).signature and (MDRawHeader).version.  Note that only the
 * low 16 bits of (MDRawHeader).version are MD_HEADER_VERSION.  Per the
 * documentation, the high 16 bits are implementation-specific. */
#define MD_HEADER_SIGNATURE 0x504d444d /* 'PMDM' */
     /* MINIDUMP_SIGNATURE */
#define MD_HEADER_VERSION   0x0000a793 /* 42899 */
     /* MINIDUMP_VERSION */

/* For (MDRawHeader).flags */
typedef enum {
  MINIDUMP_NORMAL                            = 0x00000000,
  MINIDUMP_WITH_DATA_SEGS                    = 0x00000001,
  MINIDUMP_WITH_FULL_MEMORY                  = 0x00000002,
  MINIDUMP_WITH_HANDLE_DATA                  = 0x00000004,
  MINIDUMP_FILTER_MEMORY                     = 0x00000008,
  MINIDUMP_SCAN_MEMORY                       = 0x00000010,
  MINIDUMP_WITH_UNLOADED_MODULES             = 0x00000020,
  MINIDUMP_WITH_INDIRECTLY_REFERENCED_MEMORY = 0X00000040,
  MINIDUMP_FILTER_MODULE_PATHS               = 0x00000080,
  MINIDUMP_WITH_PROCESS_THREAD_DATA          = 0x00000100,
  MINIDUMP_WITH_PRIVATE_READ_WRITE_MEMORY    = 0x00000200,
  MINIDUMP_WITHOUT_OPTIONAL_DATA             = 0x00000400,
  MINIDUMP_WITH_FULL_MEMORY_INFO             = 0x00000800,
  MINIDUMP_WITH_THREAD_INFO                  = 0x00001000,
  MINIDUMP_WITH_CODE_SEGS                    = 0x00002000
} MDType; /* MINIDUMP_TYPE */


typedef struct {
  u_int32_t            stream_type;
  MDLocationDescriptor location;
} MDRawDirectory; /* MINIDUMP_DIRECTORY */

/* For (MDRawDirectory).stream_type */
typedef enum {
  UNUSED_STREAM               =  0,
  RESERVED_STREAM_0           =  1,
  RESERVED_STREAM_1           =  2,
  THREAD_LIST_STREAM          =  3,
  MODULE_LIST_STREAM          =  4,
  MEMORY_LIST_STREAM          =  5,
  EXCEPTION_STREAM            =  6,
  SYSTEM_INFO_STREAM          =  7,
  THREAD_EX_LIST_STREAM       =  8,
  MEMORY_64_LIST_STREAM       =  9,
  COMMENT_STREAM_A            = 10,
  COMMENT_STREAM_W            = 11,
  HANDLE_DATA_STREAM          = 12,
  FUNCTION_TABLE_STREAM       = 13,
  UNLOADED_MODULE_LIST_STREAM = 14,
  MISC_INFO_STREAM            = 15,
  LAST_RESERVED_STREAM        = 0x0000FFFF
} MDStreamType; /* MINIDUMP_STREAM_TYPE */


typedef struct {
  u_int32_t            thread_id;
  u_int32_t            suspend_count;
  u_int32_t            priority_class;
  u_int32_t            priority;
  u_int64_t            teb;           /* Thread environment block */
  MDMemoryDescriptor   stack;
  MDLocationDescriptor thread_context;
} MDRawThread; /* MINIDUMP_THREAD */


typedef struct {
  u_int32_t number_of_threads;
  MDRawThread  threads[0];
} MDRawThreadList; /* MINIDUMP_THREAD_LIST */


typedef struct {
  u_int64_t            base_of_image;
  u_int32_t            size_of_image;
  u_int32_t            checksum;
  u_int32_t            time_date_stamp;
  MDRVA                module_name_rva;
  MDVSFixedFileInfo    version_info;

  /* The next field stores a CodeView record and is populated when a module's
   * debug information resides in a PDB file.  It identifies the PDB file. */
  MDLocationDescriptor cv_record;

  /* The next field is populated when a module's debug information resides
   * in a DBG file.  It identifies the DBG file.  This field is effectively
   * obsolete with modules built by recent toolchains. */
  MDLocationDescriptor misc_record;

  /* Alignment problem: reserved0 and reserved1 are defined by the platform
   * SDK as 64-bit quantities.  However, that results in a structure whose
   * alignment is unpredictable on different CPUs and ABIs.  If the ABI
   * specifies full alignment of 64-bit quantities in structures (as ppc
   * does), there will be padding between miscRecord and reserved0.  If
   * 64-bit quantities can be aligned on 32-bit boundaries (as on x86),
   * this padding will not exist.  (Note that the structure up to this point
   * contains 1 64-bit member followed by 21 32-bit members.)
   * As a workaround, reserved0 and reserved1 are instead defined here as
   * four 32-bit quantities.  This should be harmless, as there are
   * currently no known uses for these fields. */
  u_int32_t            reserved0[2];
  u_int32_t            reserved1[2];
} MDRawModule; /* MINIDUMP_MODULE */

/* The inclusion of a 64-bit type in MINIDUMP_MODULE forces the struct to
 * be tail-padded out to a multiple of 64 bits under some ABIs (such as PPC).
 * This doesn't occur on systems that don't tail-pad in this manner.  Define
 * this macro to be the usable size of the MDRawModule struct, and use it in
 * place of sizeof(MDRawModule). */
#define MD_MODULE_SIZE 108


/* (MDRawModule).cv_record can reference MDCVInfoPDB20 or MDCVInfoPDB70.
 * Ref.: http://www.debuginfo.com/articles/debuginfomatch.html
 * MDCVInfoPDB70 is the expected structure type with recent toolchains. */

typedef struct {
  u_int32_t signature;
  u_int32_t offset;    /* Offset to debug data (expect 0 in minidump) */
} MDCVHeader;

typedef struct {
  MDCVHeader cv_header;
  u_int32_t  signature;        /* time_t debug information created */
  u_int32_t  age;              /* revision of PDB file */
  u_int8_t   pdb_file_name[0];
} MDCVInfoPDB20;

#define MD_CVINFOPDB20_SIGNATURE 0x3031424e /* cvHeader.signature = '01BN' */

typedef struct {
  u_int32_t cv_signature;
  MDGUID    signature;        /* GUID, identifies PDB file */
  u_int32_t age;              /* Identifies incremental changes to PDB file */
  u_int8_t  pdb_file_name[0]; /* 0-terminated 8-bit character data (UTF-8?) */
} MDCVInfoPDB70;

#define MD_CVINFOPDB70_SIGNATURE 0x53445352 /* cvSignature = 'SDSR' */

/* (MDRawModule).miscRecord can reference MDImageDebugMisc.  The Windows
 * structure is actually defined in WinNT.h.  This structure is effectively
 * obsolete with modules built by recent toolchains. */

typedef struct {
  u_int32_t data_type;
  u_int32_t length;
  u_int8_t  unicode;
  u_int8_t  reserved[3];
  u_int8_t  data[0];
} MDImageDebugMisc; /* IMAGE_DEBUG_MISC */


typedef struct {
  u_int32_t number_of_modules;
  MDRawModule  modules[0];
} MDRawModuleList; /* MINIDUMP_MODULE_LIST */


typedef struct {
  u_int32_t          number_of_memory_ranges;
  MDMemoryDescriptor memory_ranges[0];
} MDRawMemoryList; /* MINIDUMP_MEMORY_LIST */


#define MD_EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct {
  u_int32_t exception_code;
  u_int32_t exception_flags;
  u_int64_t exception_record;
  u_int64_t exception_address;
  u_int32_t number_parameters;
  u_int32_t __align;
  u_int64_t exception_information[MD_EXCEPTION_MAXIMUM_PARAMETERS];
} MDException; /* MINIDUMP_EXCEPTION */


typedef struct {
  u_int32_t            thread_id;
  u_int32_t            __align;
  MDException          exception_record;
  MDLocationDescriptor thread_context;
} MDRawExceptionStream; /* MINIDUMP_EXCEPTION_STREAM */


typedef union {
  struct {
    u_int32_t vendor_id[3];              /* cpuid 0: eax, ebx, ecx */
    u_int32_t version_information;       /* cpuid 1: eax */
    u_int32_t feature_information;       /* cpuid 1: edx */
    u_int32_t amd_extended_cpu_features; /* cpuid 0x80000001, ebx */
  } x86_cpu_info;
  struct {
    u_int64_t processor_features[2];
  } other_cpu_info;
} MDCPUInformation; /* CPU_INFORMATION */


typedef struct {
  /* The next 3 fields and numberOfProcessors are from the SYSTEM_INFO
   * structure as returned by GetSystemInfo */
  u_int16_t        processor_architecture;
  u_int16_t        processor_level;
  u_int16_t        processor_revision;
  union {
    u_int16_t      reserved0;
    struct {
      u_int8_t     number_of_processors;
      u_int8_t     product_type;
    };
  };

  /* The next 5 fields are from the OSVERSIONINFO structure as returned
   * by GetVersionEx */
  u_int32_t        major_version;
  u_int32_t        minor_version;
  u_int32_t        build_number;
  u_int32_t        platform_id;
  MDRVA            csd_version_rva; /* Name of the installed OS service pack */

  union {
    u_int32_t      reserved1;
    struct {
      u_int16_t    suite_mask;
      u_int16_t    reserved2;
    };
  };
  MDCPUInformation cpu;
} MDRawSystemInfo; /* MINIDUMP_SYSTEM_INFO */


typedef struct {
  u_int32_t size_of_info;
  u_int32_t flags1;
  u_int32_t process_id;
  u_int32_t process_create_time;
  u_int32_t process_user_time;
  u_int32_t process_kernel_time;

  /* The following fields are not present in MINIDUMP_MISC_INFO but are
   * in MINIDUMP_MISC_INFO_2.  When this struct is populated, these values
   * may not be set.  Use flags1 or sizeOfInfo to determine whether these
   * values are present. */
  u_int32_t processor_max_mhz;
  u_int32_t processor_current_mhz;
  u_int32_t processor_mhz_limit;
  u_int32_t processor_max_idle_state;
  u_int32_t processor_current_idle_state;
} MDRawMiscInfo; /* MINIDUMP_MISC_INFO, MINIDUMP_MISC_INFO2 */

#define MD_MISCINFO_SIZE 24
#define MD_MISCINFO2_SIZE 44


#endif /* PROCESSOR_MINIDUMP_FORMAT_H__ */
