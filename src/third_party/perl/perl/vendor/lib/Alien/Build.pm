package Alien::Build;

use strict;
use warnings;
use Path::Tiny ();
use Carp ();
use File::chdir;
use JSON::PP ();
use Env qw( @PATH );
use Env qw( @PKG_CONFIG_PATH );
use Config ();

# ABSTRACT: Build external dependencies for use in CPAN
our $VERSION = '1.74'; # VERSION


sub _path { goto \&Path::Tiny::path }


sub new
{
  my($class, %args) = @_;
  my $self = bless {
    install_prop => {
      root  => _path($args{root} || "_alien")->absolute->stringify,
      patch => (defined $args{patch}) ? _path($args{patch})->absolute->stringify : undef,
    },
    runtime_prop => {
      alien_build_version => $Alien::Build::VERSION || 'dev',
    },
    bin_dir => [],
    pkg_config_path => [],
    aclocal_path => [],
  }, $class;

  $self->meta->filename(
    $args{filename} || do {
      my(undef, $filename) = caller;
      _path($filename)->absolute->stringify;
    }
  );

  if($args{meta_prop})
  {
    $self->meta->prop->{$_} = $args{meta_prop}->{$_} for keys %{ $args{meta_prop} };
  }

  $self;
}


my $count = 0;

sub load
{
  my(undef, $alienfile, @args) = @_;

  my $rcfile = Path::Tiny->new($ENV{ALIEN_BUILD_RC} || '~/.alienbuild/rc.pl')->absolute;
  if(-r $rcfile)
  {
    package Alien::Build::rc;
    sub logx ($)
    {
      unshift @_, 'Alien::Build';
      goto &Alien::Build::log;
    };
    sub preload ($)
    {
      push @Alien::Build::rc::PRELOAD, $_[0];
    }
    sub postload ($)
    {
      push @Alien::Build::rc::POSTLOAD, $_[0];
    }
    require $rcfile;
  }

  unless(-r $alienfile)
  {
    Carp::croak "Unable to read alienfile: $alienfile";
  }

  my $file = _path $alienfile;
  my $name = $file->parent->basename;
  $name =~ s/^alien-//i;
  $name =~ s/[^a-z]//g;
  $name = 'x' if $name eq '';
  $name = ucfirst $name;

  my $class = "Alien::Build::Auto::$name@{[ $count++ ]}";

  { no strict 'refs';
  @{ "${class}::ISA" } = ('Alien::Build');
  *{ "${class}::Alienfile::meta" } = sub {
    $class =~ s{::Alienfile$}{};
    $class->meta;
  }};

  my @preload = qw( Core::Setup Core::Download Core::FFI Core::Override Core::CleanInstall );
  push @preload, @Alien::Build::rc::PRELOAD;
  push @preload, split ';', $ENV{ALIEN_BUILD_PRELOAD}
    if defined $ENV{ALIEN_BUILD_PRELOAD};

  my @postload = qw( Core::Legacy Core::Gather Core::Tail );
  push @postload, @Alien::Build::rc::POSTLOAD;
  push @postload, split ';', $ENV{ALIEN_BUILD_POSTLOAD}
    if defined $ENV{ALIEN_BUILD_POSTLOAD};

  my $self = $class->new(
    filename => $file->absolute->stringify,
    @args,
  );

  require alienfile;

  foreach my $preload (@preload)
  {
    ref $preload eq 'CODE' ? $preload->($self->meta) : $self->meta->apply_plugin($preload);
  }

  # TODO: do this without a string eval ?
  eval '# line '. __LINE__ . ' "' . __FILE__ . qq("\n) . qq{
    package ${class}::Alienfile;
    do '@{[ $file->absolute->stringify ]}';
    die \$\@ if \$\@;
  };
  die $@ if $@;

  foreach my $postload (@postload)
  {
    ref $postload eq 'CODE' ? $postload->($self->meta) : $self->meta->apply_plugin($postload);
  }

  $self->{args} = \@args;
  unless(defined $self->meta->prop->{arch})
  {
    $self->meta->prop->{arch} = 1;
  }

  unless(defined $self->meta->prop->{network})
  {
    $self->meta->prop->{network} = 1;
    ## https://github.com/Perl5-Alien/Alien-Build/issues/23#issuecomment-341114414
    #$self->meta->prop->{network} = 0 if $ENV{NO_NETWORK_TESTING};
    $self->meta->prop->{network} = 0 if (defined $ENV{ALIEN_INSTALL_NETWORK}) && ! $ENV{ALIEN_INSTALL_NETWORK};
  }

  unless(defined $self->meta->prop->{local_source})
  {
    if(! defined $self->meta->prop->{start_url})
    {
      $self->meta->prop->{local_source} = 0;
    }
    # we assume URL schemes are at least two characters, that
    # way Windows absolute paths can be used as local start_url
    elsif($self->meta->prop->{start_url} =~ /^([a-z]{2,}):/i)
    {
      my $scheme = $1;
      $self->meta->prop->{local_source} = $scheme eq 'file';
    }
    else
    {
      $self->meta->prop->{local_source} = 1;
    }
  }

  return $self;
}


sub resume
{
  my(undef, $alienfile, $root) = @_;
  my $h = JSON::PP::decode_json(_path("$root/state.json")->slurp);
  my $self = Alien::Build->load("$alienfile", @{ $h->{args} });
  $self->{install_prop} = $h->{install};
  $self->{runtime_prop} = $h->{runtime};
  $self;
}


sub meta_prop
{
  my($class) = @_;
  $class->meta->prop;
}


sub install_prop
{
  shift->{install_prop};
}


sub runtime_prop
{
  shift->{runtime_prop};
}


sub hook_prop
{
  shift->{hook_prop};
}

sub _command_prop
{
  my($self) = @_;

  return {
    alien => {
      install => $self->install_prop,
      runtime => $self->runtime_prop,
      hook    => $self->hook_prop,
      meta    => $self->meta_prop,
    },
    perl => {
      config => \%Config::Config,
    },
    env => \%ENV,
  };
}


sub checkpoint
{
  my($self) = @_;
  my $root = $self->root;
  _path("$root/state.json")->spew(
    JSON::PP->new->pretty->canonical(1)->encode({
      install => $self->install_prop,
      runtime => $self->runtime_prop,
      args    => $self->{args},
    })
  );
  $self;
}


