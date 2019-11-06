package Net::SSH2::Listener;

use strict;
use warnings;
use Carp;

# methods


1;
__END__

=head1 NAME

Net::SSH2::Listener - SSH 2 listener object

=head1 DESCRIPTION

A listener object is created by the L<Net::SSH2> C<listen> method.  The
L<Net::SSH2> C<poll> method can be used to check for connections.

=head2 accept

Accept a connection.  Returns a channel object on success, undef on failure.

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
