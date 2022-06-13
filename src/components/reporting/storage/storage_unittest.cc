// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage.h"

#include <atomic>
#include <cstdint>
#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/compression/test_compression_module.h"
#include "components/reporting/encryption/decryption.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/test_encryption_module.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_interface.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Between;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::WithoutArgs;

namespace reporting {
namespace {

// Test uploader counter - for generation of unique ids.
std::atomic<int64_t> next_uploader_id{0};

// Context of single decryption. Self-destructs upon completion or failure.
class SingleDecryptionContext {
 public:
  SingleDecryptionContext(
      const EncryptedRecord& encrypted_record,
      scoped_refptr<test::Decryptor> decryptor,
      base::OnceCallback<void(StatusOr<base::StringPiece>)> response)
      : encrypted_record_(encrypted_record),
        decryptor_(decryptor),
        response_(std::move(response)) {}

  SingleDecryptionContext(const SingleDecryptionContext& other) = delete;
  SingleDecryptionContext& operator=(const SingleDecryptionContext& other) =
      delete;

  ~SingleDecryptionContext() {
    DCHECK(!response_) << "Self-destruct without prior response";
  }

  void Start() {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&SingleDecryptionContext::RetrieveMatchingPrivateKey,
                       base::Unretained(this)));
  }

 private:
  void Respond(StatusOr<base::StringPiece> result) {
    std::move(response_).Run(result);
    delete this;
  }

  void RetrieveMatchingPrivateKey() {
    // Retrieve private key that matches public key hash.
    decryptor_->RetrieveMatchingPrivateKey(
        encrypted_record_.encryption_info().public_key_id(),
        base::BindOnce(
            [](SingleDecryptionContext* self,
               StatusOr<std::string> private_key_result) {
              if (!private_key_result.ok()) {
                self->Respond(private_key_result.status());
                return;
              }
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  base::BindOnce(&SingleDecryptionContext::DecryptSharedSecret,
                                 base::Unretained(self),
                                 private_key_result.ValueOrDie()));
            },
            base::Unretained(this)));
  }

  void DecryptSharedSecret(base::StringPiece private_key) {
    // Decrypt shared secret from private key and peer public key.
    auto shared_secret_result = decryptor_->DecryptSecret(
        private_key, encrypted_record_.encryption_info().encryption_key());
    if (!shared_secret_result.ok()) {
      Respond(shared_secret_result.status());
      return;
    }
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(&SingleDecryptionContext::OpenRecord,
                                  base::Unretained(this),
                                  shared_secret_result.ValueOrDie()));
  }

  void OpenRecord(base::StringPiece shared_secret) {
    decryptor_->OpenRecord(
        shared_secret,
        base::BindOnce(
            [](SingleDecryptionContext* self,
               StatusOr<test::Decryptor::Handle*> handle_result) {
              if (!handle_result.ok()) {
                self->Respond(handle_result.status());
                return;
              }
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  base::BindOnce(&SingleDecryptionContext::AddToRecord,
                                 base::Unretained(self),
                                 base::Unretained(handle_result.ValueOrDie())));
            },
            base::Unretained(this)));
  }

  void AddToRecord(test::Decryptor::Handle* handle) {
    handle->AddToRecord(
        encrypted_record_.encrypted_wrapped_record(),
        base::BindOnce(
            [](SingleDecryptionContext* self, test::Decryptor::Handle* handle,
               Status status) {
              if (!status.ok()) {
                self->Respond(status);
                return;
              }
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  base::BindOnce(&SingleDecryptionContext::CloseRecord,
                                 base::Unretained(self),
                                 base::Unretained(handle)));
            },
            base::Unretained(this), base::Unretained(handle)));
  }

  void CloseRecord(test::Decryptor::Handle* handle) {
    handle->CloseRecord(base::BindOnce(
        [](SingleDecryptionContext* self,
           StatusOr<base::StringPiece> decryption_result) {
          self->Respond(decryption_result);
        },
        base::Unretained(this)));
  }

 private:
  const EncryptedRecord encrypted_record_;
  const scoped_refptr<test::Decryptor> decryptor_;
  base::OnceCallback<void(StatusOr<base::StringPiece>)> response_;
};

