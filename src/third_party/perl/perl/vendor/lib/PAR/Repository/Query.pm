package PAR::Repository::Query;

use 5.006;
use strict;
use warnings;

use Carp qw/croak/;

our $VERSION = '0.14';

=head1 NAME

PAR::Repository::Query - Implements repository queries

=head1 SYNOPSIS

  use PAR::Repository;
  # or:
  use PAR::Repository::Client;

=head1 DESCRIPTION

This module is for internal use by L<PAR::Repository> or
L<PAR::Repository::Client> only. Both modules inherit from this.
C<PAR::Repository::Query> implements a unified query interface for
both the server- and client-side components of PAR repositories.

If you decide to inherit from this class (for whatever reason),
you should provide at least two methods: C<modules_dbm> which returns
a L<DBM::Deep> object representing the modules DBM file.
(See L<PAR::Repository::DBM> for details.) And C<scripts_dbm> which is
the equivalent for the scripts DBM file.

=head2 EXPORT

None. But the methods are callable on C<PAR::Repository> and 
C<PAR::Repository::Client> objects.

=head1 METHODS

Following is a list of class and instance methods.
(Instance methods until otherwise mentioned.)

There is no C<PAR::Repository::Query> object.

=cut

=head2 query_module

Polls the repository for modules matching certain criteria.
Takes named arguments. Either a C<regex> or a C<name> parameter
must be present but not both.

Returns a reference to an array containing alternating distribution
file names and module versions. This method returns the following
structure

  [ 'Foo-Bar-0.01-any_arch-5.8.7.par', '0.01', ... ]

that means the module was found in the distribution
F<Foo-Bar-0.01-any_arch-5.8.7.par> and the copy in that file has version
0.01.

Parameters:

=over 2

=item B<name>

The name of the module to look for. This is used for an exact match.
If you want to find C<Foo> in C<Foo::Bar>, use the C<regex> parameter.
Only one of C<name> and C<regex> may be specified.

=item B<regex>

Same as C<name>, but interpreted as a regular expression.
Only one of C<name> and C<regex> may be specified.

=item B<arch>

Can be used to reduce the number of matches to a specific architecture.
Always interpreted as a regular expression.

=back

=cut

sub query_module {
    my $self = shift;
#    $self->verbose(2, "Entering query_module()");
    croak("query_module() called with uneven number of arguments.")
      if @_ % 2;
    my %args = @_;
    
    my $name = $args{name};
    my $regex = $args{regex};

    if (defined $name and defined $regex) {
        croak("query_module() accepts only one of 'name' and 'regex' parameters.");
    }
    elsif (not defined $name and not defined $regex) {
        croak("query_module() needs one of 'name' and 'regex' parameters.");
    }
    elsif (defined $name) {
        $regex = qr/^\Q$name\E$/;
    }
    else { # regex defined
        $regex = qr/$regex/ if not ref($regex) eq 'Regexp';
    }

    my ($modh, $modfile) = $self->modules_dbm
      or die("Could not get modules DBM.");

    my @modules;
    
    my $arch_regex = $args{arch};
    $arch_regex = qr/$arch_regex/
      if defined $arch_regex and not ref($arch_regex) eq 'Regexp';
    
    # iterate over all modules in the mod_dbm hash
    while (my ($mod_name, $dists) = each(%$modh)) {
        # skip non-matching
        next if $mod_name !~ $regex;
        
        if (defined $arch_regex) {
            while (my ($distname, $version) = each(%$dists)) {
                (undef, undef, my $arch, undef)
                  = PAR::Dist::parse_dist_name($distname);
                next if $arch !~ $arch_regex;
                push @modules, [$distname, $version];
            }
        }
        else {
            while (my ($distname, $version) = each(%$dists)) {
                push @modules, [$distname, $version];
            }
        }
    }
    
    my %seen;
    # sort return list alphabetically
    return [
        map  { @$_ }
        sort { $a->[0] cmp $b->[0] }
        grep { not $seen{$_->[0] . '|' . $_->[1]}++ }
        @modules
    ];
}

=head2 query_script

Note: Usually, you probably want to use C<query_script_hash()>
instead. The usage of both methods is very similar (and described
right below), but the data structure returned differes somewhat.

Polls the repository for scripts matching certain criteria.
Takes named arguments. Either a C<regex> or a C<name> parameter
must be present but not both.

Returns a reference to an array containing alternating distribution
file names and script versions. This method returns the following
structure

  [ 'Foo-Bar-0.01-any_arch-5.8.7.par', '0.01', ... ]

that means the script was found in the distribution
F<Foo-Bar-0.01-any_arch-5.8.7.par> and the copy in that file has version
0.01.

Parameters:

=over 2

=item B<name>

The name of the script to look for. This is used for an exact match.
If you want to find C<foo> in C<foobar>, use the C<regex> parameter.
Only one of C<name> and C<regex> may be specified.

=item B<regex>

Same as C<name>, but interpreted as a regular expression.
Only one of C<name> and C<regex> may be specified.

=item B<arch>

Can be used to reduce the number of matches to a specific architecture.
Always interpreted as a regular expression.

=back

=cut

# FIXME: factor out common code from query_script and query_module!
sub query_script {
    my $self = shift;
#    $self->verbose(2, "Entering query_script()");

    my $scripts = $self->query_script_hash(@_);

    my %seen;
    # sort return list alphabetically
    return [
        map  { @$_ }
        sort { $a->[0] cmp $b->[0] }
        grep { not $seen{$_->[0] . '|' . $_->[1]}++ }
        map  {
          my $scripthash = $scripts->{$_};
          map { [$_, $scripthash->{$_}] } keys %$scripthash;
        }
        keys %$scripts
    ];
}


