use 5.006;
use strict;
use warnings;

package CPAN::Mini;
$CPAN::Mini::VERSION = '1.111016';
# ABSTRACT: create a minimal mirror of CPAN

## no critic RequireCarping

#pod =head1 SYNOPSIS
#pod
#pod (If you're not going to do something weird, you probably want to look at the
#pod L<minicpan> command, instead.)
#pod
#pod   use CPAN::Mini;
#pod
#pod   CPAN::Mini->update_mirror(
#pod     remote => "http://cpan.mirrors.comintern.su",
#pod     local  => "/usr/share/mirrors/cpan",
#pod     log_level => 'debug',
#pod   );
#pod
#pod =head1 DESCRIPTION
#pod
#pod CPAN::Mini provides a simple mechanism to build and update a minimal mirror of
#pod the CPAN on your local disk.  It contains only those files needed to install
#pod the newest version of every distribution.  Those files are:
#pod
#pod =for :list
#pod * 01mailrc.txt.gz
#pod * 02packages.details.txt.gz
#pod * 03modlist.data.gz
#pod * the last non-developer release of every dist for every author
#pod
#pod =cut

use Carp ();

use File::Basename ();
use File::Copy     ();
use File::HomeDir 0.57 ();  # Win32 support
use File::Find ();
use File::Path 2.04 ();     # new API, bugfixes
use File::Spec ();
use File::Temp ();

use URI 1            ();
use LWP::UserAgent 5 ();

use Compress::Zlib 1.20 ();

#pod =method update_mirror
#pod
#pod   CPAN::Mini->update_mirror(
#pod     remote => "http://cpan.mirrors.comintern.su",
#pod     local  => "/usr/share/mirrors/cpan",
#pod     force  => 0,
#pod     log_level => 'debug',
#pod   );
#pod
#pod This is the only method that need be called from outside this module.  It will
#pod update the local mirror with the files from the remote mirror.
#pod
#pod If called as a class method, C<update_mirror> creates an ephemeral CPAN::Mini
#pod object on which other methods are called.  That object is used to store mirror
#pod location and state.
#pod
#pod This method returns the number of files updated.
#pod
#pod The following options are recognized:
#pod
#pod =begin :list
#pod
#pod * C<local>
#pod
#pod This is the local file path where the mirror will be written or updated.
#pod
#pod * C<remote>
#pod
#pod This is the URL of the CPAN mirror from which to work.  A reasonable default
#pod will be picked by default.  A list of CPAN mirrors can be found at
#pod L<http://www.cpan.org/SITES.html>
#pod
#pod * C<dirmode>
#pod
#pod Generally an octal number, this option sets the permissions of created
#pod directories.  It defaults to 0711.
#pod
#pod * C<exact_mirror>
#pod
#pod If true, the C<files_allowed> method will allow all extra files to be mirrored.
#pod
#pod * C<ignore_source_control>
#pod
#pod If true, CPAN::Mini will not try to remove source control files during
#pod cleanup. See C<clean_unmirrored> for details.
#pod
#pod * C<force>
#pod
#pod If true, this option will cause CPAN::Mini to read the entire module list and
#pod update anything out of date, even if the module list itself wasn't out of date
#pod on this run.
#pod
#pod * C<skip_perl>
#pod
#pod If true, CPAN::Mini will skip the major language distributions: perl, parrot,
#pod and ponie.  It will also skip embperl, sybperl, bioperl, and kurila.
#pod
#pod * C<log_level>
#pod
#pod This defines the minimum level of message to log: debug, info, warn, or fatal
#pod
#pod * C<errors>
#pod
#pod If true, CPAN::Mini will warn with status messages on errors.  (default: true)
#pod
#pod * C<path_filters>
#pod
#pod This options provides a set of rules for filtering paths.  If a distribution
#pod matches one of the rules in C<path_filters>, it will not be mirrored.  A regex
#pod rule is matched if the path matches the regex; a code rule is matched if the
#pod code returns 1 when the path is passed to it.  For example, the following
#pod setting would skip all distributions from RJBS and SUNGO:
#pod
#pod  path_filters => [
#pod    qr/RJBS/,
#pod    sub { $_[0] =~ /SUNGO/ }
#pod  ]
#pod
#pod * C<module_filters>
#pod
#pod This option provides a set of rules for filtering modules.  It behaves like
#pod path_filters, but acts only on module names.  (Since most modules are in
#pod distributions with more than one module, this setting will probably be less
#pod useful than C<path_filters>.)  For example, this setting will skip any
#pod distribution containing only modules with the word "Acme" in them:
#pod
#pod  module_filters => [ qr/Acme/i ]
#pod
#pod * C<also_mirror>
#pod
#pod This option should be an arrayref of extra files in the remote CPAN to mirror
#pod locally.
#pod
#pod * C<skip_cleanup>
#pod
#pod If this option is true, CPAN::Mini will not try delete unmirrored files when it
#pod has finished mirroring
#pod
#pod * C<offline>
#pod
#pod If offline, CPAN::Mini will not attempt to contact remote resources.
#pod
#pod * C<no_conn_cache>
#pod
#pod If true, no connection cache will be established.  This is mostly useful as a
#pod workaround for connection cache failures.
#pod
#pod =end :list
#pod
#pod =cut