class StorageTest
    : public ::testing::TestWithParam<::testing::tuple<bool, size_t>> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());

    // Disallow uploads unless other expectation is set (any later EXPECT_CALL
    // will take precedence over this one).
    EXPECT_CALL(set_mock_uploader_expectations_, Call(_))
        .WillRepeatedly(Invoke([](UploaderInterface::UploadReason reason) {
          return Status(
              error::UNAVAILABLE,
              base::StrCat({"Test uploader not provided by the test, reason=",
                            UploaderInterface::ReasonToString(reason)}));
        }));
    // Encryption is enabled by default.
    ASSERT_TRUE(EncryptionModuleInterface::is_enabled());
    if (is_encryption_enabled()) {
      // Generate signing key pair.
      test::GenerateSigningKeyPair(signing_private_key_,
                                   signature_verification_public_key_);
      // Create decryption module.
      auto decryptor_result = test::Decryptor::Create();
      ASSERT_OK(decryptor_result.status()) << decryptor_result.status();
      decryptor_ = std::move(decryptor_result.ValueOrDie());
      // Prepare the key.
      signed_encryption_key_ = GenerateAndSignKey();
      // First record enqueue to Storage would need key delivered.
      expect_to_need_key_ = true;
    } else {
      // Disable encryption.
      scoped_feature_list_.InitFromCommandLine(
          {}, {EncryptionModuleInterface::kEncryptedReporting});
    }
    test_compression_module_ =
        base::MakeRefCounted<test::TestCompressionModule>();
  }

  void TearDown() override {
    ResetTestStorage();
    // Make sure all memory is deallocated.
    ASSERT_THAT(GetMemoryResource()->GetUsed(), Eq(0u));
    // Make sure all disk is not reserved (files remain, but Storage is not
    // responsible for them anymore).
    ASSERT_THAT(GetDiskResource()->GetUsed(), Eq(0u));
    // Log next uploader id for possible verification.
    LOG(ERROR) << "Next uploader id=" << next_uploader_id.load();
  }

  class MockUpload : public base::RefCountedDeleteOnSequence<
                         ::testing::NiceMock<MockUpload>> {
   public:
    MockUpload(scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
               base::OnceClosure key_generation)
        : base::RefCountedDeleteOnSequence<::testing::NiceMock<MockUpload>>(
              sequenced_task_runner),
          key_generation_(std::move(key_generation)) {
      DETACH_FROM_SEQUENCE(mock_uploader_checker_);
      upload_progress_.assign("\nStart\n");
    }
    MockUpload(const MockUpload& other) = delete;
    MockUpload& operator=(const MockUpload& other) = delete;

    void KeyGeneration() { std::move(key_generation_).Run(); }

    void DoEncounterSeqId(int64_t uploader_id,
                          Priority priority,
                          int64_t sequence_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
      upload_progress_.append("SeqId: ")
          .append(base::NumberToString(sequence_id))
          .append("\n");
      EncounterSeqId(uploader_id, priority, sequence_id);
    }
    bool DoUploadRecord(int64_t uploader_id,
                        Priority priority,
                        int64_t sequence_id,
                        base::StringPiece data) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
      upload_progress_.append("Record: ")
          .append(base::NumberToString(sequence_id))
          .append(" '")
          .append(data.data(), data.size())
          .append("'\n");
      return UploadRecord(uploader_id, priority, sequence_id, data);
    }
    bool DoUploadRecordFailure(int64_t uploader_id,
                               Priority priority,
                               int64_t sequence_id,
                               Status status) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
      upload_progress_.append("Failure: ")
          .append(base::NumberToString(sequence_id))
          .append(" '")
          .append(status.ToString())
          .append("'\n");
      return UploadRecordFailure(uploader_id, priority, sequence_id, status);
    }
    bool DoUploadGap(int64_t uploader_id,
                     Priority priority,
                     int64_t sequence_id,
                     uint64_t count) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
      upload_progress_.append("Gap: ")
          .append(base::NumberToString(sequence_id))
          .append("(")
          .append(base::NumberToString(count))
          .append(")\n");
      return UploadGap(uploader_id, priority, sequence_id, count);
    }
    void DoUploadComplete(int64_t uploader_id, Status status) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
      upload_progress_.append("Complete: ")
          .append(status.ToString())
          .append("\n");
      LOG(ERROR) << "TestUploader: " << upload_progress_ << "End\n";
      UploadComplete(uploader_id, status);
    }

    MOCK_METHOD(void,
                EncounterSeqId,
                (int64_t /*uploader_id*/, Priority, int64_t),
                (const));
    MOCK_METHOD(bool,
                UploadRecord,
                (int64_t /*uploader_id*/, Priority, int64_t, base::StringPiece),
                (const));
    MOCK_METHOD(bool,
                UploadRecordFailure,
                (int64_t /*uploader_id*/, Priority, int64_t, Status),
                (const));
    MOCK_METHOD(bool,
                UploadGap,
                (int64_t /*uploader_id*/, Priority, int64_t, uint64_t),
                (const));
    MOCK_METHOD(void,
                UploadComplete,
                (int64_t /*uploader_id*/, Status),
                (const));

   protected:
    virtual ~MockUpload() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    }

   private:
    friend class base::RefCountedDeleteOnSequence<
        ::testing::NiceMock<MockUpload>>;

    base::OnceClosure key_generation_;

    SEQUENCE_CHECKER(mock_uploader_checker_);

    // Snapshot of data received in this upload (for debug purposes).
    std::string upload_progress_;
  };

  class TestUploader : public UploaderInterface {
   public:
    // Mapping of <generation id, sequencing id> to matching record digest.
    // Whenever a record is uploaded and includes last record digest, this map
    // should have that digest already recorded. Only the first record in a
    // generation is uploaded without last record digest.
    using LastRecordDigestMap =
        base::flat_map<std::tuple<Priority,
                                  int64_t /*generation id*/,
                                  int64_t /*sequencing id*/>,
                       absl::optional<std::string /*digest*/>>;

    explicit TestUploader(StorageTest* self)
        : uploader_id_(next_uploader_id.fetch_add(1)),
          last_record_digest_map_(&self->last_record_digest_map_),
          main_thread_task_runner_(self->main_thread_task_runner_),
          mock_upload_(base::MakeRefCounted<::testing::NiceMock<MockUpload>>(
              main_thread_task_runner_,
              base::BindOnce(&Storage::UpdateEncryptionKey,
                             base::Unretained(self->storage_.get()),
                             self->signed_encryption_key_))),
          decryptor_(self->decryptor_) {
      DETACH_FROM_SEQUENCE(test_uploader_checker_);
    }

    ~TestUploader() override {
      DCHECK(!mock_upload_);
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
    }

    void ProcessRecord(EncryptedRecord encrypted_record,
                       base::OnceCallback<void(bool)> processed_cb) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      DCHECK(mock_upload_);
      auto sequence_information = encrypted_record.sequence_information();
      if (!encrypted_record.has_encryption_info()) {
        // Wrapped record is not encrypted.
        WrappedRecord wrapped_record;
        ASSERT_TRUE(wrapped_record.ParseFromString(
            encrypted_record.encrypted_wrapped_record()));
        ScheduleVerifyRecord(std::move(sequence_information),
                             std::move(wrapped_record),
                             std::move(processed_cb));
        return;
      }
      // Decrypt encrypted_record.
      (new SingleDecryptionContext(
           encrypted_record, decryptor_,
           base::BindOnce(
               [](SequenceInformation sequence_information,
                  base::OnceCallback<void(bool)> processed_cb,
                  TestUploader* uploader, StatusOr<base::StringPiece> result) {
                 ASSERT_OK(result.status());
                 WrappedRecord wrapped_record;
                 ASSERT_TRUE(wrapped_record.ParseFromArray(
                     result.ValueOrDie().data(), result.ValueOrDie().size()));
                 // Verify wrapped record once decrypted.
                 uploader->ScheduleVerifyRecord(std::move(sequence_information),
                                                std::move(wrapped_record),
                                                std::move(processed_cb));
               },
               std::move(sequence_information), std::move(processed_cb),
               base::Unretained(this))))
          ->Start();
    }

    void ProcessGap(SequenceInformation sequence_information,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      DCHECK(mock_upload_);
      // Verify generation match.
      if (generation_id_.has_value() &&
          generation_id_.value() != sequence_information.generation_id()) {
        main_thread_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](SequenceInformation sequence_information,
                   int64_t uploader_id, int64_t generation_id,
                   scoped_refptr<MockUpload> mock_upload,
                   base::OnceCallback<void(bool)> processed_cb) {
                  std::move(processed_cb)
                      .Run(mock_upload->DoUploadRecordFailure(
                          uploader_id, sequence_information.priority(),
                          sequence_information.sequencing_id(),
                          Status(
                              error::DATA_LOSS,
                              base::StrCat({"Generation id mismatch, expected=",
                                            base::NumberToString(generation_id),
                                            " actual=",
                                            base::NumberToString(
                                                sequence_information
                                                    .generation_id())}))));
                },
                std::move(sequence_information), uploader_id_,
                generation_id_.value(), mock_upload_, std::move(processed_cb)));
        return;
      }
      if (!generation_id_.has_value()) {
        generation_id_ = sequence_information.generation_id();
      }

      last_record_digest_map_->emplace(
          std::make_tuple(sequence_information.priority(),
                          sequence_information.sequencing_id(),
                          sequence_information.generation_id()),
          absl::nullopt);

      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](uint64_t count, SequenceInformation sequence_information,
                 int64_t uploader_id, scoped_refptr<MockUpload> mock_upload,
                 base::OnceCallback<void(bool)> processed_cb) {
                for (uint64_t c = 0; c < count; ++c) {
                  mock_upload->DoEncounterSeqId(
                      uploader_id, sequence_information.priority(),
                      sequence_information.sequencing_id() +
                          static_cast<int64_t>(c));
                }
                std::move(processed_cb)
                    .Run(mock_upload->DoUploadGap(
                        uploader_id, sequence_information.priority(),
                        sequence_information.sequencing_id(), count));
              },
              count, std::move(sequence_information), uploader_id_,
              mock_upload_, std::move(processed_cb)));
    }

    void Completed(Status status) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      DCHECK(mock_upload_);
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MockUpload::DoUploadComplete,
                         std::move(mock_upload_),  // No longer needed.
                         uploader_id_, status));
    }

    // Helper class for setting up mock uploader expectations of a successful
    // completion.
    class SetUp {
     public:
      SetUp(Priority priority,
            test::TestCallbackWaiter* waiter,
            StorageTest* self)
          : priority_(priority),
            uploader_(std::make_unique<TestUploader>(self)),
            uploader_id_(uploader_->uploader_id_),
            waiter_(waiter) {}
      SetUp(const SetUp& other) = delete;
      SetUp& operator=(const SetUp& other) = delete;
      ~SetUp() { CHECK(!uploader_) << "Missed 'Complete' call"; }

      std::unique_ptr<TestUploader> Complete(
          Status status = Status::StatusOK()) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecordFailure(Eq(uploader_id_), _, _, _))
            .Times(0)
            .InSequence(uploader_->test_upload_sequence_);
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadComplete(Eq(uploader_id_), Eq(status)))
            .InSequence(uploader_->test_upload_sequence_,
                        uploader_->test_encounter_sequence_)
            .WillOnce(DoAll(
                WithoutArgs(
                    Invoke(waiter_.get(), &test::TestCallbackWaiter::Signal)),
                WithoutArgs(
                    Invoke([]() { LOG(ERROR) << "Completion signaled"; }))));
        return std::move(uploader_);
      }

      SetUp& Required(int64_t sequencing_id, base::StringPiece value) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_id_), Eq(priority_),
                                 Eq(sequencing_id), StrEq(std::string(value))))
            .InSequence(uploader_->test_upload_sequence_)
            .WillOnce(Return(true));
        return *this;
      }

      SetUp& Possible(int64_t sequencing_id, base::StringPiece value) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_id_), Eq(priority_),
                                 Eq(sequencing_id), StrEq(std::string(value))))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_upload_sequence_)
            .WillRepeatedly(Return(true));
        return *this;
      }

      SetUp& PossibleGap(int64_t sequence_number, uint64_t count) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadGap(Eq(uploader_id_), Eq(priority_),
                              Eq(sequence_number), Eq(count)))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_upload_sequence_)
            .WillRepeatedly(Return(true));
        return *this;
      }

      // The following two expectations refer to the fact that specific
      // sequencing ids have been encountered, regardless of whether they
      // belonged to records or gaps. The expectations are set on a separate
      // test sequence.
      SetUp& RequiredSeqId(int64_t sequence_number) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    EncounterSeqId(Eq(uploader_id_), Eq(priority_),
                                   Eq(sequence_number)))
            .Times(1)
            .InSequence(uploader_->test_encounter_sequence_);
        return *this;
      }

      SetUp& PossibleSeqId(int64_t sequence_number) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    EncounterSeqId(Eq(uploader_id_), Eq(priority_),
                                   Eq(sequence_number)))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_encounter_sequence_);
        return *this;
      }

     private:
      const Priority priority_;
      std::unique_ptr<TestUploader> uploader_;
      const int64_t uploader_id_;
      const raw_ptr<test::TestCallbackWaiter> waiter_;
    };

    // Helper class for setting up mock uploader expectations on empty queue.
    class SetEmpty {
     public:
      explicit SetEmpty(StorageTest* self)
          : uploader_(std::make_unique<TestUploader>(self)) {}
      SetEmpty(const SetEmpty& other) = delete;
      SetEmpty& operator=(const SetEmpty& other) = delete;
      ~SetEmpty() { CHECK(!uploader_) << "Missed 'Complete' call"; }

      std::unique_ptr<TestUploader> Complete() {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_->uploader_id_), _, _, _))
            .Times(0);
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecordFailure(Eq(uploader_->uploader_id_), _, _, _))
            .Times(0);
        EXPECT_CALL(
            *uploader_->mock_upload_,
            UploadComplete(Eq(uploader_->uploader_id_), Eq(Status::StatusOK())))
            .Times(1);
        return std::move(uploader_);
      }

     private:
      std::unique_ptr<TestUploader> uploader_;
    };

    // Helper class for setting up mock uploader expectations for key delivery.
    class SetKeyDelivery {
     public:
      explicit SetKeyDelivery(StorageTest* self)
          : uploader_(std::make_unique<TestUploader>(self)) {}
      SetKeyDelivery(const SetKeyDelivery& other) = delete;
      SetKeyDelivery& operator=(const SetKeyDelivery& other) = delete;
      ~SetKeyDelivery() { CHECK(!uploader_) << "Missed 'Complete' call"; }

      std::unique_ptr<TestUploader> Complete() {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_->uploader_id_), _, _, _))
            .Times(0);
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecordFailure(Eq(uploader_->uploader_id_), _, _, _))
            .Times(0);
        EXPECT_CALL(
            *uploader_->mock_upload_,
            UploadComplete(Eq(uploader_->uploader_id_), Eq(Status::StatusOK())))
            .WillOnce(WithoutArgs(Invoke(uploader_->mock_upload_.get(),
                                         &MockUpload::KeyGeneration)))
            .RetiresOnSaturation();
        return std::move(uploader_);
      }

     private:
      std::unique_ptr<TestUploader> uploader_;
    };

   private:
    void ScheduleVerifyRecord(SequenceInformation sequence_information,
                              WrappedRecord wrapped_record,
                              base::OnceCallback<void(bool)> processed_cb) {
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&TestUploader::VerifyRecord, base::Unretained(this),
                         std::move(sequence_information),
                         std::move(wrapped_record), std::move(processed_cb)));
    }

    void VerifyRecord(SequenceInformation sequence_information,
                      WrappedRecord wrapped_record,
                      base::OnceCallback<void(bool)> processed_cb) {
      // Verify generation match.
      if (generation_id_.has_value() &&
          generation_id_.value() != sequence_information.generation_id()) {
        std::move(processed_cb)
            .Run(mock_upload_->DoUploadRecordFailure(
                uploader_id_, sequence_information.priority(),
                sequence_information.sequencing_id(),
                Status(error::DATA_LOSS,
                       base::StrCat(
                           {"Generation id mismatch, expected=",
                            base::NumberToString(generation_id_.value()),
                            " actual=",
                            base::NumberToString(
                                sequence_information.generation_id())}))));
        return;
      }
      if (!generation_id_.has_value()) {
        generation_id_ = sequence_information.generation_id();
      }

      // Verify digest and its match.
      {
        std::string serialized_record;
        wrapped_record.record().SerializeToString(&serialized_record);
        const auto record_digest = crypto::SHA256HashString(serialized_record);
        DCHECK_EQ(record_digest.size(), crypto::kSHA256Length);
        if (record_digest != wrapped_record.record_digest()) {
          std::move(processed_cb)
              .Run(mock_upload_->DoUploadRecordFailure(
                  uploader_id_, sequence_information.priority(),
                  sequence_information.sequencing_id(),
                  Status(error::DATA_LOSS, "Record digest mismatch")));
          return;
        }
        if (wrapped_record.has_last_record_digest()) {
          auto it = last_record_digest_map_->find(
              std::make_tuple(sequence_information.priority(),
                              sequence_information.sequencing_id() - 1,
                              sequence_information.generation_id()));
          if (it == last_record_digest_map_->end()) {
            // Previous record has not been seen yet, reschedule. This can
            // happen because decryption is done asynchronously and only sets on
            // main_thread_task_runner_ after it. As a result, later record may
            // get decrypted early and be posted to main_thread_task_runner_ for
            // verification before its predecessor. Rescheduling will move it
            // back in the sequence. Rescheduling may happen multiple times, but
            // once the earlier record is decrypted, it will be also posted to
            // main_thread_task_runner_ and get its digest recorded, making it
            // ready for the current one. This is not an efficient method, but
            // is simple and good enough for the test.
            ScheduleVerifyRecord(std::move(sequence_information),
                                 std::move(wrapped_record),
                                 std::move(processed_cb));
            return;
          }
          // Previous record has been seen, last record digest must match it.
          if (it->second != wrapped_record.last_record_digest()) {
            std::move(processed_cb)
                .Run(mock_upload_->DoUploadRecordFailure(
                    uploader_id_, sequence_information.priority(),
                    sequence_information.sequencing_id(),
                    Status(error::DATA_LOSS, "Last record digest mismatch")));
            return;
          }
        }
        last_record_digest_map_->emplace(
            std::make_tuple(sequence_information.priority(),
                            sequence_information.sequencing_id(),
                            sequence_information.generation_id()),
            record_digest);
      }

      mock_upload_->DoEncounterSeqId(uploader_id_,
                                     sequence_information.priority(),
                                     sequence_information.sequencing_id());
      std::move(processed_cb)
          .Run(mock_upload_->DoUploadRecord(
              uploader_id_, sequence_information.priority(),
              sequence_information.sequencing_id(),
              wrapped_record.record().data()));
    }

    SEQUENCE_CHECKER(test_uploader_checker_);

    // Unique ID of the uploader - even if the uploader is allocated
    // on the same address as an earlier one (already released),
    // it will get a new id and thus will ensure the expectations
    // match the expected uploader.
    const int64_t uploader_id_;

    absl::optional<int64_t> generation_id_;
    const raw_ptr<LastRecordDigestMap> last_record_digest_map_;

    // Single task runner where all EXPECTs will happen.
    const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

    scoped_refptr<::testing::NiceMock<MockUpload>> mock_upload_;

    base::OnceClosure key_generation_;
    const scoped_refptr<test::Decryptor> decryptor_;

    Sequence test_encounter_sequence_;
    Sequence test_upload_sequence_;
  };

  StatusOr<scoped_refptr<Storage>> CreateTestStorage(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module) {
    // Initialize Storage with no key.
    test::TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    test_compression_module_ =
        base::MakeRefCounted<test::TestCompressionModule>();
    Storage::Create(options,
                    base::BindRepeating(&StorageTest::AsyncStartMockUploader,
                                        base::Unretained(this)),
                    encryption_module, test_compression_module_, e.cb());
    ASSIGN_OR_RETURN(auto storage, e.result());
    return storage;
  }

  void CreateTestStorageOrDie(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module =
          EncryptionModule::Create(
              /*renew_encryption_key_period=*/base::Minutes(30))) {
    ASSERT_FALSE(storage_) << "TestStorage already assigned";
    StatusOr<scoped_refptr<Storage>> storage_result =
        CreateTestStorage(options, encryption_module);
    ASSERT_OK(storage_result)
        << "Failed to create TestStorage, error=" << storage_result.status();
    storage_ = std::move(storage_result.ValueOrDie());

    if (expect_to_need_key_) {
      // Set uploader expectations for any queue; expect no records and need
      // key. Make sure no uploads happen, and key is requested.
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(UploaderInterface::UploadReason::KEY_DELIVERY))
          .WillOnce(Invoke([this](UploaderInterface::UploadReason) {
            return TestUploader::SetKeyDelivery(this).Complete();
          }))
          .RetiresOnSaturation();
    }
  }

  void ResetTestStorage() {
    // Let asynchronous activity finish.
    task_environment_.RunUntilIdle();
    storage_.reset();
    // StorageQueue is destructed on a thread,
    // so we need to wait for all queues to destruct.
    task_environment_.RunUntilIdle();
    expect_to_need_key_ = false;
  }

  StatusOr<scoped_refptr<Storage>> CreateTestStorageWithFailedKeyDelivery(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module =
          EncryptionModule::Create(
              /*renew_encryption_key_period=*/base::Minutes(30))) {
    // Initialize Storage with no key.
    test::TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    test_compression_module_ =
        base::MakeRefCounted<test::TestCompressionModule>();
    Storage::Create(
        options,
        base::BindRepeating(&StorageTest::AsyncStartMockUploaderFailing,
                            base::Unretained(this)),
        encryption_module, test_compression_module_, e.cb());
    ASSIGN_OR_RETURN(auto storage, e.result());
    return storage;
  }

  StorageOptions BuildTestStorageOptions() const {
    auto options = StorageOptions()
                       .set_directory(base::FilePath(location_.GetPath()));
    if (is_encryption_enabled()) {
      // Encryption enabled.
      options.set_signature_verification_public_key(std::string(
          reinterpret_cast<const char*>(signature_verification_public_key_),
          kKeySize));
    }
    return options;
  }

  void AsyncStartMockUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](UploaderInterface::UploadReason reason,
               UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
               StorageTest* self) {
              auto result = self->set_mock_uploader_expectations_.Call(reason);
              LOG(ERROR) << "Upload reason="
                         << UploaderInterface::ReasonToString(reason) << " "
                         << result.status();
              if (!result.ok()) {
                std::move(start_uploader_cb).Run(result.status());
                return;
              }
              auto uploader = std::move(result.ValueOrDie());
              std::move(start_uploader_cb).Run(std::move(uploader));
            },
            reason, std::move(start_uploader_cb), base::Unretained(this)));
  }

  void AsyncStartMockUploaderFailing(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    if (reason == UploaderInterface::UploadReason::KEY_DELIVERY &&
        key_delivery_failure_.load()) {
      std::move(start_uploader_cb)
          .Run(Status(error::FAILED_PRECONDITION, "Test cannot start upload"));
      return;
    }
    AsyncStartMockUploader(reason, std::move(start_uploader_cb));
  }

  Status WriteString(Priority priority, base::StringPiece data) {
    EXPECT_TRUE(storage_) << "Storage not created yet";
    test::TestEvent<Status> w;
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    LOG(ERROR) << "Write priority=" << priority << " data='" << record.data()
               << "'";
    storage_->Write(priority, std::move(record), w.cb());
    return w.result();
  }

  void WriteStringOrDie(Priority priority, base::StringPiece data) {
    const Status write_result = WriteString(priority, data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(Priority priority,
                    absl::optional<std::int64_t> sequencing_id,
                    bool force = false) {
    test::TestEvent<Status> c;
    LOG(ERROR) << "Confirm priority=" << priority << " force=" << force
               << " seq="
               << (sequencing_id.has_value()
                       ? base::NumberToString(sequencing_id.value())
                       : "null");
    storage_->Confirm(priority, sequencing_id, force, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  SignedEncryptionInfo GenerateAndSignKey() {
    DCHECK(decryptor_) << "Decryptor not created";
    // Generate new pair of private key and public value.
    uint8_t private_key[kKeySize];
    Encryptor::PublicKeyId public_key_id;
    uint8_t public_value[kKeySize];
    test::GenerateEncryptionKeyPair(private_key, public_value);
    test::TestEvent<StatusOr<Encryptor::PublicKeyId>> prepare_key_pair;
    decryptor_->RecordKeyPair(
        std::string(reinterpret_cast<const char*>(private_key), kKeySize),
        std::string(reinterpret_cast<const char*>(public_value), kKeySize),
        prepare_key_pair.cb());
    auto prepare_key_result = prepare_key_pair.result();
    DCHECK(prepare_key_result.ok());
    public_key_id = prepare_key_result.ValueOrDie();
    // Prepare signed encryption key to be delivered to Storage.
    SignedEncryptionInfo signed_encryption_key;
    signed_encryption_key.set_public_asymmetric_key(
        std::string(reinterpret_cast<const char*>(public_value), kKeySize));
    signed_encryption_key.set_public_key_id(public_key_id);
    // Sign public key.
    uint8_t value_to_sign[sizeof(Encryptor::PublicKeyId) + kKeySize];
    memcpy(value_to_sign, &public_key_id, sizeof(Encryptor::PublicKeyId));
    memcpy(value_to_sign + sizeof(Encryptor::PublicKeyId), public_value,
           kKeySize);
    uint8_t signature[kSignatureSize];
    test::SignMessage(
        signing_private_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature);
    signed_encryption_key.set_signature(
        std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
    // Double check signature.
    DCHECK(VerifySignature(
        signature_verification_public_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature));
    return signed_encryption_key;
  }

  bool is_encryption_enabled() const { return ::testing::get<0>(GetParam()); }
  size_t single_file_size_limit() const {
    return ::testing::get<1>(GetParam());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Single task runner where all EXPECTs will happen - main thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_{
      base::ThreadTaskRunnerHandle::Get()};

  base::test::ScopedFeatureList scoped_feature_list_;

  uint8_t signature_verification_public_key_[kKeySize];
  uint8_t signing_private_key_[kSignKeySize];

  base::ScopedTempDir location_;
  scoped_refptr<test::Decryptor> decryptor_;
  scoped_refptr<Storage> storage_;
  SignedEncryptionInfo signed_encryption_key_;
  bool expect_to_need_key_{false};
  std::atomic<bool> key_delivery_failure_{false};
  scoped_refptr<test::TestCompressionModule> test_compression_module_;

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all TestUploaders created by test fixture.
  TestUploader::LastRecordDigestMap last_record_digest_map_;

  ::testing::MockFunction<StatusOr<std::unique_ptr<TestUploader>>(
      UploaderInterface::UploadReason /*reason*/)>
      set_mock_uploader_expectations_;
};

constexpr std::array<const char*, 3> kData = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> kMoreData = {"More1111", "More222",
                                                  "More33"};

TEST_P(StorageTest, WriteIntoNewStorageAndReopen) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
}

TEST_P(StorageTest, WriteIntoNewStorageReopenAndWriteMore) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);
}

TEST_P(StorageTest, WriteIntoNewStorageAndUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(FAST_BATCH, &waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageTest, WriteIntoNewStorageAndUploadWithKeyUpdate) {
  // Run the test only when encryption is enabled.
  if (!is_encryption_enabled()) {
    return;
  }

  static constexpr auto kKeyRenewalTime = base::Seconds(5);
  CreateTestStorageOrDie(BuildTestStorageOptions(),
                         EncryptionModule::Create(kKeyRenewalTime));
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);
  WriteStringOrDie(MANUAL_BATCH, kData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY)))
        // Called once with empty queue.
        .WillOnce(Invoke([this](UploaderInterface::UploadReason reason) {
          return TestUploader::SetEmpty(this).Complete();
        }))
        // Can be called later again, reject it.
        .WillRepeatedly(Invoke([](UploaderInterface::UploadReason reason) {
          return Status(error::CANCELLED, "Repeated key delivery rejected");
        }));
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::MANUAL)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(MANUAL_BATCH, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Trigger upload with no key update.
    EXPECT_OK(storage_->Flush(MANUAL_BATCH));
  }

  // Confirm written data to prevent upload retry.
  ConfirmOrDie(MANUAL_BATCH, /*sequencing_id=*/2);

  // Write more data.
  WriteStringOrDie(MANUAL_BATCH, kMoreData[0]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[1]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[2]);

  // Wait to trigger encryption key request on the next upload
  task_environment_.FastForwardBy(kKeyRenewalTime + base::Seconds(1));

  // Set uploader expectations with encryption key request.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(MANUAL_BATCH, &waiter, this)
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      // Can be called later again, reject it.
      .WillRepeatedly(Invoke([](UploaderInterface::UploadReason reason) {
        return Status(error::CANCELLED, "Repeated key delivery rejected");
      }));

  // Trigger upload with key update after a long wait.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_P(StorageTest, WriteIntoNewStorageReopenWriteMoreAndUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(FAST_BATCH, &waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageTest, WriteIntoNewStorageAndFlush) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);
  WriteStringOrDie(MANUAL_BATCH, kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::MANUAL)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(MANUAL_BATCH, &waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_P(StorageTest, WriteIntoNewStorageReopenWriteMoreAndFlush) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);
  WriteStringOrDie(MANUAL_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, kMoreData[0]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[1]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::MANUAL)))
      .WillOnce(Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
        return TestUploader::SetUp(MANUAL_BATCH, &waiter, this)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2])
            .Complete();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_P(StorageTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #0 and forward time again, removing data #0
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing data #1
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/1);
  {
    test::TestCallbackAutoWaiter waiter;
    // Set uploader expectations.
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Add more records and verify that #2 and new records are returned.
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing data #2
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/2);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageTest, WriteAndRepeatedlySecurityUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(SECURITY, &waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(SECURITY,
                     kData[0]);  // Immediately uploads and verifies.
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(SECURITY, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(SECURITY,
                     kData[1]);  // Immediately uploads and verifies.
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(SECURITY, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(SECURITY,
                     kData[2]);  // Immediately uploads and verifies.
  }
}

TEST_P(StorageTest, WriteAndRepeatedlyImmediateUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE,
                     kData[0]);  // Immediately uploads and verifies.
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE,
                     kData[1]);  // Immediately uploads and verifies.
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE,
                     kData[2]);  // Immediately uploads and verifies.
  }
}

