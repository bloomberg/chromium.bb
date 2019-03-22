#!/usr/bin/perl -sw
##
## Crypt::RSA::Key::Private 
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Private.pm,v 1.15 2001/09/25 14:11:22 vipul Exp $

package Crypt::RSA::Key::Private;
use strict; 
use vars qw($AUTOLOAD $VERSION);
use base 'Crypt::RSA::Errorhandler';
use Tie::EncryptedHash; 
use Data::Dumper;
use Math::Pari qw(PARI pari2pv Mod isprime lcm lift);
use Carp;

$Crypt::RSA::Key::Private::VERSION = '1.99'; 

sub new { 

    my ($class, %params) = @_; 
    my $self    = { Version => $Crypt::RSA::Key::Private::VERSION };
    if ($params{Filename}) { 
        bless $self, $class;
        $self = $self->read (%params);
        return bless $self, $class; 
    } else { 
        bless $self, $class;
        $self->Identity ($params{Identity}) if $params{Identity};
        $self->Cipher   ($params{Cipher}||"Blowfish");
        $self->Password ($params{Password}) if $params{Password};
        return $self;
    } 

} 


sub AUTOLOAD { 

    my ($self, $value) = @_;
    my $key = $AUTOLOAD; $key =~ s/.*:://;
    if ($key =~ /^(e|n|d|p|q|dp|dq|u|phi)$/) { 
        if (ref $value eq 'Math::Pari') { 
            $self->{private}{"_$key"} = $value;
            $self->{Checked} = 0;
        } elsif ($value && !(ref $value)) { 
            if ($value =~ /^0x/) { 
                $self->{private}->{"_$key"} = 
                    Math::Pari::_hex_cvt($value);
                $self->{Checked} = 0;
            } else { $self->{private}{"_$key"} = PARI($value) } 
        }
        return $self->{private}{"_$key"} || 
               $self->{private_encrypted}{"_$key"} || 
               "";
    } elsif ($key =~ /^Identity|Cipher|Password$/) { 
        $self->{$key} = $value if $value; 
        return $self->{$key};
    } elsif ($key =~ /^Checked$/) { 
        my ($package) = caller();
        $self->{Checked} = $value if ($value && $package eq "Crypt::RSA::Key::Private") ;
        return $self->{Checked};
    }
} 


sub hide { 

    my ($self) = @_; 

    return undef unless $$self{Password};

    $self->{private_encrypted} = new Tie::EncryptedHash 
            __password => $self->{Password},
            __cipher   => $self->{Cipher};

    for (keys %{$$self{private}}) { 
        $$self{private_encrypted}{$_} = pari2pv($$self{private}{$_});
    }

    my $private = $self->{private_encrypted};
    delete $private->{__password};
    delete $$self{private};
    delete $$self{Password};

} 


sub reveal { 

    my ($self, %params) = @_; 
    $$self{Password} = $params{Password} if $params{Password};
    return undef unless $$self{Password};
    $$self{private_encrypted}{__password} = $params{Password};
    for (keys %{$$self{private_encrypted}}) { 
        $$self{private}{$_} = PARI($$self{private_encrypted}{$_});
    }
 
}


sub check { 

    my ($self) = @_;

    return 1 if $self->{Checked};

    return $self->error ("Incomplete key.") unless 
        ($self->n && $self->d) || ($self->n && $self->p && $self->q);

    if ($self->p && $self->q) { 
        return $self->error ("n is not a number.") if $self->n =~ /\D/;
        return $self->error ("p is not a number.") if $self->p =~ /\D/;
        return $self->error ("p is not a number.") if $self->p =~ /\D/;
        return $self->error ("n is not p*q."  ) unless $self->n == $self->p * $self->q;
        return $self->error ("p is not prime.") unless isprime($self->p);
        return $self->error ("q is not prime.") unless isprime($self->q);
    }

    if ($self->e) { 
        # d * e == 1 mod lcm(p-1, q-1)
        return $self->error ("e is not a number.") if $self->e =~ /\D/;
        my $k = lcm (($self->p -1), ($self->q -1));
        my $K = Mod (1, $k); my $KI = lift($K * $self->d * $self->e);
        return $self->error ("Bad `d'.") unless $KI == 1;
    }

    if ($self->dp) {
        # dp == d mod (p-1)
        return $self->error ("Bad `dp'.") unless $self->dp == $self->d % ($self->p - 1);
    }

    if ($self->dq) {
        # dq == d mod (q-1)
        return $self->error ("Bad `dq'.") unless $self->dq == $self->d % ($self->q - 1);
    }

    if ($self->u && $self->q && $self->p) { 
        my $m =  Mod (1,$self->q); $m = lift ($m / $self->p);
        return $self->error ("Bad `u'.") unless $self->u == $m;
    }

    $self->Checked(1);
    return 1;

}


