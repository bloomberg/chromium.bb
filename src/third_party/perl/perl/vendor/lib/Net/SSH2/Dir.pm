package Net::SSH2::Dir;

use strict;
use warnings;
use Carp;

# methods


1;
__END__

=head1 NAME

Net::SSH2::Dir - SSH 2 SFTP directory object

=head1 DESCRIPTION

An SFTP file object is created by the L<Net::SSH2::SFTP> C<opendir> method.

=head2 read

Returns a hash (hashref in scalar context); keys are C<name> and those returned
by Net::SSH2::SFTP::stat; returns empty list or undef if no more files.

=head1 SEE ALSO

L<Net::SSH2::SFTP>.

=head1 AUTHOR

David B. Robins, E<lt>dbrobins@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005, 2006 by David B. Robins; all rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.0 or,
at your option, any later version of Perl 5 you may have available.

=cut
