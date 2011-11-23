/* libs/graphics/ports/SkFontHost_fontconfig_direct.h
**
** Copyright 2009, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef FontConfigDirect_DEFINED
#define FontConfigDirect_DEFINED
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "SkThread.h"
#include "SkFontHost_fontconfig_impl.h"

class SK_API FontConfigDirect : public FontConfigInterface {
 public:
  FontConfigDirect();
  virtual ~FontConfigDirect();

  // FontConfigInterface implementation. Thread safe.
  virtual bool Match(std::string* result_family, unsigned* result_filefaceid,
                     bool filefaceid_valid, unsigned filefaceid,
                     const std::string& family,
                     const void* characters, size_t characters_bytes,
                     bool* is_bold, bool* is_italic) OVERRIDE;
  virtual int Open(unsigned filefaceid) OVERRIDE;

 private:
  SkMutex mutex_;
  // fileid stored in two maps below are unique per font file.
  std::map<unsigned, std::string> fileid_to_filename_;
  std::map<std::string, unsigned> filename_to_fileid_;
  unsigned next_file_id_;
};

#endif  // FontConfigDirect_DEFINED
