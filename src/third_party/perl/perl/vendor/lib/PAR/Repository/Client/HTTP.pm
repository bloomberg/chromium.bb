package PAR::Repository::Client::HTTP;

use 5.006;
use strict;
use warnings;

use vars qw/$ua/;
require LWP::Simple;
LWP::Simple->import('$ua');

use base 'PAR::Repository::Client';

use Carp qw/croak/;

our $VERSION = '0.24';

=head1 NAME

PAR::Repository::Client::HTTP - PAR repository via HTTP

=head1 SYNOPSIS

  use PAR::Repository::Client;
  
  my $client = PAR::Repository::Client->new(
    uri => 'http:///foo/repository',
    http_timeout => 20, # but default is 180s
  );

=head1 DESCRIPTION

This module implements repository accesses via HTTP.

If you create a new L<PAR::Repository::Client> object and pass it
an uri parameter which starts with C<http://> or C<https://>,
it will create an object of this class. It inherits from
C<PAR::Repository::Client>.

The repository is accessed using L<LWP::Simple>.

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
  return() if not defined $dist;

  my $url = $self->{uri};
  $url =~ s/\/$//;

  my ($n, $v, $a, $p) = PAR::Dist::parse_dist_name($dist);
  $url .= "/$a/$p/$n-$v-$a-$p.par";

  my $file = $self->_fetch_file($url);

  if (not defined $file) {
    $self->{error} = "Could not fetch distribution from URI '$url'";
    return();
  }

  return $file;
}


{
  my %escapes;
  sub _fetch_file {
    my $self = shift;
    $self->{error} = undef;
    my $file = shift;
    #warn "FETCHING FILE: $file";

    my $cache_dir = $self->{cache_dir}; # used to be PAR_TEMP, but now configurable
    %escapes = map { chr($_) => sprintf("%%%02X", $_) } 0..255 unless %escapes;

    $file =~ m!/([^/]+)$!;
    my $local_file = (defined($1) ? $1 : $file);
    $local_file =~ s/([^\w\._])/$escapes{$1}/g;
    $local_file = File::Spec->catfile( $self->{cache_dir}, $local_file );

    my $timeout = $self->{http_timeout};
    my $old_timeout = $ua->timeout();
    $ua->timeout($timeout) if defined $timeout;
    my $rc = LWP::Simple::mirror( $file, $local_file );
    $ua->timeout($old_timeout) if defined $timeout;
    if (!LWP::Simple::is_success($rc) and not $rc == HTTP::Status::RC_NOT_MODIFIED()) {
      $self->{error} = "Error $rc: " . LWP::Simple::status_message($rc) . " ($file)\n";
      return();
    }

    return $local_file if -f $local_file;
    return();
  }
}


sub _fetch_as_data {
  my $self = shift;
  $self->{error} = undef;
  my $file = shift;
  #warn "FETCHING DATA: $file";

  my $timeout = $self->{http_timeout};
  my $old_timeout = $ua->timeout();
  $ua->timeout($timeout) if defined $timeout;
  my $data = LWP::Simple::get( $file );
  $ua->timeout($old_timeout) if defined $timeout;

  return $data if defined $data;

  $self->{error} = "Could not get '$file' from repository";
  return();
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
  return() if not defined $mod_db;

  return() if not $self->validate_repository_version;

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

  my $url = $self->{uri};
  $url =~ s/\/$//;

  my $file = $self->_fetch_file(
    $url.'/'.PAR::Repository::Client::REPOSITORY_INFO_FILE()
  );

  return() if not defined $file;

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
returns the name of the temporary local file or the
empty list on failure.

An error message is available via the C<error()>
method in case of failure.

=cut

sub _fetch_dbm_file {
  my $self = shift;
  $self->{error} = undef;
  my $file = shift;
  return if not defined $file;

  my $url = $self->{uri};
  $url =~ s/\/$//;

  my $local = $self->_fetch_file("$url/$file");
  return() if not defined $local or not -f $local;

  return $local;
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

  my $url = $self->{uri};
  $url =~ s/\/$//;

  # if we're running on a "trust-the-checksums-for-this-long" basis...
  # ... return if the timeout hasn't elapsed
  if ($self->{checksums} and $self->{checksums_timeout}) {
    my $time = time();
    if ($time - $self->{last_checksums_refresh} < $self->{checksums_timeout}) {
      return($self->{checksums});
    }
  }

  my $data = $self->_fetch_as_data(
    $url.'/'.PAR::Repository::Client::DBM_CHECKSUMS_FILE()
  );

  if (not defined $data) {
    $self->{error} = "Repository does not support checksums";
    return();
  }

  return $self->_parse_dbm_checksums(\$data);
}


=head2 _init

This private method is called by the C<new()> method of
L<PAR::Repository::Client>. It is used to initialize
the client object and C<new()> passes it a hash ref to
its arguments.

Should return a true value on success.

=cut

sub _init {
  my $self = shift;
  my $args = shift || {};
  # We implement additional object attributes here
  $self->{http_timeout} = $args->{http_timeout};
  $self->{http_timeout} = 180 if not defined $self->{http_timeout};

  return 1;
}



1;
__END__

=head1 SEE ALSO

This module is part of the L<PAR::Repository::Client> distribution.

This module is directly related to the C<PAR> project. You need to have
basic familiarity with it. The PAR homepage is at L<http://par.perl.org>.

See L<PAR>, L<PAR::Dist>, L<PAR::Repository>, etc.

L<PAR::Repository> implements the server side creation and manipulation
of PAR repositories.

L<PAR::WebStart> is doing something similar but is otherwise unrelated.

The repository access is done via L<LWP::Simple>.

=head1 AUTHOR

Steffen Mueller, E<lt>smueller@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2009 by Steffen Mueller

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6 or,
at your option, any later version of Perl 5 you may have available.

=cut