sub root
{
  my($self) = @_;
  my $root = $self->install_prop->{root};
  _path($root)->mkpath unless -d $root;
  $root;
}


sub install_type
{
  my($self) = @_;
  $self->{runtime_prop}->{install_type} ||= $self->probe;
}


sub set_prefix
{
  my($self, $prefix) = @_;

  if($self->meta_prop->{destdir})
  {
    $self->runtime_prop->{prefix} =
    $self->install_prop->{prefix} = $prefix;
  }
  else
  {
    $self->runtime_prop->{prefix} = $prefix;
    $self->install_prop->{prefix} = $self->install_prop->{stage};
  }
}


sub set_stage
{
  my($self, $dir) = @_;
  $self->install_prop->{stage} = $dir;
}

sub _merge
{
  my %h;
  while(@_)
  {
    my $mod = shift;
    my $ver = shift;
    if((!defined $h{$mod}) || $ver > $h{$mod})
    { $h{$mod} = $ver }
  }
  \%h;
}


sub requires
{
  my($self, $phase) = @_;
  $phase ||= 'any';
  my $meta = $self->meta;
  $phase =~ /^(?:any|configure)$/
  ? $meta->{require}->{$phase} || {}
  : _merge %{ $meta->{require}->{any} }, %{ $meta->{require}->{$phase} };
}


sub load_requires
{
  my($self, $phase, $eval) = @_;
  my $reqs = $self->requires($phase);
  foreach my $mod (keys %$reqs)
  {
    my $ver = $reqs->{$mod};
    my $check = sub {
      my $pm = "$mod.pm";
      $pm =~ s{::}{/}g;
      require $pm;
    };
    if($eval)
    {
      eval { $check->() };
      die "Required $mod @{[ $ver || 'undef' ]}, missing" if $@;
    }
    else
    {
      $check->();
    }
    # note Test::Alien::Build#alienfile_skip_if_missing_prereqs does a regex
    # on this diagnostic, so if you change it here, change it there too.
    die "Required $mod $ver, have @{[ $mod->VERSION || 0 ]}" if $ver && ! $mod->VERSION($ver);

    # allow for requires on Alien::Build or Alien::Base
    next if $mod eq 'Alien::Build';
    next if $mod eq 'Alien::Base';

    if($mod->can('bin_dir'))
    {
      push @{ $self->{bin_dir} }, $mod->bin_dir;
    }

    if(($mod->can('runtime_prop') && $mod->runtime_prop)
    || ($mod->isa('Alien::Base')  && $mod->install_type('share')))
    {
      for my $dir (qw(lib share)) {
          my $path = _path($mod->dist_dir)->child("$dir/pkgconfig");
          if(-d $path)
          {
            push @{ $self->{pkg_config_path} }, $path->stringify;
          }
      }
      my $path = _path($mod->dist_dir)->child('share/aclocal');
      if(-d $path)
      {
        $path = "$path";
        if($^O eq 'MSWin32')
        {
          # convert to MSYS path
          $path =~ s{^([a-z]):}{/$1/}i;
        }
        push @{ $self->{aclocal_path} }, $path;
      }
    }

    # sufficiently new Autotools have a aclocal_dir which will
    # give us the directories we need.
    if($mod eq 'Alien::Autotools' && $mod->can('aclocal_dir'))
    {
      push @{ $self->{aclocal_path} }, $mod->aclocal_dir;
    }

    if($mod->can('alien_helper'))
    {
      my $helpers = $mod->alien_helper;
      foreach my $name (sort keys %$helpers)
      {
        my $code = $helpers->{$name};
        $self->meta->interpolator->replace_helper($name => $code);
      }
    }

  }
  1;
}

sub _call_hook
{
  my $self = shift;

  local $ENV{PATH} = $ENV{PATH};
  unshift @PATH, @{ $self->{bin_dir} };

  local $ENV{PKG_CONFIG_PATH} = $ENV{PKG_CONFIG_PATH};
  unshift @PKG_CONFIG_PATH, @{ $self->{pkg_config_path} };

  local $ENV{ACLOCAL_PATH} = $ENV{ACLOCAL_PATH};
  # autoconf uses MSYS paths, even for the ACLOCAL_PATH environment variable, so we can't use Env for this.
  {
    my @path;
    @path = split ':', $ENV{ACLOCAL_PATH} if defined $ENV{ACLOCAL_PATH};
    unshift @path, @{ $self->{aclocal_path} };
    $ENV{ACLOCAL_PATH} = join ':', @path;
  }

  my $config = ref($_[0]) eq 'HASH' ? shift : {};
  my($name, @args) = @_;

  local $self->{hook_prop} = {};

  $self->meta->call_hook( $config, $name => $self, @args );
}


sub probe
{
  my($self) = @_;
  local $CWD = $self->root;
  my $dir;

  my $env = $self->_call_hook('override');
  my $type;
  my $error;

  $env = '' if $env eq 'default';

  if($env eq 'share')
  {
    $type = 'share';
  }
  else
  {
    $type = eval {
      $self->_call_hook(
        {
          before   => sub {
            $dir = Alien::Build::TempDir->new($self, "probe");
            $CWD = "$dir";
          },
          after    => sub {
            $CWD = $self->root;
          },
          ok       => 'system',
          continue => sub { $_[0] ne 'system' },
        },
        'probe',
      );
    };
    $error = $@;
    $type = 'share' unless defined $type;
  }

  if($error)
  {
    if($env eq 'system')
    {
      die $error;
    }
    $self->log("error in probe (will do a share install): $@");
    $self->log("Don't panic, we will attempt a share build from source if possible.");
    $self->log("Do not file a bug unless you expected a system install to succeed.");
    $type = 'share';
  }

  if($env && $env ne $type)
  {
    die "requested $env install not available";
  }

  if($type !~ /^(system|share)$/)
  {
    Carp::croak "probe hook returned something other than system or share: $type";
  }

  if($type eq 'share' && (!$self->meta_prop->{network}) && (!$self->meta_prop->{local_source}))
  {
    $self->log("install type share requested or detected, but network fetch is turned off");
    $self->log("see https://metacpan.org/pod/Alien::Build::Manual::FAQ#Network-fetch-is-turned-off");
    Carp::croak "network fetch is turned off";
  }

  $self->runtime_prop->{install_type} = $type;

  $type;
}


