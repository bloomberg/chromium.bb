// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.module('MediaSourceTest');
goog.setTestOnly('MediaSourceTest');

const MediaSource = goog.require('mr.dial.MediaSource');

describe('Tests MediaSource', function() {
  it('Does not create from empty input', function() {
    expect(MediaSource.create('')).toBeNull();
  });

  it('Creates from a valid URL', function() {
    expect(MediaSource.create(
               'https://www.youtube.com/tv#__dialAppName__=YouTube'))
        .toEqual(new MediaSource('YouTube'));
  });

  it('Creates from a valid URL with launch parameters', function() {
    expect(MediaSource.create(
               'https://www.youtube.com/tv#' +
               '__dialAppName__=YouTube/__dialPostData__=dj0xMjM='))
        .toEqual(new MediaSource('YouTube', 'v=123'));
    expect(MediaSource.create(
               'https://www.youtube.com/tv#' +
               '__dialAppName__=YouTube/__dialPostData__=dj1NSnlKS3d6eEZwWQ=='))
        .toEqual(new MediaSource('YouTube', 'v=MJyJKwzxFpY'));
  });

  it('Does not create from an invalid URL', function() {
    expect(MediaSource.create(
               'https://www.youtube.com/tv#___emanPpaLiad__=YouTube'))
        .toBeNull();
  });

  it('Does not create from an invalid postData', function() {
    expect(MediaSource.create(
               'https://www.youtube.com/tv#___emanPpaLiad__=YouTube' +
               '/__dialPostData__=dj1=N'))
        .toBeNull();
  });

  it('Creates from DIAL URL', () => {
    expect(MediaSource.create('dial:YouTube'))
        .toEqual(new MediaSource('YouTube'));
    expect(MediaSource.create('dial:YouTube?foo=bar'))
        .toEqual(new MediaSource('YouTube'));
    expect(MediaSource.create('dial:YouTube?foo=bar&postData=dj0xMjM='))
        .toEqual(new MediaSource('YouTube', 'v=123'));
  });

  it('Does not create from invalid DIAL URL', () => {
    expect(MediaSource.create('dial:')).toBeNull();
    expect(MediaSource.create('dial://')).toBeNull();
    expect(MediaSource.create('dial://YouTube')).toBeNull();
    expect(MediaSource.create('dial:YouTube?postData=notEncodedProperly111'))
        .toBeNull();
  });

  it('Does not create from URL of unknown protocol', () => {
    expect(MediaSource.create('unknown:YouTube')).toBeNull();
  });
});