TEST_P(StorageTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kData[2]);
  }

  // Confirm #1, removing data #0 and #1
  ConfirmOrDie(IMMEDIATE, /*sequencing_id=*/1);

  // Add more data to verify that #2 and new data are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kMoreData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kMoreData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kMoreData[2]);
  }
}

TEST_P(StorageTest, WriteAndRepeatedlyUploadMultipleQueues) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kData[0]);
  }

  WriteStringOrDie(SLOW_BATCH, kMoreData[0]);

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kData[1]);
  }

  WriteStringOrDie(SLOW_BATCH, kMoreData[1]);

  // Confirm #1 IMMEDIATE, removing data #0 and #1, to prevent upload retry.
  ConfirmOrDie(IMMEDIATE, /*sequencing_id=*/1);

  // Set uploader expectations for FAST_BATCH and SLOW_BATCH.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(SLOW_BATCH, &waiter, this)
                  .Required(0, kMoreData[0])
                  .Required(1, kMoreData[1])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(20));
  }

  // Confirm #0 SLOW_BATCH, removing data #0
  ConfirmOrDie(SLOW_BATCH, /*sequencing_id=*/0);

  // Add more data
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE, kData[2]);
  }
  WriteStringOrDie(SLOW_BATCH, kMoreData[2]);

  // Confirm #2 IMMEDIATE, to prevent upload retry.
  ConfirmOrDie(IMMEDIATE, /*sequencing_id=*/2);

  // Set uploader expectations for FAST_BATCH and SLOW_BATCH.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(SLOW_BATCH, &waiter, this)
                  .Required(1, kMoreData[1])
                  .Required(2, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(20));
  }
}

