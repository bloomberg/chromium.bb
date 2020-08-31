// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/task/thread_pool.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_key_filtering.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "services/network/trust_tokens/types.h"
#include "url/url_constants.h"

namespace network {

using Cryptographer = TrustTokenRequestIssuanceHelper::Cryptographer;

struct TrustTokenRequestIssuanceHelper::CryptographerAndBlindedTokens {
  std::unique_ptr<Cryptographer> cryptographer;
  base::Optional<std::string> blinded_tokens;
};

struct TrustTokenRequestIssuanceHelper::CryptographerAndUnblindedTokens {
  std::unique_ptr<Cryptographer> cryptographer;
  std::unique_ptr<Cryptographer::UnblindedTokens> unblinded_tokens;
};

namespace {

TrustTokenRequestIssuanceHelper::CryptographerAndBlindedTokens
BeginIssuanceOnPostedSequence(std::unique_ptr<Cryptographer> cryptographer,
                              int batch_size) {
  base::Optional<std::string> blinded_tokens =
      cryptographer->BeginIssuance(batch_size);
  return {std::move(cryptographer), std::move(blinded_tokens)};
}

TrustTokenRequestIssuanceHelper::CryptographerAndUnblindedTokens
ConfirmIssuanceOnPostedSequence(std::unique_ptr<Cryptographer> cryptographer,
                                std::string response_header) {
  std::unique_ptr<Cryptographer::UnblindedTokens> unblinded_tokens =
      cryptographer->ConfirmIssuance(response_header);
  return {std::move(cryptographer), std::move(unblinded_tokens)};
}

base::Value CreateLogValue(base::StringPiece outcome) {
  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetStringKey("outcome", outcome);
  return ret;
}

// Define convenience aliases for the NetLogEventTypes for brevity.
enum NetLogOp { kBegin, kFinalize };
void LogOutcome(const net::NetLogWithSource& log,
                NetLogOp begin_or_finalize,
                base::StringPiece outcome) {
  log.EndEvent(
      begin_or_finalize == kBegin
          ? net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_ISSUANCE
          : net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE,
      [outcome]() { return CreateLogValue(outcome); });
}

}  // namespace

TrustTokenRequestIssuanceHelper::TrustTokenRequestIssuanceHelper(
    SuitableTrustTokenOrigin top_level_origin,
    TrustTokenStore* token_store,
    const TrustTokenKeyCommitmentGetter* key_commitment_getter,
    std::unique_ptr<Cryptographer> cryptographer,
    net::NetLogWithSource net_log)
    : top_level_origin_(std::move(top_level_origin)),
      token_store_(token_store),
      key_commitment_getter_(std::move(key_commitment_getter)),
      cryptographer_(std::move(cryptographer)),
      net_log_(std::move(net_log)) {
  DCHECK(token_store_);
  DCHECK(key_commitment_getter_);
  DCHECK(cryptographer_);
}

TrustTokenRequestIssuanceHelper::~TrustTokenRequestIssuanceHelper() = default;

TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens::
    UnblindedTokens() = default;
TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens::
    ~UnblindedTokens() = default;

void TrustTokenRequestIssuanceHelper::Begin(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(request);
  DCHECK(!request->initiator() ||
         IsOriginPotentiallyTrustworthy(*request->initiator()))
      << *request->initiator();

  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_ISSUANCE);

  issuer_ = SuitableTrustTokenOrigin::Create(request->url());
  if (!issuer_) {
    LogOutcome(net_log_, kBegin, "Unsuitable issuer URL");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
    return;
  }

