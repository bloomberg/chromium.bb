// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/certificate_provider/test_certificate_provider_extension.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/test.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace ash {

namespace {

constexpr char kExtensionId[] = "ecmhnokcdiianioonpgakiooenfnonid";
// Paths relative to |chrome::DIR_TEST_DATA|:
constexpr base::FilePath::CharType kExtensionPath[] =
    FILE_PATH_LITERAL("extensions/test_certificate_provider/extension/");
constexpr base::FilePath::CharType kExtensionPemPath[] =
    FILE_PATH_LITERAL("extensions/test_certificate_provider/extension.pem");

// List of algorithms that the extension claims to support for the returned
// certificates.
constexpr extensions::api::certificate_provider::Algorithm
    kSupportedAlgorithms[] = {extensions::api::certificate_provider::Algorithm::
                                  ALGORITHM_RSASSA_PKCS1_V1_5_SHA256,
                              extensions::api::certificate_provider::Algorithm::
                                  ALGORITHM_RSASSA_PKCS1_V1_5_SHA1};

base::Value ConvertBytesToValue(base::span<const uint8_t> bytes) {
  base::Value value(base::Value::Type::LIST);
  for (auto byte : bytes)
    value.Append(byte);
  return value;
}

std::vector<uint8_t> ExtractBytesFromValue(const base::Value& value) {
  std::vector<uint8_t> bytes;
  for (const base::Value& item_value : value.GetList())
    bytes.push_back(base::checked_cast<uint8_t>(item_value.GetInt()));
  return bytes;
}

base::span<const uint8_t> GetCertDer(const net::X509Certificate& certificate) {
  return base::as_bytes(base::make_span(
      net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer())));
}

base::Value MakeClientCertificateInfoValue(
    const net::X509Certificate& certificate) {
  base::Value cert_info_value(base::Value::Type::DICTIONARY);
  base::Value certificate_chain(base::Value::Type::LIST);
  certificate_chain.Append(ConvertBytesToValue(GetCertDer(certificate)));
  cert_info_value.SetKey("certificateChain", std::move(certificate_chain));
  base::Value supported_algorithms_value(base::Value::Type::LIST);
  for (auto supported_algorithm : kSupportedAlgorithms) {
    supported_algorithms_value.Append(
        extensions::api::certificate_provider::ToString(supported_algorithm));
  }
  cert_info_value.SetKey("supportedAlgorithms",
                         std::move(supported_algorithms_value));
  return cert_info_value;
}

std::string ConvertValueToJson(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::Write(value, &json));
  return json;
}

base::Value ParseJsonToValue(const std::string& json) {
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value);
  return std::move(*value);
}

bool RsaSignRawData(crypto::RSAPrivateKey* key,
                    uint16_t openssl_signature_algorithm,
                    const std::vector<uint8_t>& input,
                    std::vector<uint8_t>* signature) {
  const EVP_MD* const digest_algorithm =
      SSL_get_signature_algorithm_digest(openssl_signature_algorithm);
  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pkey_ctx = nullptr;
  if (!EVP_DigestSignInit(ctx.get(), &pkey_ctx, digest_algorithm,
                          /*ENGINE* e=*/nullptr, key->key()))
    return false;
  if (SSL_is_signature_algorithm_rsa_pss(openssl_signature_algorithm)) {
    // For RSA-PSS, configure the special padding and set the salt length to be
    // equal to the hash size.
    if (!EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, /*salt_len=*/-1)) {
      return false;
    }
  }
  size_t sig_len = 0;
  // Determine the signature length for the buffer.
  if (!EVP_DigestSign(ctx.get(), /*out_sig=*/nullptr, &sig_len, input.data(),
                      input.size()))
    return false;
  signature->resize(sig_len);
  return EVP_DigestSign(ctx.get(), signature->data(), &sig_len, input.data(),
                        input.size()) != 0;
}

void SendReplyToJs(ExtensionTestMessageListener* message_listener,
                   const base::Value& response) {
  message_listener->Reply(ConvertValueToJson(response));
  message_listener->Reset();
}

