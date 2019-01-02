#!/usr/bin/perl -sw
##
## Class::Loader
##
## Copyright (c) 2001, Vipul Ved Prakash.  All rights reserved.
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Loader.pm,v 2.2 2001/07/18 20:21:39 vipul Exp $

package Class::Loader;
use Data::Dumper;
use vars qw($VERSION);

($VERSION)  = '$Revision: 2.03 $' =~ /\s(\d+\.\d+)\s/; 
my %MAPS = ();

sub new { 
    return bless {}, shift;
}


sub _load { 

    my ($self, $field, @source) = @_;
    if ((scalar @source) % 2) { 
        unshift @source, $field;
        $field = ""
    }

    local ($name, $module, $constructor, $args);
    my %source = @source;
    my $class = ref $self || $self;
    my $object;

    for (keys %source) { ${lc($_)} = $source{$_} }

    if ($name) { 
        my $classmap = $self->_retrmap ($class) || return;
        my $map = $$classmap{$name} || return;
        for (keys %$map) { ${lc($_)} = $$map{$_} };
    } 

    if ($module) { 
        unless (eval "require $module") { 
            if ($source{CPAN}) { 
                require CPAN; CPAN->import;
                my $obj = CPAN::Shell->expand ('Module', $module);
                return unless $obj;
                $obj->install; 
                eval "require $module" || return;
            } else { return }
        }
        $constructor ||= 'new';
        if ($args) { 
            my $topass = __prepare_args ($args);
            $object = eval "$module->$constructor($topass)" or return;
            undef $topass; undef $args;
        } else { $object = eval "$module->$constructor" or return }
    } else { return }

    return $field ? $$self{$field} = $object : $object

}


sub _storemap { 
    my ($self, %map) = @_;
    my $class = ref $self;
    for (keys %map) { $MAPS{$class}{$_} = $map{$_} }
}


sub _retrmap { 
    my ($self) = @_; 
    my $class = ref $self;
    return $MAPS{$class} if $MAPS{$class};
    return;
}


sub __prepare_args { 

    my $topass = Dumper shift;
    $topass =~ s/\$VAR1 = \[//; 
    $topass =~ s/];\s*//g; 
    $topass =~ m/(.*)/s;
    $topass = $1;
    return $topass;

}

1;

=head1 NAME

Class::Loader - Load modules and create objects on demand.

=head1 VERSION

    $Revision: 2.2 $
    $Date: 2001/07/18 20:21:39 $

=head1 SYNOPSIS

    package Web::Server;
    use Class::Loader; 
    @ISA = qw(Class::Loader);
    
    $self->_load( 'Content_Handler', { 
                             Module => "Filter::URL",
                        Constructor => "new",
                               Args => [ ],
                         } 
                );
    

=head1 DESCRIPTION

Certain applications like to defer the decision to use a particular module
till runtime. This is possible in perl, and is a useful trick in
situations where the type of data is not known at compile time and the
application doesn't wish to pre-compile modules to handle all types of
data it can work with. Loading modules at runtime can also provide
flexible interfaces for perl modules. Modules can let the programmer
decide what modules will be used by it instead of hard-coding their names.

Class::Loader is an inheritable class that provides a method, _load(),
to load a module from disk and construct an object by calling its
constructor. It also provides a way to map modules names and
associated metadata with symbolic names that can be used in place of
module names at _load().

=head1 METHODS

=over 4

=item B<new()>

A basic constructor. You can use this to create an object of
Class::Loader, in case you don't want to inherit Class::Loader.

=item B<_load()>

_load() loads a module and calls its constructor. It returns the newly
constructed object on success or a non-true value on failure. The first
argument can be the name of the key in which the returned object is
stored. This argument is optional. The second (or the first) argument is a
hash which can take the following keys:

=over 4

=item B<Module>

This is name of the class to load. (It is not the module's filename.)

=item B<Name> 

Symbolic name of the module defined with _storemap(). Either one of Module
or Name keys must be present in a call to _load().

=item B<Constructor>

Name of the Module constructor. Defaults to "new".

=item B<Args>

A reference to the list of arguments for the constructor. _load() calls
the constructor with this list. If no Args are present, _load() will call
the constructor without any arguments.

=item B<CPAN>

If the Module is not installed on the local system, _load() can fetch &
install it from CPAN provided the CPAN key is present. This functionality
assumes availability of a pre-configured CPAN shell.

=back

=item B<_storemap()>

Class::Loader maintains a class table that maps symbolic names to
parameters accepted by _load(). It takes a hash as argument whose keys are
symbolic names and value are hash references that contain a set of _load()
arguments. Here's an example:

    $self->_storemap ( "URL" => { Module => "Filter::URL", 
                                  Constructor => "foo", 
                                  Args => [qw(bar baz)], 
                                }
                     );

    # time passes...

    $self->{handler} = $self->_load ( Name => 'URL' );

=item B<_retrmap()>

_retrmap() returns the entire map stored with Class::Loader. Class::Loader
maintains separate maps for different classes, and _retrmap() returns the
map valid in the caller class.

=back

=head1 SEE ALSO

AnyLoader(3)

=head1 AUTHOR

Vipul Ved Prakash, E<lt>mail@vipul.netE<gt>

=head1 LICENSE

Copyright (c) 2001, Vipul Ved Prakash. All rights reserved. This code is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=cut


