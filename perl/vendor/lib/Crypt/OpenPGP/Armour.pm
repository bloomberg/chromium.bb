package Crypt::OpenPGP::Armour;
use strict;

use Crypt::OpenPGP;
use MIME::Base64;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub armour {
    my $class = shift;
    my %param = @_;
    my $data = $param{Data} or
        return $class->error("No Data to armour");
    my $headers = $param{Headers} || {};
    $headers->{Version} = Crypt::OpenPGP->version_string
        unless $param{NoVersion};
    my $head = join "\n", map { "$_: $headers->{$_}" } keys %$headers;
    my $object = $param{Object} || 'MESSAGE';
    (my $sdata = encode_base64($data, '')) =~ s!(.{1,64})!$1\n!g;

    "-----BEGIN PGP $object-----\n" .
    $head . "\n\n" .
    $sdata .
    '=' . $class->_checksum($data) .
    "-----END PGP $object-----\n";
}

sub unarmour {
    my $class = shift;
    my($blob) = @_;
    ## Get rid of DOSish newlines.
    $blob =~ s!\r!!g;
    my($begin, $obj, $head, $data, $end) = $blob =~
        m!(-----BEGIN ([^\n\-]+)-----)\n(.*?\n\n)?(.+)(-----END .*?-----)!s
        or return $class->error("Unrecognizable armour");
    unless ($data =~ s!=([^\n]+)$!!s) {
        return $class->error("No checksum");
    }
    my $csum = $1;
    $data = decode_base64($data);
    (my $check = $class->_checksum($data)) =~ s!\n!!;
    return $class->error("Bad checksum") unless $check eq $csum;
    my %headers;
    if ($head) {
        %headers = map { split /: /, $_, 2 } grep { /\S/ } split /\n/, $head;
    }
    {  Data    => $data,
       Headers => \%headers,
       Object  => $obj }
}

sub _checksum {
    my $class = shift;
    my($data) = @_;
    encode_base64(substr(pack('N', crc24($data)), 1));
}

