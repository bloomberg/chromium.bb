#include "StdAfx.h"
#include "debug_breakin_point.h"
#include "debug_core.h"

namespace {
unsigned char* my_strstr(unsigned char* str, size_t str_length, unsigned char* substr, size_t substr_length) {
  for (size_t i = 0; i < str_length; i++ ) {
    unsigned char* p = str + i;
    size_t str_length_now = str_length - i;
    if (str_length_now < substr_length)
      return NULL;

    if (memcmp(p, substr, substr_length) == 0)
      return p;
  }
  return 0;
}
}  // namespace

namespace debug {
bool AppBreakinPoint::IsNexeThread(DebuggeeProcess* proc, void** mem_base) {
//return false;
#ifdef _WIN64
  return false;
#endif

  bool result = false;
  char* addr = 0;
  char* max_addr = (char*)(2UL * 1024UL * 1024UL * 1024UL);
  int count = 0;
  do {
    MEMORY_BASIC_INFORMATION32 mbi;
    memset(&mbi, 0, sizeof(mbi));
    SIZE_T sz = ::VirtualQueryEx(proc->Handle(), addr, (MEMORY_BASIC_INFORMATION*)&mbi, sizeof(mbi));
    if (sz != sizeof(mbi))
      break;
    addr += mbi.RegionSize;

    if ((mbi.Protect == PAGE_EXECUTE) ||
       (mbi.Protect == PAGE_EXECUTE_READ) ||
       (mbi.Protect == PAGE_EXECUTE_READWRITE)) {
      if (mbi.RegionSize) {
        unsigned char* copy = (unsigned char*)malloc(mbi.RegionSize);
        memset(copy, 0, mbi.RegionSize);
        if (copy) {
          proc->ReadMemory((void*)mbi.AllocationBase, mbi.RegionSize, copy);
          //TODO: remove hardcodedness, take input from the user?
          //unsigned char br_instr[] = {0xf7, 0x7d, 0xe4, 0x89 , 0x45 , 0xe8 , 0x8b , 0x45 , 0x08, 0x89, 0x04,0x24};
//          unsigned char br_instr[] = {0x5e, 0x48, 0x89, 0xe1, 0x8d, 0x5c, 0xb4, 0x04};
		  unsigned char br_instr[] = {0x31, 0xed, 0x5e, 0x89, 0xe1, 0x8d, 0x5c, 0xb4, 0x04};
          unsigned char* brk_addr = my_strstr(copy, mbi.RegionSize, br_instr, sizeof(br_instr));
          if (brk_addr) {
            size_t offs = brk_addr - copy;
            byte* brk_proc_addr = (byte*)mbi.AllocationBase + offs;
            size_t nexe_start_addr = 0x20080;  //TODO: remove hardcodedness: read from app.nexe (elf) file?
  
			//size_t divz_addr =  0x20080; //0x20563;  //TODO: same here
            size_t divz_addr =  0x20563;  //TODO: same here
            *mem_base = reinterpret_cast<void*>(brk_proc_addr - divz_addr);
            printf( "Ohoho #%d at %p membase=%p mbi.RegionSize=%d\n", count++, brk_proc_addr, *mem_base, mbi.RegionSize);
      
            debug::Blob blob(brk_addr, sizeof(br_instr));
            std::string str = blob.ToHexString(false);
            printf( "[%s]\n", str.c_str());
            result = true;
          }
          free(copy);
        }
      }
    }
  } while (addr < max_addr);
  return result;
}
}  // namespace debug