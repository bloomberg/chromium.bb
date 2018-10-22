package PAR::Repository::Client::DBM;

use 5.006;
use strict;
use warnings;

our $VERSION = '0.24';

use Carp qw/croak/;

=head1 NAME

PAR::Repository::Client::DBM - Contains all the DBM access functions

=head1 SYNOPSIS

  use PAR::Repository::Client;

=head1 DESCRIPTION

This module implements access to the underlying DBMs.

All of the methods described here shouldn't be used frivolously in user
code even if some of them are part of the API and are guaranteed not
to change.

=cut


=head2 need_dbm_update

Takes one or no arguments. Without arguments, all DBM files are
checked. With an argument, only the specified DBM file will be checked.

Returns true if either one of the following conditions match:

=over 2

=item

The repository does not support checksums.

=item

The checksums (and thus also the DBM files) haven't been
downloaded yet.

=item

The local copies of the checksums do not match those of the repository.

=back

In cases two and three above, the return value is actually the hash
reference of checksums that was fetched from the repository.

Returns the empty list if the local checksums match those of the
repository exactly.

You don't usually need to call this directly. By default, DBM files
are only fetched from the repository if necessary.

=cut

sub need_dbm_update {
  my $self = shift;
  $self->{error} = undef;

  my $check_file = shift;
  $check_file .= '.zip' if defined $check_file and not $check_file =~ /\.zip$/;

  my $support = $self->{supports_checksums};
  if (defined $support and not $support) {
    return 1;
  }

  my $checksums = $self->_dbm_checksums();
  $self->{last_checksums_refresh} = time() if $self->{checksums_timeout};

  if (not defined $checksums) {
    $self->{supports_checksums} = 0;
    return 1;
  }
  else {
    $self->{supports_checksums} = 1;
  }

  if (not defined $self->{checksums} or keys %{$self->{checksums}} == 0) {
    # never fetched checksums before.
    return $checksums;
  }
  else {
    # we fetched checksums earlier, match them
    my $local_checksums = $self->{checksums};
    if (not defined $check_file) {
      return $checksums if keys(%$local_checksums) != keys(%$checksums);
      foreach my $file (keys %$checksums) {
        return $checksums
          if not exists $local_checksums->{$file}
          or not $local_checksums->{$file} eq $checksums->{$file};
      }
    }
    else {
      return $checksums
        if not exists $local_checksums->{$check_file}
        or not exists $checksums->{$check_file} # shouldn't happen
        or not $local_checksums->{$check_file} eq $checksums->{$check_file};
    }
    return();
  }
}


=head2 modules_dbm

Fetches the C<modules_dists.dbm> database from the repository,
ties it to a L<DBM::Deep> object and returns a tied hash
reference or the empty list on failure. Second return
value is the name of the local temporary file.

In case of failure, an error message is available via
the C<error()> method.

The method uses the C<_fetch_dbm_file()> method which must be
implemented in a subclass such as L<PAR::Repository::Client::HTTP>.

=cut

sub modules_dbm {
  my $self = shift;
  return( $self->_get_a_dbm('modules', PAR::Repository::Client::MODULES_DBM_FILE()) );
}


=head2 scripts_dbm

Fetches the C<scripts_dists.dbm> database from the repository,
ties it to a L<DBM::Deep> object and returns a tied hash
reference or the empty list on failure. Second return
value is the name of the local temporary file.

In case of failure, an error message is available via
the C<error()> method.

The method uses the C<_fetch_dbm_file()> method which must be
implemented in a subclass such as L<PAR::Repository::Client::HTTP>.

=cut

sub scripts_dbm {
  my $self = shift;
  return( $self->_get_a_dbm('scripts', PAR::Repository::Client::SCRIPTS_DBM_FILE()) );
}


=head2 dependencies_dbm 

Fetches the C<dependencies.dbm> database from the repository,
ties it to a L<DBM::Deep> object and returns a tied hash
reference or the empty list on failure. Second return
value is the name of the local temporary file.

In case of failure, an error message is available via
the C<error()> method.

The method uses the C<_fetch_dbm_file()> method which must be
implemented in a subclass such as L<PAR::Repository::Client::HTTP>.

=cut