sub update_mirror {
  my $self = shift;
  $self = $self->new(@_) unless ref $self;

  unless ($self->{offline}) {
    my $local = $self->{local};

    $self->log("Updating $local");
    $self->log("Mirroring from $self->{remote}");
    $self->log("=" x 63);

    die "local mirror target $local is not writable" unless -w $local;

    # mirrored tracks the already done, keyed by filename
    # 1 = local-checked, 2 = remote-mirrored
    $self->mirror_indices;

    return unless $self->{force} or $self->{changes_made};

    # mirror all the files
    $self->_mirror_extras;
    $self->mirror_file($_, 1) for @{ $self->_get_mirror_list };

    # install indices after files are mirrored in case we're interrupted
    # so indices will seem new again when continuing
    $self->_install_indices;

    $self->_write_out_recent;

    # eliminate files we don't need
    $self->clean_unmirrored unless $self->{skip_cleanup};
  }

  return $self->{changes_made};
}

sub _recent { $_[0]->{recent}{ $_[1] } = 1 }

sub _write_out_recent {
  my ($self) = @_;
  return unless my @keys = keys %{ $self->{recent} };

  my $recent = File::Spec->catfile($self->{local}, 'RECENT');
  open my $recent_fh, '>', $recent or die "can't open $recent for writing: $!";

  for my $file (sort keys %{ $self->{recent} }) {
    print {$recent_fh} "$file\n" or die "can't write to $recent: $!";
  }

  die "error closing $recent: $!" unless close $recent_fh;
  return;
}

sub _get_mirror_list {
  my $self = shift;

  my %mirror_list;

  # now walk the packages list
  my $details = File::Spec->catfile(
    $self->_scratch_dir,
    qw(modules 02packages.details.txt.gz)
  );

  my $gz = Compress::Zlib::gzopen($details, "rb")
    or die "Cannot open details: $Compress::Zlib::gzerrno";

  my $inheader = 1;
  my $file_ok  = 0;
  while ($gz->gzreadline($_) > 0) {
    if ($inheader) {
      if (/\S/) {
        my ($header, $value) = split /:\s*/, $_, 2;
        chomp $value;
        if ($header eq 'File'
            and ($value eq '02packages.details.txt'
                 or $value eq '02packages.details.txt.gz')) {
          $file_ok = 1;
        }
      } else {
        $inheader = 0;
      }

      next;
    }

    die "02packages.details.txt file is not a valid index\n"
      unless $file_ok;

    my ($module, $version, $path) = split;
    next if $self->_filter_module({
      module  => $module,
      version => $version,
      path    => $path,
    });

    $mirror_list{"authors/id/$path"}++;
  }

  return [ sort keys %mirror_list ];
}

#pod =method new
#pod
#pod   my $minicpan = CPAN::Mini->new;
#pod
#pod This method constructs a new CPAN::Mini object.  Its parameters are described
#pod above, under C<update_mirror>.
#pod
#pod =cut

sub new {
  my $class    = shift;
  my %defaults = (
    changes_made => 0,
    dirmode      => 0711,  ## no critic Zero
    errors       => 1,
    mirrored     => {},
    log_level    => 'info',
  );

  my $self = bless { %defaults, @_ } => $class;

  $self->{dirmode} = $defaults{dirmode} unless defined $self->{dirmode};

  $self->{recent} = {};

  Carp::croak "no local mirror supplied" unless $self->{local};

  substr($self->{local}, 0, 1, $class->__homedir)
    if substr($self->{local}, 0, 1) eq q{~};

  Carp::croak "local mirror path exists but is not a directory"
    if (-e $self->{local})
    and not(-d $self->{local});

  unless (-e $self->{local}) {
    File::Path::mkpath(
      $self->{local},
      {
        verbose => $self->{log_level} eq 'debug',
        mode    => $self->{dirmode},
      },
    );
  }

  Carp::croak "no write permission to local mirror" unless -w $self->{local};

  Carp::croak "no remote mirror supplied" unless $self->{remote};

  $self->{remote} = "$self->{remote}/" if substr($self->{remote}, -1) ne '/';

  my $version = $class->VERSION;
  $version = 'v?' unless defined $version;

  $self->{__lwp} = LWP::UserAgent->new(
    agent     => "$class/$version",
    env_proxy => 1,
    ($self->{no_conn_cache} ? () : (keep_alive => 5)),
    ($self->{timeout} ? (timeout => $self->{timeout}) : ()),
  );

  unless ($self->{offline}) {
    my $test_uri = URI->new_abs(
      'modules/02packages.details.txt.gz',
      $self->{remote},
    )->as_string;

    Carp::croak "unable to contact the remote mirror"
      unless eval { $self->__lwp->head($test_uri)->is_success };
  }

  return $self;
}

