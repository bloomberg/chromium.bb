/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncfileutil.c - open an executable file. FOR TESTING ONLY.
 */
#include "native_client/src/include/portability.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include "native_client/src/include/portability_io.h"

#include "native_client/src/trusted/validator_x86/ncfileutil.h"

/* This module is intended for testing use only, not for production use */
/* in sel_ldr. To prevent unintended production usage, define a symbol  */
/* that will cause a load-time error for sel_ldr.                       */
int gNaClValidateImage_foo = 0;
void NaClValidateImage() { gNaClValidateImage_foo += 1; }

static Elf_Addr NCPageTrunc(Elf_Addr v) {
  return (v & ~(kNCFileUtilPageSize - 1));
}

static Elf_Addr NCPageRound(Elf_Addr v) {
  return(NCPageTrunc(v + (kNCFileUtilPageSize - 1)));
}

/* Define the default print error function to use for this module. */
void NcLoadFilePrintError(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

static nc_loadfile_error_fn nc_error_fn = NcLoadFilePrintError;

nc_loadfile_error_fn NcLoadFileRegisterErrorFn(nc_loadfile_error_fn fn) {
  nc_loadfile_error_fn return_value = nc_error_fn;
  nc_error_fn = fn;
  return return_value;
}

/***********************************************************************/
/* THIS ROUTINE IS FOR DEBUGGING/TESTING ONLY, NOT FOR SECURE RUNTIME  */
/* ALL PAGES ARE LEFT WRITEABLE BY THIS LOADER.                        */
/***********************************************************************/
/* Loading a NC executable from a host file */
static off_t readat(const int fd, void *buf, const off_t sz, const size_t at) {
  /* TODO(karl) fix types for off_t and size_t so that the work for 64-bits */
  size_t sofar = 0;
  int nread;
  char *cbuf = (char *)buf;

  if (0 > lseek(fd, at, SEEK_SET)) {
    nc_error_fn("readat: lseek failed\n");
    return -1;
  }

  /* TODO(robertm) Figure out if O_BINARY flag fixes this. */
  /* Strangely this loop is needed on Windows. It seems the read()   */
  /* implementation doesn't always return as many bytes as requested */
  /* so you have to keep on trying.                                  */
  do {
    nread = read(fd, &cbuf[sofar], sz - sofar);
    if (nread <= 0) {
      nc_error_fn("readat: read failed\n");
      return -1;
    }
    sofar += nread;
  } while (sz != sofar);
  return nread;
}

static const char* GetEiClassName(unsigned char c) {
  if (c == ELFCLASS32) {
    return "(32 bit executable)";
  } else if (c == ELFCLASS64) {
    return "(64 bit executable)";
  } else {
    return "(invalid class)";
  }
}

static int nc_load(ncfile *ncf, int fd, int nc_rules) {

  Elf_Ehdr h;
  ssize_t nread;
  Elf_Addr vmemlo, vmemhi;
  size_t shsize, phsize;
  int i;

  /* Read and check the ELF header */
  nread = readat(fd, &h, sizeof(h), 0);
  if (nread < 0 || (size_t) nread < sizeof(h)) {
    nc_error_fn("nc_load(%s): could not read ELF header", ncf->fname);
    return -1;
  }

  /* do a bunch of sanity checks */
  if (strncmp((char *)h.e_ident, ELFMAG, strlen(ELFMAG))) {
    nc_error_fn("nc_load(%s): bad magic number", ncf->fname);
    return -1;
  }
  if (h.e_ident[EI_OSABI] != ELFOSABI_NACL) {
    nc_error_fn("nc_load(%s): bad OS ABI %x\n",
                ncf->fname, h.e_ident[EI_OSABI]);
    /* return -1; */
  }
  if (h.e_ident[EI_ABIVERSION] != EF_NACL_ABIVERSION) {
    nc_error_fn("nc_load(%s): bad ABI version %d\n", ncf->fname,
            h.e_ident[EI_ABIVERSION]);
    /* return -1; */
  }

  if (h.e_ident[EI_CLASS] != NACL_ELF_CLASS) {
    nc_error_fn("nc_load(%s): bad EI CLASS %d %s\n", ncf->fname,
                h.e_ident[EI_CLASS], GetEiClassName(h.e_ident[EI_CLASS]));
  }

  if ((h.e_flags & EF_NACL_ALIGN_MASK) == EF_NACL_ALIGN_16) {
    ncf->ncalign = 16;
  } else if ((h.e_flags & EF_NACL_ALIGN_MASK) == EF_NACL_ALIGN_32) {
    ncf->ncalign = 32;
  } else {
    nc_error_fn("nc_load(%s): bad align mask %x\n", ncf->fname,
                (uint32_t)(h.e_flags & EF_NACL_ALIGN_MASK));
    ncf->ncalign = 16;
    /* return -1; */
  }

  /* Read the program header table */
  if (h.e_phnum <= 0 || h.e_phnum > kMaxPhnum) {
    nc_error_fn("nc_load(%s): h.e_phnum %d > kMaxPhnum %d\n",
                ncf->fname, h.e_phnum, kMaxPhnum);
    return -1;
  }
  ncf->phnum = h.e_phnum;
  ncf->pheaders = (Elf_Phdr *)calloc(h.e_phnum, sizeof(Elf_Phdr));
  if (NULL == ncf->pheaders) {
    nc_error_fn("nc_load(%s): calloc(%d, %"NACL_PRIdS") failed\n",
                ncf->fname, h.e_phnum, sizeof(Elf_Phdr));
    return -1;
  }
  phsize = h.e_phnum * sizeof(*ncf->pheaders);
  /* TODO(karl) Remove the cast to size_t, or verify size. */
  nread = readat(fd, ncf->pheaders, phsize, (size_t) h.e_phoff);
  if (nread < 0 || (size_t) nread < phsize) return -1;

  /* Iterate through the program headers to find the virtual */
  /* size of loaded text.                                    */
  vmemlo = MAX_ELF_ADDR;
  vmemhi = MIN_ELF_ADDR;
  for (i = 0; i < h.e_phnum; i++) {
    if (ncf->pheaders[i].p_type != PT_LOAD) continue;
    if (0 == (ncf->pheaders[i].p_flags & PF_X)) continue;
    /* This is executable text. Check low and high addrs */
    if (vmemlo > ncf->pheaders[i].p_vaddr) vmemlo = ncf->pheaders[i].p_vaddr;
    if (vmemhi < ncf->pheaders[i].p_vaddr + ncf->pheaders[i].p_memsz) {
      vmemhi = ncf->pheaders[i].p_vaddr + ncf->pheaders[i].p_memsz;
    }
  }
  vmemhi = NCPageRound(vmemhi);
  ncf->size = vmemhi - vmemlo;
  ncf->vbase = vmemlo;
  if (nc_rules && vmemlo != NCPageTrunc(vmemlo)) {
    nc_error_fn("nc_load(%s): vmemlo is not aligned\n", ncf->fname);
    return -1;
  }
  /* TODO(karl) Remove the cast to size_t, or verify size. */
  ncf->data = (uint8_t *)calloc(1, (size_t) ncf->size);
  if (NULL == ncf->data) {
    nc_error_fn("nc_load(%s): calloc(1, %d) failed\n",
                ncf->fname, (int)ncf->size);
    return -1;
  }

  /* Load program text segments */
  for (i = 0; i < h.e_phnum; i++) {
    const Elf_Phdr *p = &ncf->pheaders[i];
    if (p->p_type != PT_LOAD) continue;
    if (0 == (ncf->pheaders[i].p_flags & PF_X)) continue;

    if (nc_rules) {
      assert(ncf->size >= NCPageRound(p->p_vaddr - ncf->vbase + p->p_memsz));
    }
    /* TODO(karl) Remove the cast to off_t, or verify value in range. */
    nread = readat(fd, &(ncf->data[p->p_vaddr - ncf->vbase]),
                   (off_t) p->p_filesz, (off_t) p->p_offset);
    if (nread < 0 || (size_t) nread < p->p_filesz) {
      nc_error_fn(
          "nc_load(%s): could not read segment %d (%d < %"
          NACL_PRIuElf_Xword")\n",
          ncf->fname, i, (int)nread, p->p_filesz);
      return -1;
    }
  }
  /* load the section headers */
  ncf->shnum = h.e_shnum;
  shsize = ncf->shnum * sizeof(*ncf->sheaders);
  ncf->sheaders = (Elf_Shdr *)calloc(1, shsize);
  if (NULL == ncf->sheaders) {
    nc_error_fn("nc_load(%s): calloc(1, %"NACL_PRIdS") failed\n",
                ncf->fname, shsize);
    return -1;
  }
  /* TODO(karl) Remove the cast to size_t, or verify value in range. */
  nread = readat(fd, ncf->sheaders, shsize, (size_t) h.e_shoff);
  if (nread < 0 || (size_t) nread < shsize) {
    nc_error_fn("nc_load(%s): could not read section headers\n",
                ncf->fname);
    return -1;
  }

  /* success! */
  return 0;
}

ncfile *nc_loadfile_depending(const char *filename, int nc_rules) {
  ncfile *ncf;
  int fd;
  int rdflags = O_RDONLY | _O_BINARY;
  fd = OPEN(filename, rdflags);
  if (fd < 0) return NULL;

  /* Allocate the ncfile structure */
  ncf = calloc(1, sizeof(ncfile));
  if (ncf == NULL) return NULL;
  ncf->size = 0;
  ncf->data = NULL;
  ncf->fname = filename;

  if (nc_load(ncf, fd, nc_rules) < 0) {
    close(fd);
    free(ncf);
    return NULL;
  }
  close(fd);
  return ncf;
}

ncfile *nc_loadfile(const char *filename) {
  return nc_loadfile_depending(filename, 1);
}


void nc_freefile(ncfile *ncf) {
  if (ncf->data != NULL) free(ncf->data);
  free(ncf);
}

/***********************************************************************/

void GetVBaseAndLimit(ncfile *ncf, PcAddress *vbase, PcAddress *vlimit) {
  int ii;
  /* TODO(karl) - Define so constant applies to 64-bit pc address. */
  PcAddress base = 0xffffffff;
  PcAddress limit = 0;

  for (ii = 0; ii < ncf->shnum; ii++) {
    if ((ncf->sheaders[ii].sh_flags & SHF_EXECINSTR) == SHF_EXECINSTR) {
      if (ncf->sheaders[ii].sh_addr < base) base = ncf->sheaders[ii].sh_addr;
      if (ncf->sheaders[ii].sh_addr + ncf->sheaders[ii].sh_size > limit)
        limit = ncf->sheaders[ii].sh_addr + ncf->sheaders[ii].sh_size;
    }
  }
  *vbase = base;
  *vlimit = limit;
}