std::unique_ptr<crypto::RSAPrivateKey> LoadPrivateKeyFromFile(
    const base::FilePath& path) {
  std::string key_pk8;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::ReadFileToString(path, &key_pk8));
  }
  return crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(
      base::as_bytes(base::make_span(key_pk8)));
}

}  // namespace

// static
extensions::ExtensionId TestCertificateProviderExtension::extension_id() {
  return kExtensionId;
}

// static
base::FilePath TestCertificateProviderExtension::GetExtensionSourcePath() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .Append(kExtensionPath);
}

// static
base::FilePath TestCertificateProviderExtension::GetExtensionPemPath() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .Append(kExtensionPemPath);
}

// static
scoped_refptr<net::X509Certificate>
TestCertificateProviderExtension::GetCertificate() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
}

// static
std::string TestCertificateProviderExtension::GetCertificateSpki() {
  const scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return std::string(spki_bytes);
}

TestCertificateProviderExtension::TestCertificateProviderExtension(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      certificate_(GetCertificate()),
      private_key_(LoadPrivateKeyFromFile(net::GetTestCertsDirectory().Append(
          FILE_PATH_LITERAL("client_1.pk8")))),
      message_listener_(/*will_reply=*/true) {
  DCHECK(browser_context_);
  CHECK(certificate_);
  CHECK(private_key_);
  // Ignore messages targeted to other extensions or browser contexts.
  message_listener_.set_extension_id(kExtensionId);
  message_listener_.set_browser_context(browser_context);
  message_listener_.SetOnRepeatedlySatisfied(
      base::BindRepeating(&TestCertificateProviderExtension::HandleMessage,
                          base::Unretained(this)));
}

TestCertificateProviderExtension::~TestCertificateProviderExtension() = default;

void TestCertificateProviderExtension::TriggerSetCertificates() {
  base::Value message_data(base::Value::Type::DICTIONARY);
  message_data.SetStringKey("name", "setCertificates");
  base::Value cert_info_values(base::Value::Type::LIST);
  if (should_provide_certificates_)
    cert_info_values.Append(MakeClientCertificateInfoValue(*certificate_));
  message_data.SetKey("certificateInfoList", std::move(cert_info_values));

  auto message = std::make_unique<base::Value>(base::Value::Type::LIST);
  message->Append(std::move(message_data));
  auto event = std::make_unique<extensions::Event>(
      extensions::events::FOR_TEST,
      extensions::api::test::OnMessage::kEventName,
      std::move(*message).TakeList(), browser_context_);
  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id(), std::move(event));
}

void TestCertificateProviderExtension::HandleMessage(
    const std::string& message) {
  // Handle the request and reply to it (possibly, asynchronously).
  base::Value message_value = ParseJsonToValue(message);
  CHECK(message_value.is_list());
  CHECK(message_value.GetList().size());
  CHECK(message_value.GetList()[0].is_string());
  const std::string& request_type = message_value.GetList()[0].GetString();
  ReplyToJsCallback send_reply_to_js_callback =
      base::BindOnce(&SendReplyToJs, &message_listener_);
  if (request_type == "getCertificates") {
    CHECK_EQ(message_value.GetList().size(), 1U);
    HandleCertificatesRequest(std::move(send_reply_to_js_callback));
  } else if (request_type == "onSignatureRequested") {
    CHECK_EQ(message_value.GetList().size(), 4U);
    HandleSignatureRequest(
        /*sign_request=*/message_value.GetList()[1],
        /*pin_status=*/message_value.GetList()[2],
        /*pin=*/message_value.GetList()[3],
        std::move(send_reply_to_js_callback));
  } else {
    LOG(FATAL) << "Unexpected JS message type: " << request_type;
  }
}

void TestCertificateProviderExtension::HandleCertificatesRequest(
    ReplyToJsCallback callback) {
  ++certificate_request_count_;
  base::Value cert_info_values(base::Value::Type::LIST);
  if (should_provide_certificates_)
    cert_info_values.Append(MakeClientCertificateInfoValue(*certificate_));
  std::move(callback).Run(cert_info_values);
}

