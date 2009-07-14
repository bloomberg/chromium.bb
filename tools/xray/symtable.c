/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// XRay symbol table

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xray_priv.h"

#if defined(XRAY)

bool g_symtable_debug = false;

struct XRayFrameInfo {
  int times_called;
  int total_ticks;
};


struct XRaySymbol {
  const char *name;
  struct XRayFrameInfo frames[XRAY_MAX_FRAMES];
};


struct XRaySymbolPoolNode {
  struct XRaySymbolPoolNode *next;
  struct XRaySymbol symbols[XRAY_SYMBOL_POOL_NODE_SIZE];
};


struct XRaySymbolPool {
  struct XRaySymbolPoolNode *head;
  struct XRaySymbolPoolNode *current;
  int index;
};


struct XRaySymbolTable {
  int num_symbols;
  struct XRayHashTable *hash_table;
  struct XRayStringPool *string_pool;
  struct XRaySymbolPool *symbol_pool;
};


const char* XRaySymbolGetName(struct XRaySymbol *symbol) {
  return (NULL == symbol) ? "(null)" : symbol->name;
}


struct XRaySymbol* XRaySymbolCreate(struct XRaySymbolPool *sympool,
                                    const char *name)
{
  struct XRaySymbol *symbol;
  symbol = XRaySymbolPoolAlloc(sympool);
  symbol->name = name;
  return symbol;
}


struct XRaySymbol* XRaySymbolPoolAlloc(struct XRaySymbolPool *sympool) {
  struct XRaySymbol *symbol;
  if (sympool->index >= XRAY_SYMBOL_POOL_NODE_SIZE) {
    struct XRaySymbolPoolNode *new_pool;
    new_pool = (struct XRaySymbolPoolNode *)XRayMalloc(sizeof(*new_pool));
    sympool->current->next = new_pool;
    sympool->current = new_pool;
    sympool->index = 0;
  }
  symbol = &sympool->current->symbols[sympool->index];
  ++sympool->index;
  return symbol;
}


struct XRaySymbolPool* XRaySymbolPoolCreate() {
  struct XRaySymbolPool *sympool;
  struct XRaySymbolPoolNode *node;
  sympool = (struct XRaySymbolPool *)XRayMalloc(sizeof(*sympool));
  node = (struct XRaySymbolPoolNode *)XRayMalloc(sizeof(*node));
  sympool->head = node;
  sympool->current = node;
  sympool->index = 0;
  return sympool;
}


void XRaySymbolPoolFree(struct XRaySymbolPool *pool) {
  struct XRaySymbolPoolNode *n = pool->head;
  while (NULL != n) {
    struct XRaySymbolPoolNode *c = n;
    n = n->next;
    XRayFree(c);
  }
  XRayFree(pool);
}


int XRaySymbolTableGetSize(struct XRaySymbolTable *symtab) {
  return XRayHashTableGetSize(symtab->hash_table);
}


struct XRaySymbol* XRaySymbolTableAtIndex(struct XRaySymbolTable *symtab,
                                          int i) {
  return (struct XRaySymbol *)XRayHashTableAtIndex(symtab->hash_table, i);
}


struct XRaySymbol* XRaySymbolTableLookup(struct XRaySymbolTable *symtab,
                                         uint32_t addr) {
  void *x = XRayHashTableLookup(symtab->hash_table, addr);
  struct XRaySymbol *r = (struct XRaySymbol *)x;
  return r;
}


struct XRaySymbol* XRaySymbolTableAdd(struct XRaySymbolTable *symtab,
                                      struct XRaySymbol *symbol,
                                      uint32_t addr) {
  return (struct XRaySymbol *)
    XRayHashTableInsert(symtab->hash_table, symbol, addr);
}


