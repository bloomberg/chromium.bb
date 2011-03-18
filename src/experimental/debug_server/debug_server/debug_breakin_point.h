#pragma once
#include <string>
#include "..\common\debug_blob.h"

namespace debug {
class DebuggeeProcess;

class AppBreakinPoint {
 public:
  AppBreakinPoint(const char* app_name,
                 const void* sinature,
                 size_t sinature_sz,
                 signed long start_offset_from_signature)
    : app_name_(app_name),
      signature_(sinature, sinature_sz),
      start_offset_from_signature_(start_offset_from_signature) {
  }

  bool IsNexeThread(DebuggeeProcess* proc, void** mem_base); 

 public:
  std::string app_name_;
  Blob signature_;
  signed long start_offset_from_signature_;  
};

}  // namespace debug