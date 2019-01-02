#!/usr/bin/perl -sw
##
##
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Generator.pm,v 1.2 2001/06/22 03:43:51 vipul Exp $

package Crypt::Random::Generator; 
use Crypt::Random qw(makerandom makerandom_itv makerandom_octet);
use Carp;

my @PROVIDERS = qw(devrandom devurandom egd rand);
my %STRENGTH  = ( 0 => [ qw(devurandom egd rand) ], 1 => [ qw(devrandom egd rand) ] );

sub new { 

    my ($class, %params) = @_;
  
    my $self = { _STRENGTH => \%STRENGTH, _PROVIDERS => \@PROVIDERS  };

    $$self{Strength} = $params{Strength} || 0;
    $$self{Provider} = $params{Provider} || "";  
    $$self{ProviderParams} = $params{ProviderParams} || "";

    bless $self, $class;

    unless ($$self{Provider}) { 
        SELECT_PROVIDER: for ($self->strength_order($$self{Strength})) { 
            my $pname = $_; my $fqpname = "Crypt::Random::Provider::$pname";
            if (eval "use $fqpname; $fqpname->available()") { 
                if (grep { $pname eq $_ } $self->providers) { 
                    $$self{Provider} = $pname; 
                    last SELECT_PROVIDER; 
                }
            }
        } 
    }

    croak "No provider available.\n" unless $$self{Provider};
    return $self;

}


sub providers { 

    my ($self, @args) = @_;
    if (@args) { $$self{_PROVIDERS} = [@args] }
    return @{$$self{_PROVIDERS}};

}


sub strength_order { 

    my ($self, $strength, @args) = @_;
    if (@args) { $$self{_STRENGTH}{$strength} = [@args] }
    return @{$$self{_STRENGTH}{$strength}}

}


sub integer { 

    my ($self, %params) = @_;
    if ($params{Size}) { 
        return makerandom ( 
                Size => $params{Size}, 
                Provider => $$self{Provider}, 
                Verbosity => $params{Verbosity} || $$self{Verbosity},
                %{$$self{ProviderParams}},
        )
    } elsif ($params{Upper}) {
         return makerandom_itv ( 
                Lower => $params{Lower} || 0,
                Upper => $params{Upper},
                Provider => $$self{Provider}, 
                Verbosity => $params{Verbosity} || $$self{Verbosity},
                %{$$self{ProviderParams}},
        )
    }

} 


sub string { 

    my ($self, %params) = @_;
    return makerandom_octet ( 
        %params, 
        Provider => $$self{Provider}, 
        Verbosity => $params{Verbosity} || $$self{Verbosity},
        %{$$self{ProviderParams}},
    )    

}


