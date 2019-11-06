package Crypt::OpenPGP::MDC;
use strict;

use Crypt::OpenPGP::Digest;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $mdc = bless { }, shift;
    $mdc->init(@_);
}

sub init {
    my $mdc = shift;
    my %param = @_;
    if (my $data = $param{Data}) {
        my $dgst = Crypt::OpenPGP::Digest->new('SHA1');
        $mdc->{body} = $dgst->hash($data);
    }
    $mdc;
}

sub digest { $_[0]->{body} }

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $mdc = $class->new;
    $mdc->{body} = $buf->get_bytes($buf->length - $buf->offset);
    $mdc;
}

sub save { $_[0]->{body} }

1;
__END__

=head1 NAME

Crypt::OpenPGP::MDC - MDC (modification detection code) packet

=head1 SYNOPSIS

    use Crypt::OpenPGP::MDC;

    my $mdc = Crypt::OpenPGP::MDC->new( Data => 'foobar' );
    my $digest = $mdc->digest;
    my $serialized = $mdc->save;

=head1 DESCRIPTION

I<Crypt::OpenPGP::MDC> is a PGP MDC (modification detection code) packet.
Such a packet is used alongside Encrypted-MDC data packets so that
modifications to the ciphertext can be detected. The MDC packet contains
a C<SHA-1> digest of the plaintext for comparison with the decrypted
plaintext.

You generally will never need to construct a I<Crypt::OpenPGP::MDC>
packet yourself; usage is by the I<Crypt::OpenPGP::Ciphertext> object.

=head1 USAGE

=head2 Crypt::OpenPGP::MDC->new( [ Data => $data ] )

Creates a new MDC packet object and returns that object. If you do not
supply any data I<$data>, the object is created empty; this is used, for
example, in I<parse> (below), to create an empty packet which is then
filled from the data in the buffer.

If you wish to initialize a non-empty object, supply I<new> with
the I<Data> parameter along with a value I<$data>. I<$data> should
contain the plaintext prefix (length = cipher blocksize + 2), the actual
plaintext, and two octets corresponding to the hex digits C<0xd3> and
C<0x14>.

=head2 $mdc->save

Returns the text of the MDC packet; this is the digest of the data passed
to I<new> (above) as I<$data>, for example.

=head2 Crypt::OpenPGP::MDC->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or
with offset pointing to) an MDC packet, returns a new <Crypt::OpenPGP::MDC>
object, initialized with the MDC data in the buffer.

=head2 $mdc->digest

Returns the MDC digest data (eg. the string passed as I<$data> to
I<new>, above).

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
