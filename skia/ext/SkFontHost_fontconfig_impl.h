/* libs/graphics/ports/SkFontHost_fontconfig_impl.h
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

/* The SkFontHost_fontconfig code requires an implementation of an abstact
 * fontconfig interface. We do this because sometimes fontconfig is not
 * directly availible and this provides an ability to change the fontconfig
 * implementation at run-time.
 */

#ifndef FontConfigInterface_DEFINED
#define FontConfigInterface_DEFINED
#pragma once

#include <string>

class FontConfigInterface {
  public:
    virtual ~FontConfigInterface() { }

    /** Performs config match
     *
     *  @param result_family (output) on success, the resulting family name.
     *  @param result_filefaceid (output) on success, the resulting fileface id.
     *  @param filefaceid_valid if true, then |filefaceid| is valid
     *  @param filefaceid the filefaceid (as returned by this function)
     *         which we are trying to match.
     *  @param family (optional) the family of the font that we are trying to
     *    match. If the length of the |family| is greater then
     *    kMaxFontFamilyLength, this function should immediately return false.
     *  @param characters (optional) UTF-16 characters the font must cover.
     *  @param characters_bytes (optional) number of bytes in |characters|
     *  @param is_bold (optional, set to NULL to ignore, in/out)
     *  @param is_italic (optional, set to NULL to ignore, in/out)
     *  @return true iff successful.
     *  Note that |filefaceid| uniquely identifies <font file, face_index) :
     *  system font: filefaceid =
     *                   (fileid(unique per font file) << 4 | face_index)
     *  remote font: filefaceid = fileid
     */
    virtual bool Match(
          std::string* result_family,
          unsigned* result_filefaceid,
          bool filefaceid_valid,
          unsigned filefaceid,
          const std::string& family,
          const void* characters,
          size_t characters_bytes,
          bool* is_bold,
          bool* is_italic) = 0;

    /** Open a font file given the filefaceid as returned by Match.
     */
    virtual int Open(unsigned filefaceid) = 0;

    static const unsigned kMaxFontFamilyLength = 2048;
};

#endif  // FontConfigInterface_DEFINED
