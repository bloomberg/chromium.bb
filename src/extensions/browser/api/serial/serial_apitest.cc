// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/serial/serial_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/serial.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "testing/gmock/include/gmock/gmock.h"

// Disable SIMULATE_SERIAL_PORTS only if all the following are true:
//
// 1. You have an Arduino or compatible board attached to your machine and
// properly appearing as the first virtual serial port ("first" is very loosely
// defined as whichever port shows up in serial.getPorts). We've tested only
// the Atmega32u4 Breakout Board and Arduino Leonardo; note that both these
// boards are based on the Atmel ATmega32u4, rather than the more common
// Arduino '328p with either FTDI or '8/16u2 USB interfaces. TODO: test more
// widely.
//
// 2. Your user has permission to read/write the port. For example, this might
// mean that your user is in the "tty" or "uucp" group on Ubuntu flavors of
// Linux, or else that the port's path (e.g., /dev/ttyACM0) has global
// read/write permissions.
//
// 3. You have uploaded a program to the board that does a byte-for-byte echo
// on the virtual serial port at 57600 bps. An example is at
// chrome/test/data/extensions/api_test/serial/api/serial_arduino_test.ino.
//
#define SIMULATE_SERIAL_PORTS (1)

using testing::_;
using testing::Return;

namespace extensions {
namespace {

class FakeSerialPort : public device::mojom::SerialPort {
 public:
  explicit FakeSerialPort(const base::FilePath& path)
      : in_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        out_stream_watcher_(FROM_HERE,
                            mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
    options_.bitrate = 9600;
    options_.data_bits = device::mojom::SerialDataBits::EIGHT;
    options_.parity_bit = device::mojom::SerialParityBit::NO_PARITY;
    options_.stop_bits = device::mojom::SerialStopBits::ONE;
    options_.cts_flow_control = false;
    options_.has_cts_flow_control = true;
  }
  ~FakeSerialPort() override = default;

 private:
  // device::mojom::SerialPort methods:
  void Open(device::mojom::SerialConnectionOptionsPtr options,
            mojo::ScopedDataPipeConsumerHandle in_stream,
            mojo::ScopedDataPipeProducerHandle out_stream,
            device::mojom::SerialPortClientAssociatedPtrInfo client,
            OpenCallback callback) override {
    DoConfigurePort(*options);
    DCHECK(client);
    client_.Bind(std::move(client));
    SetUpInStreamPipe(std::move(in_stream));
    SetUpOutStreamPipe(std::move(out_stream));
    std::move(callback).Run(true);
  }
  void ClearSendError(mojo::ScopedDataPipeConsumerHandle consumer) override {
    if (in_stream_) {
      return;
    }
    SetUpInStreamPipe(std::move(consumer));
  }
  void ClearReadError(mojo::ScopedDataPipeProducerHandle producer) override {
    if (out_stream_) {
      return;
    }
    SetUpOutStreamPipe(std::move(producer));
  }
  void Flush(FlushCallback callback) override { std::move(callback).Run(true); }
  void GetControlSignals(GetControlSignalsCallback callback) override {
    auto signals = device::mojom::SerialPortControlSignals::New();
    signals->dcd = true;
    signals->cts = true;
    signals->ri = true;
    signals->dsr = true;
    std::move(callback).Run(std::move(signals));
  }
  void SetControlSignals(device::mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override {
    std::move(callback).Run(true);
  }
  void ConfigurePort(device::mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override {
    DoConfigurePort(*options);
    std::move(callback).Run(true);
  }
  void GetPortInfo(GetPortInfoCallback callback) override {
    auto info = device::mojom::SerialConnectionInfo::New();
    info->bitrate = options_.bitrate;
    info->data_bits = options_.data_bits;
    info->parity_bit = options_.parity_bit;
    info->stop_bits = options_.stop_bits;
    info->cts_flow_control = options_.cts_flow_control;
    std::move(callback).Run(std::move(info));
  }
  void SetBreak(SetBreakCallback callback) override {
    std::move(callback).Run(true);
  }
  void ClearBreak(ClearBreakCallback callback) override {
    std::move(callback).Run(true);
  }

  void SetUpInStreamPipe(mojo::ScopedDataPipeConsumerHandle consumer) {
    in_stream_.swap(consumer);
    in_stream_watcher_.Watch(
        in_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&FakeSerialPort::DoWrite, base::Unretained(this)));
    in_stream_watcher_.ArmOrNotify();
  }

  void DoWrite(MojoResult result, const mojo::HandleSignalsState& state) {
    const void* data;
    uint32_t num_bytes;

    if (result == MOJO_RESULT_OK) {
      result = in_stream_->BeginReadData(&data, &num_bytes,
                                         MOJO_READ_DATA_FLAG_NONE);
    }
    if (result == MOJO_RESULT_OK) {
      // Control the bytes read from in_stream_ to trigger a variaty of
      // transfer cases between SerialConnection::send_pipe_.
      write_step_++;
      if ((write_step_ % 4) < 2 && num_bytes > 1) {
        num_bytes = 1;
      }
      const uint8_t* uint8_data = reinterpret_cast<const uint8_t*>(data);
      buffer_.insert(buffer_.end(), uint8_data, uint8_data + num_bytes);
      in_stream_->EndReadData(num_bytes);
      in_stream_watcher_.ArmOrNotify();

      // Enable the notification to write this data to the out stream.
      out_stream_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // If there is no space to write, wait for more space.
      in_stream_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION ||
        result == MOJO_RESULT_CANCELLED) {
      // The |in_stream_| has been closed.
      in_stream_.reset();
      return;
    }
    // The code should not reach other cases.
    NOTREACHED();
  }

  void SetUpOutStreamPipe(mojo::ScopedDataPipeProducerHandle producer) {
    out_stream_.swap(producer);
    out_stream_watcher_.Watch(
        out_stream_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&FakeSerialPort::DoRead, base::Unretained(this)));
    out_stream_watcher_.ArmOrNotify();
  }

