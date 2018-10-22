package Crypt::OpenPGP::KeyServer;
use strict;

use Crypt::OpenPGP;
use Crypt::OpenPGP::KeyRing;
use LWP::UserAgent;
use URI::Escape;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $class = shift;
    my $server = bless { }, $class;
    $server->init(@_)
        or return $class->error($server->errstr);
    $server;
}

sub init {
    my $server = shift;
    my %param = @_;
    $server->{keyserver} = $param{Server}
        or return $server->error("Need a keyserver ('Server')");
    $server->{keyserver} = 'http://' . $server->{keyserver} . ':11371' .
                           '/pks/lookup';
    $server->{include_revoked} = $param{IncludeRevoked} || 0;
    $server;
}


sub find_keyblock_by_uid {
    my $server = shift;
    my($address) = @_;
    my $ua = LWP::UserAgent->new;
    $ua->agent('Crypt::OpenPGP/' . Crypt::OpenPGP->VERSION);
    my $url = $server->{keyserver} . '?op=index&search=' .
              uri_escape($address);
    my $req = HTTP::Request->new(GET => $url);
    my $res = $ua->request($req);
    return $server->error("HTTP error: " . $res->status_line)
        unless $res->is_success;
    my $page = $res->content;
    my @kb;
    while ($page =~ m!(pub.*?&gt;)!gs) {
        my $line = $1;
        next if index($line, "*** KEY REVOKED ***") != -1 &&
                !$server->{include_revoked};
        my($key_id) = $line =~ m!<a.*?>(.{8})</a>!g;
        my $kb = $server->find_keyblock_by_keyid(pack 'H*', $key_id) or next;
        push @kb, $kb;
    }
    @kb;
}

sub find_keyblock_by_keyid {
    my $server = shift;
    my($key_id) = @_;
    $key_id = unpack 'H*', $key_id;
    my $ua = LWP::UserAgent->new;
    $ua->agent('Crypt::OpenPGP/' . Crypt::OpenPGP->VERSION);
    $key_id = substr($key_id, -8, 8);
    my $url = $server->{keyserver} . '?op=get&search=0x' . $key_id;
    my $req = HTTP::Request->new(GET => $url);
    my $res = $ua->request($req);
    return $server->error("HTTP error: " . $res->status_line)
        unless $res->is_success;
    my $page = $res->content;
    my($key) = $page =~ /(-----BEGIN PGP PUBLIC KEY BLOCK-----.*?-----END PGP PUBLIC KEY BLOCK-----)/s;
    return $server->error("No matching keys") unless $key;
    my $ring = Crypt::OpenPGP::KeyRing->new( Data => $key )
        or return Crypt::OpenPGP::KeyRing->errstr;
    $ring->find_keyblock_by_index(0);
}

1;
__END__

=head1 NAME

Crypt::OpenPGP::KeyServer - Interface to HKP keyservers

=head1 SYNOPSIS

    use Crypt::OpenPGP::KeyServer;

    my $key_id = '...';
    my $server = Crypt::OpenPGP::KeyServer->new(
        Server => 'wwwkeys.us.pgp.net'
    );
    my $kb = $server->find_keyblock_by_keyid($key_id);
    print $kb->primary_uid, "\n";
    my $cert = $kb->key;

    my @kbs = $server->find_keyblock_by_uid( 'foo@bar.com' );

=head1 DESCRIPTION

I<Crypt::OpenPGP::KeyServer> is an interface to HKP keyservers; it provides
lookup by UID and by key ID. At the moment only HKP keyservers are
supported; future releases will likely support the NAI LDAP servers and
the email keyservers.

=head1 USAGE

=head2 Crypt::OpenPGP::KeyServer->new( %arg )

Constructs a new I<Crypt::OpenPGP::KeyServer> object and returns that
object.

I<%arg> can contain:

=over 4

=item * Server

The hostname of the HKP keyserver. This is a required argument. You can get
a list of keyservers through

    % host -l pgp.net | grep wwwkeys

=item * IncludeRevoked

By default, revoked keys will be skipped when calling I<find_keyblock_by_uid>.
If you set I<IncludeRevoked> to C<1>, I<find_keyblock_by_keyid> will include
any revoked keys in the list of keys returned.

=back

=head2 $ring->find_keyblock_by_keyid($key_id)

Looks up the key ID I<$key_id> in the keyring I<$ring>. For consistency
with the I<Crypt::OpenPGP::KeyRing::find_keyblock_by_keyid> interface,
I<$key_id> should be either a 4-octet or 8-octet string--it should
B<not> be a string of hexadecimal digits. If you have the hex key ID, use
I<pack> to convert it to an octet string:

    pack 'H*', $hex_key_id

Returns false on failure.

=head2 $ring->find_keyblock_by_uid($uid)

Given a string I<$uid>, searches the keyserver for all keyblocks matching
the user ID I<$uid>, including partial matches. Returns all of the matching
keyblocks, the empty list on failure.

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
