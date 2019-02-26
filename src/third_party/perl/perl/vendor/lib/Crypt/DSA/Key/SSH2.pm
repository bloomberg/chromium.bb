package Crypt::DSA::Key::SSH2;

use strict;
use MIME::Base64 qw( decode_base64 );
use Crypt::DSA::Key;

use vars qw{$VERSION @ISA};
BEGIN {
    $VERSION = '1.17';
    @ISA     = 'Crypt::DSA::Key';
}

use constant PRIVKEY_MAGIC => 0x3f6ff9eb;

sub deserialize {
    my $key = shift;
    my %param = @_;

    chomp($param{Content});
    my($head, $object, $content, $tail) = $param{Content} =~
        m:(---- BEGIN ([^\n\-]+) ----)\n(.+)(---- END .*? ----)$:s;
    my @lines = split /\n/, $content;
    my $escaped = 0;
    my @real;
    for my $l (@lines) {
        if (substr($l, -1) eq '\\') {
            $escaped++;
            next;
        }
        next if index($l, ':') != -1;
        if ($escaped) {
            $escaped--;
            next;
        }
        push @real, $l;
    }
    $content = join "\n", @real;
    $content = decode_base64($content);

    my $b = BufferWithInt->new;
    $b->append($content);
    my $magic = $b->get_int32;
    return unless $magic == PRIVKEY_MAGIC;

    my($ignore);
    $ignore = $b->get_int32;
    my $type = $b->get_str;
    my $cipher = $b->get_str;
    $ignore = $b->get_int32 for 1..3;

    return unless $cipher eq 'none';

    $key->p( $b->get_mp_ssh2 );
    $key->g( $b->get_mp_ssh2 );
    $key->q( $b->get_mp_ssh2 );
    $key->pub_key( $b->get_mp_ssh2 );
    $key->priv_key( $b->get_mp_ssh2 );

    #return unless $b->length == $b->offset;

    $key;
}

sub serialize {
    my $key = shift;
    my %param = @_;
    die "serialize is unimplemented";
}

package BufferWithInt;
use strict;

use Data::Buffer;
use Crypt::DSA::Util qw( bin2mp );
use base qw( Data::Buffer );

sub get_mp_ssh2 {
    my $buf = shift;
    my $bits = $buf->get_int32;
    my $off = $buf->{offset};
    my $bytes = int(($bits+7) / 8);
    my $int = bin2mp( $buf->bytes($off, $bytes) );
    $buf->{offset} += $bytes;
    $int;
}

1;
__END__

=head1 NAME

Crypt::DSA::Key::SSH2 - Read/write DSA SSH2 files

=head1 SYNOPSIS

    use Crypt::DSA::Key;
    my $key = Crypt::DSA::Key->new( Type => 'SSH2', ...);
    $key->write( Type => 'SSH2', ...);

=head1 DESCRIPTION

I<Crypt::DSA::Key::SSH2> provides an interface to reading and
writing DSA SSH2 files, using I<Data::Buffer>, which provides
functionality for SSH-compatible binary in/out buffers.

Currently encrypted key files are not supported.

You shouldn't use this module directly. As the SYNOPSIS above
suggests, this module should be considered a plugin for
I<Crypt::DSA::Key>, and all access to SSH2 files (reading DSA
keys from disk, etc.) should be done through that module.

Read the I<Crypt::DSA::Key> documentation for more details.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::DSA manpage for author, copyright,
and license information.

=cut