void TestCertificateProviderExtension::HandleSignatureRequest(
    const base::Value& sign_request,
    const base::Value& pin_status,
    const base::Value& pin,
    ReplyToJsCallback callback) {
  CHECK_EQ(*sign_request.FindKey("certificate"),
           ConvertBytesToValue(GetCertDer(*certificate_)));
  const std::string pin_status_string = pin_status.GetString();
  const std::string pin_string = pin.GetString();

  const int sign_request_id = sign_request.FindKey("signRequestId")->GetInt();
  const std::vector<uint8_t> input =
      ExtractBytesFromValue(*sign_request.FindKey("input"));

  const extensions::api::certificate_provider::Algorithm algorithm =
      extensions::api::certificate_provider::ParseAlgorithm(
          sign_request.FindKey("algorithm")->GetString());
  int openssl_signature_algorithm = 0;
  if (algorithm == extensions::api::certificate_provider::Algorithm::
                       ALGORITHM_RSASSA_PKCS1_V1_5_SHA256) {
    openssl_signature_algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  } else if (algorithm == extensions::api::certificate_provider::Algorithm::
                              ALGORITHM_RSASSA_PKCS1_V1_5_SHA1) {
    openssl_signature_algorithm = SSL_SIGN_RSA_PKCS1_SHA1;
  } else {
    LOG(FATAL) << "Unexpected signature request algorithm: " << algorithm;
  }

  if (should_fail_sign_digest_requests_) {
    // Simulate a failure.
    std::move(callback).Run(/*response=*/base::Value());
    return;
  }

  base::Value response(base::Value::Type::DICTIONARY);
  if (required_pin_.has_value()) {
    if (pin_status_string == "not_requested") {
      // The PIN is required but not specified yet, so request it via the JS
      // side before generating the signature.
      base::Value pin_request_parameters(base::Value::Type::DICTIONARY);
      pin_request_parameters.SetIntKey("signRequestId", sign_request_id);
      if (remaining_pin_attempts_ == 0) {
        pin_request_parameters.SetStringKey("errorType",
                                            "MAX_ATTEMPTS_EXCEEDED");
      }
      response.SetKey("requestPin", std::move(pin_request_parameters));
      std::move(callback).Run(response);
      return;
    }
    if (remaining_pin_attempts_ == 0) {
      // The error about the lockout is already displayed, so fail immediately.
      std::move(callback).Run(/*response=*/base::Value());
      return;
    }
    if (pin_status_string == "canceled" ||
        base::StartsWith(pin_status_string,
                         "failed:", base::CompareCase::SENSITIVE)) {
      // The PIN request failed.
      LOG(WARNING) << "PIN request failed: " << pin_status_string;
      // Respond with a failure.
      std::move(callback).Run(/*response=*/base::Value());
      return;
    }
    DCHECK_EQ(pin_status_string, "ok");
    if (pin_string != *required_pin_) {
      // The entered PIN is wrong, so decrement the remaining attempt count, and
      // update the PIN dialog with displaying an error.
      if (remaining_pin_attempts_ > 0)
        --remaining_pin_attempts_;
      base::Value pin_request_parameters(base::Value::Type::DICTIONARY);
      pin_request_parameters.SetIntKey("signRequestId", sign_request_id);
      pin_request_parameters.SetStringKey(
          "errorType", remaining_pin_attempts_ == 0 ? "MAX_ATTEMPTS_EXCEEDED"
                                                    : "INVALID_PIN");
      if (remaining_pin_attempts_ > 0) {
        pin_request_parameters.SetIntKey("attemptsLeft",
                                         remaining_pin_attempts_);
      }
      response.SetKey("requestPin", std::move(pin_request_parameters));
      std::move(callback).Run(response);
      return;
    }
    // The entered PIN is correct. Stop the PIN request and proceed to
    // generating the signature.
    base::Value stop_pin_request_parameters(base::Value::Type::DICTIONARY);
    stop_pin_request_parameters.SetIntKey("signRequestId", sign_request_id);
    response.SetKey("stopPinRequest", std::move(stop_pin_request_parameters));
  }
  // Generate and return a valid signature.
  std::vector<uint8_t> signature;
  CHECK(RsaSignRawData(private_key_.get(), openssl_signature_algorithm, input,
                       &signature));
  response.SetKey("signature", ConvertBytesToValue(signature));
  std::move(callback).Run(response);
}

}  // namespace ash
