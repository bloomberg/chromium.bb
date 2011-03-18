#include "StdAfx.h"
#include "debug_debug_event.h"

namespace {
json::StructDefinitions loc_DEBUG_EVENT_struct_defs;
json::Object* CreateFromEXCEPTION_RECORD(const EXCEPTION_RECORD& er);
}  // namespace

namespace debug {
json::StructDefinitions* DEBUG_EVENT_struct_defs = &loc_DEBUG_EVENT_struct_defs;

json::Value* CONTEXT_ToJSON(CONTEXT ct) {
  DEBUG_EVENT_ToJSON_Init();
  return CreateFromStruct(&ct, "CONTEXT", loc_DEBUG_EVENT_struct_defs);
}

json::Value* MEMORY_BASIC_INFORMATION32_ToJSON(MEMORY_BASIC_INFORMATION32 mbi) {
  DEBUG_EVENT_ToJSON_Init();
  return CreateFromStruct(&mbi, "MEMORY_BASIC_INFORMATION32", loc_DEBUG_EVENT_struct_defs);
}

json::Value* DEBUG_EVENT_ToJSON(DEBUG_EVENT de) {
  DEBUG_EVENT_ToJSON_Init();
  json::Object* root_obj = CreateFromStruct(&de, "DEBUG_EVENT", loc_DEBUG_EVENT_struct_defs);
  switch (de.dwDebugEventCode) {
    case CREATE_PROCESS_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.CreateProcessInfo, "CREATE_PROCESS_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.CreateProcessInfo", u_obj);
      break;
    }
    case CREATE_THREAD_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.CreateThread, "CREATE_THREAD_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.CreateThread", u_obj);
      break;
    }
    case EXCEPTION_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.Exception, "EXCEPTION_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      json::Object* ex_obj = CreateFromEXCEPTION_RECORD(de.u.Exception.ExceptionRecord);
      u_obj->SetProperty("u.Exception.ExceptionRecord", ex_obj);
      root_obj->SetProperty("u.Exception", u_obj);
      break;
    }
    case EXIT_PROCESS_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.ExitProcess, "EXIT_PROCESS_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.ExitProcess", u_obj);
      break;
    }
    case EXIT_THREAD_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.ExitThread, "EXIT_THREAD_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.ExitThread", u_obj);
      break;
    }
    case LOAD_DLL_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.LoadDll, "LOAD_DLL_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.LoadDll", u_obj);
      break;
    }
    case OUTPUT_DEBUG_STRING_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.DebugString, "OUTPUT_DEBUG_STRING_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.DebugString", u_obj);
      break;
    }
    case RIP_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.RipInfo, "RIP_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.RipInfo", u_obj);
      break;
    }
    case UNLOAD_DLL_DEBUG_EVENT: {
      json::Object* u_obj = CreateFromStruct(&de.u.UnloadDll, "UNLOAD_DLL_DEBUG_INFO", loc_DEBUG_EVENT_struct_defs);
      root_obj->SetProperty("u.UnloadDll", u_obj);
      break;
    }
  }
  return root_obj;
}

void DEBUG_EVENT_ToJSON(DEBUG_EVENT de, std::string* text_out) {
  json::StructTextCreator text_creator;
  text_creator.SetGenerateComments(true, &loc_DEBUG_EVENT_struct_defs);
  json::Value* root_obj = DEBUG_EVENT_ToJSON(de);
  text_creator.CreateText(*root_obj, text_out);
  delete root_obj;
}

