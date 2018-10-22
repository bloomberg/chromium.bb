package Net::SSH2::PublicKey;

use strict;
use warnings;
use Carp;

# methods


1;
__END__

=head1 NAME

Net::SSH2::PublicKey - SSH 2 public key object

=head1 DESCRIPTION

A public key object is created by the L<Net::SSH2> C<public_key> method.

=head2 add ( name, blob, overwrite flag, attributes... )

Adds a new public key; attributes is a list of hashes with C<name>, C<value>,
and C<mandatory> keys; mandatory defaults to false and value to empty.

=head2 remove ( name, blob )

Remove the given public key.

=head2 fetch

Returns a list of public keys in array context (count in scalar context);
each item is a hash with keys C<name>, C<blob>, and C<attr>, with the latter
being a hash with C<name>, C<value>, and C<mandatory> keys.

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
