# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from profile_creators import extensions_profile_creator


class ManyExtensionsProfileCreator(
    extensions_profile_creator.ExtensionsProfileCreator):
  """Install many popular extensions."""

  def __init__(self):
    extensions_to_install = [
        "nklfajnmfbchcceflgddnkignfheooic",
        "keigpnkjljkelclbjbekcfnaomfodamj",
        "fjnbnpbmkenffdnngjfgmeleoegfcffe",
        "gighmmpiobklfepjocnamgkkbiglidom",
        "oadboiipflhobonjjffjbfekfjcgkhco",
        "elioihkkcdgakfbahdoddophfngopipi",
        "fheoggkfdfchfphceeifdbepaooicaho",
        "nkcpopggjcjkiicpenikeogioednjeac",
        "ifohbjbgfchkkfhphahclmkpgejiplfo",
        "kbmfpngjjgdllneeigpgjifpgocmfgmb",
        "pdnfnkhpgegpcingjbfihlkjeighnddk",
        "aapbdbdomjkkjkaonfhkkikfgjllcleb",
        "fjbbjfdilbioabojmcplalojlmdngbjl",
        "hdokiejnpimakedhajhdlcegeplioahd",
        "ninpnjfmichdlipckmfacdjbpbkkbcfa",
        "mgijmajocgfcbeboacabfgobmjgjcoja",
        "ohjkicjidmohhfcjjlahfppkdblibkkb"]

    super(ManyExtensionsProfileCreator, self).__init__(
        extensions_to_install=extensions_to_install)