void DEBUG_EVENT_ToJSON_Init() {
  static bool initialized = false;
  if (initialized) return;

  const char* dwDebugEventCode_enums =
      "3:CREATE_PROCESS_DEBUG_EVENT,2:CREATE_THREAD_DEBUG_EVENT,1:EXCEPTION_DEBUG_EVENT,"
      "5:EXIT_PROCESS_DEBUG_EVENT,4:EXIT_THREAD_DEBUG_EVENT,6:LOAD_DLL_DEBUG_EVENT,"
      "8:OUTPUT_DEBUG_STRING_EVENT,9:RIP_EVENT,7:UNLOAD_DLL_DEBUG_EVENT";
  
  json::StructDefinitions* defs = &loc_DEBUG_EVENT_struct_defs;
  START_STRUCT_DEF(DEBUG_EVENT);
  DEF_ENUM_FIELD(dwDebugEventCode, dwDebugEventCode_enums);
  DEF_INT_FIELD(dwProcessId);
  DEF_INT_FIELD(dwThreadId);
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(CREATE_PROCESS_DEBUG_INFO);
  DEF_HANDLE_FIELD(hFile);
  DEF_HANDLE_FIELD(hProcess);
  DEF_HANDLE_FIELD(hThread);
  DEF_PTR_FIELD(lpBaseOfImage);
  DEF_INT_FIELD(dwDebugInfoFileOffset);
  DEF_INT_FIELD(nDebugInfoSize);
  DEF_PTR_FIELD(lpThreadLocalBase);
  DEF_PTR_FIELD(lpStartAddress);
  //DEF_USTR_FIELD(lpImageName, fUnicode);
  DEF_INT_FIELD(lpImageName);  //TODO: test only!
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(CREATE_THREAD_DEBUG_INFO);
  DEF_HANDLE_FIELD(hThread);
  DEF_PTR_FIELD(lpThreadLocalBase);
  DEF_PTR_FIELD(lpStartAddress);
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(EXCEPTION_DEBUG_INFO);
  DEF_INT_FIELD(dwFirstChance);
  STOP_STRUCT_DEF;

  std::string ex_code_enums;
  ADD_INT_C(EXCEPTION_ACCESS_VIOLATION, ex_code_enums);
  ADD_INT_C(EXCEPTION_DATATYPE_MISALIGNMENT, ex_code_enums);
  ADD_INT_C(EXCEPTION_BREAKPOINT, ex_code_enums);
  ADD_INT_C(EXCEPTION_SINGLE_STEP, ex_code_enums);
  ADD_INT_C(EXCEPTION_ARRAY_BOUNDS_EXCEEDED, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_DENORMAL_OPERAND, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_DIVIDE_BY_ZERO, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_INEXACT_RESULT, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_INVALID_OPERATION, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_OVERFLOW, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_STACK_CHECK, ex_code_enums);
  ADD_INT_C(EXCEPTION_FLT_UNDERFLOW, ex_code_enums);
  ADD_INT_C(EXCEPTION_INT_DIVIDE_BY_ZERO, ex_code_enums);
  ADD_INT_C(EXCEPTION_INT_OVERFLOW, ex_code_enums);
  ADD_INT_C(EXCEPTION_PRIV_INSTRUCTION, ex_code_enums);
  ADD_INT_C(EXCEPTION_IN_PAGE_ERROR, ex_code_enums);
  ADD_INT_C(EXCEPTION_ILLEGAL_INSTRUCTION, ex_code_enums);
  ADD_INT_C(EXCEPTION_NONCONTINUABLE_EXCEPTION, ex_code_enums);
  ADD_INT_C(EXCEPTION_STACK_OVERFLOW, ex_code_enums);
  ADD_INT_C(EXCEPTION_INVALID_DISPOSITION, ex_code_enums);
  ADD_INT_C(EXCEPTION_GUARD_PAGE, ex_code_enums);
  ADD_INT_C(EXCEPTION_INVALID_HANDLE, ex_code_enums);
  ADD_INT_C(CONTROL_C_EXIT, ex_code_enums);
  ADD_INT_C(VS2008_THREAD_INFO, ex_code_enums);

  START_STRUCT_DEF(EXCEPTION_RECORD);
  DEF_ENUM_FIELD(ExceptionCode, ex_code_enums);
  DEF_ENUM_FIELD(ExceptionFlags, "0:EXCEPTION_CONTINUABLE,1:EXCEPTION_NONCONTINUABLE");
  DEF_PTR_FIELD(ExceptionAddress);
  STOP_STRUCT_DEF;


  START_STRUCT_DEF(EXIT_PROCESS_DEBUG_INFO);
  DEF_INT_FIELD(dwExitCode);
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(EXIT_THREAD_DEBUG_INFO);
  DEF_INT_FIELD(dwExitCode);
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(LOAD_DLL_DEBUG_INFO);
  DEF_HANDLE_FIELD(hFile);
  DEF_PTR_FIELD(lpBaseOfDll);
  DEF_INT_FIELD(dwDebugInfoFileOffset);
  DEF_INT_FIELD(nDebugInfoSize);
//  DEF_USTR_FIELD(lpImageName, fUnicode);
  DEF_INT_FIELD(lpImageName);  //TODO: test only!

  STOP_STRUCT_DEF;

  START_STRUCT_DEF(OUTPUT_DEBUG_STRING_INFO);
  DEF_USTR_FIELD(lpDebugStringData, fUnicode);
  DEF_INT_FIELD(nDebugStringLength);
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(RIP_INFO);
  DEF_INT_FIELD(dwError);
  DEF_ENUM_FIELD(dwType, "1:SLE_ERROR,2:SLE_MINORERROR,3:SLE_WARNING");
  STOP_STRUCT_DEF;

  START_STRUCT_DEF(UNLOAD_DLL_DEBUG_INFO);
  DEF_PTR_FIELD(lpBaseOfDll);
  STOP_STRUCT_DEF;

  if (1 ) {
    std::string protect_enums;
    ADD_INT_C(PAGE_EXECUTE, protect_enums);
    ADD_INT_C(PAGE_EXECUTE_READ, protect_enums);
    ADD_INT_C(PAGE_EXECUTE_READWRITE, protect_enums);
    ADD_INT_C(PAGE_EXECUTE_WRITECOPY, protect_enums);
    ADD_INT_C(PAGE_NOACCESS, protect_enums);
    ADD_INT_C(PAGE_READONLY, protect_enums);
    ADD_INT_C(PAGE_READWRITE, protect_enums);
    ADD_INT_C(PAGE_WRITECOPY, protect_enums);
    ADD_INT_C(PAGE_GUARD, protect_enums);
    ADD_INT_C(PAGE_NOCACHE, protect_enums);
    ADD_INT_C(PAGE_WRITECOMBINE, protect_enums);

    std::string state_enums;
    ADD_INT_C(MEM_COMMIT, state_enums);
    ADD_INT_C(MEM_FREE, state_enums);
    ADD_INT_C(MEM_RESERVE, state_enums);

    std::string type_enums;
    ADD_INT_C(MEM_IMAGE, state_enums);
    ADD_INT_C(MEM_MAPPED, state_enums);
    ADD_INT_C(MEM_PRIVATE, state_enums);

    START_STRUCT_DEF(MEMORY_BASIC_INFORMATION32);
    DEF_PTR_FIELD(BaseAddress);
    DEF_PTR_FIELD(AllocationBase);
    DEF_ENUM_FIELD(AllocationProtect, protect_enums);
    DEF_INT_FIELD(RegionSize);
    DEF_ENUM_FIELD(State, state_enums);
    DEF_ENUM_FIELD(Protect, protect_enums);
    DEF_ENUM_FIELD(Type, type_enums);
    STOP_STRUCT_DEF;
  }

  std::string ContextFlags_enums;
  ADD_INT_C(CONTEXT_CONTROL, ContextFlags_enums);
  ADD_INT_C(CONTEXT_INTEGER, ContextFlags_enums);
  ADD_INT_C(CONTEXT_SEGMENTS, ContextFlags_enums);
  ADD_INT_C(CONTEXT_FLOATING_POINT, ContextFlags_enums);
  ADD_INT_C(CONTEXT_DEBUG_REGISTERS, ContextFlags_enums);

#ifdef _WIN64
  START_STRUCT_DEF(CONTEXT);
    DEF_ENUM_FIELD(ContextFlags, ContextFlags_enums);
    DEF_INT_FIELD(Dr0);
    DEF_INT_FIELD(Dr1);
    DEF_INT_FIELD(Dr2);
    DEF_INT_FIELD(Dr3);
    DEF_INT_FIELD(Dr6);
    DEF_INT_FIELD(Dr7);
    DEF_INT_FIELD(SegGs);
    DEF_INT_FIELD(SegFs);
    DEF_INT_FIELD(SegEs);
    DEF_INT_FIELD(SegDs);
    DEF_INT_FIELD(Rdi);
    DEF_INT_FIELD(Rsi);
    DEF_INT_FIELD(Rbx);
    DEF_INT_FIELD(Rdx);
    DEF_INT_FIELD(Rcx);
    DEF_INT_FIELD(Rax);
    DEF_INT_FIELD(Rbp);
    DEF_PTR_FIELD(Rip);
    DEF_INT_FIELD(SegCs);
    DEF_INT_FIELD(EFlags);
    DEF_INT_FIELD(Rsp);
    DEF_INT_FIELD(SegSs);
    DEF_INT_FIELD(R8);
    DEF_INT_FIELD(R9);
    DEF_INT_FIELD(R10);
    DEF_INT_FIELD(R11);
    DEF_INT_FIELD(R12);
    DEF_INT_FIELD(R13);
    DEF_INT_FIELD(R14);
    DEF_INT_FIELD(R15);
  STOP_STRUCT_DEF;
#else
  START_STRUCT_DEF(CONTEXT);
    DEF_ENUM_FIELD(ContextFlags, ContextFlags_enums);
    DEF_INT_FIELD(Dr0);
    DEF_INT_FIELD(Dr1);
    DEF_INT_FIELD(Dr2);
    DEF_INT_FIELD(Dr3);
    DEF_INT_FIELD(Dr6);
    DEF_INT_FIELD(Dr7);
    DEF_INT_FIELD(SegGs);
    DEF_INT_FIELD(SegFs);
    DEF_INT_FIELD(SegEs);
    DEF_INT_FIELD(SegDs);
    DEF_INT_FIELD(Edi);
    DEF_INT_FIELD(Esi);
    DEF_INT_FIELD(Ebx);
    DEF_INT_FIELD(Edx);
    DEF_INT_FIELD(Ecx);
    DEF_INT_FIELD(Eax);
    DEF_INT_FIELD(Ebp);
    DEF_PTR_FIELD(Eip);
    DEF_INT_FIELD(SegCs);
    DEF_INT_FIELD(EFlags);
    DEF_INT_FIELD(Esp);
    DEF_INT_FIELD(SegSs);
  STOP_STRUCT_DEF;
#endif

  initialized = true;
}
}  // namespace debug

namespace {
json::Object* CreateFromEXCEPTION_RECORD(const EXCEPTION_RECORD& er) {
  json::Object* obj = CreateFromStruct(&er, "EXCEPTION_RECORD", loc_DEBUG_EVENT_struct_defs);
  //TODO: implement
  return obj;
}
}  // namespace

