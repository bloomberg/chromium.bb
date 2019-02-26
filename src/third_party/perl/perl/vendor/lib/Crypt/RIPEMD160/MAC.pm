package Crypt::RIPEMD160::MAC;

use Crypt::RIPEMD160 0.03;

use strict;
use vars qw($VERSION @ISA @EXPORT);

require Exporter;
@ISA = qw(Exporter);

# Items to export into callers namespace by default
@EXPORT = qw();
$VERSION = '0.01';

sub new {
    my($pkg, $key) = @_;

    my $self = {
	'key' => $key,
	'hash' => new Crypt::RIPEMD160,
	'k_ipad' => chr(0x36) x 64,
	'k_opad' => chr(0x5c) x 64,
	};

    bless $self, $pkg;

    if (length($self->{'key'}) > 64) {
	$self->{'key'} = Crypt::RIPEMD160->hash($self->{'key'});
    }

    $self->{'k_ipad'} ^= $self->{'key'};
    $self->{'k_opad'} ^= $self->{'key'};

    $self->{'hash'}->add($self->{'k_ipad'});

    return $self;
}

sub reset {
    my($self) = @_;

    $self->{'hash'}->reset();
    $self->{'k_ipad'} = chr(0x36) x 64;
    $self->{'k_opad'} = chr(0x5c) x 64;

    if (length($self->{'key'}) > 64) {
	$self->{'key'} = Crypt::RIPEMD160->hash($self->{'key'});
    }

    $self->{'k_ipad'} ^= $self->{'key'};
    $self->{'k_opad'} ^= $self->{'key'};

    $self->{'hash'}->add($self->{'k_ipad'});

    return $self;
}

sub add {
    my($self, @data) = @_;

    $self->{'hash'}->add(@data);
}

sub addfile
{
    no strict 'refs';
    my ($self, $handle) = @_;
    my ($package, $file, $line) = caller;
    my ($data);

    if (!ref($handle)) {
	$handle = $package . "::" . $handle unless ($handle =~ /(\:\:|\')/);
    }
    while (read($handle, $data, 8192)) {
	$self->{'hash'}->add($data);
    }
}

sub mac {
    my($self) = @_;

    my($inner) = $self->{'hash'}->digest();

    my($outer) = Crypt::RIPEMD160->hash($self->{'k_opad'}.$inner);

    $self->{'key'} = "";
    $self->{'k_ipad'} = "";
    $self->{'k_opad'} = "";

    return($outer);
}

sub hexmac {
    my($self) = @_;

    my($inner) = $self->{'hash'}->digest();

    my($outer) = Crypt::RIPEMD160->hexhash($self->{'k_opad'}.$inner);

    $self->{'key'} = "";
    $self->{'k_ipad'} = "";
    $self->{'k_opad'} = "";

    return($outer);
}

1;
__END__
# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

Crypt::RIPEMD160::MAC - Perl extension for RIPEMD-160 MAC function

=head1 SYNOPSIS

    use Crypt::RIPEMD160::MAC;
    
    $key = "This is the secret key";

    $mac = new Crypt::RIPEMD160::MAC($key);

    $mac->reset();
    
    $mac->add(LIST);
    $mac->addfile(HANDLE);
    
    $digest = $mac->mac();
    $string = $mac->hexmac();

=head1 DESCRIPTION

The B<Crypt::RIPEMD160> module allows you to use the RIPEMD160
Message Digest algorithm from within Perl programs.

=head1 EXAMPLES

    use Crypt::RIPEMD160;
    
    $ripemd160 = new Crypt::RIPEMD160;
    $ripemd160->add('foo', 'bar');
    $ripemd160->add('baz');
    $digest = $ripemd160->digest();
    
    print("Digest is " . unpack("H*", $digest) . "\n");

The above example would print out the message

    Digest is f137cb536c05ec2bc97e73327937b6e81d3a4cc9

provided that the implementation is working correctly.

=head1 AUTHOR

The RIPEMD-160 interface was written by Christian H. Geuer 
(C<christian.geuer@crypto.gun.de>).

=head1 SEE ALSO

MD5(3pm) and SHA(1).

=cut
