# chromite x86-generic target spec file 
# Use RFC 822 format

[BUILD]
# If the profile is pulled in from an overlay you will need to specify it.
# Well known locations of the "src/overlays, /usr/local/portage" etc will 
# be searched for the overlay
overlay: overlay-x86-generic

# The profile to use for building this target
# ALL is a reserved target that builds all specfiles recursively beneath
# e.g:
# profile: x86-generic/base
profile: x86-generic/dev

# portagechannel speficies which version of the upstream portage is being built
#   "stable" is the current stable version
#   "unstable" is the next version of portage we are stablizing to
#   "bleedingedge" is upto the minute upstream portage
# e.g: 
# portagechannel: stable
portagechannel: stable

# prebuilt mirror hosts prebuilts for stage4/chroot and per profile prebuilts
# e.g:
# prebuiltmirror:http://build.chromium.org/mirror/chromiumos/stage4mirror 
prebuiltmirror:http://build.chromium.org/mirror/chromiumos/stage4mirror 

# stage4 is the Portage Stage3 + any additions deps (hard-host-deps etc)
#  "latest" tries to fetch the "latest" from the prebuilt mirror (default)  
#            -- This is pulled in from the current portagechannel i.e stable
#  "nofetch" will prevent fetching stage4 and attempt to compile a stage4
#            If nofetch is specified a stage3 and portage are required
#  "version" This will attempt to download a particular prebuilt version 
#           - Version is specfied as s<stage3ver>-p<portagever>
# e.g:
# stage4: stage4-s20100309-p20100310
stage4: latest

# stage3 is the pristine stage3 to use to build your stage4/chroot.
# This is ignored if stage4 is latest
#   "latest" fetches the latest version of upstream stage3
#   "version" pulls in the specified version of stage3
# e.g:
# stage3: 20100309
stage3: latest

# portage is the upstream portage version to use to build your stage4/chroot.
#   "latest" fetches the latest version of upstream portage
#   "version" pulls in the specified version of portage
# e.g:
# portage: 20100310

portage: latest


[IMAGE]
# TODO(vince): update the following imaging sections as appropriate. 
# TODO(anush): figure out how this can work for virtual ALL targets since
# since each profile will require partition/filesystem/hook information.
p0: ',c,*,,83', 'ext3', '/', 'p0hook'
p1: ',c,*,,82', 'ext2', '/boot', 'p1hook'
p2: ',c,*,,83', 'ext3', '/var', 'p2hook'
