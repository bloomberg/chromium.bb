// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_tracker.h"

#include <algorithm>
#include <utility>

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace search_provider_logos {

namespace {

const int64_t kMaxDownloadBytes = 1024 * 1024;
const int kDecodeLogoTimeoutSeconds = 30;

// Implements a callback for image_fetcher::ImageDecoder. If Run() is called on
// a callback returned by GetCallback() within 30 seconds, forwards the decoded
// image to the wrapped callback. If not, sends an empty image to the wrapped
// callback instead. Either way, deletes the object and prevents further calls.
//
// TODO(sfiera): find a more idiomatic way of setting a deadline on the
// callback. This is implemented as a self-deleting object in part because it
// needed to when it used to be a delegate and in part because I couldn't figure
// out a better way, now that it isn't.
class ImageDecodedHandlerWithTimeout {
 public:
  static base::Callback<void(const gfx::Image&)> Wrap(
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback) {
    auto* handler = new ImageDecodedHandlerWithTimeout(image_decoded_callback);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ImageDecodedHandlerWithTimeout::OnImageDecoded,
                       handler->weak_ptr_factory_.GetWeakPtr(), gfx::Image()),
        base::TimeDelta::FromSeconds(kDecodeLogoTimeoutSeconds));
    return base::Bind(&ImageDecodedHandlerWithTimeout::OnImageDecoded,
                      handler->weak_ptr_factory_.GetWeakPtr());
  }

 private:
  explicit ImageDecodedHandlerWithTimeout(
      const base::Callback<void(const SkBitmap&)>& image_decoded_callback)
      : image_decoded_callback_(image_decoded_callback),
        weak_ptr_factory_(this) {}

  void OnImageDecoded(const gfx::Image& decoded_image) {
    image_decoded_callback_.Run(decoded_image.AsBitmap());
    delete this;
  }

  base::Callback<void(const SkBitmap&)> image_decoded_callback_;
  base::WeakPtrFactory<ImageDecodedHandlerWithTimeout> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodedHandlerWithTimeout);
};

// Returns whether the metadata for the cached logo indicates that the logo is
// OK to show, i.e. it's not expired or it's allowed to be shown temporarily
// after expiration.
bool IsLogoOkToShow(const LogoMetadata& metadata, base::Time now) {
  base::TimeDelta offset =
      base::TimeDelta::FromMilliseconds(kMaxTimeToLiveMS * 3 / 2);
  base::Time distant_past = now - offset;
  // Sanity check so logos aren't accidentally cached forever.
  if (metadata.expiration_time < distant_past) {
    return false;
  }
  return metadata.can_show_after_expiration || metadata.expiration_time >= now;
}

// Reads the logo from the cache and returns it. Returns NULL if the cache is
// empty, corrupt, expired, or doesn't apply to the current logo URL.
std::unique_ptr<EncodedLogo> GetLogoFromCacheOnFileThread(LogoCache* logo_cache,
                                                          const GURL& logo_url,
                                                          base::Time now) {
  const LogoMetadata* metadata = logo_cache->GetCachedLogoMetadata();
  if (!metadata)
    return nullptr;

  if (metadata->source_url != logo_url || !IsLogoOkToShow(*metadata, now)) {
    logo_cache->SetCachedLogo(nullptr);
    return nullptr;
  }

  return logo_cache->GetCachedLogo();
}

void NotifyAndClear(std::vector<EncodedLogoCallback>* encoded_callbacks,
                    std::vector<LogoCallback>* decoded_callbacks,
                    LogoCallbackReason type,
                    const EncodedLogo* encoded_logo,
                    const Logo* decoded_logo) {
  auto opt_encoded_logo =
      encoded_logo ? base::Optional<EncodedLogo>(*encoded_logo) : base::nullopt;
  for (EncodedLogoCallback& callback : *encoded_callbacks) {
    std::move(callback).Run(type, opt_encoded_logo);
  }
  encoded_callbacks->clear();

  auto opt_decoded_logo =
      decoded_logo ? base::Optional<Logo>(*decoded_logo) : base::nullopt;
  for (LogoCallback& callback : *decoded_callbacks) {
    std::move(callback).Run(type, opt_decoded_logo);
  }
  decoded_callbacks->clear();
}

}  // namespace

LogoTracker::LogoTracker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
    std::unique_ptr<LogoCache> logo_cache,
    base::Clock* clock)
    : is_idle_(true),
      is_cached_logo_valid_(false),
      image_decoder_(std::move(image_decoder)),
      cache_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      logo_cache_(logo_cache.release(),
                  base::OnTaskRunnerDeleter(cache_task_runner_)),
      clock_(clock),
      url_loader_factory_(std::move(url_loader_factory)),
      weak_ptr_factory_(this) {}