sub download
{
  my($self) = @_;

  return $self unless $self->install_type eq 'share';
  return $self if $self->install_prop->{complete}->{download};

  if($self->meta->has_hook('download'))
  {
    my $tmp;
    local $CWD;
    my $valid = 0;

    $self->_call_hook(
      {
        before => sub {
          $tmp = Alien::Build::TempDir->new($self, "download");
          $CWD = "$tmp";
        },
        verify => sub {
          my @list = grep { $_->basename !~ /^\./, } _path('.')->children;

          my $count = scalar @list;

          if($count == 0)
          {
            die "no files downloaded";
          }
          elsif($count == 1)
          {
            my($archive) = $list[0];
            if(-d $archive)
            {
              $self->log("single dir, assuming directory");
            }
            else
            {
              $self->log("single file, assuming archive");
            }
            $self->install_prop->{download} = $archive->absolute->stringify;
            $self->install_prop->{complete}->{download} = 1;
            $valid = 1;
          }
          else
          {
            $self->log("multiple files, assuming directory");
            $self->install_prop->{complete}->{download} = 1;
            $self->install_prop->{download} = _path('.')->absolute->stringify;
            $valid = 1;
          }
        },
        after  => sub {
          $CWD = $self->root;
        },
      },
      'download',
    );

    return $self if $valid;
  }
  else
  {
    # This will call the default download hook
    # defined in Core::Download since the recipe
    # does not provide a download hook
    return $self->_call_hook('download');
  }

  die "download failed";
}


sub fetch
{
  my($self, $url) = @_;
  $self->_call_hook( 'fetch' => $url );
}


sub decode
{
  my($self, $res) = @_;
  $self->_call_hook( decode => $res );
}


sub prefer
{
  my($self, $res) = @_;
  $self->_call_hook( prefer => $res );
}


sub extract
{
  my($self, $archive) = @_;

  $archive ||= $self->install_prop->{download};

  unless(defined $archive)
  {
    die "tried to call extract before download";
  }

  my $nick_name = 'build';

  if($self->meta_prop->{out_of_source})
  {
    $nick_name = 'extract';
    my $extract = $self->install_prop->{extract};
    return $extract if defined $extract && -d $extract;
  }

  my $tmp;
  local $CWD;
  my $ret;

  $self->_call_hook({

    before => sub {
      # called build instead of extract, because this
      # will be used for the build step, and technically
      # extract is a substage of build anyway.
      $tmp = Alien::Build::TempDir->new($self, $nick_name);
      $CWD = "$tmp";
    },
    verify => sub {

      my $path = '.';
      if($self->meta_prop->{out_of_source} && $self->install_prop->{extract})
      {
        $path = $self->install_prop->{extract};
      }

      my @list = grep { $_->basename !~ /^\./ && $_->basename ne 'pax_global_header' } _path($path)->children;

      my $count = scalar @list;

      if($count == 0)
      {
        die "no files extracted";
      }
      elsif($count == 1 && -d $list[0])
      {
        $ret = $list[0]->absolute->stringify;
      }
      else
      {
        $ret = "$tmp";
      }

    },
    after => sub {
      $CWD = $self->root;
    },

  }, 'extract', $archive);

  $self->install_prop->{extract} ||= $ret;
  $ret ? $ret : ();
}


sub build
{
  my($self) = @_;

  # save the evironment, in case some plugins decide
  # to alter it.  Or us!  See just a few lines below.
  local %ENV = %ENV;

  my $stage = _path($self->install_prop->{stage});
  $stage->mkpath;

  my $tmp;

  if($self->install_type eq 'share')
  {
    foreach my $suffix ('', '_ffi')
    {
      local $CWD;
      delete $ENV{DESTDIR} unless $self->meta_prop->{destdir};

      my %env_meta = %{ $self->meta_prop   ->{env} || {} };
      my %env_inst = %{ $self->install_prop->{env} || {} };

      if($self->meta_prop->{env_interpolate})
      {
        foreach my $key (keys %env_meta)
        {
          $env_meta{$key} = $self->meta->interpolator->interpolate($env_meta{$key});
        }
      }

      %ENV = (%ENV, %env_meta);
      %ENV = (%ENV, %env_inst);

      my $destdir;

      $self->_call_hook(
      {
        before => sub {
          if($self->meta_prop->{out_of_source})
          {
            $self->extract;
            $CWD = $tmp = Alien::Build::TempDir->new($self, 'build');
          }
          else
          {
            $CWD = $tmp = $self->extract;
          }
          if($self->meta_prop->{destdir})
          {
            $destdir = Alien::Build::TempDir->new($self, 'destdir');
            $ENV{DESTDIR} = "$destdir";
          }
          $self->_call_hook({ all => 1 }, "patch${suffix}");
        },
        after => sub {
          $destdir = "$destdir" if $destdir;
        },
      }, "build${suffix}");

      $self->install_prop->{"_ab_build@{[ $suffix || '_share' ]}"} = "$CWD";

      $self->_call_hook("gather@{[ $suffix || '_share' ]}");
    }
  }

  elsif($self->install_type eq 'system')
  {
    local $CWD = $self->root;
    my $dir;

    $self->_call_hook(
      {
        before => sub {
          $dir = Alien::Build::TempDir->new($self, "gather");
          $CWD = "$dir";
        },
        after  => sub {
          $CWD = $self->root;
        },
      },
      'gather_system',
    );

    $self->install_prop->{finished} = 1;
    $self->install_prop->{complete}->{gather_system} = 1;
  }

  $self;
}


sub test
{
  my($self) = @_;

  if($self->install_type eq 'share')
  {
    foreach my $suffix ('_share', '_ffi')
    {
      if($self->meta->has_hook("test$suffix"))
      {
        my $dir = $self->install_prop->{"_ab_build$suffix"};
        Carp::croak("no build directory to run tests") unless $dir && -d $dir;
        local $CWD = $dir;
        $self->_call_hook("test$suffix");
      }
    }
  }
  else
  {
    if($self->meta->has_hook("test_system"))
    {
      my $dir = Alien::Build::TempDir->new($self, "test");
      local $CWD = "$dir";
      $self->_call_hook("test_system");
    }
  }

}


sub clean_install
{
  my($self) = @_;
  if($self->install_type eq 'share')
  {
    $self->_call_hook("clean_install");
  }
}