sub __lwp { $_[0]->{__lwp} }

#pod =method mirror_indices
#pod
#pod   $minicpan->mirror_indices;
#pod
#pod This method updates the index files from the CPAN.
#pod
#pod =cut

sub _fixed_mirrors {
  qw(
    authors/01mailrc.txt.gz
    modules/02packages.details.txt.gz
    modules/03modlist.data.gz
  );
}

sub _scratch_dir {
  my ($self) = @_;

  $self->{scratch} ||= File::Temp::tempdir(CLEANUP => 1);
  return $self->{scratch};
}

sub mirror_indices {
  my $self = shift;

  $self->_make_index_dirs($self->_scratch_dir);

  for my $path ($self->_fixed_mirrors) {
    my $local_file   = File::Spec->catfile($self->{local},   split m{/}, $path);
    my $scratch_file = File::Spec->catfile(
      $self->_scratch_dir,
      split(m{/}, $path),
    );

    File::Copy::copy($local_file, $scratch_file);

    utime((stat $local_file)[ 8, 9 ], $scratch_file);

    $self->mirror_file($path, undef, { to_scratch => 1 });
  }
}

sub _mirror_extras {
  my $self = shift;

  for my $path (@{ $self->{also_mirror} }) {
    $self->mirror_file($path, undef);
  }
}

sub _make_index_dirs {
  my ($self, $base_dir, $dir_mode, $trace) = @_;
  $base_dir ||= $self->_scratch_dir;
  $dir_mode = 0711 if !defined $dir_mode;  ## no critic Zero
  $trace    = 0    if !defined $trace;

  for my $index ($self->_fixed_mirrors) {
    my $dir = File::Basename::dirname($index);
    my $needed = File::Spec->catdir($base_dir, $dir);
    File::Path::mkpath($needed, { verbose => $trace, mode => $dir_mode });
    die "couldn't create $needed: $!" unless -d $needed;
  }
}

sub _install_indices {
  my $self = shift;

  $self->_make_index_dirs(
    $self->{local},
    $self->{dirmode},
    $self->{log_level} eq 'debug',
  );

  for my $file ($self->_fixed_mirrors) {
    my $local_file = File::Spec->catfile($self->{local}, split m{/}, $file);

    unlink $local_file;

    File::Copy::copy(
      File::Spec->catfile($self->_scratch_dir, split m{/}, $file),
      $local_file,
    );

    $self->{mirrored}{$local_file} = 1;
  }
}

#pod =method mirror_file
#pod
#pod   $minicpan->mirror_file($path, $skip_if_present)
#pod
#pod This method will mirror the given file from the remote to the local mirror,
#pod overwriting any existing file unless C<$skip_if_present> is true.
#pod
#pod =cut

sub mirror_file {
  my ($self, $path, $skip_if_present, $arg) = @_;

  $arg ||= {};

  # full URL
  my $remote_uri = eval { $path->isa('URI') }
                 ? $path
                 : URI->new_abs($path, $self->{remote})->as_string;

  # native absolute file
  my $local_file = File::Spec->catfile(
    $arg->{to_scratch} ? $self->_scratch_dir : $self->{local},
    split m{/}, $path
  );

  my $checksum_might_be_up_to_date = 1;

  if ($skip_if_present and -f $local_file) {
    ## upgrade to checked if not already
    $self->{mirrored}{$local_file} ||= 1;
  } elsif (($self->{mirrored}{$local_file} || 0) < 2) {
    ## upgrade to full mirror
    $self->{mirrored}{$local_file} = 2;

    File::Path::mkpath(
      File::Basename::dirname($local_file),
      {
        verbose => $self->{log_level} eq 'debug',
        mode    => $self->{dirmode},
      },
    );

    $self->log($path, { no_nl => 1 });
    my $res = $self->{__lwp}->mirror($remote_uri, $local_file);

    if ($res->is_success) {
      utime undef, undef, $local_file if $arg->{update_times};
      $checksum_might_be_up_to_date = 0;
      $self->_recent($path);
      $self->log(" ... updated");
      $self->{changes_made}++;
    } elsif ($res->code != 304) {  # not modified
      $self->log(" ... resulted in an HTTP error with status " . $res->code);
      $self->log_warn("$remote_uri: " . $res->status_line);
      return;
    } else {
      $self->log(" ... up to date");
    }
  }

  if ($path =~ m{^authors/id}) {   # maybe fetch CHECKSUMS
    my $checksum_path
      = URI->new_abs("CHECKSUMS", $remote_uri)->rel($self->{remote})->as_string;

    if ($path ne $checksum_path) {
      $self->mirror_file($checksum_path, $checksum_might_be_up_to_date);
    }
  }
}