=head2 query_script_hash

Works exactly the same as C<query_script> except it returns
a different resulting structure which includes the matching
script's name:

  { 'fooscript' => { 'Foo-Bar-0.01-any_arch-5.8.7.par' => '0.01', ... }, ... }

that means the script C<fooscript> was found in the distribution
F<Foo-Bar-0.01-any_arch-5.8.7.par> and the copy in that file has version
0.01.

Parameters are the same as for C<query_script>

=cut

# FIXME: factor out common code from query_script_hash and query_module!
sub query_script_hash {
    my $self = shift;
#    $self->verbose(2, "Entering query_script_hash()");
    croak("query_script() or query_script_hash() called with uneven number of arguments.")
      if @_ % 2;
    my %args = @_;
    
    my $name = $args{name};
    my $regex = $args{regex};

    if (defined $name and defined $regex) {
        croak("query_script() or query_script_hash() accepts only one of 'name' and 'regex' parameters.");
    }
    elsif (not defined $name and not defined $regex) {
        croak("query_script() or query_script_hash() needs one of 'name' and 'regex' parameters.");
    }
    elsif (defined $name) {
        $regex = qr/^\Q$name\E$/;
    }
    else { # regex defined
        $regex = qr/$regex/ if not ref($regex) eq 'Regexp';
    }

    my ($scrh, $scrfile) = $self->scripts_dbm
      or die("Could not get scripts DBM.");

    my %scripts;
    
    my $arch_regex = $args{arch};
    $arch_regex = qr/$arch_regex/
      if defined $arch_regex and not ref($arch_regex) eq 'Regexp';
    
    # iterate over all scripts in the scripts hash
    while (my ($scr_name, $dists) = each(%$scrh)) {
        # skip non-matching
        next if $scr_name !~ $regex;
        
        while (my ($distname, $version) = each(%$dists)) {
            if (defined $arch_regex) {
                (undef, undef, my $arch, undef)
                  = PAR::Dist::parse_dist_name($distname);
                next if $arch !~ $arch_regex;
            }
            $scripts{$scr_name} = {} if not exists $scripts{$scr_name};
            $scripts{$scr_name}{$distname} = $version; # distname => version
        }
    }

    return \%scripts;    
}



=head2 query_dist

Polls the repository for distributions matching certain criteria.
Takes named arguments. Either a C<regex> or a C<name> parameter
must be present but not both.

Returns a reference to an array containing alternating distribution
file names and hash references. The hashes contain module names
and associated versions in the distribution.
This method returns the following structure

  [
    'Foo-Bar-0.01-any_arch-5.8.7.par',
    {Foo::Bar => '0.01', Foo::Bar::Baz => '0.02'},
    ...
  ]

that means the distribution F<Foo-Bar-0.01-any_arch-5.8.7.par> matched and
that distribution contains the modules C<Foo::Bar> and C<Foo::Bar::Baz>
with versions 0.01 and 0.02 respectively.

Parameters:

=over 2

=item B<name>

The name of the distribution to look for. This is used for an exact match.
If you want to find C<Foo> in C<Foo-Bar-0.01-any_arch-5.8.8.par>,
use the C<regex> parameter.
Only one of C<name> and C<regex> may be specified.

=item B<regex>

Same as C<name>, but interpreted as a regular expression.
Only one of C<name> and C<regex> may be specified.

=item B<arch>

Can be used to reduce the number of matches to a specific architecture.
Always interpreted as a regular expression.

=back

=cut

sub query_dist {
    my $self = shift;
#    $self->verbose(2, "Entering query_dist()");
    croak("query_dist() called with uneven number of arguments.")
      if @_ % 2;
    my %args = @_;
    
    my $name = $args{name};
    my $regex = $args{regex};

    if (defined $name and defined $regex) {
        croak("query_dist() accepts only one of 'name' and 'regex' parameters.");
    }
    elsif (not defined $name and not defined $regex) {
        croak("query_dist() needs one of 'name' and 'regex' parameters.");
    }
    elsif (defined $name) {
        $regex = qr/^\Q$name\E$/;
    }
    else { # regex defined
        $regex = qr/$regex/ if not ref($regex) eq 'Regexp';
    }

    my ($modh, $modfile) = $self->modules_dbm
      or die("Could not get modules DBM.");

    my %dists;
    
    my $arch_regex = $args{arch};
    $arch_regex = qr/$arch_regex/
      if defined $arch_regex and not ref($arch_regex) eq 'Regexp';
    
    # iterate over all modules in the mod_dbm hash
    while (my ($mod_name, $this_dists) = each(%$modh)) {
        # get the distributions for the module
        my $this_dists = $modh->{$mod_name};
        
        while (my ($dist_name, $dist) = each(%$this_dists)) {
            # skip non-matching
            next if $dist_name !~ $regex;
            
            # skip non-matching archs
            if (defined $arch_regex) {
                (undef, undef, my $arch, undef)
                  = PAR::Dist::parse_dist_name($dist_name);
                next if $arch !~ $arch_regex;
            }
            
            $dists{$dist_name}{$mod_name} = $dist;
        }
    }
    
    # sort return list alphabetically
    return [
        map  { @$_ }
        sort { $a->[0] cmp $b->[0] }
        map  { [$_, $dists{$_}] }
        keys %dists
    ];
}



1;
__END__

=head1 AUTHOR

Steffen Müller, E<lt>smueller@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2009 by Steffen Müller

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6 or,
at your option, any later version of Perl 5 you may have available.

=cut