sub system
{
  my($self, $command, @args) = @_;

  my $prop = $self->_command_prop;

  ($command, @args) = map {
    $self->meta->interpolator->interpolate($_, $prop)
  } ($command, @args);

  $self->log("+ $command @args");

  scalar @args
    ? system $command, @args
    : system $command;
}


sub log
{
  my(undef, $message) = @_;
  my $caller = caller;
  chomp $message;
  print "$caller> $message\n";
}


{
  my %meta;

  sub meta
  {
    my($class) = @_;
    $class = ref $class if ref $class;
    $meta{$class} ||= Alien::Build::Meta->new( class => $class );
  }
}

package Alien::Build::Meta;

our @CARP_NOT = qw( alienfile );

sub new
{
  my($class, %args) = @_;
  my $self = bless {
    phase => 'any',
    build_suffix => '',
    require => {
      any    => {},
      share  => {},
      system => {},
    },
    around => {},
    prop => {},
    %args,
  }, $class;
  $self;
}


sub prop
{
  shift->{prop};
}

sub filename
{
  my($self, $new) = @_;
  $self->{filename} = $new if defined $new;
  $self->{filename};
}


sub add_requires
{
  my $self = shift;
  my $phase = shift;
  while(@_)
  {
    my $module = shift;
    my $version = shift;
    my $old = $self->{require}->{$phase}->{$module};
    if((!defined $old) || $version > $old)
    { $self->{require}->{$phase}->{$module} = $version }
  }
  $self;
}


sub interpolator
{
  my($self, $new) = @_;
  if(defined $new)
  {
    if(defined $self->{intr})
    {
      Carp::croak "tried to set interpolator twice";
    }
    if(ref $new)
    {
      $self->{intr} = $new;
    }
    else
    {
      $self->{intr} = $new->new;
    }
  }
  elsif(!defined $self->{intr})
  {
    require Alien::Build::Interpolate::Default;
    $self->{intr} = Alien::Build::Interpolate::Default->new;
  }
  $self->{intr};
}


sub has_hook
{
  my($self, $name) = @_;
  defined $self->{hook}->{$name};
}


sub _instr
{
  my($self, $name, $instr) = @_;
  if(ref($instr) eq 'CODE')
  {
    return $instr;
  }
  elsif(ref($instr) eq 'ARRAY')
  {
    my %phase = (
      download      => 'share',
      fetch         => 'share',
      decode        => 'share',
      prefer        => 'share',
      extract       => 'share',
      patch         => 'share',
      patch_ffi     => 'share',
      build         => 'share',
      build_ffi     => 'share',
      stage         => 'share',
      gather_ffi    => 'share',
      gather_share  => 'share',
      gather_system => 'system',
      test_ffi      => 'share',
      test_share    => 'share',
      test_system   => 'system',
    );
    require Alien::Build::CommandSequence;
    my $seq = Alien::Build::CommandSequence->new(@$instr);
    $seq->apply_requirements($self, $phase{$name} || 'any');
    return $seq;
  }
  else
  {
    Carp::croak "type not supported as a hook";
  }
}

sub register_hook
{
  my($self, $name, $instr) = @_;
  push @{ $self->{hook}->{$name} }, _instr $self, $name, $instr;
  $self;
}


sub default_hook
{
  my($self, $name, $instr) = @_;
  $self->{default_hook}->{$name} = _instr $self, $name, $instr;
  $self;
}


sub around_hook
{
  my($self, $name, $code) = @_;
  if(my $old = $self->{around}->{$name})
  {
    # this is the craziest shit I have ever
    # come up with.
    $self->{around}->{$name} = sub {
      my $orig = shift;
      $code->(sub { $old->($orig, @_) }, @_);
    };
  }
  else
  {
    $self->{around}->{$name} = $code;
  }
}

sub after_hook
{
  my($self, $name, $code) = @_;
  $self->around_hook(
    $name => sub {
      my $orig = shift;
      my $ret = $orig->(@_);
      $code->(@_);
      $ret;
    }
  );
}

sub before_hook
{
  my($self, $name, $code) = @_;
  $self->around_hook(
    $name => sub {
      my $orig = shift;
      $code->(@_);
      my $ret = $orig->(@_);
      $ret;
    }
  );
}


sub call_hook
{
  my $self = shift;
  my %args = ref $_[0] ? %{ shift() } : ();
  my($name, @args) = @_;
  my $error;

  my @hooks = @{ $self->{hook}->{$name} || []};

  if(@hooks == 0)
  {
    if(defined $self->{default_hook}->{$name})
    {
      @hooks = ($self->{default_hook}->{$name})
    }
    elsif(!$args{all})
    {
      Carp::croak "No hooks registered for $name";
    }
  }

  my $value;

  foreach my $hook (@hooks)
  {
    if(eval { $args[0]->isa('Alien::Build') })
    {
      %{ $args[0]->{hook_prop} } = (
        name => $name,
      );
    }

    my $wrapper = $self->{around}->{$name} || sub { my $code = shift; $code->(@_) };
    my $value;
    $args{before}->() if $args{before};
    if(ref($hook) eq 'CODE')
    {
      $value = eval {
        my $value = $wrapper->(sub { $hook->(@_) }, @args);
        $args{verify}->('code') if $args{verify};
        $value;
      };
    }
    else
    {
      $value = $wrapper->(sub {
        eval {
          $hook->execute(@_);
          $args{verify}->('command') if $args{verify};
        };
        defined $args{ok} ? $args{ok} : 1;
      }, @args);
    }
    $error = $@;
    $args{after}->() if $args{after};
    if($args{all})
    {
      die if $error;
    }
    else
    {
      next if $error;
      next if $args{continue} && $args{continue}->($value);
      return $value;
    }
  }

  die $error if $error && ! $args{all};

  $value;
}


sub apply_plugin
{
  my($self, $name, @args) = @_;

  my $class;
  my $pm;
  my $found;

  if($name =~ /^=(.*)$/)
  {
    $class = $1;
    $pm    = "$class.pm";
    $pm    =~ s!::!/!g;
    $found = 1;
  }

  if($name !~ /::/ && !$found)
  {
    foreach my $inc (@INC)
    {
      # TODO: allow negotiators to work with @INC hooks
      next if ref $inc;
      my $file = Path::Tiny->new("$inc/Alien/Build/Plugin/$name/Negotiate.pm");
      if(-r $file)
      {
        $class = "Alien::Build::Plugin::${name}::Negotiate";
        $pm    = "Alien/Build/Plugin/$name/Negotiate.pm";
        $found = 1;
        last;
      }
    }
  }

  unless($found)
  {
    $class = "Alien::Build::Plugin::$name";
    $pm    = "Alien/Build/Plugin/$name.pm";
    $pm    =~ s{::}{/}g;
  }

  require $pm unless $class->can('new');
  my $plugin = $class->new(@args);
  $plugin->init($self);
  $self;
}

