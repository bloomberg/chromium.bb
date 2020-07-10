package Crypt::OpenPGP::Plaintext;
use strict;

use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $class = shift;
    my $pt = bless { }, $class;
    $pt->init(@_);
}

sub data { $_[0]->{data} }
sub mode { $_[0]->{mode} }

sub init {
    my $pt = shift;
    my %param = @_;
    if (my $data = $param{Data}) {
        $pt->{data} = $data;
        $pt->{mode} = $param{Mode} || 'b';
        $pt->{timestamp} = time;
        $pt->{filename} = $param{Filename} || '';
    }
    $pt;
}

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $pt = $class->new;
    $pt->{mode} = $buf->get_char;
    $pt->{filename} = $buf->get_bytes($buf->get_int8);
    $pt->{timestamp} = $buf->get_int32;
    $pt->{data} = $buf->get_bytes( $buf->length - $buf->offset );
    $pt;
}

sub save {
    my $pt = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_char($pt->{mode});
    $buf->put_int8(length $pt->{filename});
    $buf->put_bytes($pt->{filename});
    $buf->put_int32($pt->{timestamp});
    $buf->put_bytes($pt->{data});
    $buf->bytes;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Plaintext - A plaintext, literal-data packet

=head1 SYNOPSIS

    use Crypt::OpenPGP::Plaintext;

    my $data = 'foo bar';
    my $file = 'foo.txt';

    my $pt = Crypt::OpenPGP::Plaintext->new(
                             Data     => $data,
                             Filename => $file,
                    );
    my $serialized = $pt->save;

=head1 DESCRIPTION

I<Crypt::OpenPGP::Plaintext> implements plaintext literal-data packets,
and is essentially just a container for a string of octets, along
with some meta-data about the plaintext.

=head1 USAGE

=head2 Crypt::OpenPGP::Plaintext->new( %arg )

Creates a new plaintext data packet object and returns that object.
If there are no arguments in I<%arg>, the object is created with an
empty data container; this is used, for example, in I<parse> (below),
to create an empty packet which is then filled from the data in the
buffer.

If you wish to initialize a non-empty object, I<%arg> can contain:

=over 4

=item * Data

A block of octets that make up the plaintext data.

This argument is required (for a non-empty object).

=item * Filename

The name of the file that this data came from, or the name of a file
where it should be saved upon extraction from the packet (after
decryption, for example, if this packet is going to be encrypted).

=item * Mode

The mode in which the data is formatted. Valid values are C<t> and
C<b>, meaning "text" and "binary", respectively.

This argument is optional; I<Mode> defaults to C<b>.

=back

=head2 $pt->save

Returns the serialized form of the plaintext object, which is the
plaintext data, preceded by some meta-data describing the data.

=head2 Crypt::OpenPGP::Plaintext->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or
with offset pointing to) a plaintext data packet, returns a new
I<Crypt::OpenPGP::Ciphertext> object, initialized with the data
in the buffer.

=head2 $pt->data

Returns the plaintext data.

=head2 $pt->mode

Returns the mode of the packet (either C<t> or C<b>).

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
