// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_system_test_util.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "services/audio/in_process_audio_manager_accessor.h"
#include "services/audio/public/cpp/audio_system_to_service_adapter.h"
#include "services/audio/public/interfaces/constants.mojom.h"
#include "services/audio/service.h"
#include "services/audio/system_info.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

using testing::Exactly;
using testing::Invoke;

namespace audio {

class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory {
 public:
  class AudioThreadContext
      : public base::RefCountedThreadSafe<AudioThreadContext> {
   public:
    explicit AudioThreadContext(media::AudioManager* audio_manager)
        : audio_manager_(audio_manager) {}

    void CreateServiceOnAudioThread(
        service_manager::mojom::ServiceRequest request) {
      if (!audio_manager_->GetTaskRunner()->BelongsToCurrentThread()) {
        audio_manager_->GetTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&AudioThreadContext::CreateServiceOnAudioThread,
                           this, std::move(request)));
        return;
      }
      DCHECK(!service_context_);
      service_context_ = std::make_unique<service_manager::ServiceContext>(
          std::make_unique<audio::Service>(
              std::make_unique<InProcessAudioManagerAccessor>(audio_manager_)),
          std::move(request));
      service_context_->SetQuitClosure(base::BindRepeating(
          &AudioThreadContext::QuitOnAudioThread, base::Unretained(this)));
    }

    void QuitOnAudioThread() {
      if (!audio_manager_->GetTaskRunner()->BelongsToCurrentThread()) {
        audio_manager_->GetTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&AudioThreadContext::QuitOnAudioThread, this));
        return;
      }
      service_context_.reset();
    }

   private:
    friend class base::RefCountedThreadSafe<AudioThreadContext>;
    virtual ~AudioThreadContext() {
      if (service_context_)
        service_context_->QuitNow();
    };

    media::AudioManager* const audio_manager_;
    std::unique_ptr<service_manager::ServiceContext> service_context_;
  };

  ServiceTestClient(service_manager::test::ServiceTest* test,
                    media::AudioManager* audio_manager)
      : service_manager::test::ServiceTestClient(test),
        audio_thread_context_(new AudioThreadContext(audio_manager)) {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(
        base::BindRepeating(&ServiceTestClient::Create,
                            base::Unretained(this)));
  }

  ~ServiceTestClient() override {}

 protected:
  bool OnServiceManagerConnectionLost() override { return true; }

  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void Create(service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  // service_manager::mojom::ServiceFactory:
  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == mojom::kServiceName)
      audio_thread_context_->CreateServiceOnAudioThread(std::move(request));
  }

 private:
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  scoped_refptr<AudioThreadContext> audio_thread_context_;
  DISALLOW_COPY_AND_ASSIGN(ServiceTestClient);
};

// if |use_audio_thread| is true, AudioManager has a dedicated audio thread and
// Audio service lives on it; otherwise audio thread is the main thread of the
// test fixture, and that's where Service lives. So in the former case the
// service is accessed from another thread, and in the latter case - from the
// thread it lives on (which imitates access to Audio service from UI thread on
// Mac).
template <bool use_audio_thread>
class InProcessServiceTest : public service_manager::test::ServiceTest {
 public:
  InProcessServiceTest() : ServiceTest("audio_unittests") {}

  ~InProcessServiceTest() override {}

 protected:
  // service_manager::test::ServiceTest:
  std::unique_ptr<service_manager::Service> CreateService() override {
    return std::make_unique<ServiceTestClient>(this, audio_manager_.get());
  }

  void SetUp() override {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>(use_audio_thread));
    ServiceTest::SetUp();
    audio_system_ =
        std::make_unique<AudioSystemToServiceAdapter>(connector()->Clone());
  }

  void TearDown() override {
    audio_system_.reset();

    // Deletes ServiceTestClient, which will result in posting
    // AuioThreadContext::QuitOnAudioThread() to AudioManager thread, so that
    // Service is delete there.
    ServiceTest::TearDown();

    // Joins AudioManager thread if it is used.
    audio_manager_->Shutdown();
  }

 protected:
  media::MockAudioManager* audio_manager() { return audio_manager_.get(); }
  media::AudioSystem* audio_system() { return audio_system_.get(); }

  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InProcessServiceTest);
};

}  // namespace audio

// AudioSystem interface conformance tests.
// AudioSystemTestTemplate is defined in media, so should be its instantiations.
namespace media {

using InProcessServiceTestVariations =
    testing::Types<audio::InProcessServiceTest<false>,
                   audio::InProcessServiceTest<true>>;

INSTANTIATE_TYPED_TEST_CASE_P(InProcessService,
                              AudioSystemTestTemplate,
                              InProcessServiceTestVariations);
}  // namespace media