package Alien::Build::TempDir;

use Path::Tiny qw( path );
use overload '""' => sub { shift->as_string };
use File::Temp qw( tempdir );

sub new
{
  my($class, $build, $name) = @_;
  my $root = $build->install_prop->{root};
  path($root)->mkpath unless -d $root;
  bless {
    dir => path(tempdir( "${name}_XXXX", DIR => $root)),
  }, $class;
}

sub as_string
{
  shift->{dir}->stringify;
}

sub DESTROY
{
  my($self) = @_;
  if(-d $self->{dir} && $self->{dir}->children == 0)
  {
    rmdir($self->{dir}) || warn "unable to remove @{[ $self->{dir} ]} $!";
  }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build - Build external dependencies for use in CPAN

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 my $build = Alien::Build->load('./alienfile');
 $build->load_requires('configure');
 $build->set_prefix('/usr/local');
 $build->set_stage('/foo/mystage');  # needs to be absolute
 $build->load_requires($build->install_type);
 $build->download;
 $build->build;
 # files are now in /foo/mystage, it is your job (or
 # ExtUtils::MakeMaker, Module::Build, etc) to copy
 # those files into /usr/local

=head1 DESCRIPTION

This module provides tools for building external (non-CPAN) dependencies
for CPAN.  It is mainly designed to be used at install time of a CPAN
client, and work closely with L<Alien::Base> which is used at runtime.

This is the detailed documentation for the L<Alien::Build> class.
If you are starting out you probably want to do so from one of these documents:

=over 4

=item L<Alien::Build::Manual::Alien>

A broad overview of C<Alien-Build> and its ecosystem.

=item L<Alien::Build::Manual::AlienUser>

For users of an C<Alien::libfoo> that is implemented using L<Alien::Base>.
(The developer of C<Alien::libfoo> I<should> provide the documentation
necessary, but if not, this is the place to start).

=item L<Alien::Build::Manual::AlienAuthor>

If you are writing your own L<Alien> based on L<Alien::Build> and L<Alien::Base>.

=item L<Alien::Build::Manual::FAQ>

If you have a common question that has already been answered, like
"How do I use L<alienfile> with some build system".

=item L<Alien::Build::Manual::PluginAuthor>

This is for the brave souls who want to write plugins that will work with
L<Alien::Build> + L<alienfile>.

=back

Note that you will not usually create a L<Alien::Build> instance
directly, but rather be using a thin installer layer, such as
L<Alien::Build::MM> (for use with L<ExtUtils::MakeMaker>) or
L<Alien::Build::MB> (for use with L<Module::Build>).  One of the
goals of this project is to remain installer agnostic.

=head1 CONSTRUCTORS

=head2 new

 my $build = Alien::Build->new;

This creates a new empty instance of L<Alien::Build>.  Normally you will
want to use C<load> below to create an instance of L<Alien::Build> from
an L<alienfile> recipe.

=head2 load

 my $build = Alien::Build->load($alienfile);

This creates an L<Alien::Build> instance with the given L<alienfile>
recipe.

=head2 resume

 my $build = Alien::Build->resume($alienfile, $root);

Load a checkpointed L<Alien::Build> instance.  You will need the original
L<alienfile> and the build root (usually C<_alien>), and a build that
had been properly checkpointed using the C<checkpoint> method below.

=head1 PROPERTIES

There are three main properties for L<Alien::Build>.  There are a number
of properties documented here with a specific usage.  Note that these
properties may need to be serialized into something primitive like JSON
that does not support: regular expressions, code references of blessed
objects.

If you are writing a plugin (L<Alien::Build::Plugin>) you should use a
prefix like "plugin_I<name>" (where I<name> is the name of your plugin)
so that it does not interfere with other plugin or future versions of
L<Alien::Build>.  For example, if you were writing
C<Alien::Build::Plugin::Fetch::NewProtocol>, please use the prefix
C<plugin_fetch_newprotocol>:

 sub init
 {
   my($self, $meta) = @_;
 
   $meta->prop( plugin_fetch_newprotocol_foo => 'some value' );
  
   $meta->register_hook(
     some_hook => sub {
       my($build) = @_;
       $build->install_prop->{plugin_fetch_newprotocol_bar} = 'some other value';
       $build->runtime_prop->{plugin_fetch_newprotocol_baz} = 'and another value';
     }
   );
 }

If you are writing a L<alienfile> recipe please use the prefix C<my_>:

 use alienfile;
 
 meta_prop->{my_foo} = 'some value';
 
 probe sub {
   my($build) = @_;
   $build->install_prop->{my_bar} = 'some other value';
   $build->install_prop->{my_baz} = 'and another value';
 };

Any property may be used from a command:

 probe [ 'some command %{.meta.plugin_fetch_newprotocol_foo}' ];
 probe [ 'some command %{.install.plugin_fetch_newprotocol_bar}' ];
 probe [ 'some command %{.runtime.plugin_fetch_newprotocol_baz}' ];
 probe [ 'some command %{.meta.my_foo}' ];
 probe [ 'some command %{.install.my_bar}' ];
 probe [ 'some command %{.runtime.my_baz}' ];

=head2 meta_prop

 my $href = $build->meta_prop;
 my $href = Alien::Build->meta_prop;

Meta properties have to do with the recipe itself, and not any particular
instance that probes or builds that recipe.  Meta properties can be changed
from within an L<alienfile> using the C<meta_prop> directive, or from
a plugin from its C<init> method (though should NOT be modified from any
hooks registered within that C<init> method).  This is not strictly enforced,
but if you do not follow this rule your recipe will likely be broken.

=over

=item arch

This is a hint to an installer like L<Alien::Build::MM> or L<Alien::Build::MB>,
that the library or tool contains architecture dependent files and so should
be stored in an architecture dependent location.  If not specified by your
L<alienfile> then it will be set to true.

=item destdir

Use the C<DESTDIR> environment variable to stage your install before
copying the files into C<blib>.  This is the preferred method of
installing libraries because it improves reliability.  This technique
is supported by C<autoconf> and others.

=item destdir_filter

Regular expression for the files that should be copied from the C<DESTDIR>
into the stage directory.  If not defined, then all files will be copied.

=item destdir_ffi_filter

Same as C<destdir_filter> except applies to C<build_ffi> instead of C<build>.

=item env

Environment variables to override during the build stage.

=item env_interpolate

Environment variable values will be interpolated with helpers.  Example:

 meta->prop->{env_interpolate} = 1;
 meta->prop->{env}->{PERL} = '%{perl}';

=item local_source

Set to true if source code package is available locally.  (that is not fetched
over the internet).  This is computed by default based on the C<start_url>
property.  Can be set by an L<alienfile> or plugin.

=item platform

Hash reference.  Contains information about the platform beyond just C<$^O>.

=over 4

=item compiler_type

Refers to the type of flags that the compiler accepts.  May be expanded in the
future, but for now, will be one of:

=over 4

=item microsoft

On Windows when using Microsoft Visual C++

=item unix

Virtually everything else, including gcc on windows.

=back

The main difference is that with Visual C++ C<-LIBPATH> should be used instead
of C<-L>, and static libraries should have the C<.LIB> suffix instead of C<.a>.

=item system_type

C<$^O> is frequently good enough to make platform specific logic in your
L<alienfile>, this handles the case when $^O can cover platforms that provide
multiple environments that Perl might run under.  The main example is windows,
but others may be added in the future.

=over 4

=item unix

=item vms

=item windows-activestate

=item windows-microsoft

=item windows-mingw

=item windows-strawberry

=item windows-unknown

=back

Note that C<cygwin> and C<msys> are considered C<unix> even though they run
on windows!

=back

=item out_of_source

Build in a different directory from the where the source code is stored.
In autoconf this is referred to as a "VPATH" build.  Everyone else calls this
an "out-of-source" build.  When this property is true, instead of extracting
to the source build root, the downloaded source will be extracted to an source
extraction directory and the source build root will be empty.  You can use the
C<extract> install property to get the location of the extracted source.

=item network

True if a network fetch is available.  This should NOT be set by an L<alienfile>
or plugin.  This is computed based on the C<ALIEN_INSTALL_NETWORK> environment
variables.

=item start_url

The default or start URL used by fetch plugins.

=back

=head2 install_prop

 my $href = $build->install_prop;

Install properties are used during the install phase (either
under C<share> or C<system> install).  They are remembered for
the entire install phase, but not kept around during the runtime
phase.  Thus they cannot be accessed from your L<Alien::Base>
based module.

=over

=item autoconf_prefix

The prefix as understood by autoconf.  This is only different on Windows
Where MSYS is used and paths like C<C:/foo> are  represented as C</C/foo>
which are understood by the MSYS tools, but not by Perl.  You should
only use this if you are using L<Alien::Build::Plugin::Autoconf> in
your L<alienfile>.

=item download

The location of the downloaded archive (tar.gz, or similar) or directory.

=item env

Environment variables to override during the build stage.

=item extract

The location of the last source extraction.  For a "out-of-source" build
(see the C<out_of_source> meta property above), this will only be set once.
For other types of builds, the source code may be extracted multiple times,
and thus this property may change.

=item old

Hash containing information on a previously installed Alien of the same
name, if available.  This may be useful in cases where you want to
reuse the previous install if it is still sufficient.

=over 4

=item prefix

The prefix for the previous install.  Versions prior to 1.42 unfortunately
had this in typo form of C<preifx>.

=item runtime

The runtime properties from the previous install.

=back

=item patch

Directory with patches.

=item prefix

The install time prefix.  Under a C<destdir> install this is the
same as the runtime or final install location.  Under a non-C<destdir>
install this is the C<stage> directory (usually the appropriate
share directory under C<blib>).

=item root

The build root directory.  This will be an absolute path.  It is the
absolute form of C<./_alien> by default.

=item stage

The stage directory where files will be copied.  This is usually the
root of the blib share directory.

=back

=head2 runtime_prop

 my $href = $build->runtime_prop;

Runtime properties are used during the install and runtime phases
(either under C<share> or C<system> install).  This should include
anything that you will need to know to use the library or tool
during runtime, and shouldn't include anything that is no longer
relevant once the install process is complete.

=over 4

=item alien_build_version

The version of L<Alien::Build> used to install the library or tool.

=item alt

Alternate configurations.  If the alienized package has multiple
libraries this could be used to store the different compiler or
linker flags for each library.

=item cflags

The compiler flags

=item cflags_static

The static compiler flags

=item command

The command name for tools where the name my differ from platform to
platform.  For example, the GNU version of make is usually C<make> in
Linux and C<gmake> on FreeBSD.

=item ffi_name

The name DLL or shared object "name" to use when searching for dynamic
libraries at runtime.  This is passed into L<FFI::CheckLib>, so if
your library is something like C<libarchive.so> or C<archive.dll> you
would set this to C<archive>.  This may be a string or an array of
strings.

=item ffi_checklib

This property contains two sub properties:

=over 4

=item share

 $build->runtime_prop->{ffi_checklib}->{share} = [ ... ];

Array of additional L<FFI::CheckLib> flags to pass in to C<find_lib>
for a C<share> install.

=item system

Array of additional L<FFI::CheckLib> flags to pass in to C<find_lib>
for a C<system> install.

Among other things, useful for specifying the C<try_linker_script>
flag:

 $build->runtime_prop->{ffi_checklib}->{system} = [ try_linker_script => 1 ];

=back

=item install_type

The install type.  Is one of:

=over 4

=item system

For when the library or tool is provided by the operating system, can be
detected by L<Alien::Build>, and is considered satisfactory by the
C<alienfile> recipe.

=item share

For when a system install is not possible, the library source will be
downloaded from the internet or retrieved in another appropriate fashion
and built.

=back

=item libs

The library flags

=item libs_static

The static library flags

=item perl_module_version

The version of the Perl module used to install the alien (if available).
For example if L<Alien::curl> is installing C<libcurl> this would be the
version of L<Alien::curl> used during the install step.

=item prefix

The final install root.  This is usually they share directory.

=item version

The version of the library or tool

=back

=head2 hook_prop

 my $href = $build->hook_prop;

Hook properties are for the currently running (if any) hook.  They are
used only during the execution of each hook and are discarded after.
If no hook is currently running then C<hook_prop> will return C<undef>.

=over 4

=item name

The name of the currently running hook.

=item version (probe)

Probe and PkgConfig plugins I<may> set this property indicating the
version of the alienized package.  Not all plugins and configurations
may be able to provide this.

=back

=head1 METHODS

=head2 checkpoint

 $build->checkpoint;

Save any install or runtime properties so that they can be reloaded on
a subsequent run in a separate process.  This is useful if your build
needs to be done in multiple stages from a C<Makefile>, such as with
L<ExtUtils::MakeMaker>.  Once checkpointed you can use the C<resume>
constructor (documented above) to resume the probe/build/install]
process.

