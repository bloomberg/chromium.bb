// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_CLIENT_H_
#define CONTENT_ANALYSIS_BROWSER_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_CLIENT_H_

#include <memory>
#include <string>

#include "content_analysis/sdk/analysis.pb.h"

// This is the main include file for code using Content Analysis Connector
// Client SDK.  No other include is needed.
//
// A browser begins by creating an instance of Client using the factory
// function Client::Create().  This instance should live as long as the browser
// intends to send content analysis requests to the content analysis agent.

namespace content_analysis {
namespace sdk {

// Represents a client that can request content analysis for locally running
// agent.  This class holds the client endpoint that the browser connects
// with when content analysis is required.
//
// The `uri` argument to the constuctor represent a specific local content
// analysis partner.  Each partner uses a specific and unique URI.
//
// See the demo directory for an example of how to use this class.
class Client {
 public:
   // Configuration options where creating an agent.  `name` is used to create
   // a channel between the agent and Google Chrome.  
   struct Config {
     // Used to create a channel between the agent and Google Chrome.  Both must
     // use the same name to properly rendezvous with each other.  The channel
     // is platform specific.
     std::string name;

     // Set to true if there is a different agent instance per OS user.  Defaults
     // to false.
     bool user_specific = false;
   };

  // Returns a new client instance and calls Start().
  static std::unique_ptr<Client> Create(Config config);

  virtual ~Client() = default;

  // Returns the configuration parameters of the client.
  virtual const Config& GetConfig() const = 0;

  // Sets an analysis request to the agent and waits for a response.
  virtual int Send(const ContentAnalysisRequest& request,
                   ContentAnalysisResponse* response) = 0;

 protected:
  Client() = default;
  Client(const Client& rhs) = delete;
  Client(Client&& rhs) = delete;
  Client& operator=(const Client& rhs) = delete;
  Client& operator=(Client&& rhs) = delete;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_CLIENT_H_
