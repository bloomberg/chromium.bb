package Dist::CheckConflicts;
BEGIN {
  $Dist::CheckConflicts::VERSION = '0.02';
}
use strict;
use warnings;
# ABSTRACT: declare version conflicts for your dist

use Carp;
use List::MoreUtils 'first_index';
use Sub::Exporter;


my $import = Sub::Exporter::build_exporter({
    exports => [ qw(conflicts check_conflicts calculate_conflicts dist) ],
    groups => {
        default => [ qw(conflicts check_conflicts calculate_conflicts dist) ],
    },
});

my %CONFLICTS;
my %DISTS;

sub import {
    my $for = caller;

    my ($conflicts, $alsos, $dist);
    ($conflicts, @_) = _strip_opt('-conflicts' => @_);
    ($alsos, @_)     = _strip_opt('-also' => @_);
    ($dist, @_)      = _strip_opt('-dist' => @_);

    my %conflicts = %{ $conflicts || {} };
    for my $also (@{ $alsos || [] }) {
        eval "require $also; 1;" or next;
        if (!exists $CONFLICTS{$also}) {
            $also .= '::Conflicts';
            eval "require $also; 1;" or next;
        }
        if (!exists $CONFLICTS{$also}) {
            next;
        }
        my %also_confs = $also->conflicts;
        for my $also_conf (keys %also_confs) {
            $conflicts{$also_conf} = $also_confs{$also_conf}
                if !exists $conflicts{$also_conf}
                || $conflicts{$also_conf} lt $also_confs{$also_conf};
        }
    }

    $CONFLICTS{$for} = \%conflicts;
    $DISTS{$for}     = $dist || $for;

    goto $import;
}

sub _strip_opt {
    my $opt = shift;
    my $idx = first_index { ( $_ || '' ) eq $opt } @_;

    return ( undef, @_ ) unless $idx >= 0 && $#_ >= $idx + 1;

    my $val = $_[ $idx + 1 ];

    splice @_, $idx, 2;

    return ( $val, @_ );
}


sub conflicts {
    my $package = shift;
    return %{ $CONFLICTS{ $package } };
}


sub dist {
    my $package = shift;
    return $DISTS{ $package };
}


sub check_conflicts {
    my $package = shift;
    my $dist = $package->dist;
    my @conflicts = $package->calculate_conflicts;
    return unless @conflicts;

    my $err = "Conflicts detected for $dist:\n";
    for my $conflict (@conflicts) {
        $err .= "  $conflict->{package} is version "
                . "$conflict->{installed}, but must be greater than version "
                . "$conflict->{required}\n";
    }
    die $err;
}


sub calculate_conflicts {
    my $package = shift;
    my %conflicts = $package->conflicts;

    my @ret;

    CONFLICT:
    for my $conflict (keys %conflicts) {
        {
            local $SIG{__WARN__} = sub { };
            eval "require $conflict; 1" or next CONFLICT;
        }
        my $installed = $conflict->VERSION;
        push @ret, {
            package   => $conflict,
            installed => $installed,
            required  => $conflicts{$conflict},
        } if $installed le $conflicts{$conflict};
    }

    return sort { $a->{package} cmp $b->{package} } @ret;
}


1;

__END__
=pod

=head1 NAME

Dist::CheckConflicts - declare version conflicts for your dist

=head1 VERSION

version 0.02

=head1 SYNOPSIS

    use Dist::CheckConflicts
        -dist => 'Class-MOP',
        -conflicts => {
            'Moose'                => '1.14',
            'namespace::autoclean' => '0.08',
        },
        -also => [
            'Package::Stash::Conflicts',
        ];

    __PACKAGE__->check_conflicts;

=head1 DESCRIPTION

One shortcoming of the CPAN clients that currently exist is that they have no
way of specifying conflicting downstream dependencies of modules. This module
attempts to work around this issue by allowing you to specify conflicting
versions of modules separately, and deal with them after the module is done
installing.

For instance, say you have a module C<Foo>, and some other module C<Bar> uses
C<Foo>. If C<Foo> were to change its API in a non-backwards-compatible way,
this would cause C<Bar> to break until it is updated to use the new API. C<Foo>
can't just depend on the fixed version of C<Bar>, because this will cause a
circular dependency (because C<Bar> is already depending on C<Foo>), and this
doesn't express intent properly anyway - C<Foo> doesn't use C<Bar> at all. The
ideal solution would be for there to be a way to specify conflicting versions
of modules in a way that would let CPAN clients update conflicting modules
automatically after an existing module is upgraded, but until that happens,
this module will allow users to do this manually.

This module accepts a hash of options passed to its C<use> statement, with
these keys being valid:

=over 4

=item -conflicts

A hashref of conflict specifications, where keys are module names, and values
are the last broken version - any version greater than the specified version
should work.

=item -also

Additional modules to get conflicts from (potentially recursively). This should
generally be a list of modules which use Dist::CheckConflicts, which correspond
to the dists that your dist depends on. (In an ideal world, this would be
intuited directly from your dependency list, but the dependency list isn't
available outside of build time).

=item -dist

The name of the distribution, to make the error message from check_conflicts
more user-friendly.

=back

The methods listed below are exported by this module into the module that uses
it, so you should call these methods on your module, not Dist::CheckConflicts.

As an example, this command line can be used to update your modules, after
installing the C<Foo> dist (assuming that C<Foo::Conflicts> is the module in
the C<Foo> dist which uses Dist::CheckConflicts):

    perl -MFoo::Conflicts -e'print "$_\n"
        for map { $_->{package} } Foo::Conflicts->calculate_conflicts' | cpanm

=head1 METHODS

=head2 conflicts

Returns the conflict specification (the C<-conflicts> parameter to
C<import()>), as a hash.

=head2 dist

Returns the dist name (either as specified by the C<-dist> parameter to
C<import()>, or the package name which C<use>d this module).

=head2 check_conflicts

Examine the modules that are currently installed, and throw an exception with
useful information if any modules are at versions which conflict with the dist.

=head2 calculate_conflicts

Examine the modules that are currently installed, and return a list of modules
which conflict with the dist. The modules will be returned as a list of
hashrefs, each containing C<package>, C<installed>, and C<required> keys.

=head1 TODO

Provide a way to insert a hook into C<@INC> which warns if a conflicting module
is loaded (would this be reasonable?)

=head1 BUGS

No known bugs.

Please report any bugs through RT: email
C<bug-dist-checkconflicts at rt.cpan.org>, or browse to
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Dist-CheckConflicts>.

=head1 SEE ALSO

=over 4

=item * L<Module::Install::CheckConflicts>

=back

=head1 SUPPORT

You can find this documentation for this module with the perldoc command.

    perldoc Dist::CheckConflicts

You can also look for information at:

=over 4

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Dist-CheckConflicts>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Dist-CheckConflicts>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Dist-CheckConflicts>

=item * Search CPAN

L<http://search.cpan.org/dist/Dist-CheckConflicts>

=back

=head1 AUTHOR

Jesse Luehrs <doy at tozt dot net>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Jesse Luehrs.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

