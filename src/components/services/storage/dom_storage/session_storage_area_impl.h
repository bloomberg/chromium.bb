// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_AREA_IMPL_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_AREA_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace storage {

class SessionStorageDataMap;

// This class provides session storage access to the renderer by binding to the
// StorageArea mojom interface. It represents the data stored for a
// namespace-origin area.
//
// This class delegates calls to SessionStorageDataMap objects, and can share
// them with other SessionStorageLevelDBImpl instances to support shallow
// cloning (copy-on-write). This should be done through the |Clone()| method and
// not manually.
//
// Unlike regular observers, adding an observer to this class only notifies when
// the whole area is deleted manually (not by using DeleteAll on this binding).
// This can happen when clearing browser data.
//
// During forking, this class is responsible for dealing with moving its
// observers from the SessionStorageDataMap's StorageArea to the new forked
// SessionStorageDataMap's StorageArea.
class SessionStorageAreaImpl : public blink::mojom::StorageArea {
 public:
  using RegisterNewAreaMap =
      base::RepeatingCallback<scoped_refptr<SessionStorageMetadata::MapData>(
          SessionStorageMetadata::NamespaceEntry namespace_entry,
          const url::Origin& origin)>;

  // Creates a area for the given |namespace_entry|-|origin| data area. All
  // StorageArea calls are delegated to the |data_map|. The
  // |register_new_map_callback| is called when a shared |data_map| needs to be
  // forked for the copy-on-write behavior and a new map needs to be registered.
  SessionStorageAreaImpl(SessionStorageMetadata::NamespaceEntry namespace_entry,
                         url::Origin origin,
                         scoped_refptr<SessionStorageDataMap> data_map,
                         RegisterNewAreaMap register_new_map_callback);
  ~SessionStorageAreaImpl() override;

  // Creates a shallow copy clone for the new namespace entry.
  // This doesn't change the refcount of the underlying map - that operation
  // must be done using
  // SessionStorageMetadata::RegisterShallowClonedNamespace.
  std::unique_ptr<SessionStorageAreaImpl> Clone(
      SessionStorageMetadata::NamespaceEntry namespace_entry);

  void Bind(mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  bool IsBound() const { return receiver_.is_bound(); }

  SessionStorageDataMap* data_map() { return shared_data_map_.get(); }

  // Notifies all observers that this area was deleted.
  void NotifyObserversAllDeleted();

  // blink::mojom::StorageArea:
  void AddObserver(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> observer) override;
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const base::Optional<std::vector<uint8_t>>& client_old_value,
           const std::string& source,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              const base::Optional<std::vector<uint8_t>>& client_old_value,
              const std::string& source,
              DeleteCallback callback) override;
  void DeleteAll(
      const std::string& source,
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      DeleteAllCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetAll(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      GetAllCallback callback) override;

  void FlushForTesting();

 private:
  void OnConnectionError();
  void OnGetAllResult(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      GetAllCallback callback,
      std::vector<blink::mojom::KeyValuePtr> entries);
  void OnDeleteAllResult(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      DeleteAllCallback callback,
      bool success);

  enum class NewMapType { FORKED, EMPTY_FROM_DELETE_ALL };

  void CreateNewMap(NewMapType map_type,
                    const base::Optional<std::string>& delete_all_source);

  SessionStorageMetadata::NamespaceEntry namespace_entry_;
  url::Origin origin_;
  scoped_refptr<SessionStorageDataMap> shared_data_map_;
  RegisterNewAreaMap register_new_map_callback_;

  mojo::RemoteSet<blink::mojom::StorageAreaObserver> observers_;
  mojo::Receiver<blink::mojom::StorageArea> receiver_{this};

  base::WeakPtrFactory<SessionStorageAreaImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionStorageAreaImpl);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_AREA_IMPL_H_