  if (!token_store_->SetAssociation(*issuer_, top_level_origin_)) {
    LogOutcome(net_log_, kBegin, "Couldn't set issuer-toplevel association");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  if (token_store_->CountTokens(*issuer_) ==
      kTrustTokenPerIssuerTokenCapacity) {
    LogOutcome(net_log_, kBegin, "Tokens at capacity");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  key_commitment_getter_->Get(
      *issuer_,
      base::BindOnce(&TrustTokenRequestIssuanceHelper::OnGotKeyCommitment,
                     weak_ptr_factory_.GetWeakPtr(), request, std::move(done)));
}

void TrustTokenRequestIssuanceHelper::OnGotKeyCommitment(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  if (!commitment_result) {
    LogOutcome(net_log_, kBegin, "No keys for issuer");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  if (!commitment_result->batch_size ||
      !cryptographer_->Initialize(commitment_result->batch_size)) {
    LogOutcome(net_log_, kBegin,
               "Internal error initializing cryptography delegate");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  for (const mojom::TrustTokenVerificationKeyPtr& key :
       commitment_result->keys) {
    if (!cryptographer_->AddKey(key->body)) {
      LogOutcome(net_log_, kBegin, "Bad key");
      std::move(done).Run(
          mojom::TrustTokenOperationStatus::kFailedPrecondition);
      return;
    }
  }

  // Evict tokens signed with keys other than those from the issuer's most
  // recent commitments.
  token_store_->PruneStaleIssuerState(*issuer_, commitment_result->keys);

  int batch_size = std::min(commitment_result->batch_size,
                            kMaximumTrustTokenIssuanceBatchSize);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BeginIssuanceOnPostedSequence, std::move(cryptographer_),
                     batch_size),
      base::BindOnce(
          &TrustTokenRequestIssuanceHelper::OnDelegateBeginIssuanceCallComplete,
          weak_ptr_factory_.GetWeakPtr(), request, std::move(done)));
  // Logic continues... in the continuation
  // OnDelegateBeginIssuanceCallComplete; don't add more code here. In
  // particular, |cryptographer_| is empty at this point.
}

void TrustTokenRequestIssuanceHelper::OnDelegateBeginIssuanceCallComplete(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    CryptographerAndBlindedTokens cryptographer_and_blinded_tokens) {
  cryptographer_ = std::move(cryptographer_and_blinded_tokens.cryptographer);
  base::Optional<std::string>& maybe_blinded_tokens =
      cryptographer_and_blinded_tokens.blinded_tokens;  // Convenience alias

  if (!maybe_blinded_tokens) {
    LogOutcome(net_log_, kBegin, "Internal error generating blinded tokens");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }
  request->SetExtraRequestHeaderByName(kTrustTokensSecTrustTokenHeader,
                                       std::move(*maybe_blinded_tokens),
                                       /*overwrite=*/true);

  // We don't want cache reads, because the highest priority is to execute the
  // protocol operation by sending the server the Trust Tokens request header
  // and getting the corresponding response header, but we want cache writes
  // in case subsequent requests are made to the same URL in non-trust-token
  // settings.
  request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_CACHE);

  LogOutcome(net_log_, kBegin, "Success");
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

void TrustTokenRequestIssuanceHelper::Finalize(
    mojom::URLResponseHead* response,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(response);

  // A response headers object should be present on all responses for
  // https-scheme requests (which Trust Tokens requests are).
  DCHECK(response->headers);

  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE);

  std::string header_value;

  // EnumerateHeader(|iter|=nullptr) asks for the first instance of the header,
  // if any.
  if (!response->headers->EnumerateHeader(
          /*iter=*/nullptr, kTrustTokensSecTrustTokenHeader, &header_value)) {
    LogOutcome(net_log_, kFinalize, "Response missing Trust Tokens header");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  response->headers->RemoveHeader(kTrustTokensSecTrustTokenHeader);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ConfirmIssuanceOnPostedSequence,
                     std::move(cryptographer_), std::move(header_value)),
      base::BindOnce(&TrustTokenRequestIssuanceHelper::
                         OnDelegateConfirmIssuanceCallComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done)));
}

void TrustTokenRequestIssuanceHelper::OnDelegateConfirmIssuanceCallComplete(
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    CryptographerAndUnblindedTokens cryptographer_and_unblinded_tokens) {
  cryptographer_ = std::move(cryptographer_and_unblinded_tokens.cryptographer);
  std::unique_ptr<Cryptographer::UnblindedTokens>& maybe_tokens =
      cryptographer_and_unblinded_tokens.unblinded_tokens;  // Convenience alias

  if (!maybe_tokens) {
    LogOutcome(net_log_, kFinalize,
               "Response rejected during processing (perhaps malformed?)");

    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    std::move(done).Run(mojom::TrustTokenOperationStatus::kBadResponse);
    return;
  }

  token_store_->AddTokens(*issuer_, base::make_span(maybe_tokens->tokens),
                          maybe_tokens->body_of_verifying_key);

  net_log_.EndEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE,
      [num_obtained_tokens = maybe_tokens->tokens.size()]() {
        base::Value ret = CreateLogValue("Success");
        ret.SetIntKey("# tokens obtained", num_obtained_tokens);
        return ret;
      });
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
  return;
}

}  // namespace network