TEST_P(StorageTest, WriteAndImmediateUploadWithFailure) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Write a record as Immediate, initiating an upload which fails
  // and then restarts.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(Invoke([](UploaderInterface::UploadReason reason) {
          return Status(error::UNAVAILABLE, "Test uploader unavailable");
        }))
        .RetiresOnSaturation();
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::FAILURE_RETRY)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                  .Required(0, kData[0])
                  .Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(IMMEDIATE,
                     kData[0]);  // Immediately uploads and fails.

    // Let it retry upload and verify.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageTest, WriteEncryptFailure) {
  if (!is_encryption_enabled()) {
    return;  // No need to test when encryption is disabled.
  }
  auto test_encryption_module =
      base::MakeRefCounted<test::TestEncryptionModule>();
  test::TestEvent<Status> key_update_event;
  test_encryption_module->UpdateAsymmetricKey("DUMMY KEY", 0,
                                              key_update_event.cb());
  ASSERT_OK(key_update_event.result());
  expect_to_need_key_ = false;
  CreateTestStorageOrDie(BuildTestStorageOptions(), test_encryption_module);
  EXPECT_CALL(*test_encryption_module, EncryptRecordImpl(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            std::move(cb).Run(Status(error::UNKNOWN, "Failing for tests"));
          })))
      .RetiresOnSaturation();
  const Status result = WriteString(FAST_BATCH, "TEST_MESSAGE");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_P(StorageTest, ForceConfirm) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, possibly removing records #0 and #1
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/1);
  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Now force confirm #0 and forward time again.
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/absl::nullopt, /*force=*/true);
  // Set uploader expectations: #0 and #1 could be returned as Gaps
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .RequiredSeqId(0)
                  .RequiredSeqId(1)
                  .RequiredSeqId(2)
                  // 0-2 must have been encountered, but actual contents
                  // can be different:
                  .Possible(0, kData[0])
                  .PossibleGap(0, 1)
                  .PossibleGap(0, 2)
                  .Possible(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Force confirm #0 and forward time again.
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/0, /*force=*/true);
  // Set uploader expectations: #0 and #1 could be returned as Gaps
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .RequiredSeqId(1)
                  .RequiredSeqId(2)
                  // 0-2 must have been encountered, but actual contents
                  // can be different:
                  .PossibleGap(1, 1)
                  .Possible(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageTest, KeyDeliveryFailureOnNewStorage) {
  static constexpr size_t kFailuresCount = 3;

  if (!is_encryption_enabled()) {
    return;  // Test only makes sense with encryption enabled.
  }

  // Initialize Storage with failure to deliver key.
  ASSERT_FALSE(storage_) << "StorageTest already assigned";
  StatusOr<scoped_refptr<Storage>> storage_result =
      CreateTestStorageWithFailedKeyDelivery(BuildTestStorageOptions());
  ASSERT_OK(storage_result)
      << "Failed to create StorageTest, error=" << storage_result.status();
  storage_ = std::move(storage_result.ValueOrDie());

  key_delivery_failure_.store(true);
  for (size_t failure = 1; failure < kFailuresCount; ++failure) {
    // Failing attempt to write
    const Status write_result = WriteString(FAST_BATCH, kData[0]);
    EXPECT_FALSE(write_result.ok());
    EXPECT_THAT(write_result.error_code(), Eq(error::FAILED_PRECONDITION))
        << write_result;
    EXPECT_THAT(write_result.message(), HasSubstr("Test cannot start upload"))
        << write_result;

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // This time key delivery is to succeed.
  // Set uploader expectations for any queue; expect no records and need
  // key. Make sure no uploads happen, and key is requested.
  key_delivery_failure_.store(false);
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::UploadReason::KEY_DELIVERY)))
      .WillOnce(Invoke([this](UploaderInterface::UploadReason) {
        return TestUploader::SetKeyDelivery(this).Complete();
      }))
      .RetiresOnSaturation();

  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::Seconds(1));

  // Successfully write data
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Trigger successful upload.
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  ResetTestStorage();

  // Reopen and write more data.
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              return TestUploader::SetUp(FAST_BATCH, &waiter, this)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2])
                  .Complete();
            }))
        .RetiresOnSaturation();

    // Trigger upload.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

INSTANTIATE_TEST_SUITE_P(
    VaryingFileSize,
    StorageTest,
    ::testing::Combine(::testing::Bool() /* true - encryption enabled */,
                       ::testing::Values(128 * 1024LL * 1024LL,
                                         256 /* two records in file */,
                                         1 /* single record in file */)));

}  // namespace
}  // namespace reporting