struct XRaySymbol* XRaySymbolTableCreateEntry(struct XRaySymbolTable *symtab,
                                              const char *line) {
  uint32_t addr;
  unsigned int uiaddr;
  char symbol_text[XRAY_LINE_SIZE];
  char *parsed_symbol;
  char *newln;
  struct XRaySymbol *symbol;
  char *recorded_symbol;
  sscanf(line,"%x %s", &uiaddr, symbol_text);
  if (uiaddr > 0x07FFFFFF) {
    printf("While parsing the mapfile, XRay encountered:\n");
    printf("%s\n", line);
    printf("XRay only works with code addresses 0x00000000 - 0x07FFFFFF\n");
    printf("All functions must reside in this address space.\n");
    exit(-1);
  }
  addr = (uint32_t)uiaddr;
  parsed_symbol = strstr(line, symbol_text);
  newln = strstr(parsed_symbol, "\n");
  if (NULL != newln) {
    *newln = 0;
  }
  /* copy the parsed symbol name into the string pool */
  recorded_symbol = XRayStringPoolAppend(symtab->string_pool, parsed_symbol);
  if (g_symtable_debug)
    printf("adding symbol %s\n", recorded_symbol);
  /* construct a symbol and put it in the symbol table */
  symbol = XRaySymbolCreate(symtab->symbol_pool, recorded_symbol);
  return XRaySymbolTableAdd(symtab, symbol, addr);
}


void XRaySymbolTableParseMapfile(struct XRaySymbolTable *symtab,
                                 const char *mapfile)
{
  FILE *f;
  char line[XRAY_LINE_SIZE];
  bool in_text = false;
  bool in_link_once = false;
  int in_link_once_counter = 0;
  int num_symbols = 0;

  printf("XRay: opening mapfile %s\n", mapfile);
  f = fopen(mapfile, "rt");
  if (0 != f) {
    printf("XRay: parsing...\n");
    while(NULL != fgets(line, XRAY_LINE_SIZE, f)) {
      if (line == strstr(line, " .text ")) {
        in_text = true;
        continue;
      }
      if (line == strstr(line, " .gnu.linkonce.t.")) {
        in_link_once = true;
        in_link_once_counter = 0;
        continue;
      }
      if (line == strstr(line, " .text.")) {
        in_link_once = true;
        in_link_once_counter = 0;
        continue;
      }
      if (line == strstr(line, "                0x")) {
        if (in_text) {
          XRaySymbolTableCreateEntry(symtab, line);
          ++num_symbols;
        } else if (in_link_once) {
          if (in_link_once_counter != 0) {
            XRaySymbolTableCreateEntry(symtab, line);
            ++num_symbols;
          } else {
            ++in_link_once_counter;
          }
        }
      } else {
        in_text = false;
        in_link_once = false;
      }
    }
    fclose(f);
    printf("XRay: loaded %d symbols into symbol table\n", num_symbols);
  } else {
    printf("XRay: failed to open %s\n", mapfile);
  }
  symtab->num_symbols += num_symbols;
}


/* returns total number of symbols in the table */
int XRaySymbolCount(struct XRaySymbolTable *symtab) {
  return symtab->num_symbols;
}


/* creates and inializes a symbol table */
struct XRaySymbolTable* XRaySymbolTableCreate(int size) {
  struct XRaySymbolTable *symtab;
  symtab = (struct XRaySymbolTable *)XRayMalloc(sizeof(*symtab));
  symtab->num_symbols = 0;
  symtab->string_pool = XRayStringPoolCreate();
  symtab->hash_table = XRayHashTableCreate(size);
  symtab->symbol_pool = XRaySymbolPoolCreate();
  return symtab;
}


/* frees symbol table */
void XRaySymbolTableFree(struct XRaySymbolTable *symtab) {
  XRayStringPoolFree(symtab->string_pool);
  XRaySymbolPoolFree(symtab->symbol_pool);
  XRayHashTableFree(symtab->hash_table);
  symtab->num_symbols = 0;
  XRayFree(symtab);
}

#endif  /* defined(XRAY) */