#pod =begin devel
#pod
#pod =method _filter_module
#pod
#pod  next
#pod    if $self->_filter_module({ module => $foo, version => $foo, path => $foo });
#pod
#pod This method holds the filter chain logic. C<update_mirror> takes an optional
#pod set of filter parameters.  As C<update_mirror> encounters a distribution, it
#pod calls this method to figure out whether or not it should be downloaded. The
#pod user provided filters are taken into account. Returns 1 if the distribution is
#pod filtered (to be skipped).  Returns 0 if the distribution is to not filtered
#pod (not to be skipped).
#pod
#pod =end devel
#pod
#pod =cut

sub __do_filter {
  my ($self, $filter, $file) = @_;
  return unless $filter;

  if (ref($filter) eq 'ARRAY') {
    for (@$filter) {
      return 1 if $self->__do_filter($_, $file);
    }
    return;
  }

  if (ref($filter) eq 'CODE') {
    return $filter->($file);
  } else {
    return $file =~ $filter;
  }
}

sub _filter_module {
  my $self = shift;
  my $args = shift;

  if ($self->{skip_perl}) {
    return 1 if $args->{path} =~ m{/(?:emb|syb|bio)?perl-\d}i;
    return 1 if $args->{path} =~ m{/(?:parrot|ponie)-\d}i;
    return 1 if $args->{path} =~ m{/(?:kurila)-\d}i;
    return 1 if $args->{path} =~ m{/\bperl-?5\.004}i;
    return 1 if $args->{path} =~ m{/\bperl_mlb\.zip}i;
  }

  return 1 if $self->__do_filter($self->{path_filters},   $args->{path});
  return 1 if $self->__do_filter($self->{module_filters}, $args->{module});
  return 0;
}

#pod =method file_allowed
#pod
#pod   next unless $minicpan->file_allowed($filename);
#pod
#pod This method returns true if the given file is allowed to exist in the local
#pod mirror, even if it isn't one of the required mirror files.
#pod
#pod By default, only dot-files are allowed.  If the C<exact_mirror> option is true,
#pod all files are allowed.
#pod
#pod =cut

sub file_allowed {
  my ($self, $file) = @_;
  return 1 if $self->{exact_mirror};

  # It's a cheap hack, but it gets the job done.
  return 1 if $file eq File::Spec->catfile($self->{local}, 'RECENT');

  return (substr(File::Basename::basename($file), 0, 1) eq q{.}) ? 1 : 0;
}

#pod =method clean_unmirrored
#pod
#pod   $minicpan->clean_unmirrored;
#pod
#pod This method looks through the local mirror's files.  If it finds a file that
#pod neither belongs in the mirror nor is allowed (see the C<file_allowed> method),
#pod C<clean_file> is called on the file.
#pod
#pod If you set C<ignore_source_control> to a true value, then this doesn't clean
#pod up files that belong to source control systems. Currently this ignores:
#pod
#pod 	.cvs .cvsignore
#pod 	.svn .svnignore
#pod 	.git .gitignore
#pod
#pod Send patches for other source control files that you would like to have added.
#pod
#pod =cut

my %Source_control_files;
BEGIN {
  %Source_control_files = map { $_ => 1 }
    qw(.cvs .svn .git .cvsignore .svnignore .gitignore);
}

sub clean_unmirrored {
  my $self = shift;

  File::Find::find sub {
    my $file = File::Spec->canonpath($File::Find::name);  ## no critic Package
    my $basename = File::Basename::basename( $file );

    if (
      $self->{ignore_source_control}
      and exists $Source_control_files{$basename}
    ) {
      $File::Find::prune = 1;
      return;
    }

    return unless (-f $file and not $self->{mirrored}{$file});
    return if $self->file_allowed($file);

    $self->clean_file($file);

  }, $self->{local};
}

