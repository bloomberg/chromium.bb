/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_PREFSTORE_H
#define INCLUDED_BLPWTK2_PREFSTORE_H

#include <blpwtk2_config.h>

#include <base/observer_list.h>
#include <components/prefs/persistent_pref_store.h>
#include <components/prefs/pref_value_map.h>

namespace blpwtk2 {

                        // ===============
                        // class PrefStore
                        // ===============

class PrefStore : public PersistentPrefStore {
    void OnInitializationCompleted();

    base::ObserverList<PrefStore::Observer,
                       true,
                       true,
                       base::internal::UncheckedObserverAdapter>
        d_observers;
    PrefValueMap d_prefs;

   public:
    PrefStore();

    // PrefStore
    void AddObserver(PrefStore::Observer* observer) override;
    void RemoveObserver(PrefStore::Observer* observer) override;
    bool HasObservers() const override;
    bool IsInitializationComplete() const override;
    bool GetValue(const std::string& key,
                  const base::Value** result) const override;
    std::unique_ptr<base::DictionaryValue> GetValues() const override;

    // PersistentPrefStore
    bool GetMutableValue(const std::string& key,
                         base::Value** result) override;
    void SetValue(const std::string& key,
                  std::unique_ptr<base::Value> value,
                  uint32_t flags) override;
    void SetValueSilently(const std::string& key,
                          std::unique_ptr<base::Value> value,
                          uint32_t flags) override;
    void RemoveValue(const std::string& key, uint32_t flags) override;
    bool ReadOnly() const override;
    PrefReadError GetReadError() const override;
    PrefReadError ReadPrefs() override;
    void ReadPrefsAsync(ReadErrorDelegate* delegate) override;
    void SchedulePendingLossyWrites() override;
    void ClearMutableValues() override;
    void ReportValueChanged(const std::string& key, uint32_t flags) override;
    void OnStoreDeletionFromDisk() override;

private:
    ~PrefStore() final;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_PREFSTORE_H
