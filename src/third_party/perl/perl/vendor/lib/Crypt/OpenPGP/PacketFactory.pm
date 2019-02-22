package Crypt::OpenPGP::PacketFactory;
use strict;

use Crypt::OpenPGP::Constants qw( :packet );
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

use vars qw( %PACKET_TYPES %PACKET_TYPES_BY_CLASS );
%PACKET_TYPES = (
    PGP_PKT_PUBKEY_ENC()    => { class => 'Crypt::OpenPGP::SessionKey' },
    PGP_PKT_SIGNATURE()     => { class => 'Crypt::OpenPGP::Signature' },
    PGP_PKT_SYMKEY_ENC()    => { class => 'Crypt::OpenPGP::SKSessionKey' },
    PGP_PKT_ONEPASS_SIG()   => { class => 'Crypt::OpenPGP::OnePassSig' },
    PGP_PKT_SECRET_KEY()    => { class => 'Crypt::OpenPGP::Certificate',
                                 args  => [ 1, 0 ] },
    PGP_PKT_PUBLIC_KEY()    => { class => 'Crypt::OpenPGP::Certificate',
                                 args  => [ 0, 0 ] },
    PGP_PKT_SECRET_SUBKEY() => { class => 'Crypt::OpenPGP::Certificate',
                                 args  => [ 1, 1 ] },
    PGP_PKT_USER_ID()       => { class => 'Crypt::OpenPGP::UserID' },
    PGP_PKT_PUBLIC_SUBKEY() => { class => 'Crypt::OpenPGP::Certificate',
                                 args  => [ 0, 1 ] },
    PGP_PKT_COMPRESSED()    => { class => 'Crypt::OpenPGP::Compressed' },
    PGP_PKT_ENCRYPTED()     => { class => 'Crypt::OpenPGP::Ciphertext' },
    PGP_PKT_MARKER()        => { class => 'Crypt::OpenPGP::Marker' },
    PGP_PKT_PLAINTEXT()     => { class => 'Crypt::OpenPGP::Plaintext' },
    PGP_PKT_RING_TRUST()    => { class => 'Crypt::OpenPGP::Trust' },
    PGP_PKT_ENCRYPTED_MDC() => { class => 'Crypt::OpenPGP::Ciphertext',
                                 args  => [ 1 ] },
    PGP_PKT_MDC()           => { class => 'Crypt::OpenPGP::MDC' },
);

%PACKET_TYPES_BY_CLASS = map { $PACKET_TYPES{$_}{class} => $_ } keys %PACKET_TYPES;

sub parse {
    my $class = shift;
    my($buf, $find, $parse) = @_;
    return unless $buf && $buf->offset < $buf->length;
    my(%find, %parse);
    if ($find) {
        %find = ref($find) eq 'ARRAY' ? (map { $_ => 1 } @$find) :
                                        ($find => 1);
    }
    else {
        %find = map { $_ => 1 } keys %PACKET_TYPES;
    }
    if ($parse) {
        %parse = ref($parse) eq 'ARRAY' ? (map { $_ => 1 } @$parse) :
                                           ($parse => 1);
    }
    else {
        %parse = %find;
    }

    my($type, $len, $partial, $hdrlen, $b);
    do {
        ($type, $len, $partial, $hdrlen) = $class->_parse_header($buf);
        $b = $buf->extract($len ? $len : $buf->length - $buf->offset);
        return unless $type;
    } while !$find{$type};                 ## Skip

    while ($partial) {
        my $off = $buf->offset;
        (my($nlen), $partial) = $class->_parse_new_len_header($buf);
        $len += $nlen + ($buf->offset - $off);
        $b->append( $buf->get_bytes($nlen) );
    }

    my $obj;
    if ($parse{$type} && (my $ref = $PACKET_TYPES{$type})) {
        my $pkt_class = $ref->{class};
        my @args = $ref->{args} ? @{ $ref->{args} } : ();
        eval "use $pkt_class;";
        return $class->error("Loading $pkt_class failed: $@") if $@;
        $obj = $pkt_class->parse($b, @args);
    }
    else {
        $obj = { type => $type, length => $len,
                 __pkt_len => $len + $hdrlen, __unparsed => 1 };
    }
    $obj;
}

sub _parse_header {
    my $class = shift;
    my($buf) = @_;
    return unless $buf && $buf->offset < $buf->length;

    my $off_start = $buf->offset;
    my $tag = $buf->get_int8;
    return $class->error("Parse error: bit 7 not set!")
        unless $tag & 0x80;
    my $is_new = $tag & 0x40;
    my($type, $len, $partial);
    if ($is_new) {
        $type = $tag & 0x3f;
        ($len, $partial) = $class->_parse_new_len_header($buf);
    }
    else {
        $type = ($tag>>2)&0xf;
        my $lenbytes = (($tag&3)==3) ? 0 : (1<<($tag & 3));
        $len = 0;
        for (1..$lenbytes) {
            $len <<= 8;
            $len += $buf->get_int8;
        }
    }
    ($type, $len, $partial, $buf->offset - $off_start);
}

