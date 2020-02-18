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

   *** WARNING: public key functionality in libssh2 is experimental
   *** and mostly abandoned. Don't expect anything on this module to
   *** work correctly.

A public key object is created by the L<Net::SSH2> C<public_key> method.

=head1 METHODS

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

=cut