LogoTracker::~LogoTracker() {
  ReturnToIdle(kDownloadOutcomeNotTracked);
}

void LogoTracker::SetServerAPI(
    const GURL& logo_url,
    const ParseLogoResponse& parse_logo_response_func,
    const AppendQueryparamsToLogoURL& append_queryparams_func) {
  if (logo_url == logo_url_)
    return;

  ReturnToIdle(kDownloadOutcomeNotTracked);

  logo_url_ = logo_url;
  parse_logo_response_func_ = parse_logo_response_func;
  append_queryparams_func_ = append_queryparams_func;
}

void LogoTracker::GetLogo(LogoCallbacks callbacks) {
  DCHECK(!logo_url_.is_empty());
  DCHECK(callbacks.on_cached_decoded_logo_available ||
         callbacks.on_cached_encoded_logo_available ||
         callbacks.on_fresh_decoded_logo_available ||
         callbacks.on_fresh_encoded_logo_available);

  if (callbacks.on_cached_encoded_logo_available) {
    on_cached_encoded_logo_.push_back(
        std::move(callbacks.on_cached_encoded_logo_available));
  }
  if (callbacks.on_cached_decoded_logo_available) {
    on_cached_decoded_logo_.push_back(
        std::move(callbacks.on_cached_decoded_logo_available));
  }
  if (callbacks.on_fresh_encoded_logo_available) {
    on_fresh_encoded_logo_.push_back(
        std::move(callbacks.on_fresh_encoded_logo_available));
  }
  if (callbacks.on_fresh_decoded_logo_available) {
    on_fresh_decoded_logo_.push_back(
        std::move(callbacks.on_fresh_decoded_logo_available));
  }

  if (is_idle_) {
    is_idle_ = false;
    base::PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&GetLogoFromCacheOnFileThread,
                   base::Unretained(logo_cache_.get()), logo_url_,
                   clock_->Now()),
        base::Bind(&LogoTracker::OnCachedLogoRead,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (is_cached_logo_valid_) {
    NotifyAndClear(&on_cached_encoded_logo_, &on_cached_decoded_logo_,
                   LogoCallbackReason::DETERMINED, cached_encoded_logo_.get(),
                   cached_logo_.get());
  }
}

void LogoTracker::ClearCachedLogo() {
  // First cancel any fetch that might be ongoing.
  ReturnToIdle(kDownloadOutcomeNotTracked);
  // Then clear any cached logo.
  SetCachedLogo(nullptr);
}

void LogoTracker::ReturnToIdle(int outcome) {
  if (outcome != kDownloadOutcomeNotTracked) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoDownloadOutcome",
                              static_cast<LogoDownloadOutcome>(outcome),
                              DOWNLOAD_OUTCOME_COUNT);
  }
  // Cancel the current asynchronous operation, if any.
  loader_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset state.
  is_idle_ = true;
  cached_logo_.reset();
  cached_encoded_logo_.reset();
  is_cached_logo_valid_ = false;

  // Clear callbacks.
  NotifyAndClear(&on_cached_encoded_logo_, &on_cached_decoded_logo_,
                 LogoCallbackReason::CANCELED, nullptr, nullptr);
  NotifyAndClear(&on_fresh_encoded_logo_, &on_fresh_decoded_logo_,
                 LogoCallbackReason::CANCELED, nullptr, nullptr);
}

void LogoTracker::OnCachedLogoRead(std::unique_ptr<EncodedLogo> cached_logo) {
  DCHECK(!is_idle_);

  if (cached_logo) {
    // Store the value of logo->encoded_image for use below. This ensures that
    // logo->encoded_image is evaulated before base::Passed(&logo), which sets
    // logo to NULL.
    scoped_refptr<base::RefCountedString> encoded_image =
        cached_logo->encoded_image;
    image_decoder_->DecodeImage(
        encoded_image->data(), gfx::Size(),  // No particular size desired.
        ImageDecodedHandlerWithTimeout::Wrap(base::Bind(
            &LogoTracker::OnCachedLogoAvailable, weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&cached_logo))));
  } else {
    OnCachedLogoAvailable({}, SkBitmap());
  }
}

