# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class KeyMobileSitesPage(page_module.Page):

  def __init__(self, url, page_set, name='', labels=None):
    super(KeyMobileSitesPage, self).__init__(
        url=url, page_set=page_set, name=name,
        credentials_path='data/credentials.json', labels=labels)
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/key_mobile_sites.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()

  def RunRepaint(self, action_runner):
    action_runner.RepaintContinuously(seconds=5)


class Page2(KeyMobileSitesPage):

  """ Why: Typical mobile business site """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url=('http://iphone.capitolvolkswagen.com/index.htm'
           '#new-inventory_p_2Fsb-new_p_2Ehtm_p_3Freset_p_3DInventoryListing'),
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='Next 35')
    action_runner.WaitForJavaScriptCondition(
        'document.body.scrollHeight > 2560')



class Page4(KeyMobileSitesPage):

  """ Why: Top tech blog """

  def __init__(self, page_set):
    super(Page4, self).__init__(
      # pylint: disable=C0301
      url='http://www.theverge.com/2012/10/28/3568746/amazon-7-inch-fire-hd-ipad-mini-ad-ballsy',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.Chorus !== undefined &&'
        'window.Chorus.Comments !== undefined &&'
        'window.Chorus.Comments.Json !== undefined &&'
        '(window.Chorus.Comments.loaded ||'
        ' window.Chorus.Comments.Json.load_comments())')