sub DESTROY { 

    my $self = shift; 
    delete $$self{private_encrypted}{__password}; 
    delete $$self{private_encrypted};
    delete $$self{private};
    delete $$self{Password};
    undef $self;

}


sub write { 

    my ($self, %params) = @_; 
    $self->hide();
    my $string = $self->serialize (%params); 
    open DISK, ">$params{Filename}" or
        croak "Can't open $params{Filename} for writing.";
    binmode DISK;
    print DISK $string;
    close DISK;

} 


sub read { 
    my ($self, %params) = @_;
    open DISK, $params{Filename} or 
        croak "Can't open $params{Filename} to read.";
    binmode DISK;
    my @key = <DISK>; 
    close DISK;
    $self = $self->deserialize (String => \@key);
    $self->reveal(%params);
    return $self;
}


sub serialize { 

    my ($self, %params) = @_;
    if ($$self{private}) {   # this is an unencrypted key
        for (keys %{$$self{private}}) { 
            $$self{private}{$_} = pari2pv($$self{private}{$_});
        }
    }
    return Dumper $self; 

} 


sub deserialize { 

    my ($self, %params) = @_; 
    my $string = join'', @{$params{String}}; 
    $string =~ s/\$VAR1 =//;
    $self = eval $string;
    if ($$self{private}) { # the key is unencrypted 
        for (keys %{$$self{private}}) { 
            $$self{private}{$_} = PARI($$self{private}{$_});
        }
        return $self;
    }
    my $private = new Tie::EncryptedHash; 
    %$private = %{$$self{private_encrypted}};
    $self->{private_encrypted} = $private;
    return $self;

}

 
1;

=head1 NAME

Crypt::RSA::Key::Private -- RSA Private Key Management.

=head1 SYNOPSIS

    $key = new Crypt::RSA::Key::Private (
                Identity => 'Lord Banquo <banquo@lochaber.com>',
                Password => 'The earth hath bubbles',
           );

    $key->hide();

    $key->write( Filename => 'rsakeys/banquo.private'  );

    $akey = new Crypt::RSA::Key::Private (
                 Filename => 'rsakeys/banquo.private'
                );   

    $akey->reveal ( Password => 'The earth hath bubbles' );

=head1 DESCRIPTION

Crypt::RSA::Key::Private provides basic private key management
functionality for Crypt::RSA private keys. Following methods are
available:

=over 4 

=item B<new()>

The constructor. Takes a hash, usually with two arguments: C<Filename> and
C<Password>. C<Filename> indicates a file from which the private key
should be read. More often than not, private keys are kept encrypted with
a symmetric cipher and MUST be decrypted before use. When a C<Password>
argument is provided, the key is also decrypted before it is returned by
C<new()>. Here's a complete list of arguments accepted by C<new()> (all of
which are optional):

=over 4

=item Identity 

A string identifying the owner of the key. Canonically, a name and
email address.

=item Filename 

Name of the file that contains the private key. 

=item Password

Password with which the private key is encrypted, or should be encrypted
(in case of a new key).

=item Cipher 

Name of the symmetric cipher in which the private key is encrypted (or
should be encrypted). The default is "Blowfish" and possible values
include DES, IDEA, Twofish and other ciphers supported by Crypt::CBC.

=back

=item B<reveal()>

If the key is not decrypted at C<new()>, it can be decrypted by
calling C<reveal()> with a C<Password> argument.

=item B<hide()>

C<hide()> causes the key to be encrypted by the chosen symmetric cipher
and password.

=item B<write()>

Causes the key to be written to a disk file specified by the
C<Filename> argument. C<write()> will call C<hide()> before
writing the key to disk. If you wish to store the key in plain,
don't specify a password at C<new()>.

=item B<read()>

Causes the key to be read from a disk file specified by
C<Filename> into the object. If C<Password> is provided, the
method automatically calls reveal() to decrypt the key.

=item B<serialize()>

Creates a Data::Dumper(3) serialization of the private key and
returns the string representation.

=item B<deserialize()>

Accepts a serialized key under the C<String> parameter and
coverts it into the perl representation stored in the object.

=item C<check()>

Check the consistency of the key. If the key checks out, it sets
$self->{Checked} = 1. Returns undef on failure.

=back

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO

Crypt::RSA::Key(3), Crypt::RSA::Public(3)

=cut