void LogoTracker::OnCachedLogoAvailable(
    std::unique_ptr<EncodedLogo> encoded_logo,
    const SkBitmap& image) {
  DCHECK(!is_idle_);

  if (!image.isNull()) {
    cached_logo_.reset(new Logo());
    cached_logo_->metadata = encoded_logo->metadata;
    cached_logo_->image = image;
    cached_encoded_logo_ = std::move(encoded_logo);
  }
  is_cached_logo_valid_ = true;
  NotifyAndClear(&on_cached_encoded_logo_, &on_cached_decoded_logo_,
                 LogoCallbackReason::DETERMINED, cached_encoded_logo_.get(),
                 cached_logo_.get());
  FetchLogo();
}

void LogoTracker::SetCachedLogo(std::unique_ptr<EncodedLogo> logo) {
  cache_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LogoCache::SetCachedLogo,
                                base::Unretained(logo_cache_.get()),
                                base::Owned(logo.release())));
}

void LogoTracker::SetCachedMetadata(const LogoMetadata& metadata) {
  cache_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LogoCache::UpdateCachedLogoMetadata,
                                base::Unretained(logo_cache_.get()), metadata));
}

void LogoTracker::FetchLogo() {
  DCHECK(!loader_);
  DCHECK(!is_idle_);

  std::string fingerprint;
  if (cached_logo_ && !cached_logo_->metadata.fingerprint.empty() &&
      cached_logo_->metadata.expiration_time >= clock_->Now()) {
    fingerprint = cached_logo_->metadata.fingerprint;
  }
  GURL url = append_queryparams_func_.Run(logo_url_, fingerprint);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("logo_tracker", R"(
        semantics {
          sender: "Logo Tracker"
          description:
            "Provides the logo image (aka Doodle) if Google is your configured "
            "search provider."
          trigger: "Displaying the new tab page on iOS or Android."
          data:
            "Logo ID, and the user's Google cookies to show for example "
            "birthday doodles at appropriate times."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Choosing a non-Google search engine in Chromium settings under "
            "'Search Engine' will disable this feature."
          policy_exception_justification:
            "Not implemented, considered not useful as it does not upload any"
            "data and just downloads a logo image."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  // TODO(https://crbug.com/808498) re-add data use measurement once
  // SimpleURLLoader supports it:
  // data_use_measurement::DataUseUserData::SEARCH_PROVIDER_LOGOS
  loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&LogoTracker::OnURLLoadComplete, base::Unretained(this),
                     loader_.get()),
      kMaxDownloadBytes);
  logo_download_start_time_ = base::TimeTicks::Now();
}

void LogoTracker::OnFreshLogoParsed(bool* parsing_failed,
                                    bool from_http_cache,
                                    std::unique_ptr<EncodedLogo> logo) {
  DCHECK(!is_idle_);

  if (logo)
    logo->metadata.source_url = logo_url_;

  if (!logo || !logo->encoded_image) {
    OnFreshLogoAvailable(std::move(logo), /*download_failed=*/false,
                         *parsing_failed, from_http_cache, SkBitmap());
  } else {
    // Store the value of logo->encoded_image for use below. This ensures that
    // logo->encoded_image is evaulated before base::Passed(&logo), which sets
    // logo to NULL.
    scoped_refptr<base::RefCountedString> encoded_image = logo->encoded_image;
    image_decoder_->DecodeImage(
        encoded_image->data(), gfx::Size(),  // No particular size desired.
        ImageDecodedHandlerWithTimeout::Wrap(base::Bind(
            &LogoTracker::OnFreshLogoAvailable, weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&logo), /*download_failed=*/false, *parsing_failed,
            from_http_cache)));
  }
}

