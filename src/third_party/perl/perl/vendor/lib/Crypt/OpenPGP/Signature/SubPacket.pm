package Crypt::OpenPGP::Signature::SubPacket;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %SUBPACKET_TYPES );
%SUBPACKET_TYPES = (
    2  => { name => 'Signature creation time',
            r    => sub { $_[0]->get_int32 },
            w    => sub { $_[0]->put_int32($_[1]) } },

    3  => { name => 'Signature expiration time',
            r    => sub { $_[0]->get_int32 },
            w    => sub { $_[0]->put_int32($_[1]) } },

    4  => { name => 'Exportable certification',
            r    => sub { $_[0]->get_int8 },
            w    => sub { $_[0]->put_int8($_[1]) } },

    5  => { name => 'Trust signature',
            r    => sub { $_[0]->get_int8 },
            w    => sub { $_[0]->put_int8($_[1]) } },

    6  => { name => 'Regular expression',
            r    => sub { $_[0]->bytes },
            w    => sub { $_[0]->append($_[1]) } },

    7  => { name => 'Revocable',
            r    => sub { $_[0]->get_int8 },
            w    => sub { $_[0]->put_int8($_[1]) } },

    9  => { name => 'Key expiration time',
            r    => sub { $_[0]->get_int32 },
            w    => sub { $_[0]->put_int32($_[1]) } },

    10 => { name => '(Unsupported placeholder',
            r    => sub { },
            w    => sub { } },

    11 => { name => 'Preferred symmetric algorithms',
            r    => sub { [ unpack 'C*', $_[0]->bytes ] },
            w    => sub { $_[0]->append(pack 'C*', @{ $_[1] }) } },

    12 => { name => 'Revocation key',
            r    => sub {
                        { class => $_[0]->get_int8,
                          alg_id => $_[0]->get_int8,
                          fingerprint => $_[0]->get_bytes(20) } },
            w    => sub {
                        $_[0]->put_int8($_[1]->{class});
                        $_[0]->put_int8($_[1]->{alg_id});
                        $_[0]->put_bytes($_[1]->{fingerprint}, 20) } },

    16 => { name => 'Issuer key ID',
            r    => sub { $_[0]->get_bytes(8) },
            w    => sub { $_[0]->put_bytes($_[1], 8) } },

    20 => { name => 'Notation data',
            r    => sub {
                        { flags => $_[0]->get_int32,
                          name => $_[0]->get_bytes($_[0]->get_int16),
                          value => $_[0]->get_bytes($_[0]->get_int16) } },
            w    => sub {
                        $_[0]->put_int32($_[1]->{flags});
                        $_[0]->put_int16(length $_[1]->{name});
                        $_[0]->put_bytes($_[1]->{name});
                        $_[0]->put_int16(length $_[1]->{value});
                        $_[0]->put_bytes($_[1]->{value}) } },

    21 => { name => 'Preferred hash algorithms',
            r    => sub { [ unpack 'C', $_[0]->bytes ] },
            w    => sub { $_[0]->put_bytes(pack 'C*', @{ $_[1] }) } },

    22 => { name => 'Preferred compression algorithms',
            r    => sub { [ unpack 'C', $_[0]->bytes ] },
            w    => sub { $_[0]->put_bytes(pack 'C*', @{ $_[1] }) } },

    23 => { name => 'Key server preferences',
            r    => sub { $_[0]->bytes },
            w    => sub { $_[0]->append($_[1]) } },

    24 => { name => 'Preferred key server',
            r    => sub { $_[0]->bytes },
            w    => sub { $_[0]->append($_[1]) } },

    25 => { name => 'Primary user ID',
            r    => sub { $_[0]->get_int8 },
            w    => sub { $_[0]->put_int8($_[1]) } },

    26 => { name => 'Policy URL',
            r    => sub { $_[0]->bytes },
            w    => sub { $_[0]->append($_[1]) } },

    27 => { name => 'Key flags',
            r    => sub { $_[0]->bytes },
            w    => sub { $_[0]->append($_[1]) } },

    28 => { name => 'Signer\'s user ID',
            r    => sub { $_[0]->bytes },
            w    => sub { $_[0]->append($_[1]) } },

    29 => { name => 'Reason for revocation',
            r    => sub {
                        { code => $_[0]->get_int8,
                          reason => $_[0]->get_bytes($_[0]->length -
                                                     $_[0]->offset) } },
            w    => sub {
                          $_[0]->put_int8($_[1]->{code});
                          $_[0]->put_bytes($_[1]->{reason}) } },
);

sub new { bless { }, $_[0] }

sub parse {
    my $class = shift;
    my($buf) = @_;
    my $sp = $class->new;
    my $tag = $buf->get_int8;
    $sp->{critical} = $tag & 0x80;
    $sp->{type} = $tag & 0x7f;
    $buf->bytes(0, 1, '');   ## Cut off tag byte
    $buf->{offset} = 0;
    my $ref = $SUBPACKET_TYPES{$sp->{type}};
    $sp->{data} = $ref->{r}->($buf) if $ref && $ref->{r};
    $sp;
}

sub save {
    my $sp = shift;
    my $buf = Crypt::OpenPGP::Buffer->new;
    my $tag = $sp->{type};
    $tag |= 0x80 if $sp->{critical};
    $buf->put_int8($tag);
    my $ref = $SUBPACKET_TYPES{$sp->{type}};
    $ref->{w}->($buf, $sp->{data}) if $ref && $ref->{w};
    $buf->bytes;
}

1;