  void DoRead(MojoResult result, const mojo::HandleSignalsState& state) {
    if (result != MOJO_RESULT_OK) {
      out_stream_.reset();
      return;
    }
    if (buffer_.empty()) {
      return;
    }
    read_step_++;
    if (read_step_ == 1) {
      // Write one byte first.
      WriteOutReadData(1);
    } else if (read_step_ == 2) {
      // Write one byte in second step and trigger a break error.
      WriteOutReadData(1);
      DCHECK(client_);
      client_->OnReadError(device::mojom::SerialReceiveError::PARITY_ERROR);
      out_stream_watcher_.Cancel();
      out_stream_.reset();
      return;
    } else {
      // Write out the rest data after reconnecting.
      WriteOutReadData(buffer_.size());
    }
    out_stream_watcher_.ArmOrNotify();
  }

  void WriteOutReadData(uint32_t num_bytes) {
    MojoResult result = out_stream_->WriteData(buffer_.data(), &num_bytes,
                                               MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      buffer_.erase(buffer_.begin(), buffer_.begin() + num_bytes);
    }
  }

  void DoConfigurePort(const device::mojom::SerialConnectionOptions& options) {
    // Merge options.
    if (options.bitrate) {
      options_.bitrate = options.bitrate;
    }
    if (options.data_bits != device::mojom::SerialDataBits::NONE) {
      options_.data_bits = options.data_bits;
    }
    if (options.parity_bit != device::mojom::SerialParityBit::NONE) {
      options_.parity_bit = options.parity_bit;
    }
    if (options.stop_bits != device::mojom::SerialStopBits::NONE) {
      options_.stop_bits = options.stop_bits;
    }
    if (options.has_cts_flow_control) {
      DCHECK(options_.has_cts_flow_control);
      options_.cts_flow_control = options.cts_flow_control;
    }
  }

  // Currently applied connection options.
  device::mojom::SerialConnectionOptions options_;
  std::vector<uint8_t> buffer_;
  int read_step_ = 0;
  int write_step_ = 0;
  device::mojom::SerialPortClientAssociatedPtr client_;
  mojo::ScopedDataPipeConsumerHandle in_stream_;
  mojo::SimpleWatcher in_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle out_stream_;
  mojo::SimpleWatcher out_stream_watcher_;

  DISALLOW_COPY_AND_ASSIGN(FakeSerialPort);
};

class FakeSerialPortManager : public device::mojom::SerialPortManager {
 public:
  FakeSerialPortManager() {
    token_path_map_ = {
        {base::UnguessableToken::Create(),
         base::FilePath(FILE_PATH_LITERAL("/dev/fakeserialmojo"))},
        {base::UnguessableToken::Create(),
         base::FilePath(FILE_PATH_LITERAL("\\\\COM800\\"))}};
  }

  ~FakeSerialPortManager() override = default;

 private:
  // device::mojom::SerialPortManager methods:
  void GetDevices(GetDevicesCallback callback) override {
    std::vector<device::mojom::SerialPortInfoPtr> devices;
    for (const auto& pair : token_path_map_) {
      auto device = device::mojom::SerialPortInfo::New();
      device->token = pair.first;
      device->path = pair.second;
      devices.push_back(std::move(device));
    }
    std::move(callback).Run(std::move(devices));
  }

  void GetPort(const base::UnguessableToken& token,
               device::mojom::SerialPortRequest request,
               device::mojom::SerialPortConnectionWatcherPtr watcher) override {
    DCHECK(!watcher);
    auto it = token_path_map_.find(token);
    DCHECK(it != token_path_map_.end());
    mojo::MakeStrongBinding(std::make_unique<FakeSerialPort>(it->second),
                            std::move(request));
  }

  std::map<base::UnguessableToken, base::FilePath> token_path_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeSerialPortManager);
};

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {
#if SIMULATE_SERIAL_PORTS
    // Because Device Service also runs in this process(browser process), we can
    // set our binder to intercept requests for
    // SerialPortManager/SerialPort interfaces to it.
    service_manager::ServiceBinding::OverrideInterfaceBinderForTesting(
        device::mojom::kServiceName,
        base::BindRepeating(&SerialApiTest::BindSerialPortManager,
                            base::Unretained(this)));
#endif
  }

  ~SerialApiTest() override {
#if SIMULATE_SERIAL_PORTS
    service_manager::ServiceBinding::ClearInterfaceBinderOverrideForTesting<
        device::mojom::SerialPortManager>(device::mojom::kServiceName);
    service_manager::ServiceBinding::ClearInterfaceBinderOverrideForTesting<
        device::mojom::SerialPort>(device::mojom::kServiceName);
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override { ExtensionApiTest::SetUpOnMainThread(); }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
  }

  void FailEnumeratorRequest() { fail_enumerator_request_ = true; }

 protected:
  void BindSerialPortManager(device::mojom::SerialPortManagerRequest request) {
    if (fail_enumerator_request_)
      return;

    mojo::MakeStrongBinding(std::make_unique<FakeSerialPortManager>(),
                            std::move(request));
  }

  bool fail_enumerator_request_ = false;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialFakeHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardwareFail) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // chrome.serial.getDevices() should get an empty list when the serial
  // enumerator interface is unavailable.
  FailEnumeratorRequest();
  ASSERT_TRUE(RunExtensionTest("serial/real_hardware_fail")) << message_;
}

}  // namespace extensions
