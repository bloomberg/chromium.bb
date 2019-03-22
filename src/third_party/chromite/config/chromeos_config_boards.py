# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for cbuildbot boards."""

from __future__ import print_function

#
# Define assorted constants describing various sets of boards.
#

# Base per-board configuration.
# Every board must appear in exactly 1 of the following sets.

#
# Define assorted constants describing various sets of boards.
#

# Base per-board configuration.
# Every board must appear in exactly 1 of the following sets.

arm_internal_release_boards = frozenset([
    'arkham',
    'beaglebone',
    'beaglebone_servo',
    'bob',
    'capri',
    'capri-zfpga',
    'cheza',
    'cobblepot',
    'daisy',
    'daisy_skate',
    'daisy_spring',
    'elm',
    'gale',
    'gonzo',
    'gru',
    'hana',
    'kevin',
    'kevin-arcnext',
    'kukui',
    'lasilla-ground',
    'nyan_big',
    'nyan_blaze',
    'nyan_kitty',
    'oak',
    'octavius',
    'peach_pi',
    'peach_pit',
    'romer',
    'scarlet',
    'veyron_fievel',
    'veyron_jaq',
    'veyron_jerry',
    'veyron_mickey',
    'veyron_mighty',
    'veyron_minnie',
    'veyron_rialto',
    'veyron_speedy',
    'veyron_tiger',
    'whirlwind',
    'wooten',
])

arm_external_boards = frozenset([
    'arm-generic',
    'arm64-generic',
    'arm64-llvmpipe',
    'tael',
])

x86_internal_release_boards = frozenset([
    'amd64-generic-cheets',
    'asuka',
    'atlas',
    'auron_paine',
    'auron_yuna',
    'banjo',
    'banon',
    'betty',
    'betty-arc64',
    'betty-arcnext',
    'buddy',
    'candy',
    'caroline',
    'caroline-arcnext',
    'cave',
    'celes',
    'chell',
    'clapper',
    'coral',
    'cyan',
    'dragonegg',
    'edgar',
    'enguarde',
    'eve',
    'eve-arcnext',
    'eve-arcvm',
    'eve-campfire',
    'expresso',
    'falco',
    'falco_li',
    'fizz',
    'fizz-accelerator',
    'fizz-moblab',
    'gandof',
    'glados',
    'glimmer',
    'gnawty',
    'grunt',
    'guado',
    'guado-accelerator',
    'guado_labstation',
    'guado_moblab',
    'hatch',
    'heli',
    'jecht',
    'kalista',
    'kefka',
    'kip',
    'lakitu',
    'lakitu-gpu',
    'lakitu-nc',
    'lakitu-st',
    'lakitu_next',
    'lars',
    'leon',
    'link',
    'lulu',
    'mccloud',
    'monroe',
    'nami',
    'nautilus',
    'ninja',
    'nocturne',
    'novato',
    'novato-arc64',
    'octopus',
    'orco',
    'panther',
    'parrot_ivb',
    'peppy',
    'poppy',
    'pyro',
    'quawks',
    'rammus',
    'reef',
    'reks',
    'relm',
    'rikku',
    'samus',
    'sand',
    'sentry',
    'setzer',
    'slippy',
    'snappy',
    'soraka',
    'squawks',
    'stout',
    'sumo',
    'swanky',
    'terra',
    'tidus',
    'tricky',
    'ultima',
    'winky',
    'wizpig',
    'wolf',
    'zako',
])

x86_external_boards = frozenset([
    'amd64-generic',
    'moblab-generic-vm',
    'tatl',
    'x32-generic',
])

# Board can appear in 1 or more of the following sets.
brillo_boards = frozenset([
    'arkham',
    'gale',
    'whirlwind',
])

accelerator_boards = frozenset([
    'fizz-accelerator',
    'guado-accelerator',
])

beaglebone_boards = frozenset([
    'beaglebone',
    'beaglebone_servo',
])

lakitu_boards = frozenset([
    'lakitu',
    'lakitu-gpu',
    'lakitu-nc',
    'lakitu-st',
    'lakitu_next',
])

lassen_boards = frozenset([
    'lassen',
])

loonix_boards = frozenset([
    'capri',
    'capri-zfpga',
    'cobblepot',
    'gonzo',
    'lasilla-ground',
    'octavius',
    'romer',
    'wooten',
])

moblab_boards = frozenset([
    'fizz-moblab',
    'guado_moblab',
    'moblab-generic-vm',
])

scribe_boards = frozenset([
    'guado-macrophage',
])

termina_boards = frozenset([
    'tatl',
    'tael',
])

nofactory_boards = (
    lakitu_boards | termina_boards | lassen_boards | frozenset([
        'x30evb',
    ])
)

toolchains_from_source = frozenset([
    'x32-generic',
])

noimagetest_boards = (lakitu_boards | loonix_boards | termina_boards
                      | scribe_boards)

nohwqual_boards = (lakitu_boards | lassen_boards | loonix_boards
                   | termina_boards | beaglebone_boards)

norootfs_verification_boards = frozenset([
])

base_layout_boards = lakitu_boards | termina_boards
