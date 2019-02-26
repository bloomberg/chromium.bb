package Crypt::OpenPGP::UserID;
use strict;

use Crypt::OpenPGP::ErrorHandler;
use base qw( Crypt::OpenPGP::ErrorHandler );

sub new {
    my $id = bless { }, shift;
    $id->init(@_);
}

sub init {
    my $id = shift;
    my %param = @_;
    if (my $ident = $param{Identity}) {
        $id->{id} = $ident;
    }
    $id;
}

sub id { $_[0]->{id} }
sub parse {
    my $class = shift;
    my($buf) = @_;
    my $id = $class->new;
    $id->{id} = $buf->bytes;
    $id;
}

sub save { $_[0]->{id} }

1;
__END__

=head1 NAME

Crypt::OpenPGP::UserID - PGP User ID packet

=head1 SYNOPSIS

    use Crypt::OpenPGP::UserID;

    my $uid = Crypt::OpenPGP::UserID->new( Identity => 'Foo' );
    my $serialized = $uid->save;
    my $identity = $uid->id;

=head1 DESCRIPTION

I<Crypt::OpenPGP::UserID> is a PGP User ID packet. Such a packet is
used to represent the name and email address of the key holder,
and typically contains an RFC822 mail name like

    Foo Bar <foo@bar.com>

=head1 USAGE

=head2 Crypt::OpenPGP::UserID->new( [ Identity => $identity ] )

Creates a new User ID packet object and returns that object. If you
do not supply an identity, the object is created empty; this is used,
for example, in I<parse> (below), to create an empty packet which is
then filled from the data in the buffer.

If you wish to initialize a non-empty object, supply I<new> with
the I<Identity> parameter along with a value I<$identity> which
should generally be in RFC822 form (above).

=head2 $uid->save

Returns the text of the user ID packet; this is the string passed to
I<new> (above) as I<$identity>, for example.

=head2 Crypt::OpenPGP::UserID->parse($buffer)

Given I<$buffer>, a I<Crypt::OpenPGP::Buffer> object holding (or
with offset pointing to) a User ID packet, returns a new
<Crypt::OpenPGP::UserID> object, initialized with the user ID data
in the buffer.

=head2 $uid->id

Returns the user ID data (eg. the string passed as I<$identity> to
I<new>, above).

=head1 AUTHOR & COPYRIGHTS

Please see the Crypt::OpenPGP manpage for author, copyright, and
license information.

=cut