#pod =method clean_file
#pod
#pod   $minicpan->clean_file($filename);
#pod
#pod This method, called by C<clean_unmirrored>, deletes the named file.  It returns
#pod true if the file is successfully unlinked.  Otherwise, it returns false.
#pod
#pod =cut

sub clean_file {
  my ($self, $file) = @_;

  unless (unlink $file) {
    $self->log_warn("$file cannot be removed: $!");
    return;
  }

  $self->log("$file removed");

  return 1;
}

#pod =method log_warn
#pod
#pod =method log
#pod
#pod =method log_debug
#pod
#pod   $minicpan->log($message);
#pod
#pod This will log (print) the given message unless the log level is too low.
#pod
#pod C<log>, which logs at the I<info> level, may also be called as C<trace> for
#pod backward compatibility reasons.
#pod
#pod =cut

sub log_level {
  return $_[0]->{log_level} if ref $_[0];
  return 'info';
}

sub log_unconditionally {
  my ($self, $message, $arg) = @_;
  $arg ||= {};

  print($message, $arg->{no_nl} ? () : "\n");
}

sub log_warn {
  return if $_[0]->log_level eq 'fatal';
  $_[0]->log_unconditionally($_[1], $_[2]);
}

sub log {
  return unless $_[0]->log_level =~ /\A(?:info|debug)\z/;
  $_[0]->log_unconditionally($_[1], $_[2]);
}

sub trace {
  my $self = shift;
  $self->log(@_);
}

sub log_debug {
  my ($self, @rest) = @_;
  return unless $_[0]->log_level eq 'debug';
  $_[0]->log_unconditionally($_[1], $_[2]);
}

#pod =method read_config
#pod
#pod   my %config = CPAN::Mini->read_config(\%options);
#pod
#pod This routine returns a set of arguments that can be passed to CPAN::Mini's
#pod C<new> or C<update_mirror> methods.  It will look for a file called
#pod F<.minicpanrc> in the user's home directory as determined by
#pod L<File::HomeDir|File::HomeDir>.
#pod
#pod =cut

sub __homedir {
  my ($class) = @_;

  my $homedir = File::HomeDir->my_home || $ENV{HOME};

  Carp::croak "couldn't determine your home directory!  set HOME env variable"
    unless defined $homedir;

  return $homedir;
}

sub __homedir_configfile {
  my ($class) = @_;
  my $default = File::Spec->catfile($class->__homedir, '.minicpanrc');
}

sub __default_configfile {
  my ($self) = @_;

  (my $pm_loc = $INC{'CPAN/Mini.pm'}) =~ s/Mini\.pm\z//;
  File::Spec->catfile($pm_loc, 'minicpan.conf');
}

sub read_config {
  my ($class, $options) = @_;

  my $config_file = $class->config_file($options);

  return unless defined $config_file;

  # This is ugly, but lets us respect -qq for now even before we have an
  # object.  I think a better fix is warranted. -- rjbs, 2010-03-04
  $class->log("Using config from $config_file")
    if ($options->{log_level}||'info') =~ /\A(?:warn|fatal)\z/;

  substr($config_file, 0, 1, $class->__homedir)
    if substr($config_file, 0, 1) eq q{~};

  return unless -e $config_file;

  open my $config_fh, '<', $config_file
    or die "couldn't open config file $config_file: $!";

  my %config;
  my %is_multivalue = map {; $_ => 1 }
                      qw(also_mirror module_filters path_filters);

  $config{$_} = [] for keys %is_multivalue;

  while (<$config_fh>) {
    chomp;
    next if /\A\s*\Z/sm;

    if (/\A(\w+):\s*(\S.*?)\s*\Z/sm) {
      my ($key, $value) = ($1, $2);

      if ($is_multivalue{ $key }) {
        push @{ $config{$key} }, split /\s+/, $value;
      } else {
        $config{ $key } = $value;
      }
    }
  }

  for (qw(also_mirror)) {
    $config{$_} = [ grep { length } @{ $config{$_} } ];
  }

  for (qw(module_filters path_filters)) {
    $config{$_} = [ map { qr/$_/ } @{ $config{$_} } ];
  }

  for (keys %is_multivalue) {
    delete $config{$_} unless @{ $config{$_} };
  }

  return %config;
}

