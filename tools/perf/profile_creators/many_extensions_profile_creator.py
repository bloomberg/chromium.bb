# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from profile_creators import extensions_profile_creator


class ManyExtensionsProfileCreator(
    extensions_profile_creator.ExtensionsProfileCreator):
  """Install 25 popular extensions."""

  def __init__(self):
    super(ManyExtensionsProfileCreator, self).__init__()

    self._extensions_to_install = [
        "nklfajnmfbchcceflgddnkignfheooic",
        "keigpnkjljkelclbjbekcfnaomfodamj",
        "fjnbnpbmkenffdnngjfgmeleoegfcffe",
        "gighmmpiobklfepjocnamgkkbiglidom",
        "oadboiipflhobonjjffjbfekfjcgkhco",
        "elioihkkcdgakfbahdoddophfngopipi",
        "fheoggkfdfchfphceeifdbepaooicaho",
        "nkcpopggjcjkiicpenikeogioednjeac",
        "gomekmidlodglbbmalcneegieacbdmki",
        "ifohbjbgfchkkfhphahclmkpgejiplfo",
        "jpmbfleldcgkldadpdinhjjopdfpjfjp",
        "kbmfpngjjgdllneeigpgjifpgocmfgmb",
        "pdnfnkhpgegpcingjbfihlkjeighnddk",
        "aapbdbdomjkkjkaonfhkkikfgjllcleb",
        "dhkplhfnhceodhffomolpfigojocbpcb",
        "fjbbjfdilbioabojmcplalojlmdngbjl",
        "gkojfkhlekighikafcpjkiklfbnlmeio",
        "cpngackimfmofbokmjmljamhdncknpmg",
        "hdokiejnpimakedhajhdlcegeplioahd",
        "ninpnjfmichdlipckmfacdjbpbkkbcfa",
        "mihcahmgecmbnbcchbopgniflfhgnkff",
        "mgijmajocgfcbeboacabfgobmjgjcoja",
        "ohjkicjidmohhfcjjlahfppkdblibkkb",
        "bfbmjmiodbnnpllbbbfblcplfjjepjdn",
        "opnbmdkdflhjiclaoiiifmheknpccalb"]