=head2 root

 my $dir = $build->root;

This is just a shortcut for:

 my $root = $build->install_prop->{root};

Except that it will be created if it does not already exist.

=head2 install_type

 my $type = $build->install_type;

This will return the install type.  (See the like named install property
above for details).  This method will call C<probe> if it has not already
been called.

=head2 set_prefix

 $build->set_prefix($prefix);

Set the final (unstaged) prefix.  This is normally only called by L<Alien::Build::MM>
and similar modules.  It is not intended for use from plugins or from an L<alienfile>.

=head2 set_stage

 $build->set_stage($dir);

Sets the stage directory.  This is normally only called by L<Alien::Build::MM>
and similar modules.  It is not intended for use from plugins or from an L<alienfile>.

=head2 requires

 my $hash = $build->requires($phase);

Returns a hash reference of the modules required for the given phase.  Phases
include:

=over 4

=item configure

These modules must already be available when the L<alienfile> is read.

=item any

These modules are used during either a C<system> or C<share> install.

=item share

These modules are used during the build phase of a C<share> install.

=item system

These modules are used during the build phase of a C<system> install.

=back

=head2 load_requires

 $build->load_requires($phase);

This loads the appropriate modules for the given phase (see C<requires> above
for a description of the phases).

=head2 probe

 my $install_type = $build->probe;

