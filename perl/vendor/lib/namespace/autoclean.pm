use strict;
use warnings;

package namespace::autoclean;
BEGIN {
  $namespace::autoclean::AUTHORITY = 'cpan:FLORA';
}
{
  $namespace::autoclean::VERSION = '0.13';
}
# ABSTRACT: Keep imports out of your namespace

use Class::MOP 0.80;
use B::Hooks::EndOfScope;
use List::Util qw( first );
use namespace::clean 0.20;


sub import {
    my ($class, %args) = @_;

    my $subcast = sub {
        my $i = shift;
        return $i if ref $i eq 'CODE';
        return sub { $_ =~ $i } if ref $i eq 'Regexp';
        return sub { $_ eq $i };
    };

    my $runtest = sub {
        my ($code, $method_name) = @_;
        local $_ = $method_name;
        return $code->();
    };

    my $cleanee = exists $args{-cleanee} ? $args{-cleanee} : scalar caller;

    my @also = map { $subcast->($_) } (
        exists $args{-also}
        ? (ref $args{-also} eq 'ARRAY' ? @{ $args{-also} } : $args{-also})
        : ()
    );

    on_scope_end {
        my $meta = Class::MOP::Class->initialize($cleanee);
        my %methods = map { ($_ => 1) } $meta->get_method_list;
        $methods{meta} = 1 if $meta->isa('Moose::Meta::Role') && Moose->VERSION < 0.90;
        my %extra = ();

        for my $method (keys %methods) {
            next if exists $extra{$_};
            next unless first { $runtest->($_, $method) } @also;
            $extra{ $method } = 1;
        }

        my @symbols = keys %{ $meta->get_all_package_symbols('CODE') };
        namespace::clean->clean_subroutines($cleanee, keys %extra, grep { !$methods{$_} } @symbols);
    };
}

1;

__END__
=pod

=encoding utf-8

=head1 NAME

namespace::autoclean - Keep imports out of your namespace

=head1 SYNOPSIS

    package Foo;
    use namespace::autoclean;
    use Some::Package qw/imported_function/;

    sub bar { imported_function('stuff') }

    # later on:
    Foo->bar;               # works
    Foo->imported_function; # will fail. imported_function got cleaned after compilation

=head1 DESCRIPTION

When you import a function into a Perl package, it will naturally also be
available as a method.

The C<namespace::autoclean> pragma will remove all imported symbols at the end
of the current package's compile cycle. Functions called in the package itself
will still be bound by their name, but they won't show up as methods on your
class or instances.

This module is very similar to L<namespace::clean|namespace::clean>, except it
will clean all imported functions, no matter if you imported them before or
after you C<use>d the pragma. It will also not touch anything that looks like a
method, according to C<Class::MOP::Class::get_method_list>.

If you're writing an exporter and you want to clean up after yourself (and your
peers), you can use the C<-cleanee> switch to specify what package to clean:

  package My::MooseX::namespace::autoclean;
  use strict;

  use namespace::autoclean (); # no cleanup, just load

  sub import {
      namespace::autoclean->import(
        -cleanee => scalar(caller),
      );
  }

=head1 PARAMETERS

=head2 -also => [ ITEM | REGEX | SUB, .. ]

=head2 -also => ITEM

=head2 -also => REGEX

=head2 -also => SUB

Sometimes you don't want to clean imports only, but also helper functions
you're using in your methods. The C<-also> switch can be used to declare a list
of functions that should be removed additional to any imports:

    use namespace::autoclean -also => ['some_function', 'another_function'];

If only one function needs to be additionally cleaned the C<-also> switch also
accepts a plain string:

    use namespace::autoclean -also => 'some_function';

In some situations, you may wish for a more I<powerful> cleaning solution.

The C<-also> switch can take a Regex or a CodeRef to match against local
function names to clean.

    use namespace::autoclean -also => qr/^_/

    use namespace::autoclean -also => sub { $_ =~ m{^_} };

    use namespace::autoclean -also => [qr/^_/ , qr/^hidden_/ ];

    use namespace::autoclean -also => [sub { $_ =~ m/^_/ or $_ =~ m/^hidden/ }, sub { uc($_) == $_ } ];

=head1 SEE ALSO

L<namespace::clean>

L<Class::MOP>

L<B::Hooks::EndOfScope>

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

