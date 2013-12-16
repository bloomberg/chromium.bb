# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import extensions_profile_creator


class ManyExtensionsProfileCreator(
    extensions_profile_creator.ExtensionsProfileCreator):
  """Install 25 popular extensions."""

  def __init__(self):
    super(ManyExtensionsProfileCreator, self).__init__()

    self._extensions_to_install = [
        "fjnbnpbmkenffdnngjfgmeleoegfcffe",
        "cpngackimfmofbokmjmljamhdncknpmg",
        "lfmhcpmkbdkbgbmkjoiopeeegenkdikp",
        "ciagpekplgpbepdgggflgmahnjgiaced",
        "gighmmpiobklfepjocnamgkkbiglidom",
        "pbjikboenpfhbbejgkoklgkhjpfogcam",
        "bfbmjmiodbnnpllbbbfblcplfjjepjdn",
        "pdnfnkhpgegpcingjbfihlkjeighnddk",
        "elioihkkcdgakfbahdoddophfngopipi",
        "aapbdbdomjkkjkaonfhkkikfgjllcleb",
        "gomekmidlodglbbmalcneegieacbdmki",
        "kfdckejfnkaemompfjhecfmhjgnchmjg",
        "igdhbblpcellaljokkpfhcjlagemhgjl",
        "ifohbjbgfchkkfhphahclmkpgejiplfo",
        "keigpnkjljkelclbjbekcfnaomfodamj",
        "hehijbfgiekmjfkfjpbkbammjbdenadd",
        "nckgahadagoaajjgafhacjanaoiihapd",
        "mihcahmgecmbnbcchbopgniflfhgnkff",
        "mgijmajocgfcbeboacabfgobmjgjcoja",
        "opnbmdkdflhjiclaoiiifmheknpccalb",
        "kbmfpngjjgdllneeigpgjifpgocmfgmb",
        "aelbknmfcacjffmgnoaaonhgoghlmlkp",
        "oadboiipflhobonjjffjbfekfjcgkhco",
        "pioclpoplcdbaefihamjohnefbikjilc",
        "dhkplhfnhceodhffomolpfigojocbpcb"]
