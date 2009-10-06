{
  'targets': [
    {
      'target_name': 'gtk_clipboard_dump',
      'type': 'executable',
      'dependencies': [
        '../../build/linux/system.gyp:gtk',
      ],
      'sources': [
        'gtk_clipboard_dump.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
