package Crypt::OpenPGP::OnePassSig;
use strict;

sub new { bless { }, $_[0] }

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $onepass = $class->new;
    $onepass->{version} = $buf->get_int8;
    $onepass->{type} = $buf->get_int8;
    $onepass->{hash_alg} = $buf->get_int8;
    $onepass->{pk_alg} = $buf->get_int8;
    $onepass->{key_id} = $buf->get_bytes(8);
    $onepass->{nested} = $buf->get_int8;
    $onepass;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::OnePassSig - One-Pass Signature packet

=head1 DESCRIPTION

I<Crypt::OpenPGP::OnePassSig> implements a PGP One-Pass Signature
packet, a packet that precedes the signature data and contains
enough information to allow the receiver of the signature to begin
computing the hashed data. Standard signature packets always come
I<before> the signed data, which forces receivers to backtrack to
the beginning of the message--to the signature packet--to add on
the signature trailer data. The one-pass signature packet allows
the receive to start computing the hashed data while reading the
data packet, then continue on sequentially when it reaches the
signature packet.

=head1 USAGE

=head2 my $onepass = Crypt::OpenPGP::OnePassSig->parse($buffer)

Given the I<Crypt::OpenPGP::Buffer> object buffer, which should
contain a one-pass signature packet, parses the object from the
buffer and returns the object.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
