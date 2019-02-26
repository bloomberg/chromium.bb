package Crypt::OpenPGP::KeyRing;
use strict;

use Crypt::OpenPGP::Constants qw( PGP_PKT_USER_ID
                                  PGP_PKT_PUBLIC_KEY
                                  PGP_PKT_SECRET_KEY
                                  PGP_PKT_PUBLIC_SUBKEY
                                  PGP_PKT_SECRET_SUBKEY );
use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::KeyBlock;
use Crypt::OpenPGP::PacketFactory;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $class = shift;
    my $ring = bless { }, $class;
    $ring->init(@_);
}

sub init {
    my $ring = shift;
    my %param = @_;
    $ring->{_data} = $param{Data} || '';
    if (!$ring->{_data} && (my $file = $param{Filename})) {
        local *FH;
        open FH, $file or
            return (ref $ring)->error("Can't open keyring $file: $!");
        binmode FH;
        { local $/; $ring->{_data} = <FH> }
        close FH;
    }
    if ($ring->{_data} =~ /-----BEGIN/) {
        require Crypt::OpenPGP::Armour;
        my $rec = Crypt::OpenPGP::Armour->unarmour($ring->{_data}) or
            return (ref $ring)->error("Unarmour failed: " .
                Crypt::OpenPGP::Armour->errstr);
        $ring->{_data} = $rec->{Data};
    }
    $ring;
}

sub save {
    my $ring = shift;
    my @blocks = $ring->blocks;
    my $res = '';
    for my $block (@blocks) {
        $res .= $block->save;
    }
    $res;
}

sub read {
    my $ring = shift;
    return $ring->error("No data to read") unless $ring->{_data};
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->append($ring->{_data});
    $ring->restore($buf);
    1;
}

sub restore {
    my $ring = shift;
    my($buf) = @_;
    $ring->{blocks} = [];
    my($kb);
    while (my $packet = Crypt::OpenPGP::PacketFactory->parse($buf)) {
        if (ref($packet) eq "Crypt::OpenPGP::Certificate" &&
            !$packet->is_subkey) {
            $kb = Crypt::OpenPGP::KeyBlock->new;
            $ring->add($kb);
        }
        $kb->add($packet) if $kb;
    }
}

sub add {
    my $ring = shift;
    my($entry) = @_;
    push @{ $ring->{blocks} }, $entry;
}

sub find_keyblock_by_keyid {
    my $ring = shift;
    my($key_id) = @_;
    my $ref = $ring->{by_keyid}{$key_id};
    unless ($ref) {
        my $len = length($key_id);
        my @kbs = $ring->find_keyblock(
            sub { substr($_[0]->key_id, -$len, $len) eq $key_id },
            [ PGP_PKT_PUBLIC_KEY, PGP_PKT_SECRET_KEY,
              PGP_PKT_PUBLIC_SUBKEY, PGP_PKT_SECRET_SUBKEY ], 1 );
        return unless @kbs;
        $ref = $ring->{by_keyid}{ $key_id } = \@kbs;
    }
    return wantarray ? @$ref : $ref->[0];
}

sub find_keyblock_by_uid {
    my $ring = shift;
    my($uid) = @_;
    $ring->find_keyblock(sub { $_[0]->id =~ /$uid/i },
        [ PGP_PKT_USER_ID ], 1 );
}

sub find_keyblock_by_index {
    my $ring = shift;
    my($index) = @_;
    ## XXX should not have to read entire keyring
    $ring->read;
    ($ring->blocks)[$index];
}

