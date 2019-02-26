package Any::Moose;
{
  $Any::Moose::VERSION = '0.18';
}
# ABSTRACT: use Moose or Mouse modules

use 5.006_002;
use strict;
use warnings;

# decide which backend to use
our $PREFERRED;
do {
    local $@;
    if ($ENV{ANY_MOOSE}) {
        $PREFERRED = $ENV{'ANY_MOOSE'};
        warn "ANY_MOOSE is not set to Moose or Mouse"
            unless $PREFERRED eq 'Moose'
                || $PREFERRED eq 'Mouse';

        # if we die here, then perl gives "unknown error" which doesn't tell
        # you what the problem is at all. argh.
        if ($PREFERRED eq 'Moose' && !eval { require Moose }) {
            warn "\$ANY_MOOSE is set to Moose but we cannot load it";
        }
        elsif ($PREFERRED eq 'Mouse' && !eval { require Mouse }) {
            warn "\$ANY_MOOSE is set to Mouse but we cannot load it";
        }
    }
    elsif (_is_moose_loaded()) {
        $PREFERRED = 'Moose';
    }
    elsif (eval { require Mouse }) {
        $PREFERRED = 'Mouse';
    }
    elsif (eval { require Moose }) {
        $PREFERRED = 'Moose';
    }
    else {
        require Carp;
        warn "Unable to locate Mouse or Moose in INC";
    }
};

sub import {
    my $self = shift;
    my $pkg  = caller;

    # Any::Moose gives you strict and warnings
    strict->import;
    warnings->import;

    # first options are for Mo*se
    unshift @_, 'Moose' if !@_ || ref($_[0]);

    while (my $module = shift) {
        my $options = @_ && ref($_[0]) ? shift : [];

        $options = $self->_canonicalize_options(
            module  => $module,
            options => $options,
            package => $pkg,
        );

        $self->_install_module($options);
    }

    # give them any_moose too
    no strict 'refs';
    *{$pkg.'::any_moose'} = \&any_moose;
}

sub unimport {
    my $sel = shift;
    my $pkg = caller;
    my $module;

    if(@_){
        $module = any_moose(shift, $pkg);
    }
    else {
        $module = _backer_of($pkg);
    }
    my $e = do{
        local $@;
        eval "package $pkg;\n"
           . '$module->unimport();';
        $@;
   };

   if ($e) {
        require Carp;
        Carp::croak("Cannot unimport Any::Moose: $e");
   }

   return;
}

sub _backer_of {
    my $pkg = shift;

    if(exists $INC{'Mouse.pm'}){
        my $meta = Mouse::Util::get_metaclass_by_name($pkg);
        if ($meta) {
            return 'Mouse::Role' if $meta->isa('Mouse::Meta::Role');
            return 'Mouse'       if $meta->isa('Mouse::Meta::Class');
       }
    }

    if (_is_moose_loaded()) {
        my $meta = Class::MOP::get_metaclass_by_name($pkg);
        if ($meta) {
            return 'Moose::Role' if $meta->isa('Moose::Meta::Role');
            return 'Moose'       if $meta->isa('Moose::Meta::Class');
        }
    }

    return undef;
}

sub _canonicalize_options {
    my $self = shift;
    my %args = @_;

    my %options;
    if (ref($args{options}) eq 'HASH') {
        %options = %{ $args{options} };
    }
    else {
        %options = (
            imports => $args{options},
        );
    }

    $options{package} = $args{package};
    $options{module}  = any_moose($args{module}, $args{package});

    return \%options;
}

sub _install_module {
    my $self    = shift;
    my $options = shift;

    my $module = $options->{module};
    (my $file = $module . '.pm') =~ s{::}{/}g;

    require $file;

    my $e = do {
        local $@;
        eval "package $options->{package};\n"
           . '$module->import(@{ $options->{imports} });';
        $@;
    };
    if ($e) {
        require Carp;
        Carp::croak("Cannot import Any::Moose: $e");
    }
    return;
}

sub any_moose {
    my $fragment = _canonicalize_fragment(shift);
    my $package  = shift || caller;

    # Mouse gets first dibs because it doesn't introspect existing classes

    my $backer = _backer_of($package) || '';

    if ($backer =~ /^Mouse/) {
        $fragment =~ s/^Moose/Mouse/;
        return $fragment;
    }

    return $fragment if $backer =~ /^Moose/;

    $fragment =~ s/^Moose/Mouse/ if mouse_is_preferred();
    return $fragment;
}

