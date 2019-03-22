package Crypt::OpenPGP::Compressed;
use strict;

use Compress::Zlib;
use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::Constants qw( DEFAULT_COMPRESS );
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %ALG %ALG_BY_NAME );
%ALG = ( 1 => 'ZIP', 2 => 'Zlib' );
%ALG_BY_NAME = map { $ALG{$_} => $_ } keys %ALG;

sub alg {
    return $_[0]->{__alg} if ref($_[0]);
    $ALG{$_[1]} || $_[1];
}

sub alg_id {
    return $_[0]->{__alg_id} if ref($_[0]);
    $ALG_BY_NAME{$_[1]} || $_[1];
}

sub new {
    my $comp = bless { }, shift;
    $comp->init(@_);
}

sub init {
    my $comp = shift;
    my %param = @_;
    if (my $data = $param{Data}) {
        my $alg = $param{Alg} || DEFAULT_COMPRESS;
        $alg = $ALG{$alg} || $alg;
        $comp->{__alg} = $alg;
        $comp->{__alg_id} = $ALG_BY_NAME{$alg};
        my %args;
        if ($comp->{__alg_id} == 1) {
            %args = (-WindowBits => -13, -MemLevel => 8);
        }
        my($d, $status, $compressed);
        ($d, $status) = deflateInit(\%args);
        return (ref $comp)->error("Zlib deflateInit error: $status")
            unless $status == Compress::Zlib::Z_OK();
        {
            my($output, $out);
            ($output, $status) = $d->deflate($data);
            last unless $status == Compress::Zlib::Z_OK();
            ($out, $status) = $d->flush();
            last unless $status == Compress::Zlib::Z_OK();
            $compressed = $output . $out;
        }
        return (ref $comp)->error("Zlib deflation error: $status")
            unless defined $compressed;
        $comp->{data} = $compressed;
    }
    $comp;
}

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $comp = $class->new;
    $comp->{__alg_id} = $buf->get_int8;
    $comp->{__alg} = $ALG{ $comp->{__alg_id} };
    $comp->{data} = $buf->get_bytes($buf->length - $buf->offset);
    $comp;
}

sub save {
    my $comp = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->put_int8($comp->{__alg_id});
    $buf->put_bytes($comp->{data});
    $buf->bytes;
}

sub decompress {
    my $comp = shift;
    my %args;
    if ($comp->{__alg_id} == 1) {
        %args = (-WindowBits => -13);
    }
    my($i, $status, $out);
    ($i, $status) = inflateInit(\%args);
    return $comp->error("Zlib inflateInit error: $status")
        unless $status == Compress::Zlib::Z_OK();
    ($out, $status) = $i->inflate($comp->{data});
    return $comp->error("Zlib inflate error: $status")
        unless defined $out;
    $out;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Compressed - Compressed data packets

=head1 SYNOPSIS

    use Crypt::OpenPGP::Compressed;

    my $data = 'serialized openpgp packets';
    my $cdata = Crypt::OpenPGP::Compressed->new( Data => $data );
    my $serialized = $cdata->save;

=head1 DESCRIPTION

I<Crypt::OpenPGP::Compressed> implements compressed data packets,
providing both compression and decompression functionality, for all
supported compression algorithms (C<Zlib> and C<ZIP>). This class
uses I<Compress::Zlib> for all compression/decompression needs for
both algorithms: C<ZIP> is simply C<Zlib> with a different setting
for the I<WindowBits> parameter.

Decompressing a compressed data packet should always yield a stream
of valid PGP packets (which you can then parse using
I<Crypt::OpenPGP::PacketFactory>). Similarly, when compressing a
packet the input data should be a stream of packets.

=head1 USAGE

=head2 Crypt::OpenPGP::Compressed->new( %arg )

Creates a new compressed data packet object and returns that object.
If there are no arguments in I<%arg>, the object is created with an
empty compressed data container; this is used, for example, in
I<parse> (below), to create an empty packet which is then filled with
the data in the buffer.

If you wish to initialize a non-empty object, I<%arg> can contain:

=over 4

=item * Data

A block of octets that make up the data that you wish to compress.
As mentioned above, the data to compress should always be a stream
of valid PGP packets (saved using I<Crypt::OpenPGP::PacketFactory::save>).

This argument is required (for a non-empty object).

=item * Alg

The name (or ID) of a supported PGP compression algorithm. Valid
names are C<Zlib> and C<ZIP>.

This argument is optional; by default I<Crypt::OpenPGP::Compressed> will
use C<ZIP>.

=back

=head2 $cdata->save

Returns the serialized compressed data packet, which consists of
a one-octet compression algorithm ID, followed by the compressed
data.

=head2 Crypt::OpenPGP::Compressed->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or with
offset pointing to) a compressed data packet, returns a new
I<Crypt::OpenPGP::Compressed> object, initialized with the data from
the buffer.

=head2 $cdata->decompress

Decompresses the compressed data in the I<Crypt::OpenPGP::Compressed>
object I<$cdata> and returns the decompressed data.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
