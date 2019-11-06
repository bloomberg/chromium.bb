use strict;
use warnings;
package Test::CleanNamespaces; # git description: v0.23-5-gf8e88b1
# ABSTRACT: Check for uncleaned imports
# KEYWORDS: testing namespaces clean dirty imports exports subroutines methods

our $VERSION = '0.24';

use Module::Runtime ();
use Sub::Identify ();
use Package::Stash 0.14;
use Test::Builder;
use File::Find ();
use File::Spec;

use Exporter 5.57 'import';
our @EXPORT = qw(namespaces_clean all_namespaces_clean);

#pod =head1 SYNOPSIS
#pod
#pod     use strict;
#pod     use warnings;
#pod     use Test::CleanNamespaces;
#pod
#pod     all_namespaces_clean;
#pod
#pod =head1 DESCRIPTION
#pod
#pod This module lets you check your module's namespaces for imported functions you
#pod might have forgotten to remove with L<namespace::autoclean> or
#pod L<namespace::clean> and are therefore available to be called as methods, which
#pod usually isn't want you want.
#pod
#pod =head1 FUNCTIONS
#pod
#pod All functions are exported by default.
#pod
#pod =head2 namespaces_clean
#pod
#pod     namespaces_clean('YourModule', 'AnotherModule');
#pod
#pod Tests every specified namespace for uncleaned imports. If the module couldn't
#pod be loaded it will be skipped.
#pod
#pod =head2 all_namespaces_clean
#pod
#pod     all_namespaces_clean;
#pod
#pod Runs L</namespaces_clean> for all modules in your distribution.
#pod
#pod =cut

sub namespaces_clean {
    my (@namespaces) = @_;
    local $@;
    my $builder = builder();

    my $result = 1;
    for my $ns (@namespaces) {
        unless (eval { Module::Runtime::require_module($ns); 1 }) {
            $builder->skip("failed to load ${ns}: $@");
            next;
        }

        my $imports = _remaining_imports($ns);

        my $ok = $builder->ok(!keys(%$imports), "${ns} contains no imported functions");
        $ok or $builder->diag($builder->explain('remaining imports: ' => $imports));

        $result &&= $ok;
    }

    return $result;
}

sub all_namespaces_clean {
    my @modules = find_modules(@_);
    builder()->plan(tests => scalar @modules);
    namespaces_clean(@modules);
}

# given a package name, returns a hashref of all remaining imports
sub _remaining_imports {
    my $ns = shift;

    my $symbols = Package::Stash->new($ns)->get_all_symbols('CODE');
    my @imports;

    my $meta;
    if ($INC{ Module::Runtime::module_notional_filename('Class::MOP') }
        and $meta = Class::MOP::class_of($ns)
        and $meta->can('get_method_list'))
    {
        my %subs = %$symbols;
        delete @subs{ $meta->get_method_list };
        @imports = keys %subs;
    }
    elsif ($INC{ Module::Runtime::module_notional_filename('Mouse::Util') }
        and Mouse::Util->can('class_of') and $meta = Mouse::Util::class_of($ns))
    {
        warn 'Mouse class detected - chance of false negatives is high!';

        my %subs = %$symbols;
        # ugh, this returns far more than the true list of methods
        delete @subs{ $meta->get_method_list };
        @imports = keys %subs;
    }
    else
    {
        @imports = grep {
            my $stash = Sub::Identify::stash_name($symbols->{$_});
            $stash ne $ns
                and $stash ne 'Role::Tiny'
                and not eval { require Role::Tiny; Role::Tiny->is_role($stash) }
        } keys %$symbols;
    }

    my %imports; @imports{@imports} = map { Sub::Identify::sub_fullname($symbols->{$_}) } @imports;

    # these subs are special-cased - they are often provided by other
    # modules, but cannot be wrapped with Sub::Name as the call stack
    # is important
    delete @imports{qw(import unimport)};

    my @overloads = grep { $imports{$_} eq 'overload::nil' || $imports{$_} eq 'overload::_nil' } keys %imports;
    delete @imports{@overloads} if @overloads;

    if ("$]" < 5.020)
    {
        # < haarg> 5.10+ allows sticking a readonly scalar ref directly in the symbol table, rather than a glob.  when auto-promoted to a sub, it will have the correct name.
        # < haarg> but that only works if the symbol table entry is empty
        # < haarg> if it exists, it has to use the *$const = sub () { $val } method, so the name is __ANON__
        # < haarg> newer versions don't use that method
        # < haarg> rather, newer versions of constant.pm don't use that method
        # < haarg> and then the name ends up being YourPackage::__ANON__
        my @constants = grep { $imports{$_} eq 'constant::__ANON__' } keys %imports;
        delete @imports{@constants} if @constants;
    }

    return \%imports;
}

