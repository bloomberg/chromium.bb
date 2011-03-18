#include "StdAfx.h"
#include "debug_registers.h"
#include <windows.h>

namespace debug {
#define REG_DEF(name) (*descriptions)[descriptions->size()] = RegisterDescription(1, #name, offsetof(CONTEXT, name), sizeof(context.name));

void GetRegisterDescriptions(std::map<int, RegisterDescription>* descriptions) {

#ifdef _WIN64
  CONTEXT context;
  REG_DEF(Rax);
  REG_DEF(Rip);
/*
  DWORD64 Rax;
    DWORD64 Rcx;
    DWORD64 Rdx;
    DWORD64 Rbx;
    DWORD64 Rsp;
    DWORD64 Rbp;
    DWORD64 Rsi;
    DWORD64 Rdi;
    DWORD64 R8;
    DWORD64 R9;
    DWORD64 R10;
    DWORD64 R11;
    DWORD64 R12;
    DWORD64 R13;
    DWORD64 R14;
    DWORD64 R15;
*/
#else
  CONTEXT context;
  REG_DEF(Eax);
  REG_DEF(Ecx);
  REG_DEF(Edx);
  REG_DEF(Ebx);
  REG_DEF(Esp);
  REG_DEF(Ebp);
  REG_DEF(Esi);
  REG_DEF(Edi);
  REG_DEF(Eip);
  REG_DEF(EFlags);
  REG_DEF(SegCs);
  REG_DEF(SegSs);
  REG_DEF(SegDs);
  REG_DEF(SegEs);
  REG_DEF(SegFs);
  REG_DEF(SegGs);
#endif
}
}  // namespace debug