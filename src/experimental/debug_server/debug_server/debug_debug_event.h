#pragma once
#include <string>
#include <windows.h>
#include "my_json.h"
#include "my_struct_to_json.h"

#define VS2008_THREAD_INFO 0x406D1388

namespace debug {
  void DEBUG_EVENT_ToJSON(DEBUG_EVENT de, std::string* text_out);
  json::Value* DEBUG_EVENT_ToJSON(DEBUG_EVENT de);
  json::Value* CONTEXT_ToJSON(CONTEXT ct);
  json::Value* MEMORY_BASIC_INFORMATION32_ToJSON(MEMORY_BASIC_INFORMATION32 mbi);

  extern json::StructDefinitions* DEBUG_EVENT_struct_defs;
  void DEBUG_EVENT_ToJSON_Init();

}  // namespace debug