sub dependencies_dbm {
  my $self = shift;
  return( $self->_get_a_dbm('dependencies', PAR::Repository::Client::DEPENDENCIES_DBM_FILE()) );
}


=head2 close_modules_dbm

Closes the C<modules_dists.dbm> file and does all necessary
cleaning up.

This is called when the object is destroyed.

=cut

sub close_modules_dbm {
  my $self = shift;
  my $hash = $self->{modules_dbm_hash};
  return if not defined $hash;

  my $obj = tied($hash);
  $self->{modules_dbm_hash} = undef;
  undef $hash;
  undef $obj;

  unlink $self->{modules_dbm_temp_file};
  $self->{modules_dbm_temp_file} = undef;
  if ($self->{checksums}) {
    delete $self->{checksums}{PAR::Repository::Client::MODULES_DBM_FILE().".zip"};
  }

  return 1;
}


=head2 close_scripts_dbm

Closes the C<scripts_dists.dbm> file and does all necessary
cleaning up.

This is called when the object is destroyed.

=cut

sub close_scripts_dbm {
  my $self = shift;
  my $hash = $self->{scripts_dbm_hash};
  return if not defined $hash;

  my $obj = tied($hash);
  $self->{scripts_dbm_hash} = undef;
  undef $hash;
  undef $obj;

  unlink $self->{scripts_dbm_temp_file};
  $self->{scripts_dbm_temp_file} = undef;
  if ($self->{checksums}) {
    delete $self->{checksums}{PAR::Repository::Client::SCRIPTS_DBM_FILE().".zip"};
  }

  return 1;
}


=head2 close_dependencies_dbm

Closes the C<dependencies.dbm> file and does all necessary
cleaning up.

This is called when the object is destroyed.

=cut

sub close_dependencies_dbm {
  my $self = shift;
  my $hash = $self->{dependencies_dbm_hash};
  return if not defined $hash;

  my $obj = tied($hash);
  $self->{dependencies_dbm_hash} = undef;
  undef $hash;
  undef $obj;

  unlink $self->{dependencies_dbm_temp_file};
  $self->{dependencies_dbm_temp_file} = undef;
  if ($self->{checksums}) {
    delete $self->{checksums}{PAR::Repository::Client::DEPENDENCIES_DBM_FILE().".zip"};
  }

  return 1;
}


=head1 PRIVATE METHODS

These private methods should not be relied upon from the outside of
the module.

=head2 _get_a_dbm

This is a private method.

Generic method returning a dbm.
Requires two arguments. The type of the DBM (C<modules>,
C<scripts>, C<dependencies>), and the name of the remote
DBM file. The latter should be taken from one of the package
constants.

=cut

sub _get_a_dbm {
  my $self = shift;
  $self->{error} = undef;

  my $dbm_type       = shift;
  my $dbm_remotefile = shift;

  my $dbm_hashkey        = $dbm_type . "_dbm_hash";
  my $tempfile_hashkey   = $dbm_type . "_dbm_temp_file";
  my $dbm_remotefile_zip = $dbm_remotefile . ".zip";

  my $checksums = $self->need_dbm_update($dbm_remotefile);

  if ($self->{$dbm_hashkey}) {
    # need new dbm file?
    return($self->{$dbm_hashkey}, $self->{$tempfile_hashkey})
      if not $checksums;

    # does this particular dbm need to be updated?
    if ($self->{checksums}) {
      my $local_checksum = $self->{checksums}{$dbm_remotefile_zip};
      my $remote_checksum = $checksums->{$dbm_remotefile_zip};
      return($self->{$dbm_hashkey}, $self->{$tempfile_hashkey})
        if defined $local_checksum and defined $remote_checksum
           and $local_checksum eq $remote_checksum;
    }

    # just to make sure
    my $method = 'close_' . $dbm_type . "_dbm";
    $self->$method;
  }

  my $file;
  if ($checksums) {
    $file = $self->_fetch_dbm_file($dbm_remotefile_zip);
    # (error set by _fetch_dbm_file)
    return() if not defined $file; # or not -f $file; # <--- _fetch_dbm_file should do the stat!
  }
  else {
    # cached!
    $file = File::Spec->catfile($self->{cache_dir}, $dbm_remotefile_zip);
    $self->{error} = "Cache miss error: Expected $file to exist, but it doesn't" if not -f $file;
  }

  my ($tempfh, $tempfile) = File::Temp::tempfile(
    'temporary_dbm_XXXXX',
    UNLINK => 0,
    DIR => File::Spec->tmpdir(),
    EXLOCK => 0, # FIXME no exclusive locking or else we block on BSD. What's the right solution?
  );

  if (not $self->_unzip_file($file, $tempfile, $dbm_remotefile)) {
    $self->{error} = "Could not unzip dbm file '$file' to '$tempfile'";
    unlink($tempfile);
    return();
  }

  $self->{$tempfile_hashkey} = $tempfile;

  my %hash;
  my $obj = tie %hash, "DBM::Deep", {
    file => $tempfile,
    locking => 1,
    autoflush => 0,
  }; 

  $self->{$dbm_hashkey} = \%hash;

  # save this dbm file checksum
  if (ref($checksums)) {
    if (not $self->{checksums}) {
      $self->{checksums} = {};
    }
    $self->{checksums}{$dbm_remotefile_zip} = $checksums->{$dbm_remotefile_zip};
  }

  return (\%hash, $tempfile);
}