#pod =head2 find_modules
#pod
#pod     my @modules = Test::CleanNamespaces->find_modules;
#pod
#pod Returns a list of modules in the current distribution. It'll search in
#pod C<blib/>, if it exists. C<lib/> will be searched otherwise.
#pod
#pod =cut

sub find_modules {
    my @modules;
    for my $top (-e 'blib' ? ('blib/lib', 'blib/arch') : 'lib') {
        File::Find::find({
            no_chdir => 1,
            wanted => sub {
                my $file = $_;
                return
                    unless $file =~ s/\.pm$//;
                push @modules, join '::' => File::Spec->splitdir(
                    File::Spec->abs2rel(File::Spec->rel2abs($file, '.'), $top)
                );
            },
        }, $top);
    }
    return @modules;
}

#pod =head2 builder
#pod
#pod     my $builder = Test::CleanNamespaces->builder;
#pod
#pod Returns the C<Test::Builder> used by the test functions.
#pod
#pod =cut

{
    my $Test = Test::Builder->new;
    sub builder { $Test }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::CleanNamespaces - Check for uncleaned imports

=head1 VERSION

version 0.24

=head1 SYNOPSIS

    use strict;
    use warnings;
    use Test::CleanNamespaces;

    all_namespaces_clean;

=head1 DESCRIPTION

This module lets you check your module's namespaces for imported functions you
might have forgotten to remove with L<namespace::autoclean> or
L<namespace::clean> and are therefore available to be called as methods, which
usually isn't want you want.

=head1 FUNCTIONS

All functions are exported by default.

=head2 namespaces_clean

    namespaces_clean('YourModule', 'AnotherModule');

Tests every specified namespace for uncleaned imports. If the module couldn't
be loaded it will be skipped.

=head2 all_namespaces_clean

    all_namespaces_clean;

Runs L</namespaces_clean> for all modules in your distribution.

=head2 find_modules

    my @modules = Test::CleanNamespaces->find_modules;

Returns a list of modules in the current distribution. It'll search in
C<blib/>, if it exists. C<lib/> will be searched otherwise.

=head2 builder

    my $builder = Test::CleanNamespaces->builder;

Returns the C<Test::Builder> used by the test functions.

=head1 KNOWN ISSUES

Uncleaned imports from L<Mouse> classes are incompletely detected, due to its
lack of ability to return the correct method list -- it assumes that all subs
are meant to be callable as methods unless they originated from (were imported
by) one of: L<Mouse>, L<Mouse::Role>, L<Mouse::Util>,
L<Mouse::Util::TypeConstraints>, L<Carp>, L<Scalar::Util>, or L<List::Util>.

=head1 SEE ALSO

=over 4

=item *

L<namespace::clean>

=item *

L<namespace::autoclean>

=item *

L<namespace::sweep>

=item *

L<Sub::Exporter::ForMethods>

=item *

L<Test::API>

=item *

L<Sub::Name>

=item *

L<Sub::Install>

=item *

L<MooseX::MarkAsMethods>

=item *

L<Dist::Zilla::Plugin::Test::CleanNamespaces>

=back

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Test-CleanNamespaces>
(or L<bug-Test-CleanNamespaces@rt.cpan.org|mailto:bug-Test-CleanNamespaces@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/perl-qa.html>.

There is also an irc channel available for users of this distribution, at
L<C<#perl> on C<irc.perl.org>|irc://irc.perl.org/#perl-qa>.

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Graham Knop

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Graham Knop <haarg@haarg.org>

=back

=head1 COPYRIGHT AND LICENCE

This software is copyright (c) 2009 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