class Page5(KeyMobileSitesPage):

  """ Why: Top news site """

  def __init__(self, page_set):
    super(Page5, self).__init__(
      # pylint: disable=C0301
      url='http://www.cnn.com/2012/10/03/politics/michelle-obama-debate/index.html',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(8)



class Page7(KeyMobileSitesPage):

  """ Why: #1 (Alexa global) """

  def __init__(self, page_set):
    super(Page7, self).__init__(
      url='https://facebook.com/barackobama',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("u_0_c") !== null &&'
        'document.body.scrollHeight > window.innerHeight')


class Page8(KeyMobileSitesPage):

  """ Why: #3 (Alexa global) """

  def __init__(self, page_set):
    super(Page8, self).__init__(
      url='http://m.youtube.com/watch?v=9hBpF_Zj4OA',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("paginatortarget") !== null')


class Page11(KeyMobileSitesPage):

  """ Why: #12 (Alexa global),Public profile """

  def __init__(self, page_set):
    super(Page11, self).__init__(
      url='https://www.linkedin.com/in/linustorvalds',
      page_set=page_set,
      name='LinkedIn')

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("profile-view-scroller") !== null')



class Page16(KeyMobileSitesPage):

  """ Why: #1 Alexa reference """

  def __init__(self, page_set):
    super(Page16, self).__init__(
      # pylint: disable=C0301
      url='http://answers.yahoo.com/question/index?qid=20110117024343AAopj8f',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='Other Answers (1 - 20 of 149)')
    action_runner.ClickElement(text='Other Answers (1 - 20 of 149)')


class Page17(KeyMobileSitesPage):

  """ Why: productivity, top google properties """

  def __init__(self, page_set):
    super(Page17, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("og_user_warning") !== null')
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("og_user_warning") === null')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(element_function=(
        'document.getElementById("views").childNodes[1].firstChild'))
    interaction.End()
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(element_function=(
        'document.getElementById("views").childNodes[1].firstChild'))
    interaction.End()




class Page21(KeyMobileSitesPage):

  """ Why: crbug.com/172906 """

  def __init__(self, page_set):
    super(Page21, self).__init__(
      url='http://groupcloned.com',
      page_set=page_set)


  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(5)
    action_runner.WaitForJavaScriptCondition('''
        document.getElementById("element-19") !== null &&
        document.getElementById("element-19").contentDocument
          .getElementById("element-22") !== null &&
        document.getElementById("element-19").contentDocument
          .getElementsByClassName(
              "container list-item gc-list-item stretched").length !== 0''')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(
        distance_expr='''
            Math.max(0, 1250 + document.getElementById("element-19")
                                       .contentDocument
                                       .getElementById("element-22")
                                       .getBoundingClientRect().top);''',
        use_touch=True)
    interaction.End()


class Page22(KeyMobileSitesPage):

  """ Why: crbug.com/172906 """

  def __init__(self, page_set):
    super(Page22, self).__init__(
      url='http://groupcloned.com/test/list-images-variable/index.html',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById("element-5") !== null')

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(
        distance_expr='''
            Math.max(0, 1250 +
                document.getElementById("element-5")
                        .getBoundingClientRect().top);''',
        use_touch=True)
    interaction.End()



class Page24(KeyMobileSitesPage):

  """ Why: Google News: accelerated scrolling version """

  def __init__(self, page_set):
    super(Page24, self).__init__(
      url='http://mobile-news.sandbox.google.com/news/pt1',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'typeof NEWS_telemetryReady !== "undefined" && '
        'NEWS_telemetryReady == true')


class Page25(KeyMobileSitesPage):

  """
  Why: Google News: this iOS version is slower than accelerated scrolling
  """

  def __init__(self, page_set):
    super(Page25, self).__init__(
      url='http://mobile-news.sandbox.google.com/news/pt0',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById(":h") != null')
    action_runner.Wait(1)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        element_function='document.getElementById(":5")',
        distance_expr='''
            Math.max(0, 2500 +
                document.getElementById(':h').getBoundingClientRect().top)''',
        use_touch=True)
    interaction.End()


class Page26(KeyMobileSitesPage):

  """
  Why: #1 world commerce website by visits; #3 commerce in the US by time spent
  """

  def __init__(self, page_set):
    super(Page26, self).__init__(
      url='http://www.amazon.com/gp/aw/s/ref=is_box_?k=nicolas+cage',
      page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollElement(
        selector='#search',
        distance_expr='document.body.scrollHeight - window.innerHeight')
    interaction.End()


class KeyMobileSitesPageSet(page_set_module.PageSet):

  """ Key mobile sites """

  def __init__(self):
    super(KeyMobileSitesPageSet, self).__init__(
      user_agent_type='mobile',
      archive_data_file='data/key_mobile_sites.json',
      bucket=page_set_module.PARTNER_BUCKET)

    # Why: Top news site.
    self.AddPage(KeyMobileSitesPage(
      url='http://nytimes.com/', page_set=self, labels=['fastpath']))

    self.AddPage(Page2(self))

    # Why: Image-heavy site.
    self.AddPage(KeyMobileSitesPage(
      url='http://cuteoverload.com', page_set=self, labels=['fastpath']))

    self.AddPage(Page4(self))
    self.AddPage(Page5(self))

    # Why: Social; top Google property; Public profile; infinite scrolls.
    self.AddPage(KeyMobileSitesPage(
       # pylint: disable=C0301
      url='https://plus.google.com/app/basic/110031535020051778989/posts?source=apppromo',
      page_set=self))

    self.AddPage(Page7(self))
    self.AddPage(Page8(self))

    # Why: #11 (Alexa global), google property; some blogger layouts
    # have infinite scroll but more interesting.
    self.AddPage(KeyMobileSitesPage(
      url='http://googlewebmastercentral.blogspot.com/',
      page_set=self, name='Blogger'))

    # Why: #18 (Alexa global), Picked an interesting post """
    self.AddPage(KeyMobileSitesPage(
      # pylint: disable=C0301
      url='http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/',
      page_set=self,
      name='Wordpress'))


    self.AddPage(Page11(self))

   # Why: #6 (Alexa) most visited worldwide, picked an interesting page
    self.AddPage(KeyMobileSitesPage(
      url='http://en.wikipedia.org/wiki/Wikipedia',
      page_set=self,
      name='Wikipedia (1 tab)'))

    # Why: #8 (Alexa global), picked an interesting page
    # Forbidden (Rate Limit Exceeded)
    # self.AddPage(KeyMobileSitesPage(
    #  url='http://twitter.com/katyperry', page_set=self, name='Twitter'))

    # Why: #37 (Alexa global) """
    self.AddPage(KeyMobileSitesPage(
        url='http://pinterest.com',
        page_set=self,
        name='Pinterest'))

    # Why: #1 sports.
    # Fails often; crbug.com/249722'
    # self.AddPage(KeyMobileSitesPage(
    # url='http://espn.go.com', page_set=self, name='ESPN'))


    self.AddPage(Page16(self))
    self.AddPage(Page17(self))

    # Why: crbug.com/169827
    self.AddPage(KeyMobileSitesPage(
      url='http://slashdot.org/', page_set=self, labels=['fastpath']))

    # Why: #5 Alexa news """

    self.AddPage(KeyMobileSitesPage(
      url='http://www.reddit.com/r/programming/comments/1g96ve',
      page_set=self, labels=['fastpath']))

    # Why: Problematic use of fixed position elements """
    self.AddPage(KeyMobileSitesPage(
      url='http://www.boingboing.net', page_set=self, labels=['fastpath']))

    # Page behaves non-deterministically, replaced with test version for now.
    # self.AddPage(Page21(self))
    # mean_input_event_latency cannot be tracked correctly for Page22.
    # See crbug.com/409086.
    # self.AddPage(Page22(self))


    # Why: crbug.com/231413
    # Doesn't scroll; crbug.com/249736
    # self.AddPage(KeyMobileSitesPage(url='http://forecast.io', page_set=self))

    self.AddPage(Page24(self))
    self.AddPage(Page25(self))
    self.AddPage(Page26(self))

    urls_list = [
      # Why: crbug.com/242544
      ('http://www.androidpolice.com/2012/10/03/rumor-evidence-mounts-that-an-'
       'lg-optimus-g-nexus-is-coming-along-with-a-nexus-phone-certification-'
       'program/'),
      # Why: crbug.com/149958
      'http://gsp.ro',
      # Why: Top tech blog
      'http://theverge.com',
      # Why: Top tech site
      'http://digg.com',
      # Why: Top Google property; a Google tab is often open
      'https://www.google.com/#hl=en&q=barack+obama',
      # Why: #1 news worldwide (Alexa global)
      'http://news.yahoo.com',
      # Why: #2 news worldwide
      'http://www.cnn.com',
      # Why: #1 commerce website by time spent by users in US
      'http://shop.mobileweb.ebay.com/searchresults?kw=viking+helmet',
      # Why: #1 Alexa recreation
      # pylint: disable=C0301
      'http://www.booking.com/searchresults.html?src=searchresults&latitude=65.0500&longitude=25.4667',
      # Why: #1 Alexa sports
      'http://sports.yahoo.com/',
      # Why: Top tech blog
      'http://techcrunch.com',
      # Why: #6 Alexa sports
      'http://mlb.com/',
      # Why: #14 Alexa California
      'http://www.sfgate.com/',
      # Why: Non-latin character set
      'http://worldjournal.com/',
      # Why: Mobile wiki
      'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria',
      # Why: #15 Alexa news
      'http://online.wsj.com/home-page',
      # Why: Image-heavy mobile site
      'http://www.deviantart.com/',
      # Why: Top search engine
      ('http://www.baidu.com/s?wd=barack+obama&rsv_bp=0&rsv_spt=3&rsv_sug3=9&'
       'rsv_sug=0&rsv_sug4=3824&rsv_sug1=3&inputT=4920'),
      # Why: Top search engine
      'http://www.bing.com/search?q=sloths',
      # Why: Good example of poor initial scrolling
      'http://ftw.usatoday.com/2014/05/spelling-bee-rules-shenanigans'
    ]

    for url in urls_list:
      self.AddPage(KeyMobileSitesPage(url, self))
