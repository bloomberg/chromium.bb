#!/usr/bin/perl -sw
##
## Crypt::RSA::Errorhandler -- Base class that provides error 
##                             handling functionality.
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Errorhandler.pm,v 1.5 2001/06/22 23:27:35 vipul Exp $

package Crypt::RSA::Errorhandler; 
use strict;

sub new { 
    bless {}, shift
}


sub error { 
    my ($self, $errstr, @towipe) = @_;
    $$self{errstr} = "$errstr\n";
    for (@towipe) { 
        my $var = $_;
        if (ref($var) =~ /Crypt::RSA/) { 
            $var->DESTROY();
        } elsif (ref($var) eq "SCALAR") { 
            $$var = ""; 
        } elsif (ref($var) eq "ARRAY") { 
            @$var = ();
        } elsif (ref($var) eq "HASH") { 
            %$var = ();
        }
    }
    return;    
} 


sub errstr { 
    my $self = shift;
    return $$self{errstr};
}

sub errstrrst { 
    my $self = shift;
    $$self{errstr} = "";
}

1;


=head1 NAME

Crypt::RSA::Errorhandler - Error handling mechanism for Crypt::RSA.

=head1 SYNOPSIS

    package Foo;

    use Crypt::RSA::Errorhandler;
    @ISA = qw(Crypt::RSA::Errorhandler);
    
    sub alive { 
        ..
        ..
        return 
        $self->error ("Awake, awake! Ring the alarum bell. \
                       Murther and treason!", $dagger) 
            if $self->murdered($king);
    }


    package main; 

    use Foo;
    my $foo = new Foo;
    $foo->alive($king) or print $foo->errstr(); 
        # prints "Awake, awake! ... "

=head1 DESCRIPTION 

Crypt::RSA::Errorhandler encapsulates the error handling mechanism used
by the modules in Crypt::RSA bundle. Crypt::RSA::Errorhandler doesn't
have a constructor and is meant to be inherited. The derived modules use
its two methods, error() and errstr(), to communicate error messages to
the caller.

When a method of the derived module fails, it calls $self->error() and
returns undef to the caller. The error message passed to error() is made
available to the caller through the errstr() accessor. error() also
accepts a list of sensitive data that it wipes out (undef'es) before
returning.

The caller should B<never> call errstr() to check for errors. errstr()
should be called only when a method indicates (usually through an undef
return value) that an error has occured. This is because errstr() is
never overwritten and will always contain a value after the occurance of
first error.

=head1 METHODS

=over 4

=item B<new()>

Barebones constructor.

=item B<error($mesage, ($wipeme, $wipemetoo))>

The first argument to error() is $message which is placed in $self-
>{errstr} and the remaining arguments are interpretted as
variables containing sensitive data that are wiped out from the
memory. error() always returns undef.

=item B<errstr()> 

errstr() is an accessor method for $self->{errstr}.

=item B<errstrrst()>

This method sets $self->{errstr} to an empty string.

=back

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 SEE ALSO 

Crypt::RSA(3)

=cut