Attempts to determine if the operating system has the library or
tool already installed.  If so, then the string C<system> will
be returned and a system install will be performed.  If not,
then the string C<share> will be installed and the tool or
library will be downloaded and built from source.

If the environment variable C<ALIEN_INSTALL_TYPE> is set, then that
will force a specific type of install.  If the detection logic
cannot accommodate the install type requested then it will fail with
an exception.

=head2 download

 $build->download;

Download the source, usually as a tarball, usually from the internet.

Under a C<system> install this does not do anything.

=head2 fetch

 my $res = $build->fetch;
 my $res = $build->fetch($url);

Fetch a resource using the fetch hook.  Returns the same hash structure
described below in the hook documentation.

=head2 decode

 my $decoded_res = $build->decode($res);

Decode the HTML or file listing returned by C<fetch>.  Returns the same
hash structure described below in the hook documentation.

=head2 prefer

 my $sorted_res = $build->prefer($res);

Filter and sort candidates.  The preferred candidate will be returned first in the list.
The worst candidate will be returned last.  Returns the same hash structure described
below in the hook documentation.

=head2 extract

 my $dir = $build->extract;
 my $dir = $build->extract($archive);

Extracts the given archive into a fresh directory.  This is normally called internally
to L<Alien::Build>, and for normal usage is not needed from a plugin or L<alienfile>.

=head2 build

 $build->build;

Run the build step.  It is expected that C<probe> and C<download>
have already been performed.  What it actually does depends on the
type of install:

=over 4

=item share

The source is extracted, and built as determined by the L<alienfile>
recipe.  If there is a C<gather_share> that will be executed last.

=item system

The C<gather_system> hook will be executed.

=back

=head2 test

 $build->test;

Run the test phase

=head2 clean_install

 $build->clean_install

Clean files from the final install location.  The default implementation removes all
files recursively except for the C<_alien> directory.  This is helpful when you have
an old install with files that may break the new build.

For a non-share install this doesn't do anything.

=head2 system

 $build->system($command);
 $build->system($command, @args);

Interpolates the command and arguments and run the results using
the Perl C<system> command.

=head2 log

 $build->log($message);

Send a message to the log.  By default this prints to C<STDOUT>.

=head2 meta

 my $meta = Alien::Build->meta;
 my $meta = $build->meta;

Returns the meta object for your L<Alien::Build> class or instance.  The
meta object is a way to manipulate the recipe, and so any changes to the
meta object should be made before the C<probe>, C<download> or C<build> steps.

=head1 META METHODS

=head2 prop

 my $href = $build->meta->prop;

Meta properties.  This is the same as calling C<meta_prop> on
the class or L<Alien::Build> instance.

=head2 add_requires

 Alien::Build->meta->add_requires($phase, $module => $version, ...);

Add the requirement to the given phase.  Phase should be one of:

=over 4

=item configure

=item any

=item share

=item system

=back

=head2 interpolator

 my $interpolator = $build->meta->interpolator;
 my $interpolator = Alien::Build->interpolator;

Returns the L<Alien::Build::Interpolate> instance for the L<Alien::Build> class.

=head2 has_hook

 my $bool = $build->meta->has_hook($name);
 my $bool = Alien::Build->has_hook($name);

Returns if there is a usable hook registered with the given name.

=head2 register_hook

 $build->meta->register_hook($name, $instructions);
 Alien::Build->meta->register_hook($name, $instructions);

Register a hook with the given name.  C<$instruction> should be either
a code reference, or a command sequence, which is an array reference.

=head2 default_hook

 $build->meta->default_hook($name, $instructions);
 Alien::Build->meta->default_hook($name, $instructions);

Register a default hook, which will be used if the L<alienfile> does not
register its own hook with that name.

=head2 around_hook

 $build->meta->around_hook($hook, $code);
 Alien::Build->meta->around_hook($name, $code);

Wrap the given hook with a code reference.  This is similar to a L<Moose>
method modifier, except that it wraps around the given hook instead of
a method.  For example, this will add a probe system requirement:

 $build->meta->around_hook(
   probe => sub {
     my $orig = shift;
     my $build = shift;
     my $type = $orig->($build, @_);
     return $type unless $type eq 'system';
     # also require a configuration file
     if(-f '/etc/foo.conf')
     {
       return 'system';
     }
     else
     {
       return 'share';
     }
   },
 );