sub _parse_new_len_header {
    my $class = shift;
    my($buf) = @_;
    return unless $buf && $buf->offset < $buf->length;
    my $lb1 = $buf->get_int8;
    my($partial, $len);
    if ($lb1 <= 191) {
        $len = $lb1;
    } elsif ($lb1 <= 223) {
        $len = (($lb1-192) << 8) + $buf->get_int8 + 192;
    } elsif ($lb1 < 255) {
        $partial++;
        $len = 1 << ($lb1 & 0x1f);
    } else {
        $len = $buf->get_int32;
    }
    ($len, $partial);
}

sub save {
    my $class = shift;
    my @objs = @_;
    my $ser = '';
    for my $obj (@objs) {
        my $body = $obj->save;
        my $len = length($body);
        my $type = $obj->can('pkt_type') ? $obj->pkt_type :
                   $PACKET_TYPES_BY_CLASS{ref($obj)};
        my $hdrlen = $obj->can('pkt_hdrlen') ? $obj->pkt_hdrlen : undef;
        my $buf = Crypt::OpenPGP::Buffer->new;
        if ($obj->{is_new} || $type > 15) {
            my $tag = 0xc0 | ($type & 0x3f);
            $buf->put_int8($tag);
            return $class->error("Can't write partial length packets")
                unless $len;
            if ($len < 192) {
                $buf->put_int8($len);
            } elsif ($len < 8384) {
                $len -= 192;
                $buf->put_int8(int($len / 256) + 192);
                $buf->put_int8($len % 256);
            } else {
                $buf->put_int8(0xff);
                $buf->put_int32($len);
            }
        }
        else {
            unless ($hdrlen) {
                if (!defined $len) {
                    $hdrlen = 0;
                } elsif ($len < 256) {
                    $hdrlen = 1;
                } elsif ($len < 65536) {
                    $hdrlen = 2;
                } else {
                    $hdrlen = 4;
                }
            }
            return $class->error("Packet overflow: overflow preset len")
                if $hdrlen == 1 && $len > 255;
            $hdrlen = 4 if $hdrlen == 2 && $len > 65535;
            my $tag = 0x80 | ($type << 2);
            if ($hdrlen == 0) {
                $buf->put_int8($tag | 3);
            } elsif ($hdrlen == 1) {
                $buf->put_int8($tag);
                $buf->put_int8($len);
            } elsif ($hdrlen == 2) {
                $buf->put_int8($tag | 1);
                $buf->put_int16($len);
            } else {
                $buf->put_int8($tag | 2);
                $buf->put_int32($len);
            }
        }
        $buf->put_bytes($body);
        $ser .= $buf->bytes;
    }
    $ser;
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::PacketFactory - Parse and save PGP packet streams

=head1 SYNOPSIS

    use Crypt::OpenPGP::PacketFactory;

    my $buf = Crypt::OpenPGP::Buffer->new;
    while (my $packet = Crypt::OpenPGP::PacketFactory->parse($buf)) {
        ## Do something with $packet
    }

    my @packets;
    my $serialized = Crypt::OpenPGP::PacketFactory->save(@packets);

=head1 DESCRIPTION

I<Crypt::OpenPGP::PacketFactory> parses PGP buffers (objects of type
I<Crypt::OpenPGP::Buffer>) and generates packet objects of various
packet classes (for example, I<Crypt::OpenPGP::Certificate> objects,
I<Crypt::OpenPGP::Signature> objects, etc.). It also takes lists of
packets, serializes each of them, and adds type/length headers to
them, forming a stream of packets suitable for armouring, writing
to disk, sending through email, etc.

=head1 USAGE

=head2 Crypt::OpenPGP::PacketFactory->parse($buffer [, $find ])

Given a buffer object I<$buffer> of type I<Crypt::OpenPGP::Buffer>,
iterates through the packets serialized in the buffer, parsing
each one, and returning each packet one by one. In other words, given
a buffer, it acts as a standard iterator.

By default I<parse> parses and returns all packets in the buffer, of
any packet type. If you are only looking for packets of a specific
type, though, it makes no sense to return every packet; you can
control which packets I<parse> parses and returns with I<$find>, which
should be a reference to a list of packet types to find in the buffer.
Only packets of those types will be parsed and returned to you. You
can get the packet type constants from I<Crypt::OpenPGP::Constants>
by importing the C<:packet> tag.

Returns the next packet in the buffer until the end of the buffer
is reached (or until there are no more of the packets which you wish
to find), at which point returns a false value.

=head2 Crypt::OpenPGP::PacketFactory->save(@packets)

Given a list of packets I<@packets>, serializes each packet, then
adds a type/length header on to each one, resulting in a string of
octets representing the serialized packets, suitable for passing in
to I<parse>, or for writing to disk, or anywhere else.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
