use strict;
use warnings;

package CPAN::Mini::App;
$CPAN::Mini::App::VERSION = '1.111016';
# ABSTRACT: the guts of the minicpan command

#pod =head1 SYNOPSIS
#pod
#pod   #!/usr/bin/perl
#pod   use CPAN::Mini::App;
#pod   CPAN::Mini::App->run;
#pod
#pod =cut

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

#pod =method run
#pod
#pod This method is called by F<minicpan> to do all the work.  Don't rely on what it
#pod does just yet.
#pod
#pod =cut

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

  my @option_spec = $class->_option_spec();
  GetOptions(\%commandline, @option_spec) or pod2usage(2);

  # These two options will cause the program to exit before finishing ->run
  pod2usage(1) if $commandline{help};
  $version = 1 if $commandline{version};

  # How noisy should we be?
  my $debug     = $commandline{debug};
  my $log_level = $commandline{log_level};
  my $quiet     = $commandline{qq} ? 2 : $commandline{quiet};

  die "can't mix --debug, --log-level, and --debug\n"
    if defined($quiet) + defined($debug) + defined($log_level) > 1;

  # Set log_level accordingly
  $quiet ||= 0;
  $log_level = $debug      ? 'debug'
             : $quiet == 1 ? 'warn'
             : $quiet >= 2 ? 'fatal'
             : $log_level  ? $log_level
             :               undef;

  my %config = CPAN::Mini->read_config({
    log_level => 'info',
    %commandline
  });

  $config{class} ||= 'CPAN::Mini';

  # Override config with commandline options
  %config = (%config, %commandline);

  $config{log_level} = $log_level || $config{log_level} || 'info';
  $class->_validate_log_level($config{log_level});

  eval "require $config{class}";
  die $@ if $@;

  _display_version($config{class}) if $version;

  if ($config{remote_from} && ! $config{remote}) {
    $config{remote} = $config{class}->remote_from(
      $config{remote_from},
      $config{remote},
      $config{quiet},
    );
  }

  $config{remote} ||= 'http://www.cpan.org/';

  pod2usage(2) unless $config{local} and $config{remote};

  $|++;

  # Convert dirmode string to a real octal value, if given
  $config{dirmode} = oct $config{dirmode} if $config{dirmode};

  # Turn the 'perl' option into 'skip_perl', for backward compatibility
  $config{skip_perl} = not delete $config{perl};

  return $config{class}->new(%config);
}

sub _option_spec {
  return qw<
    class|c=s
    help|h
    version|v
    quiet|q+
    qq
    debug
    log_level|log-level=s
    local|l=s
    remote|r=s
    dirmode|d=s
    offline
    force|f
    perl
    exact_mirror|x
    timeout|t=i
    config_file|config|C=s
    remote-from=s
  >;
}

#pod =head1 SEE ALSO
#pod
#pod Randal Schwartz's original article, which can be found here:
#pod
#pod   http://www.stonehenge.com/merlyn/LinuxMag/col42.html
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

CPAN::Mini::App - the guts of the minicpan command

=head1 VERSION

version 1.111016

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
