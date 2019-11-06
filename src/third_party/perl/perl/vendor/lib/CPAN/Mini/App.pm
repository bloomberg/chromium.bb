use strict;
use warnings;

package CPAN::Mini::App;
{
  $CPAN::Mini::App::VERSION = '1.111008';
}

# ABSTRACT: the guts of the minicpan command


use CPAN::Mini;
use File::HomeDir;
use File::Spec;
use Getopt::Long qw(:config no_ignore_case);
use Pod::Usage 1.00;

sub _display_version {
  my $class = shift;
  no strict 'refs';
  print "minicpan",
    ($class ne 'CPAN::Mini' ? ' (from CPAN::Mini)' : q{}),
    ", powered by $class ", $class->VERSION, "\n\n";
  exit;
}


sub _validate_log_level {
  my ($class, $level) = @_;
  return $level if $level =~ /\A(?:fatal|warn|debug|info)\z/;
  die "unknown logging level: $level\n";
}

sub run {
  my ($class) = @_;

  my $minicpan = $class->initialize_minicpan;

  $minicpan->update_mirror;
}

sub initialize_minicpan {
  my ($class) = @_;

  my $version;

  my %commandline;

  my ($quiet, $debug, $log_level);

  GetOptions(
    'c|class=s'   => \$commandline{class},

    # These two options will cause the program to exit before finishing ->run
    'h|help'      => sub { pod2usage(1); },
    'v|version'   => sub { $version = 1 },

    # How noisy should we be?
    'quiet|q+'    => \$quiet,
    'qq'          => sub { $quiet = 2 },
    'debug'       => \$debug,
    'log-level=s' => \$log_level,

    'l|local=s'   => \$commandline{local},
    'r|remote=s'  => \$commandline{remote},

    'd|dirmode=s' => \$commandline{dirmode},
    'offline'     => \$commandline{offline},
    'f'           => \$commandline{force},
    'p'           => \$commandline{perl},
    'x'           => \$commandline{exact_mirror},
    't|timeout=i' => \$commandline{timeout},

    # Where to look for config not provided on the command line:
    'C|config=s'  => \$commandline{config_file},
  ) or pod2usage(2);

  die "can't mix --debug, --log-level, and --debug\n"
    if defined($quiet) + defined($debug) + defined($log_level) > 1;

  $quiet ||= 0;
  $log_level = $debug      ? 'debug'
             : $quiet == 1 ? 'warn'
             : $quiet >= 2 ? 'fatal'
             : $log_level  ? $log_level
             :               undef;

  $class->_validate_log_level($log_level) if defined $log_level;

  my %config = CPAN::Mini->read_config({
    log_level => 'info',
    %commandline
  });

  $config{class} ||= 'CPAN::Mini';

  foreach my $key (keys %commandline) {
    $config{$key} = $commandline{$key} if defined $commandline{$key};
  }

  $config{log_level} = $log_level || $config{log_level} || 'info';

  $class->_validate_log_level($config{log_level});

  eval "require $config{class}";
  die $@ if $@;

  _display_version($config{class}) if $version;
  pod2usage(2) unless $config{local} and $config{remote};

  $|++;
  $config{dirmode} &&= oct($config{dirmode});

  return $config{class}->new(
    remote         => $config{remote},
    local          => $config{local},
    force          => $config{force},
    offline        => $config{offline},
    also_mirror    => $config{also_mirror},
    exact_mirror   => $config{exact_mirror},
    module_filters => $config{module_filters},
    path_filters   => $config{path_filters},
    skip_cleanup   => $config{skip_cleanup},
    skip_perl      => (not $config{perl}),
    timeout        => $config{timeout},
    ignore_source_control => $config{ignore_source_control},
    (defined $config{dirmode} ? (dirmode => $config{dirmode}) : ()),

    log_level      => $config{log_level},
  );
}


1;

__END__
=pod

=head1 NAME

CPAN::Mini::App - the guts of the minicpan command

=head1 VERSION

version 1.111008

=head1 SYNOPSIS

  #!/usr/bin/perl
  use CPAN::Mini::App;
  CPAN::Mini::App->run;

=head1 METHODS

=head2 run

This method is called by F<minicpan> to do all the work.  Don't rely on what it
does just yet.

=head1 SEE ALSO 

Randal Schwartz's original article, which can be found here:

  http://www.stonehenge.com/merlyn/LinuxMag/col42.html

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

