package Class::Load;
{
  $Class::Load::VERSION = '0.19';
}
use strict;
use warnings;
use base 'Exporter';
use Data::OptList 'mkopt';
use Module::Implementation 0.04;
use Module::Runtime 0.012 qw(
    check_module_name
    module_notional_filename
    require_module
    use_module
);
use Try::Tiny;

{
    my $loader = Module::Implementation::build_loader_sub(
        implementations => [ 'XS', 'PP' ],
        symbols         => ['is_class_loaded'],
    );

    $loader->();
}

our @EXPORT_OK = qw/load_class load_optional_class try_load_class is_class_loaded load_first_existing_class/;
our %EXPORT_TAGS = (
    all => \@EXPORT_OK,
);

our $ERROR;

sub load_class {
    my $class   = shift;
    my $options = shift;

    my ($res, $e) = try_load_class($class, $options);
    return 1 if $res;

    _croak($e);
}

sub load_first_existing_class {
    my $classes = Data::OptList::mkopt(\@_)
        or return;

    foreach my $class (@{$classes}) {
        check_module_name($class->[0]);
    }

    for my $class (@{$classes}) {
        my ($name, $options) = @{$class};

        # We need to be careful not to pass an undef $options to this sub,
        # since the XS version will blow up if that happens.
        return $name if is_class_loaded($name, ($options ? $options : ()));

        my ($res, $e) = try_load_class($name, $options);

        return $name if $res;

        my $file = module_notional_filename($name);

        next if $e =~ /^Can't locate \Q$file\E in \@INC/;
        next
            if $options
                && defined $options->{-version}
                && $e =~ _version_fail_re($name, $options->{-version});

        _croak("Couldn't load class ($name) because: $e");
    }

    my @list = map {
        $_->[0]
            . ( $_->[1] && defined $_->[1]{-version}
            ? " (version >= $_->[1]{-version})"
            : q{} )
    } @{$classes};

    my $err
        .= q{Can't locate }
        . _or_list(@list)
        . " in \@INC (\@INC contains: @INC).";
    _croak($err);
}

sub _version_fail_re {
    my $name = shift;
    my $vers = shift;

    return qr/\Q$name\E version \Q$vers\E required--this is only version/;
}

sub _nonexistent_fail_re {
    my $name = shift;

    my $file = module_notional_filename($name);
    return qr/Can't locate \Q$file\E in \@INC/;
}

sub _or_list {
    return $_[0] if @_ == 1;

    return join ' or ', @_ if @_ ==2;

    my $last = pop;

    my $list = join ', ', @_;
    $list .= ', or ' . $last;

    return $list;
}

sub load_optional_class {
    my $class   = shift;
    my $options = shift;

    check_module_name($class);

    my ($res, $e) = try_load_class($class, $options);
    return 1 if $res;

    return 0
        if $options
            && defined $options->{-version}
            && $e =~ _version_fail_re($class, $options->{-version});

    return 0
        if $e =~ _nonexistent_fail_re($class);

    _croak($ERROR);
}

sub try_load_class {
    my $class   = shift;
    my $options = shift;

    check_module_name($class);

    local $@;
    undef $ERROR;

    if (is_class_loaded($class)) {
        # We need to check this here rather than in is_class_loaded() because
        # we want to return the error message for a failed version check, but
        # is_class_loaded just returns true/false.
        return 1 unless $options && defined $options->{-version};
        return try {
            $class->VERSION($options->{-version});
            1;
        }
        catch {
            _error($_);
        };
    }

    my $file = module_notional_filename($class);
    # This says "our diagnostics of the package
    # say perl's INC status about the file being loaded are
    # wrong", so we delete it from %INC, so when we call require(),
    # perl will *actually* try reloading the file.
    #
    # If the file is already in %INC, it won't retry,
    # And on 5.8, it won't fail either!
    #
    # The extra benefit of this trick, is it helps even on
    # 5.10, as instead of dying with "Compilation failed",
    # it will die with the actual error, and thats a win-win.
    delete $INC{$file};
    return try {
        local $SIG{__DIE__} = 'DEFAULT';
        if ($options && defined $options->{-version}) {
            use_module($class, $options->{-version});
        }
        else {
            require_module($class);
        }
        1;
    }
    catch {
        _error($_);
    };
}

sub _error {
    $ERROR = shift;
    return 0 unless wantarray;
    return 0, $ERROR;
}

sub _croak {
    require Carp;
    local $Carp::CarpLevel = $Carp::CarpLevel + 1;
    Carp::croak(shift);
}

1;

# ABSTRACT: a working (require "Class::Name") and more



=pod

=head1 NAME

Class::Load - a working (require "Class::Name") and more

=head1 VERSION

version 0.19

=head1 SYNOPSIS

    use Class::Load ':all';

    try_load_class('Class::Name')
        or plan skip_all => "Class::Name required to run these tests";

    load_class('Class::Name');

    is_class_loaded('Class::Name');

    my $baseclass = load_optional_class('Class::Name::MightExist')
        ? 'Class::Name::MightExist'
        : 'Class::Name::Default';

=head1 DESCRIPTION

C<require EXPR> only accepts C<Class/Name.pm> style module names, not
C<Class::Name>. How frustrating! For that, we provide
C<load_class 'Class::Name'>.

It's often useful to test whether a module can be loaded, instead of throwing
an error when it's not available. For that, we provide
C<try_load_class 'Class::Name'>.

Finally, sometimes we need to know whether a particular class has been loaded.
Asking C<%INC> is an option, but that will miss inner packages and any class
for which the filename does not correspond to the package name. For that, we
provide C<is_class_loaded 'Class::Name'>.

=head1 FUNCTIONS

=head2 load_class Class::Name, \%options

C<load_class> will load C<Class::Name> or throw an error, much like C<require>.

If C<Class::Name> is already loaded (checked with C<is_class_loaded>) then it
will not try to load the class. This is useful when you have inner packages
which C<require> does not check.

The C<%options> hash currently accepts one key, C<-version>. If you specify a
version, then this subroutine will call C<< Class::Name->VERSION(
$options{-version} ) >> internally, which will throw an error if the class's
version is not equal to or greater than the version you requested.

=head2 try_load_class Class::Name, \%options -> (0|1, error message)

Returns 1 if the class was loaded, 0 if it was not. If the class was not
loaded, the error will be returned as a second return value in list context.

Again, if C<Class::Name> is already loaded (checked with C<is_class_loaded>)
then it will not try to load the class. This is useful when you have inner
packages which C<require> does not check.

Like C<load_class>, you can pass a C<-version> in C<%options>. If the version
is not sufficient, then this subroutine will return false.

=head2 is_class_loaded Class::Name, \%options -> 0|1

This uses a number of heuristics to determine if the class C<Class::Name> is
loaded. There heuristics were taken from L<Class::MOP>'s old pure-perl
implementation.

Like C<load_class>, you can pass a C<-version> in C<%options>. If the version
is not sufficient, then this subroutine will return false.

=head2 load_first_existing_class Class::Name, \%options, ...

This attempts to load the first loadable class in the list of classes
given. Each class name can be followed by an options hash reference.

If any one of the classes loads and passes the optional version check, that
class name will be returned. If I<none> of the classes can be loaded (or none
pass their version check), then an error will be thrown.

If, when attempting to load a class, it fails to load because of a syntax
error, then an error will be thrown immediately.

=head2 load_optional_class Class::Name, \%options -> 0|1

C<load_optional_class> is a lot like C<try_load_class>, but also a lot like
C<load_class>.

If the class exists, and it works, then it will return 1. If you specify a
version in C<%options>, then the version check must succeed or it will return
0.

If the class doesn't exist, and it appears to not exist on disk either, it
will return 0.

If the class exists on disk, but loading from disk results in an error
( i.e.: a syntax error ), then it will C<croak> with that error.

This is useful for using if you want a fallback module system, i.e.:

    my $class = load_optional_class($foo) ? $foo : $default;

That way, if $foo does exist, but can't be loaded due to error, you won't
get the behaviour of it simply not existing.

=head1 SEE ALSO

=over 4

=item L<http://blog.fox.geek.nz/2010/11/searching-design-spec-for-ultimate.html>

This blog post is a good overview of the current state of the existing modules
for loading other modules in various ways.

=item L<http://blog.fox.geek.nz/2010/11/handling-optional-requirements-with.html>

This blog post describes how to handle optional modules with L<Class::Load>.

=item L<http://d.hatena.ne.jp/tokuhirom/20110202/1296598578>

This Japanese blog post describes why L<DBIx::Skinny> now uses L<Class::Load>
over its competitors.

=item L<Moose>, L<Jifty>, L<Prophet>, etc

This module was designed to be used anywhere you have
C<if (eval "require $module"; 1)>, which occurs in many large projects.

=back

=head1 AUTHOR

Shawn M Moore <sartak at bestpractical.com>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Shawn M Moore.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut


__END__