for my $name (qw/
    load_class
    is_class_loaded
    class_of
    get_metaclass_by_name
    get_all_metaclass_instances
    get_all_metaclass_names
    load_first_existing_class
        /) {
    no strict 'refs';
    *{__PACKAGE__."::$name"} = moose_is_preferred()
        ? *{"Class::MOP::$name"}
        : *{"Mouse::Util::$name"};
}

sub moose_is_preferred { $PREFERRED eq 'Moose' }
sub mouse_is_preferred { $PREFERRED eq 'Mouse' }

sub _is_moose_loaded { exists $INC{'Moose.pm'} }

sub is_moose_loaded {
    require Carp;
    Carp::carp("Any::Moose::is_moose_loaded is deprecated. Please use Any::Moose::moose_is_preferred instead");
    goto \&_is_moose_loaded;
}

sub _canonicalize_fragment {
    my $fragment = shift;

    return 'Moose' if !$fragment;

    # any_moose("X::Types") -> any_moose("MooseX::Types")
    $fragment =~ s/^X::/MooseX::/;

    # any_moose("::Util") -> any_moose("Moose::Util")
    $fragment =~ s/^::/Moose::/;

    # any_moose("Mouse::Util") -> any_moose("Moose::Util")
    $fragment =~ s/^Mouse(X?)\b/Moose$1/;

    # any_moose("Util") -> any_moose("Moose::Util")
    $fragment =~ s/^(?!Moose)/Moose::/;

    return $fragment;
}

1;


=pod

=head1 NAME

Any::Moose - use Moose or Mouse modules

=head1 VERSION

version 0.18

=head1 SYNOPSIS

=head2 BASIC

    package Class;

    # uses Moose if it's loaded or demanded, Mouse otherwise
    use Any::Moose;

    # cleans the namespace up
    no Any::Moose;

=head2 OTHER MODULES

    package Other::Class;
    use Any::Moose;

    # uses Moose::Util::TypeConstraints if the class has loaded Moose,
    # Mouse::Util::TypeConstraints otherwise.
    use Any::Moose '::Util::TypeConstraints';

=head2 ROLES

    package My::Sorter;
    use Any::Moose 'Role';

    requires 'cmp';

=head2 COMPLEX USAGE

    package My::Meta::Class;
    use Any::Moose;

    # uses subtype from Moose::Util::TypeConstraints if the class loaded Moose,
    # subtype from Mouse::Util::TypeConstraints otherwise.
    # similarly for Mo*se::Util's does_role
    use Any::Moose (
        '::Util::TypeConstraints' => ['subtype'],
        '::Util' => ['does_role'],
    );

    # uses MouseX::Types or MooseX::Types
    use Any::Moose 'X::Types';

    # gives you the right class name depending on which Mo*se was loaded
    extends any_moose('::Meta::Class');

=head1 DESCRIPTION

Though we recommend that people generally use L<Moose>, we accept that Moose
cannot yet be used for everything everywhere. People generally like the Moose
sugar, so many people use L<Mouse>, a lightweight replacement for parts of
Moose.

Because Mouse strives for compatibility with Moose, it's easy to substitute one
for the other. This module facilitates that substitution. By default, Mouse
will be provided to libraries, unless Moose is already loaded -or-
explicitly requested by the end-user. The end-user can force the decision
of which backend to use by setting the environment variable C<ANY_MOOSE> to
be C<Moose> or C<Mouse>.

Note that the decision of which backend to use is made only once, so that if
Any-Moose picks Mouse, then a third-party library loads Moose, anything else
that uses Any-Moose will continue to pick Mouse.

So, if you have to use L<Mouse>, please be considerate to the Moose fanboys
(like myself!) and use L<Any-Moose> instead. C<:)>

=head1 SEE ALSO

L<Moose>

L<Mouse>

L<Squirrel> - a deprecated first-stab at Any-Moose-like logic. Its biggest
fault was in making the decision of which backend to use every time it was
used, rather than just once.

=head1 AUTHORS

=over 4

=item *

Shawn M Moore <sartak@gmail.com>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Stevan Little <stevan@iinteractive.com>

=item *

Tokuhiro Matsuno <tokuhirom@gmail.com>

=item *

Goro Fuji <gfuji@cpan.org>

=item *

Ricardo Signes <rjbs@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Best Practical Solutions.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__

