package Crypt::RSA::Key::Public;
use strict;
use warnings;

## Crypt::RSA::Key::Public
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.

use vars qw($AUTOLOAD);
use Carp;
use Data::Dumper;
use base 'Crypt::RSA::Errorhandler';
use Math::BigInt try => 'GMP, Pari';

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
        if (defined $value) {
          if (ref $value eq 'Math::BigInt') {
            $self->{$key} = $value;
          } elsif (ref $value eq 'Math::Pari') {
            $self->{$key} = Math::BigInt->new($value->pari2pv);
          } else {
            $self->{$key} = Math::BigInt->new("$value");
          }
        }
        if (defined $self->{$key}) {
          $self->{$key} = Math::BigInt->new("$self->{$key}") unless ref($self->{$key}) eq 'Math::BigInt';
          return $self->{$key};
        }
        return;
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
    return $self->error ("Incomplete key.")
       unless defined $self->n && defined $self->e;
    return 1;

}


sub write {

    my ($self, %params) = @_;
    $self->hide();
    my $string = $self->serialize (%params);
    open(my $disk, '>', $params{Filename}) or
        croak "Can't open $params{Filename} for writing.";
    binmode $disk;
    print $disk $string;
    close $disk;

}


sub read {
    my ($self, %params) = @_;
    open(my $disk, '<', $params{Filename}) or
        croak "Can't open $params{Filename} to read.";
    binmode $disk;
    my @key = <$disk>;
    close $disk;
    $self = $self->deserialize (String => \@key);
    return $self;
}


sub serialize {

    my ($self, %params) = @_;
    # Convert bigints to strings
    foreach my $key (qw/n e/) {
      $self->{$key} = $self->{$key}->bstr if ref($self->{$key}) eq 'Math::BigInt';
    }
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