#pod =method config_file
#pod
#pod   my $config_file = CPAN::Mini->config_file( { options } );
#pod
#pod This routine returns the config file name. It first looks at for the
#pod C<config_file> setting, then the C<CPAN_MINI_CONFIG> environment
#pod variable, then the default F<~/.minicpanrc>, and finally the
#pod F<CPAN/Mini/minicpan.conf>. It uses the first defined value it finds.
#pod If the filename it selects does not exist, it returns false.
#pod
#pod OPTIONS is an optional hash reference of the C<CPAN::Mini> config hash.
#pod
#pod =cut

sub config_file {
  my ($class, $options) = @_;

  my $config_file = do {
    if (defined eval { $options->{config_file} }) {
      $options->{config_file};
    } elsif (defined $ENV{CPAN_MINI_CONFIG}) {
      $ENV{CPAN_MINI_CONFIG};
    } elsif (defined $class->__homedir_configfile) {
      $class->__homedir_configfile;
    } elsif (defined $class->__default_configfile) {
      $class->__default_configfile;
    } else {
      ();
    }
  };

  return (
    (defined $config_file && -e $config_file)
    ? $config_file
    : ()
  );
}

#pod =head2 remote_from
#pod
#pod   my $remote = CPAN::Mini->remote_from( $remote_from, $orig_remote, $quiet );
#pod
#pod This routine take an string argument and turn it into a method
#pod call to handle to retrieve the a cpan mirror url from a source.
#pod Currently supported methods:
#pod
#pod     cpan     - fetch the first mirror from your CPAN.pm config
#pod     cpanplus - fetch the first mirror from your CPANPLUS.pm config
#pod
#pod =cut

sub remote_from {
  my ( $class, $remote_from, $orig_remote, $quiet ) = @_;

  my $method = lc "remote_from_" . $remote_from;

  Carp::croak "unknown remote_from value: $remote_from"
    unless $class->can($method);

  my $new_remote = $class->$method;

  warn "overriding '$orig_remote' with '$new_remote' via $method\n"
    if !$quiet && $orig_remote;

  return $new_remote;
}

#pod =head2 remote_from_cpan
#pod
#pod   my $remote = CPAN::Mini->remote_from_cpan;
#pod
#pod This routine loads your CPAN.pm config and returns the first mirror in mirror
#pod list.  You can set this as your default by setting remote_from:cpan in your
#pod F<.minicpanrc> file.
#pod
#pod =cut

sub remote_from_cpan {
  my ($self) = @_;

  Carp::croak "unable find a CPAN, maybe you need to install it"
    unless eval { require CPAN; 1 };

  CPAN::HandleConfig::require_myconfig_or_config();

  Carp::croak "unable to find mirror list in your CPAN config"
    unless exists $CPAN::Config->{urllist};

  Carp::croak "unable to find first mirror url in your CPAN config"
    unless ref( $CPAN::Config->{urllist} ) eq 'ARRAY' && $CPAN::Config->{urllist}[0];

  return $CPAN::Config->{urllist}[0];
}

#pod =head2 remote_from_cpanplus
#pod
#pod   my $remote = CPAN::Mini->remote_from_cpanplus;
#pod
#pod This routine loads your CPANPLUS.pm config and returns the first mirror in
#pod mirror list.  You can set this as your default by setting remote_from:cpanplus
#pod in your F<.minicpanrc> file.
#pod
#pod =cut

sub remote_from_cpanplus {
  my ($self) = @_;

  Carp::croak "unable find a CPANPLUS, maybe you need to install it"
    unless eval { require CPANPLUS::Backend };

  my $cb = CPANPLUS::Backend->new;
  my $hosts = $cb->configure_object->get_conf('hosts');

  Carp::croak "unable to find mirror list in your CPANPLUS config"
    unless $hosts;

  Carp::croak "unable to find first mirror in your CPANPLUS config"
    unless ref($hosts) eq 'ARRAY' && $hosts->[0];

  my $url_parts = $hosts->[0];
  return $url_parts->{scheme} . "://" . $url_parts->{host} . ( $url_parts->{path} || '' );
}

#pod =head1 SEE ALSO
#pod
#pod Randal Schwartz's original article on minicpan, here:
#pod
#pod 	http://www.stonehenge.com/merlyn/LinuxMag/col42.html
#pod
#pod L<CPANPLUS::Backend>, which provides the C<local_mirror> method, which performs
#pod the same task as this module.
#pod
#pod =head1 THANKS
#pod
#pod Thanks to David Dyck for letting me know about my stupid documentation errors.
#pod
#pod Thanks to Roy Fulbright for finding an obnoxious bug on Win32.
#pod
#pod Thanks to Shawn Sorichetti for fixing a stupid octal-number-as-string bug.
#pod
#pod Thanks to sungo for implementing the filters, so I can finally stop mirroring
#pod bioperl, and Robert Rothenberg for suggesting adding coderef rules.
#pod
#pod Thanks to Adam Kennedy for noticing and complaining about a lot of stupid
#pod little design decisions.
#pod
#pod Thanks to Michael Schwern and Jason Kohles, for pointing out missing
#pod documentation.
#pod
#pod Thanks to David Golden for some important bugfixes and refactoring.
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

