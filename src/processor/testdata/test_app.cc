// This file is used to generate minidump2.dmp and minidump2.sym.
// cl /Zi /Fetest_app.exe test_app.cc dbghelp.lib
// Then run test_app to generate a dump, and dump_syms to create the .sym file.

#include <windows.h>
#include <dbghelp.h>

static LONG HandleException(EXCEPTION_POINTERS *exinfo) {
  HANDLE dump_file = CreateFile("dump.dmp",
                                GENERIC_WRITE,
                                FILE_SHARE_WRITE,
                                NULL,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

  MINIDUMP_EXCEPTION_INFORMATION except_info;
  except_info.ThreadId = GetCurrentThreadId();
  except_info.ExceptionPointers = exinfo;
  except_info.ClientPointers = false;
  
  MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    dump_file,
                    MiniDumpNormal,
                    &except_info,
                    NULL,
                    NULL);

  CloseHandle(dump_file);
  return EXCEPTION_EXECUTE_HANDLER;
}

void CrashFunction() {
  int *i = NULL;
  *i = 5;  // crash!
}

int main(int argc, char *argv[]) {
  __try {
    CrashFunction();
  } __except(HandleException(GetExceptionInformation())) {
  }
  return 0;
}
