package Net::SSH2::SFTP;

use strict;
use warnings;
use Carp;

# methods


1;
__END__

=head1 NAME

Net::SSH2::SFTP - SSH 2 Secure FTP object

=head1 DESCRIPTION

An SFTP object is created by the L<Net::SSH2> C<sftp> method.

=head2 error

Returns the last SFTP error (one of the LIBSSH2_FX_* constants).  Use this
when Net::SSH2::error returns LIBSSH2_ERROR_SFTP_PROTOCOL.  In list context,
returns (code, error name).

=head2 open ( file [, flags [, mode ]]] )

Open or create a file on the remote host.  The flags are the standard O_RDONLY,
O_WRONLY, O_RDWR, O_APPEND, O_CREAT, O_TRUNC, and O_EXCL, which may be
combined as usual.  Flags default to O_RDONLY and mode to 0666 (create only).
Returns a Net::SSH2::File object on success.

=head2 opendir ( dir )

Open a directory on the remote host; returns a Net::SSH2::Dir object on success.

=head2 unlink ( file )

Delete the remote file.

=head2 rename ( old, new [, flags ] )

Rename old to new.  Flags are taken from LIBSSH2_SFTP_RENAME_*, and may be
combined; the default is to use all (overwrite, atomic, native).

=head2 mkdir ( path [, mode ] )

Create directory; mode defaults to 0777.

=head2 rmdir ( path )

Remove directory.

=head2 stat ( path [, follow ] )

Get file attributes for the given path.  If follow is set (default), will
follow symbolic links.  On success, returns a hash containing the following:

=over 4

=item mode

=item size

=item uid

=item gid

=item atime

=item mtime

=back

=head2 setstat ( path, key, value... )

Set file attributes for given path; keys are the same as those returned by stat;
note that it's not necessary to pass them all.

=head2 symlink ( path, target [, type ] )

Create a symbolic link to a given target.

=head2 readlink ( path )

Return the target of the given link, undef on failure.

=head2 realpath ( path )

Resolve a filename's path; returns the resolved path, or undef on error.

=head1 SEE ALSO

L<Net::SSH2>.

=head1 AUTHOR

David B. Robins, E<lt>dbrobins@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005, 2006 by David B. Robins; all rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

=cut
