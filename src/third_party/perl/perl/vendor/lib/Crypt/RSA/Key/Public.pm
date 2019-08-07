#!/usr/bin/perl -sw
##
## Crypt::RSA::Key::Public
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Public.pm,v 1.8 2001/09/25 14:11:23 vipul Exp $

package Crypt::RSA::Key::Public;
use strict; 
use vars qw($AUTOLOAD);
use Carp;
use Data::Dumper;
use base 'Crypt::RSA::Errorhandler';
use Math::Pari qw(PARI pari2pv);

$Crypt::RSA::Key::Public::VERSION = '1.99';

sub new { 

    my ($class, %params) = @_; 
    my $self    = { Version => $Crypt::RSA::Key::Public::VERSION };
    if ($params{Filename}) { 
        bless $self, $class;
        $self = $self->read (%params);
        return bless $self, $class;
    } else { 
        return bless $self, $class;
    } 

} 


sub AUTOLOAD { 
    my ($self, $value) = @_;
    my $key = $AUTOLOAD; $key =~ s/.*:://;
    if ($key =~ /^n|e$/) { 
        if (ref $value eq 'Math::Pari') { 
            $self->{$key} = pari2pv($value)
        } elsif ($value && !(ref $value)) { 
            if ($value =~ /^0x/) { 
                $self->{$key} = pari2pv(Math::Pari::_hex_cvt($value));
            } else { $self->{$key} = $value } 
        }
        my $return  = $self->{$key} || "";
        $return = PARI("$return") if $return =~ /^\d+$/;
        return $return;
    } elsif ($key =~ /^Identity$/) { 
        $self->{$key} = $value if $value;
        return $self->{$key};
    }
        
} 


sub DESTROY { 

    my $self = shift; 
    undef $self;

}


sub check { 

    my $self = shift;
    return $self->error ("Incomplete key.") unless $self->n && $self->e;
    return 1;

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
    return $self;
}


sub serialize { 

    my ($self, %params) = @_;
    return Dumper $self; 

} 


sub deserialize { 

    my ($self, %params) = @_; 
    my $string = join'', @{$params{String}}; 
    $string =~ s/\$VAR1 =//;
    $self = eval $string;
    return $self;

}

    
1;

=head1 NAME

Crypt::RSA::Key::Public -- RSA Public Key Management.

=head1 SYNOPSIS

    $key = new Crypt::RSA::Key::Public; 
    $key->write ( Filename => 'rsakeys/banquo.public' );

    $akey = new Crypt::RSA::Key::Public (
                Filename => 'rsakeys/banquo.public' 
            );


=head1 DESCRIPTION

Crypt::RSA::Key::Public provides basic key management functionality for
Crypt::RSA public keys. Following methods are available:

=over 4

=item B<new()>

The constructor. Reads the public key from a disk file when
called with a C<Filename> argument.

=item B<write()>

Causes the key to be written to a disk file specified by the
C<Filename> argument.

=item B<read()>

Causes the key to be read from a disk file specified by
C<Filename> into the object.

=item B<serialize()>

Creates a Data::Dumper(3) serialization of the private key and
returns the string representation.

=item B<deserialize()>

Accepts a serialized key under the C<String> parameter and
coverts it into the perl representation stored in the object.

=item C<check()>

Check the consistency of the key. Returns undef on failure.

=back

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO

Crypt::RSA::Key(3), Crypt::RSA::Key::Private(3)

=cut


