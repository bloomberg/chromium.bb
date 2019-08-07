// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CONTEXTUAL_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_CONTEXTUAL_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/access_token_info.h"
#include "url/gurl.h"

namespace base {
class Time;
}
namespace identity {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace identity
class GoogleServiceAuthError;
class TemplateURLService;

namespace network {
struct ResourceRequest;
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// A service to fetch suggestions from a remote endpoint given a URL.
class ContextualSuggestionsService : public KeyedService {
 public:
  // |identity_manager| may be null but only unauthenticated requests will
  // issued.
  ContextualSuggestionsService(
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~ContextualSuggestionsService() override;

  using StartCallback = base::OnceCallback<void(
      std::unique_ptr<network::SimpleURLLoader> loader)>;

  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              std::unique_ptr<std::string> response_body)>;

  // Creates a loader for contextual suggestions for |current_url| and passes
  // the loader to |start_callback|. It uses a number of signals to create the
  // loader, including field trial / experimental parameters, and it may
  // pass a nullptr to |start_callback| (see below for details).
  //
  // |current_url| may be empty, in which case the system will never use the
  // experimental suggestions service. It's possible the non-experimental
  // service may decide to offer general-purpose suggestions.
  //
  // |visit_time| is the time of the visit for the URL for which suggestions
  // should be fetched.
  //
  // |input| is the current user input of the autocomplete query, whose
  // |current_page_classification| will be used to build the suggestion url.
  //
  // |template_url_service| may be null, but some services may be disabled.
  //
  // |start_callback| is called to transfer ownership of the created loader to
  //  whatever function/class receives the callback.
  // |start_callback| is called with a nullptr argument if:
  //   * The service is waiting for a previously-requested authentication token.
  //   * The authentication token had any issues.
  //
  // |completion_callback| will be invoked when the transfer is done.
  //
  // This method sends a request for an OAuth2 token and temporarily
  // instantiates |token_fetcher_|.
  void CreateContextualSuggestionsRequest(
      const std::string& current_url,
      const base::Time& visit_time,
      const AutocompleteInput& input,
      const TemplateURLService* template_url_service,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  // Advises the service to stop any process that creates a suggestion request.
  void StopCreatingContextualSuggestionsRequest();

  // Returns a URL representing the address of the server where the zero suggest
  // request is being sent. Does not take into account whether sending this
  // request is prohibited (e.g. in an incognito window).
  // Returns an invalid URL (i.e.: GURL::is_valid() == false) in case of an
  // error.
  //
  // |current_url| is the page the user is currently browsing and may be empty.
  //
  // |input| is the current user input of the autocomplete query, whose
  // |current_page_classification| will be used to build the suggestion url.
  //
  // Note that this method is public and is also used by ZeroSuggestProvider for
  // suggestions that do not take |current_url| into consideration.
  static GURL ContextualSuggestionsUrl(
      const std::string& current_url,
      const AutocompleteInput& input,
      const TemplateURLService* template_url_service);

 private:
  // Returns a URL representing the address of the server where the zero suggest
  // requests are being redirected when serving experimental suggestions.
  //
  // Returns a valid URL only if all the folowing conditions are true:
  // * The |current_url| is non-empty.
  // * The |default_provider| is Google.
  // * The user is in the ZeroSuggestRedirectToChrome field trial.
  // * The field trial provides a valid server address where the suggest request
  //   is redirected.
  // * The suggest request is over HTTPS. This avoids leaking the current page
  //   URL or personal data in unencrypted network traffic.
  // Note: these checks are in addition to CanSendUrl() on the default
  // contextual suggestion URL.
  GURL ExperimentalContextualSuggestionsUrl(
      const std::string& current_url,
      const TemplateURLService* template_url_service) const;

  // Upon succesful creation of an HTTP GET request for default contextual
  // suggestions, the |callback| function is run with the HTTP GET request as a
  // parameter.
  //
  // This function is called by CreateContextualSuggestionsRequest. See its
  // function definition for details on the parameters.
  void CreateDefaultRequest(const std::string& current_url,
                            const AutocompleteInput& input,
                            const TemplateURLService* template_url_service,
                            StartCallback start_callback,
                            CompletionCallback completion_callback);

  // Upon succesful creation of an HTTP POST request for experimental contextual
  // suggestions, the |callback| function is run with the HTTP POST request as a
  // parameter.
  //
  // This function is called by CreateContextualSuggestionsRequest. See its
  // function definition for details on the parameters.
  void CreateExperimentalRequest(const std::string& current_url,
                                 const base::Time& visit_time,
                                 const GURL& suggest_url,
                                 StartCallback start_callback,
                                 CompletionCallback completion_callback);

  // Tries to load the suggestions from network, using the token when available.
  //
  // |request| is expected to be the request for suggestions filled in with
  // everything but maybe the authentication token and the request body.
  //
  // |request_body|, if not empty, will be attached as the upload body to
  // the request.
  //
  // |traffic_annotation| is the appropriate annotations for making the network
  // request to load the suggestions.
  //
  // |start_callback|, |completion_callback| are the same as passed to
  // CreateContextualSuggestionsRequest()
  //
  // |error| and |access_token| are the results of token request.
  void AccessTokenAvailable(std::unique_ptr<network::ResourceRequest> request,
                            std::string request_body,
                            net::NetworkTrafficAnnotationTag traffic_annotation,
                            StartCallback start_callback,
                            CompletionCallback completion_callback,
                            GoogleServiceAuthError error,
                            identity::AccessTokenInfo access_token_info);

  // Activates a loader for |request|, wiring it up to |completion_callback|,
  // and calls |start_callback|.  If |request_body| isn't empty, it will be
  // attached as upload bytes.
  void StartDownloadAndTransferLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  identity::IdentityManager* identity_manager_;

  // Helper for fetching OAuth2 access tokens. This is non-null when an access
  // token request is currently in progress.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsService);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CONTEXTUAL_SUGGESTIONS_SERVICE_H_
