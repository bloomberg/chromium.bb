// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_
#define CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_

class GURL;

class FlashEmbedRewrite {
 public:
  // Entry point that will then call a private website-specific method.
  static GURL RewriteFlashEmbedURL(const GURL&);

  // Used for UMA. Values should not be reorderer or reused.
  // SUCCESS refers to an embed properly rewritten. SUCCESS_PARAMS_REWRITE
  // refers to an embed rewritten with the params fixed. SUCCESS_ENABLEJSAPI
  // refers to a rewritten embed even though the JS API was enabled (Chrome
  // Android only). FAILURE_ENABLEJSAPI indicates the embed was not rewritten
  // because the JS API was enabled.
  enum class YouTubeRewriteStatus {
    kSuccess = 0,
    kSuccessParamsRewrite = 1,
    kSuccessEnableJSAPI = 2,
    kFailureEnableJSAPI = 3,
    kLast = kFailureEnableJSAPI,
  };

  static const char kFlashYouTubeRewriteUMA[];

 private:
  // YouTube specific method.
  static GURL RewriteYouTubeFlashEmbedURL(const GURL&);

  // Dailymotion specific method.
  static GURL RewriteDailymotionFlashEmbedURL(const GURL&);
};

#endif  // CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_
