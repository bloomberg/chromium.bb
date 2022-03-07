/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef CONTENT_PUBLIC_BROWSER_SPELLCHECK_DATA_H_
#define CONTENT_PUBLIC_BROWSER_SPELLCHECK_DATA_H_

#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace content {

// An instance of SpellcheckData can be attached to the browser-context.
class CONTENT_EXPORT SpellcheckData : public base::SupportsUserData::Data {
 public:
  // Interface to implement for observing changes.
  class Observer {
   public:
    // Called when custom words are added or removed.
    virtual void OnCustomWordsChanged(
        const std::vector<base::StringPiece>& words_added,
        const std::vector<base::StringPiece>& words_removed) = 0;
  };

  // Creates a SpellcheckData object and attaches it to the specified context.
  // If an instance is already attached, this method does nothing.
  static void CreateForContext(base::SupportsUserData* context);

  // Retrieves the SpellcheckData instance that was attached to the specified
  // context (via CreateForContext above) and returns it.  If no instance of
  // SpellcheckData was attached, returns NULL.
  static SpellcheckData* FromContext(base::SupportsUserData* context);
  static const SpellcheckData* FromContext(
      const base::SupportsUserData* context);

 public:
  SpellcheckData();
  ~SpellcheckData() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void AdjustCustomWords(const std::vector<base::StringPiece>& to_add,
                         const std::vector<base::StringPiece>& to_remove);

  const std::set<std::string>& custom_words() const { return custom_words_; }

 private:
  typedef std::set<std::string> CustomWordsSet;

  CustomWordsSet custom_words_;
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckData);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPELLCHECK_DATA_H_