sub find_keyblock {
    my $ring = shift;
    my($test, $pkttypes, $multiple) = @_;
    $pkttypes ||= [];
    return $ring->error("No data to read") unless $ring->{_data};
    my $buf = Crypt::OpenPGP::Buffer->new_with_init($ring->{_data});
    my($last_kb_start_offset, $last_kb_start_cert, @kbs);
    while (my $pkt = Crypt::OpenPGP::PacketFactory->parse($buf,
                      [ PGP_PKT_SECRET_KEY, PGP_PKT_PUBLIC_KEY,
                        @$pkttypes ], $pkttypes)) {
        if (($pkt->{__unparsed} && ($pkt->{type} == PGP_PKT_SECRET_KEY ||
                                   $pkt->{type} == PGP_PKT_PUBLIC_KEY)) ||
            (ref($pkt) eq 'Crypt::OpenPGP::Certificate' && !$pkt->is_subkey)) {
            $last_kb_start_offset = $buf->offset;
            $last_kb_start_cert = $pkt;
        }
        next unless !$pkt->{__unparsed} && $test->($pkt);
        my $kb = Crypt::OpenPGP::KeyBlock->new;

        ## Rewind buffer; if start-cert is parsed, rewind to offset
        ## after start-cert--otherwise rewind before start-cert
        if ($last_kb_start_cert->{__unparsed}) {
            $buf->set_offset($last_kb_start_offset -
                $last_kb_start_cert->{__pkt_len});
            my $cert = Crypt::OpenPGP::PacketFactory->parse($buf);
            $kb->add($cert);
        } else {
            $buf->set_offset($last_kb_start_offset);
            $kb->add($last_kb_start_cert);
        }
        {
            my $off = $buf->offset;
            my $packet = Crypt::OpenPGP::PacketFactory->parse($buf);
            last unless $packet;
            $buf->set_offset($off),
                last if ref($packet) eq "Crypt::OpenPGP::Certificate" &&
                    !$packet->is_subkey;
            $kb->add($packet) if $kb;
            redo;
        }
        unless ($multiple) {
            return wantarray ? ($kb, $pkt) : $kb;
        } else {
            return $kb unless wantarray;
            push @kbs, $kb;
        }
    }
    @kbs;
}

sub blocks { $_[0]->{blocks} ? @{ $_[0]->{blocks} } : () }

1;
__END__

=head1 NAME

Crypt::OpenPGP::KeyRing - Key ring object

=head1 SYNOPSIS

    use Crypt::OpenPGP::KeyRing;

    my $ring = Crypt::OpenPGP::KeyRing->new( Filename => 'foo.ring' );

    my $key_id = '...';
    my $kb = $ring->find_keyblock_by_keyid($key_id);

=head1 DESCRIPTION

I<Crypt::OpenPGP::KeyRing> provides keyring management and key lookup
for I<Crypt::OpenPGP>. A I<KeyRing>, in this case, does not necessarily
have to be a keyring file; a I<KeyRing> object is just a collection of
key blocks, where each key block contains exactly one master key,
zero or more subkeys, some user ID packets, some signatures, etc.

=head1 USAGE

=head2 Crypt::OpenPGP::KeyRing->new( %arg )

Constructs a new I<Crypt::OpenPGP::KeyRing> object and returns that
object. This has the effect os hooking the object to a particular
keyring, so that all subsequent methods called on the I<KeyRing>
object will use the data specified in the arguments to I<new>.

I<%arg> can contain:

=over 4

=item * Data

A block of data specifying the serialized keyring, presumably as read
in from a file on disk. This data can be either in binary form or in
ASCII-armoured form; if the latter it will be unarmoured automatically.

This argument is optional.

=item * Filename

The path to a keyring file, or at least, a file containing a key (and
perhaps other associated keyblock data). The data in this file can be
either in binary form or in ASCII-armoured form; if the latter it will be
unarmoured automatically.

This argument is optional.

=back

=head2 $ring->find_keyblock_by_keyid($key_id)

Looks up the key ID I<$key_id> in the keyring I<$ring>. I<$key_id>
should be either a 4-octet or 8-octet string--it should I<not> be a
string of hexadecimal digits. If that is what you have, use I<pack> to
convert it to an octet string:

    pack 'H*', $hex_key_id

If a keyblock is found where the key ID of either the master key or
subkey matches I<$key_id>, that keyblock will be returned. The
definition of "match" depends on the length of I<$key_id>: if it is a
16-digit hex number, only exact matches will be returned; if it is an
8-digit hex number, any keyblocks containing keys whose last 8 hex
digits match I<$key_id> will be returned.

In scalar context, only the first keyblock found in the keyring is
returned; in list context, all matching keyblocks are returned. In
practice, duplicated key IDs are rare, particularly so if you specify
the full 16 hex digits in I<$key_id>.

Returns false on failure (C<undef> in scalar context, an empty list in
list context).

=head2 $ring->find_keyblock_by_uid($uid)

Given a string I<$uid>, looks up all keyblocks with User ID packets
matching the string I<$uid>, including partial matches.

In scalar context, returns only the first keyblock with a matching
user ID; in list context, returns all matching keyblocks.

Returns false on failure.

=head2 $ring->find_keyblock_by_index($index)

Given an index into a list of keyblocks I<$index>, returns the keyblock
(a I<Crypt::OpenPGP::KeyBlock> object) at that index. Accepts negative
indexes, so C<-1> will give you the last keyblock in the keyring.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
