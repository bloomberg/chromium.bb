package Crypt::OpenPGP::Marker;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new { bless { }, $_[0] }
sub parse {
    my $class = shift;
    my($buf) = @_;
    my $marker = $class->new;
    $marker->{mark} = $buf->bytes;
    $marker;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Marker - PGP Marker packet

=head1 DESCRIPTION

I<Crypt::OpenPGP::Marker> is a PGP Marker packet. These packets are
used by PGP 5.x to signal to earlier versions of PGP (eg. 2.6.x)
that the message requires newer software to be read and understood.

The contents of the Marker packet are always the same: the three
octets 0x50, 0x47, and 0x50 (which spell C<PGP>).

It is very likely that you will never have to use a Marker packet
directly.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
