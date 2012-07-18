package PAR::Repository::Client::Util;

use 5.006;
use strict;
use warnings;

our $VERSION = '0.24';

use Carp qw/croak/;

=head1 NAME

PAR::Repository::Client::Util - Small helper methods common to all implementations

=head1 SYNOPSIS

  use PAR::Repository::Client;

=head1 DESCRIPTION

This module implements small helper methods which are common to all
L<PAR::Repository::Client> implementations.

=head1 PRIVATE METHODS

These private methods should not be relied upon from the outside of
the module.

=head2 _unzip_file

This is a private method. Callable as class or instance method.

Unzips the file given as first argument to the file
given as second argument.
If a third argument is used, the zip member of that name
is extracted. If the zip member name is omitted, it is
set to the target file name.

Returns the name of the unzipped file.

=cut

sub _unzip_file {
  my $class = shift;
  my $file = shift;
  my $target = shift;
  my $member = shift;
  $member = $target if not defined $member;
  return unless -f $file;

  my $zip = Archive::Zip->new;
  local %SIG;
  $SIG{__WARN__} = sub { print STDERR $_[0] unless $_[0] =~ /\bstat\b/ };

  return unless $zip->read($file) == Archive::Zip::AZ_OK()
    and $zip->extractMember($member, $target) == Archive::Zip::AZ_OK();

  return $target;
}


# given a distribution name, recursively determines all distributions
# it depends on
sub _resolve_static_dependencies {
  my $self = shift;
  my $distribution = shift;

  my ($deph) = $self->dependencies_dbm();
  return([]) if not exists $deph->{$distribution};
  
  my ($modh) = $self->modules_dbm();

  my @module_queue = (keys %{$deph->{$distribution}});
  my @dep_dists;
  my %module_seen;
  my %dist_seen;

  while (@module_queue) {
    #use Data::Dumper; warn Dumper \@module_queue;
    my $module = shift @module_queue;
    next if $module_seen{$module}++;
    next if not exists $modh->{$module}; # FIXME should this be somehow reported?
    my $dist = $self->prefered_distribution($module, $modh->{$module});
    next if not defined $dist;
    next if $dist_seen{$dist}++;
    push @dep_dists, $dist;
    push @module_queue, keys %{$deph->{$dist}} if exists $deph->{$dist};
  }

  return \@dep_dists;
}

sub generate_private_cache_dir {
  my $self = shift;
  my $uri = $self->{uri};
  my $digester = PAR::SetupTemp::_get_digester(); # requires PAR 0.987!
  $digester->add($uri);
  my $digest = $digester->b64digest();
  $digest =~ s/\W/_/g;
  my $user_temp_dir = PAR::SetupTemp::_get_par_user_tempdir();
  my $priv_cache_dir = File::Spec->catdir($user_temp_dir, "par-repo-$digest");
  return $priv_cache_dir;
}

1;
__END__

=head1 SEE ALSO

This module is directly related to the C<PAR> project. You need to have
basic familiarity with it. Its homepage is at L<http://par.perl.org/>

See L<PAR>, L<PAR::Dist>, L<PAR::Repository>, etc.

L<PAR::Repository::Query> implements the querying interface. The methods
described in that module's documentation can be called on
C<PAR::Repository::Client> objects.

L<PAR::Repository> implements the server side creation and manipulation
of PAR repositories.

L<PAR::WebStart> is doing something similar but is otherwise unrelated.

=head1 AUTHOR

Steffen Mueller, E<lt>smueller@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2009 by Steffen Mueller

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6 or,
at your option, any later version of Perl 5 you may have available.

=cut
