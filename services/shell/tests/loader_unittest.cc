// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/catalog.h"
#include "services/catalog/store.h"
#include "services/shell/connect_util.h"
#include "services/shell/loader.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "services/shell/shell.h"
#include "services/shell/tests/test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell {
namespace test {

const char kTestURLString[] = "test:testService";
const char kTestAURLString[] = "test:TestA";
const char kTestBURLString[] = "test:TestB";

struct TestContext {
  TestContext() : num_impls(0), num_loader_deletes(0) {}
  std::string last_test_string;
  int num_impls;
  int num_loader_deletes;
};

void QuitClosure(const Identity& expected,
                 bool* value,
                 const Identity& actual) {
  if (expected == actual) {
    *value = true;
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

class TestServiceImpl : public TestService {
 public:
  TestServiceImpl(TestContext* context, TestServiceRequest request)
      : context_(context), binding_(this, std::move(request)) {
    ++context_->num_impls;
  }

  ~TestServiceImpl() override {
    --context_->num_impls;
    if (!base::MessageLoop::current()->is_running())
      return;
    base::MessageLoop::current()->QuitWhenIdle();
  }

  // TestService implementation:
  void Test(const mojo::String& test_string,
            const mojo::Closure& callback) override {
    context_->last_test_string = test_string;
    callback.Run();
  }

 private:
  TestContext* context_;
  mojo::StrongBinding<TestService> binding_;
};

class TestClient {
 public:
  explicit TestClient(TestServicePtr service)
      : service_(std::move(service)), quit_after_ack_(false) {}

  void AckTest() {
    if (quit_after_ack_)
      base::MessageLoop::current()->QuitWhenIdle();
  }

  void Test(const std::string& test_string) {
    quit_after_ack_ = true;
    service_->Test(test_string,
                   base::Bind(&TestClient::AckTest, base::Unretained(this)));
  }

 private:
  TestServicePtr service_;
  bool quit_after_ack_;
  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

class TestLoader : public Loader,
                   public ShellClient,
                   public InterfaceFactory<TestService> {
 public:
  explicit TestLoader(TestContext* context)
      : context_(context), num_loads_(0) {}

  ~TestLoader() override {
    ++context_->num_loader_deletes;
    shell_connection_.reset();
  }

  int num_loads() const { return num_loads_; }
  const std::string& last_requestor_name() const {
    return last_requestor_name_;
  }

 private:
  // Loader implementation.
  void Load(const std::string& name,
            mojom::ShellClientRequest request) override {
    ++num_loads_;
    shell_connection_.reset(new ShellConnection(this, std::move(request)));
  }

  // ShellClient implementation.
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<TestService>(this);
    last_requestor_name_ = connection->GetRemoteIdentity().name();
    return true;
  }

  // InterfaceFactory<TestService> implementation.
  void Create(Connection* connection, TestServiceRequest request) override {
    new TestServiceImpl(context_, std::move(request));
  }

  std::unique_ptr<ShellConnection> shell_connection_;
  TestContext* context_;
  int num_loads_;
  std::string last_requestor_name_;

  DISALLOW_COPY_AND_ASSIGN(TestLoader);
};

class ClosingLoader : public Loader {
 private:
  // Loader implementation.
  void Load(const std::string& name,
            mojom::ShellClientRequest request) override {
  }
};

class TesterContext {
 public:
  explicit TesterContext(base::MessageLoop* loop)
      : num_b_calls_(0),
        num_c_calls_(0),
        num_a_deletes_(0),
        num_b_deletes_(0),
        num_c_deletes_(0),
        tester_called_quit_(false),
        a_called_quit_(false),
        loop_(loop) {}

  void IncrementNumBCalls() {
    base::AutoLock lock(lock_);
    num_b_calls_++;
  }

  void IncrementNumCCalls() {
    base::AutoLock lock(lock_);
    num_c_calls_++;
  }

  void IncrementNumADeletes() {
    base::AutoLock lock(lock_);
    num_a_deletes_++;
  }

  void IncrementNumBDeletes() {
    base::AutoLock lock(lock_);
    num_b_deletes_++;
  }

  void IncrementNumCDeletes() {
    base::AutoLock lock(lock_);
    num_c_deletes_++;
  }

  void set_tester_called_quit() {
    base::AutoLock lock(lock_);
    tester_called_quit_ = true;
  }

  void set_a_called_quit() {
    base::AutoLock lock(lock_);
    a_called_quit_ = true;
  }

  int num_b_calls() {
    base::AutoLock lock(lock_);
    return num_b_calls_;
  }
  int num_c_calls() {
    base::AutoLock lock(lock_);
    return num_c_calls_;
  }
  int num_a_deletes() {
    base::AutoLock lock(lock_);
    return num_a_deletes_;
  }
  int num_b_deletes() {
    base::AutoLock lock(lock_);
    return num_b_deletes_;
  }
  int num_c_deletes() {
    base::AutoLock lock(lock_);
    return num_c_deletes_;
  }
  bool tester_called_quit() {
    base::AutoLock lock(lock_);
    return tester_called_quit_;
  }
  bool a_called_quit() {
    base::AutoLock lock(lock_);
    return a_called_quit_;
  }

  void QuitSoon() {
    loop_->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

 private:
  // lock_ protects all members except for loop_ which must be unchanged for the
  // lifetime of this class.
  base::Lock lock_;
  int num_b_calls_;
  int num_c_calls_;
  int num_a_deletes_;
  int num_b_deletes_;
  int num_c_deletes_;
  bool tester_called_quit_;
  bool a_called_quit_;

  base::MessageLoop* loop_;
};

// Used to test that the requestor name will be correctly passed.
class TestAImpl : public TestA {
 public:
  TestAImpl(Connector* connector,
            TesterContext* test_context,
            TestARequest request,
            InterfaceFactory<TestC>* factory)
      : test_context_(test_context), binding_(this, std::move(request)) {
    connection_ = connector->Connect(kTestBURLString);
    connection_->AddInterface<TestC>(factory);
    connection_->GetInterface(&b_);
  }

  ~TestAImpl() override {
    test_context_->IncrementNumADeletes();
    if (base::MessageLoop::current()->is_running())
      Quit();
  }

 private:
  void CallB() override {
    b_->B(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void CallCFromB() override {
    b_->CallC(base::Bind(&TestAImpl::Quit, base::Unretained(this)));
  }

  void Quit() {
    base::MessageLoop::current()->QuitWhenIdle();
    test_context_->set_a_called_quit();
    test_context_->QuitSoon();
  }

  std::unique_ptr<Connection> connection_;
  TesterContext* test_context_;
  TestBPtr b_;
  mojo::StrongBinding<TestA> binding_;
};

class TestBImpl : public TestB {
 public:
  TestBImpl(Connection* connection,
            TesterContext* test_context,
            TestBRequest request)
      : test_context_(test_context), binding_(this, std::move(request)) {
    connection->GetInterface(&c_);
  }

  ~TestBImpl() override {
    test_context_->IncrementNumBDeletes();
    if (base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->QuitWhenIdle();
    test_context_->QuitSoon();
  }

 private:
  void B(const mojo::Closure& callback) override {
    test_context_->IncrementNumBCalls();
    callback.Run();
  }

  void CallC(const mojo::Closure& callback) override {
    test_context_->IncrementNumBCalls();
    c_->C(callback);
  }

  TesterContext* test_context_;
  TestCPtr c_;
  mojo::StrongBinding<TestB> binding_;
};

class TestCImpl : public TestC {
 public:
  TestCImpl(Connection* connection,
            TesterContext* test_context,
            TestCRequest request)
      : test_context_(test_context), binding_(this, std::move(request)) {}

  ~TestCImpl() override { test_context_->IncrementNumCDeletes(); }

 private:
  void C(const mojo::Closure& callback) override {
    test_context_->IncrementNumCCalls();
    callback.Run();
  }

  TesterContext* test_context_;
  mojo::StrongBinding<TestC> binding_;
};

class Tester : public ShellClient,
               public Loader,
               public InterfaceFactory<TestA>,
               public InterfaceFactory<TestB>,
               public InterfaceFactory<TestC> {
 public:
  Tester(TesterContext* context, const std::string& requestor_name)
      : context_(context), requestor_name_(requestor_name) {}
  ~Tester() override {}

 private:
  void Load(const std::string& name,
            mojom::ShellClientRequest request) override {
    app_.reset(new ShellConnection(this, std::move(request)));
  }

  bool AcceptConnection(Connection* connection) override {
    if (!requestor_name_.empty() &&
        requestor_name_ != connection->GetRemoteIdentity().name()) {
      context_->set_tester_called_quit();
      context_->QuitSoon();
      base::MessageLoop::current()->QuitWhenIdle();
      return false;
    }
    // If we're coming from A, then add B, otherwise A.
    if (connection->GetRemoteIdentity().name() == kTestAURLString)
      connection->AddInterface<TestB>(this);
    else
      connection->AddInterface<TestA>(this);
    return true;
  }

  void Create(Connection* connection, TestARequest request) override {
    a_bindings_.push_back(
        new TestAImpl(app_->connector(), context_, std::move(request), this));
  }

  void Create(Connection* connection, TestBRequest request) override {
    new TestBImpl(connection, context_, std::move(request));
  }

  void Create(Connection* connection, TestCRequest request) override {
    new TestCImpl(connection, context_, std::move(request));
  }

  TesterContext* context_;
  std::unique_ptr<ShellConnection> app_;
  std::string requestor_name_;
  ScopedVector<TestAImpl> a_bindings_;
};

void OnConnect(base::RunLoop* loop,
               mojom::ConnectResult result,
               const mojo::String& user_id,
               uint32_t instance_id) {
  loop->Quit();
}

class LoaderTest : public testing::Test {
 public:
  LoaderTest() : tester_context_(&loop_) {}
  ~LoaderTest() override {}

  void SetUp() override {
    blocking_pool_ = new base::SequencedWorkerPool(3, "blocking_pool");
    catalog_.reset(
        new catalog::Catalog(blocking_pool_.get(), nullptr, nullptr));
    shell_.reset(new Shell(nullptr, catalog_->TakeShellClient()));
    test_loader_ = new TestLoader(&context_);
    shell_->set_default_loader(std::unique_ptr<Loader>(test_loader_));

    TestServicePtr service_proxy;
    ConnectToInterface(kTestURLString, &service_proxy);
    test_client_.reset(new TestClient(std::move(service_proxy)));
  }

  void TearDown() override {
    test_client_.reset();
    shell_.reset();
    blocking_pool_->Shutdown();
  }

  void AddLoaderForName(const std::string& name,
                        const std::string& requestor_name) {
    shell_->SetLoaderForName(
        base::WrapUnique(new Tester(&tester_context_, requestor_name)), name);
  }

  bool HasRunningInstanceForName(const std::string& name) {
    Shell::TestAPI test_api(shell_.get());
    return test_api.HasRunningInstanceForName(name);
  }

 protected:
  template <typename Interface>
  void ConnectToInterface(const std::string& name,
                          mojo::InterfacePtr<Interface>* ptr) {
    base::RunLoop loop;
    mojom::InterfaceProviderPtr remote_interfaces;
    std::unique_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(CreateShellIdentity());
    params->set_target(Identity(name, mojom::kRootUserID));
    params->set_remote_interfaces(mojo::GetProxy(&remote_interfaces));
    params->set_connect_callback(
        base::Bind(&OnConnect, base::Unretained(&loop)));
    shell_->Connect(std::move(params));
    loop.Run();

    GetInterface(remote_interfaces.get(), ptr);
  }

  base::ShadowingAtExitManager at_exit_;
  TestLoader* test_loader_;
  TesterContext tester_context_;
  TestContext context_;
  base::MessageLoop loop_;
  std::unique_ptr<TestClient> test_client_;
  std::unique_ptr<catalog::Catalog> catalog_;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;
  std::unique_ptr<Shell> shell_;

  DISALLOW_COPY_AND_ASSIGN(LoaderTest);
};

TEST_F(LoaderTest, Basic) {
  test_client_->Test("test");
  loop_.Run();
  EXPECT_EQ(std::string("test"), context_.last_test_string);
}

TEST_F(LoaderTest, ClientError) {
  test_client_->Test("test");
  EXPECT_TRUE(HasRunningInstanceForName(kTestURLString));
  loop_.Run();
  EXPECT_EQ(1, context_.num_impls);
  test_client_.reset();
  loop_.Run();
  EXPECT_EQ(0, context_.num_impls);
  EXPECT_TRUE(HasRunningInstanceForName(kTestURLString));
}

TEST_F(LoaderTest, Deletes) {
  {
    catalog::Catalog catalog(blocking_pool_.get(), nullptr, nullptr);
    Shell shell(nullptr, catalog.TakeShellClient());
    TestLoader* default_loader = new TestLoader(&context_);
    TestLoader* name_loader1 = new TestLoader(&context_);
    TestLoader* name_loader2 = new TestLoader(&context_);
    shell.set_default_loader(std::unique_ptr<Loader>(default_loader));
    shell.SetLoaderForName(std::unique_ptr<Loader>(name_loader1), "test:test1");
    shell.SetLoaderForName(std::unique_ptr<Loader>(name_loader2), "test:test1");
  }
  EXPECT_EQ(3, context_.num_loader_deletes);
}

// Test for SetLoaderForName() & set_default_loader().
TEST_F(LoaderTest, SetLoaders) {
  TestLoader* default_loader = new TestLoader(&context_);
  TestLoader* name_loader = new TestLoader(&context_);
  shell_->set_default_loader(std::unique_ptr<Loader>(default_loader));
  shell_->SetLoaderForName(std::unique_ptr<Loader>(name_loader), "test:test1");

  // test::test1 should go to name_loader.
  TestServicePtr test_service;
  ConnectToInterface("test:test1", &test_service);
  EXPECT_EQ(1, name_loader->num_loads());
  EXPECT_EQ(0, default_loader->num_loads());

  // http::test1 should go to default loader.
  ConnectToInterface("http:test1", &test_service);
  EXPECT_EQ(1, name_loader->num_loads());
  EXPECT_EQ(1, default_loader->num_loads());
}

TEST_F(LoaderTest, NoServiceNoLoad) {
  AddLoaderForName(kTestAURLString, std::string());

  // There is no TestC service implementation registered with the Shell, so this
  // cannot succeed (but also shouldn't crash).
  TestCPtr c;
  ConnectToInterface(kTestAURLString, &c);
  c.set_connection_error_handler(
      []() { base::MessageLoop::current()->QuitWhenIdle(); });

  loop_.Run();
  EXPECT_TRUE(c.encountered_error());
}

TEST_F(LoaderTest, TestEndApplicationClosure) {
  ClosingLoader* loader = new ClosingLoader();
  shell_->SetLoaderForName(std::unique_ptr<Loader>(loader), "test:test");

  bool called = false;
  std::unique_ptr<ConnectParams> params(new ConnectParams);
  params->set_source(CreateShellIdentity());
  params->set_target(Identity("test:test", mojom::kRootUserID));
  shell_->SetInstanceQuitCallback(
      base::Bind(&QuitClosure, params->target(), &called));
  shell_->Connect(std::move(params));
  loop_.Run();
  EXPECT_TRUE(called);
}

TEST_F(LoaderTest, SameIdentityShouldNotCauseDuplicateLoad) {
  // 1 because LoaderTest connects once at startup.
  EXPECT_EQ(1, test_loader_->num_loads());

  TestServicePtr test_service;
  ConnectToInterface("test:foo", &test_service);
  EXPECT_EQ(2, test_loader_->num_loads());

  // Exactly the same name as above.
  ConnectToInterface("test:foo", &test_service);
  EXPECT_EQ(2, test_loader_->num_loads());

  // A different identity because the domain is different.
  ConnectToInterface("test:bar", &test_service);
  EXPECT_EQ(3, test_loader_->num_loads());
}

}  // namespace test
}  // namespace shell