{
    my @CRC_TABLE;
    use constant CRC24_INIT => 0xb704ce;

    sub crc24 {
        my @data = unpack 'C*', $_[0];
        my $crc = CRC24_INIT;
        for my $d (@data) {
            $crc = ($crc << 8) ^ $CRC_TABLE[(($crc >> 16) ^ $d) & 0xff]
        }
        $crc & 0xffffff;
    }

    @CRC_TABLE = (
        0x00000000, 0x00864cfb, 0x018ad50d, 0x010c99f6, 0x0393e6e1,
        0x0315aa1a, 0x021933ec, 0x029f7f17, 0x07a18139, 0x0727cdc2,
        0x062b5434, 0x06ad18cf, 0x043267d8, 0x04b42b23, 0x05b8b2d5,
        0x053efe2e, 0x0fc54e89, 0x0f430272, 0x0e4f9b84, 0x0ec9d77f,
        0x0c56a868, 0x0cd0e493, 0x0ddc7d65, 0x0d5a319e, 0x0864cfb0,
        0x08e2834b, 0x09ee1abd, 0x09685646, 0x0bf72951, 0x0b7165aa,
        0x0a7dfc5c, 0x0afbb0a7, 0x1f0cd1e9, 0x1f8a9d12, 0x1e8604e4,
        0x1e00481f, 0x1c9f3708, 0x1c197bf3, 0x1d15e205, 0x1d93aefe,
        0x18ad50d0, 0x182b1c2b, 0x192785dd, 0x19a1c926, 0x1b3eb631,
        0x1bb8faca, 0x1ab4633c, 0x1a322fc7, 0x10c99f60, 0x104fd39b,
        0x11434a6d, 0x11c50696, 0x135a7981, 0x13dc357a, 0x12d0ac8c,
        0x1256e077, 0x17681e59, 0x17ee52a2, 0x16e2cb54, 0x166487af,
        0x14fbf8b8, 0x147db443, 0x15712db5, 0x15f7614e, 0x3e19a3d2,
        0x3e9fef29, 0x3f9376df, 0x3f153a24, 0x3d8a4533, 0x3d0c09c8,
        0x3c00903e, 0x3c86dcc5, 0x39b822eb, 0x393e6e10, 0x3832f7e6,
        0x38b4bb1d, 0x3a2bc40a, 0x3aad88f1, 0x3ba11107, 0x3b275dfc,
        0x31dced5b, 0x315aa1a0, 0x30563856, 0x30d074ad, 0x324f0bba,
        0x32c94741, 0x33c5deb7, 0x3343924c, 0x367d6c62, 0x36fb2099,
        0x37f7b96f, 0x3771f594, 0x35ee8a83, 0x3568c678, 0x34645f8e,
        0x34e21375, 0x2115723b, 0x21933ec0, 0x209fa736, 0x2019ebcd,
        0x228694da, 0x2200d821, 0x230c41d7, 0x238a0d2c, 0x26b4f302,
        0x2632bff9, 0x273e260f, 0x27b86af4, 0x252715e3, 0x25a15918,
        0x24adc0ee, 0x242b8c15, 0x2ed03cb2, 0x2e567049, 0x2f5ae9bf,
        0x2fdca544, 0x2d43da53, 0x2dc596a8, 0x2cc90f5e, 0x2c4f43a5,
        0x2971bd8b, 0x29f7f170, 0x28fb6886, 0x287d247d, 0x2ae25b6a,
        0x2a641791, 0x2b688e67, 0x2beec29c, 0x7c3347a4, 0x7cb50b5f,
        0x7db992a9, 0x7d3fde52, 0x7fa0a145, 0x7f26edbe, 0x7e2a7448,
        0x7eac38b3, 0x7b92c69d, 0x7b148a66, 0x7a181390, 0x7a9e5f6b,
        0x7801207c, 0x78876c87, 0x798bf571, 0x790db98a, 0x73f6092d,
        0x737045d6, 0x727cdc20, 0x72fa90db, 0x7065efcc, 0x70e3a337,
        0x71ef3ac1, 0x7169763a, 0x74578814, 0x74d1c4ef, 0x75dd5d19,
        0x755b11e2, 0x77c46ef5, 0x7742220e, 0x764ebbf8, 0x76c8f703,
        0x633f964d, 0x63b9dab6, 0x62b54340, 0x62330fbb, 0x60ac70ac,
        0x602a3c57, 0x6126a5a1, 0x61a0e95a, 0x649e1774, 0x64185b8f,
        0x6514c279, 0x65928e82, 0x670df195, 0x678bbd6e, 0x66872498,
        0x66016863, 0x6cfad8c4, 0x6c7c943f, 0x6d700dc9, 0x6df64132,
        0x6f693e25, 0x6fef72de, 0x6ee3eb28, 0x6e65a7d3, 0x6b5b59fd,
        0x6bdd1506, 0x6ad18cf0, 0x6a57c00b, 0x68c8bf1c, 0x684ef3e7,
        0x69426a11, 0x69c426ea, 0x422ae476, 0x42aca88d, 0x43a0317b,
        0x43267d80, 0x41b90297, 0x413f4e6c, 0x4033d79a, 0x40b59b61,
        0x458b654f, 0x450d29b4, 0x4401b042, 0x4487fcb9, 0x461883ae,
        0x469ecf55, 0x479256a3, 0x47141a58, 0x4defaaff, 0x4d69e604,
        0x4c657ff2, 0x4ce33309, 0x4e7c4c1e, 0x4efa00e5, 0x4ff69913,
        0x4f70d5e8, 0x4a4e2bc6, 0x4ac8673d, 0x4bc4fecb, 0x4b42b230,
        0x49ddcd27, 0x495b81dc, 0x4857182a, 0x48d154d1, 0x5d26359f,
        0x5da07964, 0x5cace092, 0x5c2aac69, 0x5eb5d37e, 0x5e339f85,
        0x5f3f0673, 0x5fb94a88, 0x5a87b4a6, 0x5a01f85d, 0x5b0d61ab,
        0x5b8b2d50, 0x59145247, 0x59921ebc, 0x589e874a, 0x5818cbb1,
        0x52e37b16, 0x526537ed, 0x5369ae1b, 0x53efe2e0, 0x51709df7,
        0x51f6d10c, 0x50fa48fa, 0x507c0401, 0x5542fa2f, 0x55c4b6d4,
        0x54c82f22, 0x544e63d9, 0x56d11cce, 0x56575035, 0x575bc9c3,
        0x57dd8538
    );
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::Armour - ASCII Armouring and Unarmouring

=head1 SYNOPSIS

    use Crypt::OpenPGP::Armour;

    my $armoured = Crypt::OpenPGP::Armour->armour(
                           Data => "foo bar baz",
                           Object => "FOO OBJECT",
                           Headers => {
                                         Version => '0.57',
                                         Comment => 'FooBar',
                                      },
                     );

    my $decoded = Crypt::OpenPGP::Armour->unarmour( $armoured ) or
        die Crypt::OpenPGP::Armour->errstr;

=head1 DESCRIPTION

This module converts arbitrary-length strings of binary octets into
Base64-encoded ASCII messages suitable for transfer as text. It
also converts in the opposite direction, taking an armoured message
and returning the binary data, along with headers.

=head1 USAGE

=head2 Crypt::OpenPGP::Armour->armour( %args )

Converts arbitrary-length strings of binary octets in an encoded
message containing 4 parts: head and tail markers that identify the
type of content contained therein; a group of newline-separated
headers at the top of the message; Base64-encoded data; and a
Base64-encoded CRC24 checksum of the message body.

Returns I<undef> on failure, the encoded message on success. In the
case of failure call the class method I<errstr> to get the error
message.

I<%args> can contain:

=over 4

=item * Object

Specifies the type of object being armoured; the string C<PGP > (PGP
followed by a space) will be prepended to the value you pass in.

This argument is required.

=item * Data

The binary octets to be encoded as the body of the armoured message;
these octets will be encoded into ASCII using I<MIME::Base64>.

This argument is required.

=item * Headers

A reference to a hash containing key-value pairs, where the key is the
name of the header and the value the header value. These headers
are placed at the top of the encoded message in the form C<Header: Value>.

=item * NoVersion

Boolean flag; if true, then default Version header will not be added
to the armour.

=back

=head2 Crypt::OpenPGP::Armour->unarmour($message)

Decodes an ASCII-armoured message and returns a hash reference whose
keys are the arguments provided to I<armour>, above. Returns I<undef>
on failure (for example, if the checksum fails to match, or if the
message is in an incomprehensible format). In case of failure call
the class method I<errstr> to get the text of the error message.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
