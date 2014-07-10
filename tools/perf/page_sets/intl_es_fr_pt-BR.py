# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class IntlEsFrPtBrPage(page_module.Page):

  def __init__(self, url, page_set):
    super(IntlEsFrPtBrPage, self).__init__(url=url, page_set=page_set)
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/intl_es_fr_pt-BR.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class IntlEsFrPtBrPageSet(page_set_module.PageSet):

  """
  Popular pages in Romance languages Spanish, French and Brazilian Portuguese.
  """

  def __init__(self):
    super(IntlEsFrPtBrPageSet, self).__init__(
      user_agent_type='desktop',
      archive_data_file='data/intl_es_fr_pt-BR.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      'http://elmundo.es/',
      'http://terra.es/',
      # pylint: disable=C0301
      'http://www.ebay.es/sch/i.html?_sacat=382&_trkparms=clkid%3D6548971389060485883&_qi=RTM1381637',
      'http://www.eltiempo.es/talavera-de-la-reina.html',
      'http://www.free.fr/adsl/index.html',
      'http://www.voila.fr/',
      'http://www.leboncoin.fr/annonces/offres/limousin/',
      'http://www.orange.fr/',
      # Why: #5 site in Brazil
      'http://www.uol.com.br/',
      # Why: #10 site in Brazil
      # pylint: disable=C0301
      'http://produto.mercadolivre.com.br/MLB-468424957-pelicula-protetora-smartphone-h5500-e-h5300-43-frete-free-_JM'
    ]

    for url in urls_list:
      self.AddPage(IntlEsFrPtBrPage(url, self))
