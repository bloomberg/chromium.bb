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

#include "content/public/browser/spellcheck_data.h"

namespace content {
namespace {

static const int kUserDataKey = 0;

}  // namespace

// static
void SpellcheckData::CreateForContext(base::SupportsUserData* context) {
  DCHECK(context);
  if (!FromContext(context))
    context->SetUserData(&kUserDataKey,
                         std::unique_ptr<SpellcheckData>(new SpellcheckData()));
}

// static
SpellcheckData* SpellcheckData::FromContext(base::SupportsUserData* context) {
  DCHECK(context);
  return static_cast<SpellcheckData*>(context->GetUserData(&kUserDataKey));
}

// static
const SpellcheckData* SpellcheckData::FromContext(
    const base::SupportsUserData* context) {
  DCHECK(context);
  return static_cast<const SpellcheckData*>(
      context->GetUserData(&kUserDataKey));
}

SpellcheckData::SpellcheckData() {
}

SpellcheckData::~SpellcheckData() {
}

void SpellcheckData::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SpellcheckData::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SpellcheckData::AdjustCustomWords(
    const std::vector<base::StringPiece>& to_add,
    const std::vector<base::StringPiece>& to_remove) {
  for (size_t i = 0; i < to_add.size(); ++i) {
    custom_words_.insert(to_add[i].as_string());
  }
  for (size_t i = 0; i < to_remove.size(); ++i) {
      CustomWordsSet::iterator it =
          custom_words_.find(to_remove[i].as_string());
      if (it != custom_words_.end()) {
        custom_words_.erase(it);
      }
  }

  for (auto& observer : observers_)
    observer.OnCustomWordsChanged(to_add, to_remove);
}

}  // namespace content
