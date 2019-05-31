package Net::SSH2::KnownHosts;

use strict;
use warnings;

1;

__END__

=head1 NAME

Net::SSH2::KnownHosts - SSH 2 knownhosts object

=head1 SYNOPSIS

  #####################################################################
  #                                                                   #
  # WARNING: The API provided by Net::SSH2::KnownHosts is             #
  # experimental and could change in future versions of the module!!! #
  #                                                                   #
  #####################################################################

  my $kh = $ssh2->known_hosts;

  my $n_ent = $kh->readfile($known_hosts_path);

  # a non-existent known_hosts file usually is not an error...
  unless (defined $n_ent) {
      if ($ssh2->error != LIBSSH2_ERROR_FILE or -f $known_hosts_path) {
          die; # propagate error;
      }
  }

  my ($key, $type) = $ssh2->remote_hostkey;

  my $flags = ( LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                LIBSSH2_KNOWNHOST_KEYENC_RAW |
                (($type + 1) << LIBSSH2_KNOWNHOST_KEY_SHIFT) );

  my $check = $kh->check($hostname, $port, $key, $flags);

  if ($check == LIBSSH2_KNOWNHOST_CHECK_MATCH) {
      # ok!
  }
  elsif ($check == LIBSSH2_KNOWNHOST_CHECK_MISMATCH) {
      die "host verification failed, the key has changed!";
  }
  elsif ($check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND) {
      die "host verification failed, key not found in known_hosts file"
          if $strict_host_key_checking;

      # else, save new key to file:
      unless ( $kh->add($hostname, '', $key, "Perl added me", $flags) and
               $kh->writefile($known_hosts_path) ) {
          warn "unable to save known_hosts file: " . ($ssh2->error)[1];
      }
  }
  else {
      die "host key verification failed, unknown reason";
  }

=head1 DESCRIPTION

  #####################################################################
  #                                                                   #
  # WARNING: The API provided by Net::SSH2::KnownHosts is             #
  # experimental and could change in future versions of the module!!! #
  #                                                                   #
  #####################################################################

The C<knownhosts> object allows one to manipulate the entries in the
C<known_host> file usually located at C<~/.ssh/known_hosts> and which
contains the public keys of the already known hosts.

The methods currently supported are as follows:

=head2 readfile (filename)

Populates the object with the entries in the given file.

It returns the number or entries read or undef on failure.

=head2 writefile (filename)

Saves the known host entries to the given file.

=head2 add (hostname, salt, key, comment, key_type|host_format|key_format)

Add a host and its associated key to the collection of known hosts.

The C<host_format> argument specifies the format of the given host:

    LIBSSH2_KNOWNHOST_TYPE_PLAIN  - ascii "hostname.domain.tld"
    LIBSSH2_KNOWNHOST_TYPE_SHA1   - SHA1(salt, host) base64-encoded!
    LIBSSH2_KNOWNHOST_TYPE_CUSTOM - another hash

If C<SHA1> is selected as host format, the salt must be provided to
the salt argument in base64 format.

The SHA-1 hash is what OpenSSH can be told to use in known_hosts
files. If a custom type is used, salt is ignored and you must provide
the host pre-hashed when checking for it in the C<check> method.

The available key formats are as follow:

    LIBSSH2_KNOWNHOST_KEYENC_RAW
    LIBSSH2_KNOWNHOST_KEYENC_BASE64

Finally, the available key types are as follow:

    LIBSSH2_KNOWNHOST_KEY_RSA1
    LIBSSH2_KNOWNHOST_KEY_SSHRSA
    LIBSSH2_KNOWNHOST_KEY_SSHDSS

The comment argument may be undef.

=head2 check (hostname, port, key, key_type|host_format|key_format)

Checks a host and its associated key against the collection of known hosts.

The C<key_type|host_format|key_format> argument has the same meaning
as in the L</add> method.

C<undef> may be passed as the port argument.

Returns:

    LIBSSH2_KNOWNHOST_CHECK_MATCH    (0)
    LIBSSH2_KNOWNHOST_CHECK_MISMATCH (1)
    LIBSSH2_KNOWNHOST_CHECK_NOTFOUND (2)
    LIBSSH2_KNOWNHOST_CHECK_FAILURE  (3)

=head2 readline (string)

Read a known_hosts entry from the given string.

For instance, the following piece of code is more or less equivalent
to the L<readfile> method:

  my $kh = $ssh2->known_hosts;
  if (open my $fh, '<', $known_hosts_path) {
      while (<>) {
          eval { $kh->readline($_) }
             or warn "unable to parse known_hosts entry $_";
      }
  }

=head2 writeline (hostname, port, key, key_type|host_format|key_format)

Searches the entry matching the given parameters (as described in the
L</check> method) and formats it into a line in the known_hosts
format.

This method returns undef when some error happens.

This method should be considered experimental, the interface may
change.

=head1 SEE ALSO

L<Net::SSH2>, L<sshd(8)>.

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2013-2015 Salvador FandiE<ntilde>o; all rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

The documentation on this file is based on the comments inside
C<libssh2.h> file from the libssh2 distribution which has the
following copyright and license:

Copyright (c) 2004-2009, Sara Golemon <sarag@libssh2.org>
Copyright (c) 2009-2012 Daniel Stenberg
Copyright (c) 2010 Simon Josefsson <simon@josefsson.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

Neither the name of the copyright holder nor the names of any other
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

=cut
