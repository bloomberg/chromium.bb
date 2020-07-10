package Crypt::OpenPGP::Message;
use strict;

use Crypt::OpenPGP::Buffer;
use Crypt::OpenPGP::PacketFactory;
use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $class = shift;
    my $msg = bless { }, $class;
    $msg->init(@_);
}

sub init {
    my $msg = shift;
    my %param = @_;
    $msg->{is_packet_stream} = delete $param{IsPacketStream};
    $msg->{pieces} = [];
    $msg->{_data} = $param{Data} || '';
    if (!$msg->{_data} && (my $file = $param{Filename})) {
        local *FH;
        open FH, $file or
            return (ref $msg)->error("Can't open message $file: $!");
        binmode FH;
        { local $/; $msg->{_data} = <FH> }
        close FH;
    }
    $msg->read or return;
    $msg;
}

sub read {
    my $msg = shift;
    my $data = $msg->{_data} or
        return $msg->error("Message contains no data");
    my $pt;
    if (!$msg->{is_packet_stream} &&
        $data =~ /-----BEGIN PGP SIGNED MESSAGE/) {
        require Crypt::OpenPGP::Armour;
        require Crypt::OpenPGP::Util;
        require Crypt::OpenPGP::Plaintext;
        my($head, $text, $sig) = $data =~
            m!-----BEGIN PGP SIGNED MESSAGE-----(.*?\r?\n\r?\n)?(.+?)(-----BEGIN PGP SIGNATURE.*?END PGP SIGNATURE-----)!s;
        ## In clear-signed messages, the line ending before the signature
        ## is not considered part of the signed text.
        $text =~ s!\r?\n$!!;
        $pt = Crypt::OpenPGP::Plaintext->new(
                              Data => Crypt::OpenPGP::Util::dash_unescape($text),
                              Mode => 't',
                    );
        $data = $sig;
    }

    if (!$msg->{is_packet_stream} && $data =~ /^-----BEGIN PGP/m) {
        require Crypt::OpenPGP::Armour;
        my $rec = Crypt::OpenPGP::Armour->unarmour($data) or
            return $msg->error("Unarmour failed: " .
                Crypt::OpenPGP::Armour->errstr);
        $data = $rec->{Data};
    }
    my $buf = Crypt::OpenPGP::Buffer->new;
    $buf->append($data);
    $msg->restore($buf);
    push @{ $msg->{pieces} }, $pt if $pt;
    1;
}

sub restore {
    my $msg = shift;
    my($buf) = @_;
    while (my $packet = Crypt::OpenPGP::PacketFactory->parse($buf)) {
        push @{ $msg->{pieces} }, $packet;
    }
}

sub pieces { @{ $_[0]->{pieces} } }

1;
__END__

=head1 NAME

Crypt::OpenPGP::Message - Sequence of PGP packets

=head1 SYNOPSIS

    use Crypt::OpenPGP::Message;

    my $data; $data .= $_ while <STDIN>;
    my $msg = Crypt::OpenPGP::Message->new( Data => $data );
    my @pieces = $msg->pieces;

=head1 DESCRIPTION

I<Crypt::OpenPGP::Message> provides a container for a sequence of PGP
packets. It transparently handles ASCII-armoured messages, as well as
cleartext signatures.

=head1 USAGE

=head2 Crypt::OpenPGP::Message->new( %arg )

Constructs a new I<Crypt::OpenPGP::Message> object, presumably to be
filled with some data, where the data is a serialized stream of PGP
packets.

Reads the packets into in-memory packet objects.

Returns the new I<Message> object on success, C<undef> on failure.

I<%arg> can contain:

=over 4

=item * Data

A scalar string containing the serialized packets.

This argument is optional, but either this argument or I<Filename> must
be provided.

=item * Filename

The path to a file that contains a serialized stream of packets.

This argument is optional, but either this argument or I<Data> must be
provided.

=item * IsPacketStream

By default I<Crypt::OpenPGP::Message> will attempt to unarmour ASCII-armoured
text. Since the armoured text can actually appear anywhere in a string, as
long as it starts at the beginning of a line, this can cause problems when a
stream of packets happens to include armoured text. At those times you want
the packets to be treated as a stream, not as a string that happens to contain
armoured text.

In this case, set I<IsPacketStream> to a true value, and the ASCII armour
detection will be skipped.

=back

=head2 $msg->pieces

Returns an array containing packet objects. For example, if the packet
stream contains a public key packet, a user ID packet, and a signature
packet, the array will contain three objects: a
I<Crypt::OpenPGP::Certificate> object; a I<Crypt::OpenPGP::UserID>
object; and a I<Crypt::OpenPGP::Signature> object, in that order.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
