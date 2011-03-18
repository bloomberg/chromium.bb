#include "StdAfx.h"
#include "debug_utils.h"

namespace {
HMODULE sm_LoadNTDLLFunctions();
typedef NTSTATUS (NTAPI *pfnNtQueryInformationProcess)(
    IN  HANDLE ProcessHandle,
    IN  PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN  ULONG ProcessInformationLength,
    OUT PULONG ReturnLength    OPTIONAL
    );
pfnNtQueryInformationProcess gNtQueryInformationProcess = 0;
}

namespace debug {
bool Utils::GetProcessCmdLine(HANDLE ProcessHandle, std::string* cmd_line) {
  if (!gNtQueryInformationProcess)
    sm_LoadNTDLLFunctions();

  if (gNtQueryInformationProcess) {
    PROCESS_BASIC_INFORMATION pbi;
    memset(&pbi, 0, sizeof(pbi));
    ULONG ReturnLength = 0;
    NTSTATUS st = (*gNtQueryInformationProcess)(ProcessHandle, ProcessBasicInformation, &pbi, sizeof(pbi), &ReturnLength);

    PEB* peb_addr = (PEB*)pbi.PebBaseAddress;
    PEB peb;
    memset(&peb, 0, sizeof(peb));

    SIZE_T sz = 0;
    if (!::ReadProcessMemory(ProcessHandle, peb_addr, &peb, sizeof(peb), &sz))
      return false;
    
#ifdef _WIN64
    RTL_USER_PROCESS_PARAMETERS* proc_params_addr = peb.ProcessParameters;
    RTL_USER_PROCESS_PARAMETERS proc_params;
    memset(&proc_params, 0, sizeof(proc_params));
    if (!::ReadProcessMemory(ProcessHandle, proc_params_addr, &proc_params, sizeof(proc_params), &sz))
      return false;

    *cmd_line = ReadUNICODE_STRING(ProcessHandle, proc_params.CommandLine);
    return true;
#endif
  }
  return false;
}

std::string Utils::ReadUNICODE_STRING(HANDLE ProcessHandle, const UNICODE_STRING& str) {
  std::string result;
  if (str.Length > 0) {
    size_t len_in_bytes = 2 * (str.Length + 1);
    wchar_t* w_str = (wchar_t*)malloc(len_in_bytes);
    char* a_str = (char*)malloc(str.Length + 1);
    if ((NULL != w_str) && (NULL != a_str)) {
      SIZE_T sz = 0;
      if (::ReadProcessMemory(ProcessHandle, str.Buffer, w_str, len_in_bytes, &sz)) {

        ::WideCharToMultiByte(CP_ACP, 0, w_str, -1, a_str, str.Length, 0, 0);
        a_str[str.Length] = 0;
        result = a_str;
      }
      free(w_str);
      free(a_str);
    }
  }
  return result;
}

int Utils::GetProcessorWordSizeInBits(HANDLE h) {
  BOOL is_wow = FALSE;
  if (!::IsWow64Process(h, &is_wow))
    return 0;
  if (is_wow)
    return 32;
#ifdef _WIN64
  return 64;
#else
  return 32;
#endif
  return 0;
}

bool Utils::GetProcessName(int pid, std::string* name) {
  HANDLE h_snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (INVALID_HANDLE_VALUE == h_snapshot)
    return false;

  PROCESSENTRY32 pe = { 0 };
  pe.dwSize = sizeof(PROCESSENTRY32);

  bool found = false;
  if (::Process32First(h_snapshot, &pe)) {
    do {
      if (pe.th32ProcessID == pid) {
        *name = pe.szExeFile;
        found = true;
      }
    } while (::Process32Next(h_snapshot, &pe));
  }
  ::CloseHandle(h_snapshot);
  return found;
}
}  // namespace debug

namespace {
HMODULE sm_LoadNTDLLFunctions() {
  HMODULE hNtDll = LoadLibrary(_T("ntdll.dll"));
  if(hNtDll == NULL) return NULL;

  gNtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(hNtDll,
                                                        "NtQueryInformationProcess");
  if(gNtQueryInformationProcess == NULL) {
    FreeLibrary(hNtDll);
    return NULL;
  }
  return hNtDll;
}
}  // namespace