=head2 apply_plugin

 Alien::Build->meta->apply_plugin($name);
 Alien::Build->meta->apply_plugin($name, @args);

Apply the given plugin with the given arguments.

=head1 ENVIRONMENT

L<Alien::Build> responds to these environment variables:

=over 4

=item ALIEN_INSTALL_NETWORK

If set to true (the default), then network fetch will be allowed.  If set to
false, then network fetch will not be allowed.

What constitutes a local vs. network fetch is determined based on the C<start_url>
and C<local_source> meta properties.  An L<alienfile> or plugin C<could> override
this detection (possibly inappropriately), so this variable is not a substitute
for properly auditing of Perl modules for environments that require that.

=item ALIEN_INSTALL_TYPE

If set to C<share> or C<system>, it will override the system detection logic.
If set to C<default>, it will use the default setting for the L<alienfile>.
The behavior of other values is undefined.

Although the recommended way for a consumer to use an L<Alien::Base> based L<Alien>
is to declare it as a static configure and build-time dependency, some consumers
may prefer to fallback on using an L<Alien> only when the consumer itself cannot
detect the necessary package. In some cases the consumer may want the user to opt-in
to using an L<Alien> before requiring it.

To keep the interface consistent among Aliens, the consumer of the fallback opt-in
L<Alien> may fallback on the L<Alien> if the environment variable C<ALIEN_INSTALL_TYPE>
is set to any value. The rationale is that by setting this environment variable the
user is aware that L<Alien> modules may be installed and have indicated consent.
The actual implementation of this, by its nature would have to be in the consuming
CPAN module.

=item ALIEN_BUILD_RC

Perl source file which can override some global defaults for L<Alien::Build>,
by, for example, setting preload and postload plugins.

=item ALIEN_BUILD_PKG_CONFIG

Override the logic in L<Alien::Build::Plugin::PkgConfig::Negotiate> which
chooses the best C<pkg-config> plugin.

=item ALIEN_BUILD_PRELOAD

semicolon separated list of plugins to automatically load before parsing
your L<alienfile>.

=item ALIEN_BUILD_POSTLOAD

semicolon separated list of plugins to automatically load after parsing
your L<alienfile>.

=item DESTDIR

This environment variable will be manipulated during a destdir install.

=item PKG_CONFIG

This environment variable can be used to override the program name for C<pkg-config>
when using the command line plugin: L<Alien::Build::Plugin::PkgConfig::CommandLine>.

=item ftp_proxy, all_proxy

If these environment variables are set, it may influence the Download negotiation
plugin L<Alien::Build::Plugin::Downaload::Negotiate>.  Other proxy variables may
be used by some Fetch plugins, if they support it.

=back

=head1 SUPPORT

The intent of the C<Alien-Build> team is to support as best as possible
all Perls from 5.8.1 to the latest production version.  So long as they
are also supported by the Perl toolchain.

Please feel encouraged to report issues that you encounter to the
project GitHub Issue tracker:

=over 4

=item L<https://github.com/Perl5-Alien/Alien-Build/issues>

=back

Better if you can fix the issue yourself, please feel encouraged to open
pull-request on the project GitHub:

=over 4

=item L<https://github.com/Perl5-Alien/Alien-Build/pulls>

=back

If you are confounded and have questions, join us on the C<#native>
channel on irc.perl.org.  The C<Alien-Build> developers frequent this
channel and can probably help point you in the right direction.  If you
don't have an IRC client handy, you can use this web interface:

=over 4

=item L<https://chat.mibbit.com/?channel=%23native&server=irc.perl.org>

=back

=head1 SEE ALSO

L<Alien::Build::Manual::AlienAuthor>,
L<Alien::Build::Manual::AlienUser>,
L<Alien::Build::Manual::Contributing>,
L<Alien::Build::Manual::FAQ>,
L<Alien::Build::Manual::PluginAuthor>

L<alienfile>, L<Alien::Build::MM>, L<Alien::Build::Plugin>, L<Alien::Base>, L<Alien>

=head1 THANKS

L<Alien::Base> was originally written by Joel Berger, the rest of this project would
not have been possible without him getting the project started.  Thanks to his support
I have been able to augment the original L<Alien::Base> system with a reliable set
of tools (L<Alien::Build>, L<alienfile>, L<Test::Alien>), which make up this toolset.

The original L<Alien::Base> is still copyright (c) 2012-2017 Joel Berger.  It has
the same license as the rest of the Alien::Build and related tools distributed as
C<Alien-Build>.  Joel Berger thanked a number of people who helped in in the development
of L<Alien::Base>, in the documentation for that module.

I would also like to acknowledge the other members of the Perl5-Alien github
organization, Zakariyya Mughal (sivoais, ZMUGHAL) and mohawk (ETJ).  Also important
in the early development of L<Alien::Build> were the early adopters Chase Whitener
(genio, CAPOEIRAB, author of L<Alien::libuv>), William N. Braswell, Jr (willthechill,
WBRASWELL, author of L<Alien::JPCRE2> and L<Alien::PCRE2>) and Ahmad Fatoum (a3f,
ATHREEF, author of L<Alien::libudev> and L<Alien::LibUSB>).

=head1 AUTHOR

Author: Graham Ollis E<lt>plicease@cpan.orgE<gt>

Contributors:

Diab Jerius (DJERIUS)

Roy Storey (KIWIROY)

Ilya Pavlov

David Mertens (run4flat)

Mark Nunberg (mordy, mnunberg)

Christian Walde (Mithaldu)

Brian Wightman (MidLifeXis)

Zaki Mughal (zmughal)

mohawk (mohawk2, ETJ)

Vikas N Kumar (vikasnkumar)

Flavio Poletti (polettix)

Salvador Fandiño (salva)

Gianni Ceccarelli (dakkar)

Pavel Shaydo (zwon, trinitum)

Kang-min Liu (劉康民, gugod)

Nicholas Shipp (nshp)

Juan Julián Merelo Guervós (JJ)

Joel Berger (JBERGER)

Petr Pisar (ppisar)

Lance Wicks (LANCEW)

Ahmad Fatoum (a3f, ATHREEF)

José Joaquín Atria (JJATRIA)

Duke Leto (LETO)

Shoichi Kaji (SKAJI)

Shawn Laffan (SLAFFAN)

Paul Evans (leonerd, PEVANS)

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011-2019 by Graham Ollis.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
