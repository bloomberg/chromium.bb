package Crypt::OpenPGP::Trust;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new { bless { }, $_[0] }
sub flags { $_[0]->{flags} }
sub parse {
    my $class = shift;
    my($buf) = @_;
    my $trust = $class->new;
    $trust->{flags} = $buf->get_int8;
    $trust;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Trust - PGP Trust packet

=head1 DESCRIPTION

I<Crypt::OpenPGP::Trust> is a PGP Trust packet. From the OpenPGP
RFC: "Trust packets contain data that record the user's specifications
of which key holders are trustworthy introducers, along with other
information that implementing software uses for trust information."

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
