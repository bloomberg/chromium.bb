/* libs/graphics/ports/SkFontHost_fontconfig_direct.cpp
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

#include "SkFontHost_fontconfig_direct.h"

#include <unistd.h>
#include <fcntl.h>

#include <fontconfig/fontconfig.h>

#include "unicode/utf16.h"

namespace {

// Equivalence classes, used to match the Liberation and Ascender fonts
// with their metric-compatible replacements.  See the discussion in
// GetFontEquivClass().
enum FontEquivClass
{
    OTHER,
    SANS,
    SERIF,
    MONO
};

// Match the font name against a whilelist of fonts, returning the equivalence
// class.
FontEquivClass GetFontEquivClass(const char* fontname)
{
    // It would be nice for fontconfig to tell us whether a given suggested
    // replacement is a "strong" match (that is, an equivalent font) or
    // a "weak" match (that is, fontconfig's next-best attempt at finding a
    // substitute).  However, I played around with the fontconfig API for
    // a good few hours and could not make it reveal this information.
    //
    // So instead, we hardcode.  Initially this function emulated
    //   /etc/fonts/conf.d/30-metric-aliases.conf
    // from my Ubuntu system, but we're better off being very conservative.

    // "Ascender Sans", "Ascender Serif" and "Ascender Sans Mono" are the
    // tentative names of another set of fonts metric-compatible with
    // Arial, Times New Roman and Courier New  with a character repertoire
    // much larger than Liberation. Note that Ascender Sans Mono
    // is metrically compatible with Courier New, but the former
    // is sans-serif while ther latter is serif.
    // Arimo, Tinos and Cousine are the names of new fonts derived from and
    // expanded upon Ascender Sans, Ascender Serif and Ascender Sans Mono.
    if (strcasecmp(fontname, "Arial") == 0 ||
        strcasecmp(fontname, "Liberation Sans") == 0 ||
        strcasecmp(fontname, "Arimo") == 0 ||
        strcasecmp(fontname, "Ascender Sans") == 0) {
        return SANS;
    } else if (strcasecmp(fontname, "Times New Roman") == 0 ||
               strcasecmp(fontname, "Liberation Serif") == 0 ||
               strcasecmp(fontname, "Tinos") == 0 ||
               strcasecmp(fontname, "Ascender Serif") == 0) {
        return SERIF;
    } else if (strcasecmp(fontname, "Courier New") == 0 ||
               strcasecmp(fontname, "Cousine") == 0 ||
               strcasecmp(fontname, "Ascender Sans Mono") == 0) {
        return MONO;
    }
    return OTHER;
}


// Return true if |font_a| and |font_b| are visually and at the metrics
// level interchangeable.
bool IsMetricCompatibleReplacement(const char* font_a, const char* font_b)
{
    FontEquivClass class_a = GetFontEquivClass(font_a);
    FontEquivClass class_b = GetFontEquivClass(font_b);

    return class_a != OTHER && class_a == class_b;
}

inline unsigned FileFaceIdToFileId(unsigned filefaceid)
{
  return filefaceid >> 4;
}

inline unsigned FileIdAndFaceIndexToFileFaceId(unsigned fileid, int face_index)
{
  SkASSERT((face_index & 0xfu) == face_index);
  return (fileid << 4) | face_index;
}

}  // anonymous namespace

FontConfigDirect::FontConfigDirect()
    : next_file_id_(0) {
  FcInit();
}

FontConfigDirect::~FontConfigDirect() {
}

// -----------------------------------------------------------------------------
// Normally we only return exactly the font asked for. In last-resort
// cases, the request either doesn't specify a font or is one of the
// basic font names like "Sans", "Serif" or "Monospace". This function
// tells you whether a given request is for such a fallback.
// -----------------------------------------------------------------------------
static bool IsFallbackFontAllowed(const std::string& family)
{
    const char* family_cstr = family.c_str();
    return family.empty() ||
           strcasecmp(family_cstr, "sans") == 0 ||
           strcasecmp(family_cstr, "serif") == 0 ||
           strcasecmp(family_cstr, "monospace") == 0;
}

bool FontConfigDirect::Match(std::string* result_family,
                             unsigned* result_filefaceid,
                             bool filefaceid_valid, unsigned filefaceid,
                             const std::string& family,
                             const void* data, size_t characters_bytes,
                             bool* is_bold, bool* is_italic) {
    if (family.length() > kMaxFontFamilyLength)
        return false;

    SkAutoMutexAcquire ac(mutex_);
    FcPattern* pattern = FcPatternCreate();

    if (filefaceid_valid) {
        const std::map<unsigned, std::string>::const_iterator
            i = fileid_to_filename_.find(FileFaceIdToFileId(filefaceid));
        if (i == fileid_to_filename_.end()) {
            FcPatternDestroy(pattern);
            return false;
        }
        int face_index = filefaceid & 0xfu;
        FcPatternAddString(pattern, FC_FILE,
            reinterpret_cast<const FcChar8*>(i->second.c_str()));
        // face_index is added only when family is empty because it is not
        // necessary to uniquiely identify a font if both file and
        // family are given.
        if (family.empty())
            FcPatternAddInteger(pattern, FC_INDEX, face_index);
    }
    if (!family.empty()) {
        FcPatternAddString(pattern, FC_FAMILY, (FcChar8*) family.c_str());
    }

    FcCharSet* charset = NULL;
    if (data) {
        charset = FcCharSetCreate();
        const uint16_t* chars = (const uint16_t*) data;
        size_t num_chars = characters_bytes / 2;
        for (size_t i = 0; i < num_chars; ++i) {
            if (U16_IS_SURROGATE(chars[i])
                && U16_IS_SURROGATE_LEAD(chars[i])
                && i != num_chars - 1
                && U16_IS_TRAIL(chars[i + 1])) {
                FcCharSetAddChar(charset, U16_GET_SUPPLEMENTARY(chars[i], chars[i+1]));
                i++;
            } else {
                FcCharSetAddChar(charset, chars[i]);
            }
        }
        FcPatternAddCharSet(pattern, FC_CHARSET, charset);
        FcCharSetDestroy(charset);  // pattern now owns it.
    }

    FcPatternAddInteger(pattern, FC_WEIGHT,
                        is_bold && *is_bold ? FC_WEIGHT_BOLD
                                            : FC_WEIGHT_NORMAL);
    FcPatternAddInteger(pattern, FC_SLANT,
                        is_italic && *is_italic ? FC_SLANT_ITALIC
                                                : FC_SLANT_ROMAN);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    // Font matching:
    // CSS often specifies a fallback list of families:
    //    font-family: a, b, c, serif;
    // However, fontconfig will always do its best to find *a* font when asked
    // for something so we need a way to tell if the match which it has found is
    // "good enough" for us. Otherwise, we can return NULL which gets piped up
    // and lets WebKit know to try the next CSS family name. However, fontconfig
    // configs allow substitutions (mapping "Arial -> Helvetica" etc) and we
    // wish to support that.
    //
    // Thus, if a specific family is requested we set @family_requested. Then we
    // record two strings: the family name after config processing and the
    // family name after resolving. If the two are equal, it's a good match.
    //
    // So consider the case where a user has mapped Arial to Helvetica in their
    // config.
    //    requested family: "Arial"
    //    post_config_family: "Helvetica"
    //    post_match_family: "Helvetica"
    //      -> good match
    //
    // and for a missing font:
    //    requested family: "Monaco"
    //    post_config_family: "Monaco"
    //    post_match_family: "Times New Roman"
    //      -> BAD match
    //
    // However, we special-case fallback fonts; see IsFallbackFontAllowed().
    FcChar8* post_config_family;
    FcPatternGetString(pattern, FC_FAMILY, 0, &post_config_family);

    FcResult result;
    FcFontSet* font_set = FcFontSort(0, pattern, 0, 0, &result);
    if (!font_set) {
        FcPatternDestroy(pattern);
        return false;
    }

    // Older versions of fontconfig have a bug where they cannot select
    // only scalable fonts so we have to manually filter the results.
    FcPattern* match = NULL;
    for (int i = 0; i < font_set->nfont; ++i) {
      FcPattern* current = font_set->fonts[i];
      FcBool is_scalable;

      if (FcPatternGetBool(current, FC_SCALABLE, 0,
                           &is_scalable) != FcResultMatch ||
          !is_scalable) {
        continue;
      }

      // fontconfig can also return fonts which are unreadable
      FcChar8* c_filename;
      if (FcPatternGetString(current, FC_FILE, 0, &c_filename) != FcResultMatch)
        continue;

      if (access(reinterpret_cast<char*>(c_filename), R_OK) != 0)
        continue;

      match = current;
      break;
    }

    if (!match) {
      FcPatternDestroy(pattern);
      FcFontSetDestroy(font_set);
      return false;
    }

    if (!IsFallbackFontAllowed(family)) {
      bool acceptable_substitute = false;
      for (int id = 0; id < 255; ++id) {
        FcChar8* post_match_family;
        if (FcPatternGetString(match, FC_FAMILY, id, &post_match_family) !=
            FcResultMatch)
          break;
        acceptable_substitute =
          (strcasecmp((char *)post_config_family,
                      (char *)post_match_family) == 0 ||
           // Workaround for Issue 12530:
           //   requested family: "Bitstream Vera Sans"
           //   post_config_family: "Arial"
           //   post_match_family: "Bitstream Vera Sans"
           // -> We should treat this case as a good match.
           strcasecmp(family.c_str(),
                      (char *)post_match_family) == 0) ||
           IsMetricCompatibleReplacement(family.c_str(),
                                         (char*)post_match_family);
        if (acceptable_substitute)
          break;
      }
      if (!acceptable_substitute) {
        FcPatternDestroy(pattern);
        FcFontSetDestroy(font_set);
        return false;
      }
    }

    FcPatternDestroy(pattern);

    FcChar8* c_filename;
    if (FcPatternGetString(match, FC_FILE, 0, &c_filename) != FcResultMatch) {
        FcFontSetDestroy(font_set);
        return false;
    }
    int face_index;
    if (FcPatternGetInteger(match, FC_INDEX, 0, &face_index) != FcResultMatch) {
        FcFontSetDestroy(font_set);
        return false;
    }
    const std::string filename(reinterpret_cast<char*>(c_filename));

    unsigned out_filefaceid;
    if (filefaceid_valid) {
        out_filefaceid = filefaceid;
    } else {
        unsigned out_fileid;
        const std::map<std::string, unsigned>::const_iterator
            i = filename_to_fileid_.find(filename);
        if (i == filename_to_fileid_.end()) {
            out_fileid = next_file_id_++;
            filename_to_fileid_[filename] = out_fileid;
            fileid_to_filename_[out_fileid] = filename;
        } else {
            out_fileid = i->second;
        }
        // fileid stored in filename_to_fileid_ and fileid_to_filename_ is
        // unique only up to the font file. We have to encode face_index for
        // the out param.
        out_filefaceid = FileIdAndFaceIndexToFileFaceId(out_fileid, face_index);
    }

    if (result_filefaceid)
        *result_filefaceid = out_filefaceid;

    FcChar8* c_family;
    if (FcPatternGetString(match, FC_FAMILY, 0, &c_family)) {
        FcFontSetDestroy(font_set);
        return false;
    }

    int resulting_bold;
    if (FcPatternGetInteger(match, FC_WEIGHT, 0, &resulting_bold))
      resulting_bold = FC_WEIGHT_NORMAL;

    int resulting_italic;
    if (FcPatternGetInteger(match, FC_SLANT, 0, &resulting_italic))
      resulting_italic = FC_SLANT_ROMAN;

    // If we ask for an italic font, fontconfig might take a roman font and set
    // the undocumented property FC_MATRIX to a skew matrix. It'll then say
    // that the font is italic or oblique. So, if we see a matrix, we don't
    // believe that it's italic.
    FcValue matrix;
    const bool have_matrix = FcPatternGet(match, FC_MATRIX, 0, &matrix) == 0;

    // If we ask for an italic font, fontconfig might take a roman font and set
    // FC_EMBOLDEN.
    FcValue embolden;
    const bool have_embolden =
        FcPatternGet(match, FC_EMBOLDEN, 0, &embolden) == 0;

    if (is_bold)
      *is_bold = resulting_bold > FC_WEIGHT_MEDIUM && !have_embolden;
    if (is_italic)
      *is_italic = resulting_italic > FC_SLANT_ROMAN && !have_matrix;

    if (result_family)
        *result_family = (char *) c_family;

    FcFontSetDestroy(font_set);

    return true;
}

int FontConfigDirect::Open(unsigned filefaceid) {
    SkAutoMutexAcquire ac(mutex_);
    const std::map<unsigned, std::string>::const_iterator
        i = fileid_to_filename_.find(FileFaceIdToFileId(filefaceid));
    if (i == fileid_to_filename_.end())
        return -1;

    return open(i->second.c_str(), O_RDONLY);
}
