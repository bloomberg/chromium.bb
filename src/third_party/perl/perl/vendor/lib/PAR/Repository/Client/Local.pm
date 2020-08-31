package PAR::Repository::Client::Local;

use 5.006;
use strict;
use warnings;

use base 'PAR::Repository::Client';

use Carp qw/croak/;
require File::Copy;

our $VERSION = '0.24';

=head1 NAME

PAR::Repository::Client::Local - PAR repo. on the local file system

=head1 SYNOPSIS

  use PAR::Repository::Client;
  
  my $client = PAR::Repository::Client->new(
    uri => 'file:///foo/repository',
  );

=head1 DESCRIPTION

This module implements repository accesses on the local filesystem.

If you create a new L<PAR::Repository::Client> object and pass it
an uri parameter which starts with C<file://> or just a path,
it will create an object of this class. It inherits from
C<PAR::Repository::Client>.

=head2 EXPORT

None.

=head1 METHODS

Following is a list of class and instance methods.
(Instance methods until otherwise mentioned.)

=cut

=head2 fetch_par

Fetches a .par distribution from the repository and stores it
locally. Returns the name of the local file or the empty list on
failure.

First argument must be the distribution name to fetch.

=cut

sub fetch_par {
  my $self = shift;
  $self->{error} = undef;
  my $dist = shift;
  if (not defined $dist) {
    $self->{error} = "undef passed as argument to fetch_par()";
    return();
  }

  my $path = $self->{uri};
  $path =~ s/(?:\/|\\)$//;
  $path =~ s!^file://!!i;

  my ($dname, $vers, $arch, $perl) = PAR::Dist::parse_dist_name($dist);
  my $file = File::Spec->catfile(
    File::Spec->catdir($path, $arch, $perl),
    "$dname-$vers-$arch-$perl.par"
  );

  if (not -f $file) {
    $self->{error} = "Could not find distribution in local repository at '$file'";
    return();
  }

  return $file;
}

=head2 validate_repository

Makes sure the repository is valid. Returns the empty list
if that is not so and a true value if the repository is valid.

Checks that the repository version is compatible.

The error message is available as C<$client->error()> on
failure.

=cut

sub validate_repository {
  my $self = shift;
  $self->{error} = undef;

  my $mod_db = $self->modules_dbm;

  return() unless defined $mod_db;

  return() unless $self->validate_repository_version;

  return 1;
}

=head2 _repository_info

Returns a YAML::Tiny object representing the repository meta
information.

This is a private method.

=cut

sub _repository_info {
  my $self = shift;
  $self->{error} = undef;
  return $self->{info} if defined $self->{info};

  my $path = $self->{uri};
  $path =~ s/(?:\/|\\)$//;
  $path =~ s!^file://!!i;

  my $file = File::Spec->catfile($path, PAR::Repository::Client::REPOSITORY_INFO_FILE());

  if (not defined $file or not -f $file) {
    $self->{error} = "File '$file' does not exist in repository.";
    return();
  }

  my $yaml = YAML::Tiny->new->read($file);
  if (not defined $yaml) {
    $self->{error} = "Error reading repository info from YAML file.";
    return();
  }

  # workaround for possible YAML::Syck/YAML::Tiny bug
  # This is not the right way to do it!
  @$yaml = ($yaml->[1]) if @$yaml > 1;
  $self->{info} = $yaml;
  return $yaml;
}

=head2 _fetch_dbm_file

This is a private method.

Fetches a dbm (index) file from the repository and
returns the name of the local file or the
empty list on failure.

An error message is available via the C<error()>
method in case of failure.

=cut

sub _fetch_dbm_file {
  my $self = shift;
  $self->{error} = undef;
  my $file = shift;
  return if not defined $file;

  my $path = $self->{uri};
  $path =~ s/(?:\/|\\)$//;
  $path =~ s!^file://!!i;

  my $url = File::Spec->catfile( $path, $file );

  if (not -f $url) {
    $self->{error} = "Could not find dbm file in local repository at '$url'";
    return();
  }

  my ($tempfh, $tempfile) = File::Temp::tempfile(
    'temp_zip_dbm_XXXXX',
    UNLINK => 1, # because we cache the suckers by default
    DIR => $self->{cache_dir},
    EXLOCK => 0, # FIXME no exclusive locking or else we block on BSD. What's the right solution?
  );

  File::Copy::copy($url, $tempfile);

  return $tempfile;
}



=head2 _dbm_checksums

This is a private method.

If the repository has a checksums file (new feature of
C<PAR::Repository> 0.15), this method returns a hash  
associating the DBM file names (e.g. C<foo_bar.dbm.zip>)
with their MD5 hashes (base 64).

This method B<always> queries the repository and never caches
the information locally. That's the whole point of having the
checksums.

In case the repository does not have checksums, this method
returns the empty list, so check the return value!
The error message (see the C<error()> method) will be
I<"Repository does not support checksums"> in that case.

=cut

sub _dbm_checksums {
  my $self = shift;
  $self->{error} = undef;

  my $path = $self->{uri};
  $path =~ s/(?:\/|\\)$//;
  $path =~ s!^file://!!i;

  # if we're running on a "trust-the-checksums-for-this-long" basis...
  # ... return if the timeout hasn't elapsed
  if ($self->{checksums} and $self->{checksums_timeout}) {
    my $time = time();
    if ($time - $self->{last_checksums_refresh} < $self->{checksums_timeout}) {
      return($self->{checksums});
    }
  }

  my $file = File::Spec->catfile($path, PAR::Repository::Client::DBM_CHECKSUMS_FILE());

  if (not defined $file or not -f $file) {
    $self->{error} = "Repository does not support checksums";
    return();
  }

  return $self->_parse_dbm_checksums($file);
}


=head2 _init

This private method is called by the C<new()> method of
L<PAR::Repository::Client>. It is used to initialize
the client object and C<new()> passes it a hash ref to
its arguments.

Should return a true value on success.

=cut

sub _init {
  # We implement additional object attributes here
  # Currently no extra attributes...
  return 1;
}


1;
__END__

=head1 SEE ALSO

This module is part of the L<PAR::Repository::Client> distribution.

This module is directly related to the C<PAR> project. You need to have
basic familiarity with it. The PAR homepage is at L<http://par.perl.org/>.

See L<PAR>, L<PAR::Dist>, L<PAR::Repository>, etc.

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
