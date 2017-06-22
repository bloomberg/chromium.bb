# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.story import expectations


class SystemHealthDesktopCommonExpectations(expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('browse:news:hackernews',
                      [expectations.ALL_WIN, expectations.ALL_MAC],
                      'crbug.com/676336')
    self.DisableStory('browse:search:google', [expectations.ALL_WIN],
                      'crbug.com/673775')
    self.DisableStory('browse:tools:maps', [expectations.ALL],
                      'crbug.com/712694')
    self.DisableStory('browse:tools:earth', [expectations.ALL],
                      'crbug.com/708590')
    self.DisableStory('play:media:google_play_music', [expectations.ALL],
                      'crbug.com/649392')
    self.DisableStory('play:media:soundcloud', [expectations.ALL_WIN],
                      'crbug.com/649392')
    self.DisableStory('play:media:pandora', [expectations.ALL],
                      'crbug.com/64939')
    self.DisableStory('browse:tech:discourse_infinite_scroll',
                      [expectations.ALL_WIN, expectations.ALL_LINUX],
                      'crbug.com/728152')
    self.DisableStory('browse:social:facebook_infinite_scroll',
                      [expectations.ALL_WIN], 'crbug.com/728152')
    self.DisableStory('browse:news:cnn',
                      [expectations.ALL_MAC], 'crbug.com/728576')
    self.DisableStory('browse:social:twitter_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728464')
    self.DisableStory('browse:media:flickr_infinite_scroll',
                      [expectations.ALL],
                      'crbug.com/728785')


class SystemHealthDesktopMemoryExpectations(expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('browse:news:hackernews',
                      [expectations.ALL_WIN, expectations.ALL_MAC],
                      'crbug.com/676336')
    self.DisableStory('browse:search:google', [expectations.ALL_WIN],
                      'crbug.com/673775')
    self.DisableStory('browse:tools:maps', [expectations.ALL],
                      'crbug.com/712694')
    self.DisableStory('browse:tools:earth', [expectations.ALL],
                      'crbug.com/708590')
    self.DisableStory('load:games:miniclip', [expectations.ALL_MAC],
                      'crbug.com/664661')
    self.DisableStory('play:media:google_play_music', [expectations.ALL],
                      'crbug.com/649392')
    self.DisableStory('play:media:soundcloud', [expectations.ALL_WIN],
                      'crbug.com/649392')
    self.DisableStory('play:media:pandora', [expectations.ALL],
                      'crbug.com/64939')
    self.DisableStory('browse:tech:discourse_infinite_scroll',
                      [expectations.ALL_WIN, expectations.ALL_LINUX],
                      'crbug.com/728152')
    self.DisableStory('browse:social:facebook_infinite_scroll',
                      [expectations.ALL_WIN], 'crbug.com/728152')
    self.DisableStory('browse:news:cnn',
                      [expectations.ALL_MAC], 'crbug.com/728576')
    self.DisableStory('browse:social:twitter_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728464')
    self.DisableStory('browse:media:flickr_infinite_scroll',
                      [expectations.ALL],
                      'crbug.com/728785')


class SystemHealthMobileCommonExpectations(expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('background:tools:gmail', [expectations.ALL_ANDROID],
                      'crbug.com/723783')
    self.DisableStory('browse:shopping:flipkart', [expectations.ALL_ANDROID],
                      'crbug.com/708300')
    self.DisableStory('browse:news:globo', [expectations.ALL_ANDROID],
                      'crbug.com/714650')
    self.DisableStory('load:tools:gmail', [expectations.ALL_ANDROID],
                      'crbug.com/657433')
    self.DisableStory('long_running:tools:gmail-background',
                      [expectations.ALL_ANDROID], 'crbug.com/726301')
    self.DisableStory('long_running:tools:gmail-foreground',
                      [expectations.ALL_ANDROID], 'crbug.com/726301')
    self.DisableStory('browse:news:toi', [expectations.ALL_ANDROID],
                      'crbug.com/728081')
    self.DisableStory('browse:social:facebook_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728152')
    self.DisableStory('browse:media:flickr_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728785')
    self.DisableStory('browse:chrome:newtab',
                      [expectations.ALL_ANDROID], 'crbug.com/735405')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('browse:chrome:omnibox',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have omnibox')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('browse:chrome:newtab',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have NTP')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('long_running:tools:gmail-background',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have tabs')
    self.DisableStory('browse:social:pinterest_infinite_scroll',
                      [expectations.ANDROID_WEBVIEW], 'crbug.com/728528')


class SystemHealthMobileMemoryExpectations(expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('background:tools:gmail', [expectations.ALL_ANDROID],
                      'crbug.com/723783')
    self.DisableStory('browse:shopping:flipkart', [expectations.ALL_ANDROID],
                      'crbug.com/708300')
    self.DisableStory('browse:news:globo', [expectations.ALL_ANDROID],
                      'crbug.com/714650')
    self.DisableStory('load:tools:gmail', [expectations.ALL_ANDROID],
                      'crbug.com/657433')
    self.DisableStory('long_running:tools:gmail-background',
                      [expectations.ALL_ANDROID], 'crbug.com/726301')
    self.DisableStory('long_running:tools:gmail-foreground',
                      [expectations.ALL_ANDROID], 'crbug.com/726301')
    self.DisableStory('browse:news:toi', [expectations.ALL_ANDROID],
                      'crbug.com/728081')
    self.DisableStory('browse:social:facebook_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728152')
    self.DisableStory('browse:media:flickr_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728785')
    self.DisableStory('browse:chrome:newtab',
                      [expectations.ALL_ANDROID], 'crbug.com/735405')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('browse:chrome:omnibox',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have omnibox')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('browse:chrome:newtab',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have NTP')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('long_running:tools:gmail-background',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have tabs')
    self.DisableStory('browse:social:pinterest_infinite_scroll',
                      [expectations.ANDROID_WEBVIEW], 'crbug.com/728528')


# Should only include browse:*:* stories.
class V8BrowsingDesktopExpecations(expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('browse:media:imgur',
                      [expectations.ALL_LINUX, expectations.ALL_WIN],
                      'crbug.com/673775')
    self.DisableStory('browse:media:pinterest',
                      [expectations.ALL_LINUX],
                      'crbug.com/735173')
    self.DisableStory('browse:media:youtube',
                      [expectations.ALL_LINUX],
                      'crbug.com/735173')
    self.DisableStory('browse:news:cnn',
                      [expectations.ALL_DESKTOP],
                      'crbug.com/735173')
    self.DisableStory('browse:news:flipboard',
                      [expectations.ALL_DESKTOP],
                      'crbug.com/735173')
    self.DisableStory('browse:news:hackernews',
                      [expectations.ALL_DESKTOP],
                      'crbug.com/735173')
    self.DisableStory('browse:news:nytimes',
                      [expectations.ALL_LINUX, expectations.ALL_WIN],
                      'crbug.com/735173')
    self.DisableStory('browse:news:reddit',
                      [expectations.ALL_LINUX, expectations.ALL_WIN],
                      'crbug.com/735173')
    self.DisableStory('browse:search:google',
                      [expectations.ALL_LINUX, expectations.ALL_WIN],
                      'crbug.com/735173')
    self.DisableStory('browse:search:google_india',
                      [expectations.ALL_LINUX],
                      'crbug.com/735173')
    self.DisableStory('browse:tools:maps', [expectations.ALL],
                      'crbug.com/712694')
    self.DisableStory('browse:tools:earth', [expectations.ALL],
                      'crbug.com/708590')
    self.DisableStory('browse:tech:discourse_infinite_scroll',
                      [expectations.ALL_WIN, expectations.ALL_LINUX],
                      'crbug.com/728152')
    self.DisableStory('browse:social:facebook_infinite_scroll',
                      [expectations.ALL_LINUX],
                      'crbug.com/735173')
    self.DisableStory('browse:social:twitter',
                      [expectations.ALL_LINUX],
                      'crbug.com/735173')
    self.DisableStory('browse:social:twitter_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728464')
    self.DisableStory('browse:media:flickr_infinite_scroll',
                      [expectations.ALL, expectations.ALL_WIN],
                      'crbug.com/728785')

# Should only include browse:*:* stories.
class V8BrowsingMobileExpecations(expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('browse:shopping:flipkart', [expectations.ALL_ANDROID],
                      'crbug.com/708300')
    self.DisableStory('browse:news:globo', [expectations.ALL_ANDROID],
                      'crbug.com/714650')
    self.DisableStory('browse:news:toi', [expectations.ALL_ANDROID],
                      'crbug.com/728081')
    self.DisableStory('browse:social:facebook_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728152')
    self.DisableStory('browse:media:flickr_infinite_scroll',
                      [expectations.ALL], 'crbug.com/728785')
    self.DisableStory('browse:chrome:newtab',
                      [expectations.ALL_ANDROID], 'crbug.com/735405')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('browse:chrome:omnibox',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have omnibox')
    # TODO(rnephew): This disabling should move to CanRunOnBrowser.
    self.DisableStory('browse:chrome:newtab',
                      [expectations.ANDROID_WEBVIEW],
                      'Webview does not have NTP')
    self.DisableStory('browse:social:pinterest_infinite_scroll',
                      [expectations.ANDROID_WEBVIEW], 'crbug.com/728528')


class SystemHealthWebviewStartupExpectations(expectations.StoryExpectations):
  def SetExpectations(self):
    pass # Nothing is disabled at this time.