void LogoTracker::OnFreshLogoAvailable(
    std::unique_ptr<EncodedLogo> encoded_logo,
    bool download_failed,
    bool parsing_failed,
    bool from_http_cache,
    const SkBitmap& image) {
  DCHECK(!is_idle_);

  LogoDownloadOutcome download_outcome = DOWNLOAD_OUTCOME_COUNT;
  std::unique_ptr<Logo> logo;

  if (download_failed) {
    download_outcome = DOWNLOAD_OUTCOME_DOWNLOAD_FAILED;
  } else if (encoded_logo && !encoded_logo->encoded_image && cached_logo_ &&
             !encoded_logo->metadata.fingerprint.empty() &&
             encoded_logo->metadata.fingerprint ==
                 cached_logo_->metadata.fingerprint) {
    // The cached logo was revalidated, i.e. its fingerprint was verified.
    // mime_type isn't sent when revalidating, so copy it from the cached logo.
    encoded_logo->metadata.mime_type = cached_logo_->metadata.mime_type;
    SetCachedMetadata(encoded_logo->metadata);
    download_outcome = DOWNLOAD_OUTCOME_LOGO_REVALIDATED;
  } else if (encoded_logo && image.isNull()) {
    // Image decoding failed. Do nothing.
    download_outcome = DOWNLOAD_OUTCOME_DECODING_FAILED;
  } else {
    // Check if the server returned a valid, non-empty response.
    if (encoded_logo) {
      UMA_HISTOGRAM_BOOLEAN("NewTabPage.LogoImageDownloaded", from_http_cache);

      DCHECK(!image.isNull());
      logo.reset(new Logo());
      logo->metadata = encoded_logo->metadata;
      logo->image = image;
    }

    if (logo) {
      download_outcome = DOWNLOAD_OUTCOME_NEW_LOGO_SUCCESS;
    } else {
      if (parsing_failed)
        download_outcome = DOWNLOAD_OUTCOME_PARSING_FAILED;
      else
        download_outcome = DOWNLOAD_OUTCOME_NO_LOGO_TODAY;
    }
  }

  LogoCallbackReason callback_type = LogoCallbackReason::FAILED;
  switch (download_outcome) {
    case DOWNLOAD_OUTCOME_NEW_LOGO_SUCCESS:
      DCHECK(encoded_logo);
      DCHECK(logo);
      callback_type = LogoCallbackReason::DETERMINED;
      break;

    case DOWNLOAD_OUTCOME_PARSING_FAILED:
    case DOWNLOAD_OUTCOME_NO_LOGO_TODAY:
      // Clear the cached logo if it was non-null. Otherwise, report this as a
      // revalidation of "no logo".
      DCHECK(!encoded_logo);
      DCHECK(!logo);
      if (cached_logo_) {
        callback_type = LogoCallbackReason::DETERMINED;
      } else {
        callback_type = LogoCallbackReason::REVALIDATED;
      }
      break;

    case DOWNLOAD_OUTCOME_DOWNLOAD_FAILED:
      // In the download failed, don't notify the callback at all, since the
      // callback should continue to use the cached logo.
      DCHECK(!encoded_logo);
      DCHECK(!logo);
      callback_type = LogoCallbackReason::FAILED;
      break;

    case DOWNLOAD_OUTCOME_DECODING_FAILED:
      DCHECK(encoded_logo);
      DCHECK(!logo);
      encoded_logo.reset();
      callback_type = LogoCallbackReason::FAILED;
      break;

    case DOWNLOAD_OUTCOME_LOGO_REVALIDATED:
      // In the server reported that the cached logo is still current, don't
      // notify the callback at all, since the callback should continue to use
      // the cached logo.
      DCHECK(encoded_logo);
      DCHECK(!logo);
      callback_type = LogoCallbackReason::REVALIDATED;
      break;

    case DOWNLOAD_OUTCOME_COUNT:
      NOTREACHED();
      return;
  }

  NotifyAndClear(&on_fresh_encoded_logo_, &on_fresh_decoded_logo_,
                 callback_type, encoded_logo.get(), logo.get());

  switch (callback_type) {
    case LogoCallbackReason::DETERMINED:
      SetCachedLogo(std::move(encoded_logo));
      break;

    default:
      break;
  }

  ReturnToIdle(download_outcome);
}

void LogoTracker::OnURLLoadComplete(const network::SimpleURLLoader* source,
                                    std::unique_ptr<std::string> body) {
  DCHECK(!is_idle_);
  std::unique_ptr<network::SimpleURLLoader> cleanup_loader(loader_.release());

  if (source->NetError() != net::OK) {
    OnFreshLogoAvailable({}, /*download_failed=*/true, false, false,
                         SkBitmap());
    return;
  }

  if (!source->ResponseInfo() || !source->ResponseInfo()->headers ||
      source->ResponseInfo()->headers->response_code() != net::HTTP_OK) {
    OnFreshLogoAvailable({}, /*download_failed=*/true, false, false,
                         SkBitmap());
    return;
  }

  UMA_HISTOGRAM_TIMES("NewTabPage.LogoDownloadTime",
                      base::TimeTicks::Now() - logo_download_start_time_);

  std::unique_ptr<std::string> response =
      body ? std::move(body) : std::make_unique<std::string>();
  base::Time response_time = clock_->Now();

  bool from_http_cache = !source->ResponseInfo()->network_accessed;

  bool* parsing_failed = new bool(false);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(parse_logo_response_func_, std::move(response),
                     response_time, parsing_failed),
      base::BindOnce(&LogoTracker::OnFreshLogoParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(parsing_failed), from_http_cache));
}

}  // namespace search_provider_logos
