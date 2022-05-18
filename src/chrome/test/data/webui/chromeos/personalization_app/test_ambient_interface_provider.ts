// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AmbientModeAlbum, AmbientObserverInterface, AmbientObserverRemote, AmbientProviderInterface, AnimationTheme, TemperatureUnit, TopicSource} from 'chrome://personalization/trusted/personalization_app.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAmbientProvider extends TestBrowserProxy implements
    AmbientProviderInterface {
  public albums: AmbientModeAlbum[] = [
    {
      id: '0',
      checked: false,
      title: '0',
      description: '0',
      numberOfPhotos: 0,
      topicSource: TopicSource.kArtGallery,
      url: {url: 'http://test_url0'}
    },
    {
      id: '1',
      checked: false,
      title: '1',
      description: '1',
      numberOfPhotos: 0,
      topicSource: TopicSource.kArtGallery,
      url: {url: 'http://test_url1'}
    },
    {
      id: '2',
      checked: true,
      title: '2',
      description: '2',
      numberOfPhotos: 0,
      topicSource: TopicSource.kArtGallery,
      url: {url: 'http://test_url2'}
    },
    {
      id: '3',
      checked: true,
      title: '3',
      description: '3',
      numberOfPhotos: 1,
      topicSource: TopicSource.kGooglePhotos,
      url: {url: 'http://test_url3'}
    }
  ];

  public googlePhotosAlbumsPreviews: Url[] = [
    {url: 'http://preview0'},
    {url: 'http://preview1'},
    {url: 'http://preview2'},
    {url: 'http://preview#'},
  ];

  constructor() {
    super([
      'isAmbientModeEnabled',
      'setAmbientObserver',
      'setAmbientModeEnabled',
      'setAnimationTheme',
      'setPageViewed',
      'setTopicSource',
      'setTemperatureUnit',
      'setAlbumSelected',
    ]);
  }

  ambientObserverRemote: AmbientObserverInterface|null = null;

  isAmbientModeEnabled(): Promise<{enabled: boolean}> {
    this.methodCalled('isAmbientModeEnabled');
    return Promise.resolve({enabled: false});
  }

  setAmbientObserver(remote: AmbientObserverRemote) {
    this.methodCalled('setAmbientObserver', remote);
    this.ambientObserverRemote = remote;
  }

  // Test only function to update the ambient observer.
  updateAmbientObserver() {
    this.ambientObserverRemote!.onAmbientModeEnabledChanged(
        /*ambientModeEnabled=*/ true);

    this.ambientObserverRemote!.onAlbumsChanged(this.albums);
    this.ambientObserverRemote!.onAnimationThemeChanged(
        AnimationTheme.kSlideshow);
    this.ambientObserverRemote!.onTopicSourceChanged(TopicSource.kArtGallery);
    this.ambientObserverRemote!.onTemperatureUnitChanged(
        TemperatureUnit.kFahrenheit);
    this.ambientObserverRemote!.onGooglePhotosAlbumsPreviewsFetched(
        this.googlePhotosAlbumsPreviews);
  }

  setAmbientModeEnabled(ambientModeEnabled: boolean) {
    this.methodCalled('setAmbientModeEnabled', ambientModeEnabled);
  }

  setAnimationTheme(animationTheme: AnimationTheme) {
    this.methodCalled('setAnimationTheme', animationTheme);
  }

  setTopicSource(topicSource: TopicSource) {
    this.methodCalled('setTopicSource', topicSource);
  }

  setTemperatureUnit(temperatureUnit: TemperatureUnit) {
    this.methodCalled('setTemperatureUnit', temperatureUnit);
  }

  setAlbumSelected(id: string, topicSource: TopicSource, selected: boolean) {
    this.methodCalled('setAlbumSelected', id, topicSource, selected);
  }

  setPageViewed() {
    this.methodCalled('setPageViewed');
  }
}