CPAN::Mini - create a minimal mirror of CPAN

=head1 VERSION

version 1.111016

=head1 SYNOPSIS

(If you're not going to do something weird, you probably want to look at the
L<minicpan> command, instead.)

  use CPAN::Mini;

  CPAN::Mini->update_mirror(
    remote => "http://cpan.mirrors.comintern.su",
    local  => "/usr/share/mirrors/cpan",
    log_level => 'debug',
  );

=head1 DESCRIPTION

CPAN::Mini provides a simple mechanism to build and update a minimal mirror of
the CPAN on your local disk.  It contains only those files needed to install
the newest version of every distribution.  Those files are:

=over 4

=item *

01mailrc.txt.gz

=item *

02packages.details.txt.gz

=item *

03modlist.data.gz

=item *

the last non-developer release of every dist for every author

=back

=head1 METHODS

=head2 update_mirror

  CPAN::Mini->update_mirror(
    remote => "http://cpan.mirrors.comintern.su",
    local  => "/usr/share/mirrors/cpan",
    force  => 0,
    log_level => 'debug',
  );

This is the only method that need be called from outside this module.  It will
update the local mirror with the files from the remote mirror.

If called as a class method, C<update_mirror> creates an ephemeral CPAN::Mini
object on which other methods are called.  That object is used to store mirror
location and state.

This method returns the number of files updated.

The following options are recognized:

=over 4

=item *

C<local>

This is the local file path where the mirror will be written or updated.

=item *

C<remote>

This is the URL of the CPAN mirror from which to work.  A reasonable default
will be picked by default.  A list of CPAN mirrors can be found at
L<http://www.cpan.org/SITES.html>

=item *

C<dirmode>

Generally an octal number, this option sets the permissions of created
directories.  It defaults to 0711.

=item *

C<exact_mirror>

If true, the C<files_allowed> method will allow all extra files to be mirrored.

=item *

C<ignore_source_control>

If true, CPAN::Mini will not try to remove source control files during
cleanup. See C<clean_unmirrored> for details.

=item *

C<force>

If true, this option will cause CPAN::Mini to read the entire module list and
update anything out of date, even if the module list itself wasn't out of date
on this run.

=item *

C<skip_perl>

If true, CPAN::Mini will skip the major language distributions: perl, parrot,
and ponie.  It will also skip embperl, sybperl, bioperl, and kurila.

=item *

C<log_level>

This defines the minimum level of message to log: debug, info, warn, or fatal

=item *

C<errors>

If true, CPAN::Mini will warn with status messages on errors.  (default: true)

=item *

C<path_filters>

This options provides a set of rules for filtering paths.  If a distribution
matches one of the rules in C<path_filters>, it will not be mirrored.  A regex
rule is matched if the path matches the regex; a code rule is matched if the
code returns 1 when the path is passed to it.  For example, the following
setting would skip all distributions from RJBS and SUNGO:

 path_filters => [
   qr/RJBS/,
   sub { $_[0] =~ /SUNGO/ }
 ]

=item *

C<module_filters>

This option provides a set of rules for filtering modules.  It behaves like
path_filters, but acts only on module names.  (Since most modules are in
distributions with more than one module, this setting will probably be less
useful than C<path_filters>.)  For example, this setting will skip any
distribution containing only modules with the word "Acme" in them:

 module_filters => [ qr/Acme/i ]

=item *

C<also_mirror>

This option should be an arrayref of extra files in the remote CPAN to mirror
locally.

=item *

C<skip_cleanup>

If this option is true, CPAN::Mini will not try delete unmirrored files when it
has finished mirroring

=item *

C<offline>

If offline, CPAN::Mini will not attempt to contact remote resources.

=item *

C<no_conn_cache>

If true, no connection cache will be established.  This is mostly useful as a
workaround for connection cache failures.

=back

=head2 new

  my $minicpan = CPAN::Mini->new;

This method constructs a new CPAN::Mini object.  Its parameters are described
above, under C<update_mirror>.

=head2 mirror_indices

  $minicpan->mirror_indices;

This method updates the index files from the CPAN.

=head2 mirror_file

  $minicpan->mirror_file($path, $skip_if_present)