=head2 _parse_dbm_checksums

This is a private method.

Given a reference to a file handle, a reference to a string
or a file name, this method parses a checksum file
and returns a hash reference associating file names
with their base64 encoded MD5 hashes.

If passed a ref to a string, the contents of the string will
be assumed to contain the checksum data.

=cut

sub _parse_dbm_checksums {
  my $self = shift;
  $self->{error} = undef;

  my $file_or_fh = shift;
  my $is_string = 0;
  my $fh;
  if (ref($file_or_fh) eq 'GLOB') {
    $fh = $file_or_fh;
  }
  elsif (ref($file_or_fh) eq 'SCALAR') {
    $is_string = 1;
  }
  else {
    open $fh, '<', $file_or_fh
      or die "Could not open file '$file_or_fh' for reading: $!";
  }

  my $hashes = {};
  my @lines;
  @lines = split /\n/, $$file_or_fh if $is_string;

  while (1) {
    local $_ = $is_string ? shift @lines : <$fh>;
    last if not defined $_;
    next if /^\s*$/ or /^\s*#/;
    my ($file, $hash) = split /\t/, $_;
    if (not defined $file or not defined $hash) {
      $self->{error} = "Error reading repository checksums.";
      return();
    }
    $hash =~ s/\s+$//;
    $hashes->{$file} = $hash;
  }

  return $hashes;
}


=head2 _calculate_cache_local_checksums

This is a private method.

Calculates the checksums of the DBMs in the local cache directory.
If the repository client isn't using a private cache directory, this
B<short circuits> and does not actually try to calculate
any checksums of potentially modified files.

Returns the checksums hash just like the checksum fetching
routine.

Maintainer note: Essentially the same code lives in
PAR::Repository's DBM code for calculating the repository checksums
in the first place.

=cut

sub _calculate_cache_local_checksums {
  my $self = shift;

  # only support inter-run cache summing if we're in a private cache dir!
  if (!$self->{private_cache_dir}) {
    return();
  }

  # find a working base64 MD5 implementation
  my $md5_function;
  eval { require Digest::MD5; $md5_function = \&Digest::MD5::md5_base64; };
  eval { require Digest::Perl::MD5;  $md5_function = \&Digest::Perl::MD5::md5_base64; } if $@;
  if ($@) {
    return();
  }
  
  my $hashes = {};
  # calculate local hashes
  foreach my $dbmfile (
      PAR::Repository::Client::MODULES_DBM_FILE(),
      PAR::Repository::Client::SCRIPTS_DBM_FILE(),
      PAR::Repository::Client::SYMLINKS_DBM_FILE(),
      PAR::Repository::Client::DEPENDENCIES_DBM_FILE(),
    ) {
    my $filepath = File::Spec->catfile($self->{cache_dir}, $dbmfile.'.zip');
    next unless -f $filepath;
    open my $fh, '<', $filepath
      or die "Could not open DBM file '$filepath' for reading: $!";
    local $/ = undef;
    my $hash = $md5_function->(<$fh>);
    close $fh;
    $hashes->{$dbmfile.'.zip'} = $hash;
  } # end foreach dbm files

  return $hashes; 
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
