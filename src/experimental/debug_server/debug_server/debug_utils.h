#pragma once
#include <string>
#include <windows.h>
#include <Psapi.h>
#include <Winternl.h>
#include <Tlhelp32.h>

namespace debug {
class Utils {
 public:
  static bool GetProcessName(int pid, std::string* name);
  static bool GetProcessName(HANDLE ProcessHandle, std::string* name);
  static bool GetProcessCmdLine(HANDLE ProcessHandle, std::string* cmd_line);
  static int GetProcessorWordSizeInBits(HANDLE h);
  static std::string ReadUNICODE_STRING(HANDLE ProcessHandle, const UNICODE_STRING& str);
};
}  // namespace debug