This method will mirror the given file from the remote to the local mirror,
overwriting any existing file unless C<$skip_if_present> is true.

=head2 file_allowed

  next unless $minicpan->file_allowed($filename);

This method returns true if the given file is allowed to exist in the local
mirror, even if it isn't one of the required mirror files.

By default, only dot-files are allowed.  If the C<exact_mirror> option is true,
all files are allowed.

=head2 clean_unmirrored

  $minicpan->clean_unmirrored;

This method looks through the local mirror's files.  If it finds a file that
neither belongs in the mirror nor is allowed (see the C<file_allowed> method),
C<clean_file> is called on the file.

If you set C<ignore_source_control> to a true value, then this doesn't clean
up files that belong to source control systems. Currently this ignores:

	.cvs .cvsignore
	.svn .svnignore
	.git .gitignore

Send patches for other source control files that you would like to have added.

=head2 clean_file

  $minicpan->clean_file($filename);

This method, called by C<clean_unmirrored>, deletes the named file.  It returns
true if the file is successfully unlinked.  Otherwise, it returns false.

=head2 log_warn

=head2 log

=head2 log_debug

  $minicpan->log($message);

This will log (print) the given message unless the log level is too low.

C<log>, which logs at the I<info> level, may also be called as C<trace> for
backward compatibility reasons.

=head2 read_config

  my %config = CPAN::Mini->read_config(\%options);

This routine returns a set of arguments that can be passed to CPAN::Mini's
C<new> or C<update_mirror> methods.  It will look for a file called
F<.minicpanrc> in the user's home directory as determined by
L<File::HomeDir|File::HomeDir>.

=head2 config_file

  my $config_file = CPAN::Mini->config_file( { options } );

This routine returns the config file name. It first looks at for the
C<config_file> setting, then the C<CPAN_MINI_CONFIG> environment
variable, then the default F<~/.minicpanrc>, and finally the
F<CPAN/Mini/minicpan.conf>. It uses the first defined value it finds.
If the filename it selects does not exist, it returns false.

OPTIONS is an optional hash reference of the C<CPAN::Mini> config hash.

=begin devel

=method _filter_module

 next
   if $self->_filter_module({ module => $foo, version => $foo, path => $foo });

This method holds the filter chain logic. C<update_mirror> takes an optional
set of filter parameters.  As C<update_mirror> encounters a distribution, it
calls this method to figure out whether or not it should be downloaded. The
user provided filters are taken into account. Returns 1 if the distribution is
filtered (to be skipped).  Returns 0 if the distribution is to not filtered
(not to be skipped).

=end devel

=head2 remote_from

  my $remote = CPAN::Mini->remote_from( $remote_from, $orig_remote, $quiet );

This routine take an string argument and turn it into a method
call to handle to retrieve the a cpan mirror url from a source.
Currently supported methods:

    cpan     - fetch the first mirror from your CPAN.pm config
    cpanplus - fetch the first mirror from your CPANPLUS.pm config

=head2 remote_from_cpan

  my $remote = CPAN::Mini->remote_from_cpan;

This routine loads your CPAN.pm config and returns the first mirror in mirror
list.  You can set this as your default by setting remote_from:cpan in your
F<.minicpanrc> file.

=head2 remote_from_cpanplus

  my $remote = CPAN::Mini->remote_from_cpanplus;

This routine loads your CPANPLUS.pm config and returns the first mirror in
mirror list.  You can set this as your default by setting remote_from:cpanplus
in your F<.minicpanrc> file.

=head1 SEE ALSO

Randal Schwartz's original article on minicpan, here:

	http://www.stonehenge.com/merlyn/LinuxMag/col42.html

L<CPANPLUS::Backend>, which provides the C<local_mirror> method, which performs
the same task as this module.

=head1 THANKS

Thanks to David Dyck for letting me know about my stupid documentation errors.

Thanks to Roy Fulbright for finding an obnoxious bug on Win32.

Thanks to Shawn Sorichetti for fixing a stupid octal-number-as-string bug.

Thanks to sungo for implementing the filters, so I can finally stop mirroring
bioperl, and Robert Rothenberg for suggesting adding coderef rules.

Thanks to Adam Kennedy for noticing and complaining about a lot of stupid
little design decisions.

Thanks to Michael Schwern and Jason Kohles, for pointing out missing
documentation.

Thanks to David Golden for some important bugfixes and refactoring.

=head1 AUTHORS

=over 4

=item *

Ricardo SIGNES <rjbs@cpan.org>

=item *

Randal Schwartz <merlyn@stonehenge.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2004 by Ricardo SIGNES.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
