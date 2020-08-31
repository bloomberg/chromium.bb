package IPC::System::Simple;

# ABSTRACT: Run commands simply, with detailed diagnostics

use 5.006;
use strict;
use warnings;
use re 'taint';
use Carp;
use List::Util qw(first);
use Scalar::Util qw(tainted);
use Config;
use constant WINDOWS => ($^O eq 'MSWin32');
use constant VMS     => ($^O eq 'VMS');

BEGIN {

    # It would be lovely to use the 'if' module here, but it didn't
    # enter core until 5.6.2, and we want to keep 5.6.0 compatibility.


    if (WINDOWS) {

        ## no critic (ProhibitStringyEval)

        eval q{
            use Win32::Process qw(INFINITE NORMAL_PRIORITY_CLASS);
            use File::Spec;
            use Win32;

            # This uses the same rules as the core win32.c/get_shell() call.

            use constant WINDOWS_SHELL => eval { Win32::IsWinNT() }
                                            ? [ qw(cmd.exe /x/d/c) ]
                                            : [ qw(command.com /c) ];

            # These are used when invoking _win32_capture
            use constant NO_SHELL  => 0;
            use constant USE_SHELL => 1;

        };

        ## use critic

        # Die nosily if any of the above broke.
        die $@ if $@;
    }
}

# Note that we don't use WIFSTOPPED because perl never uses
# the WUNTRACED flag, and hence will never return early from
# system() if the child processes is suspended with a SIGSTOP.

use POSIX qw(WIFEXITED WEXITSTATUS WIFSIGNALED WTERMSIG);

use constant FAIL_START     => q{"%s" failed to start: "%s"};
use constant FAIL_PLUMBING  => q{Error in IPC::System::Simple plumbing: "%s" - "%s"};
use constant FAIL_CMD_BLANK => q{Entirely blank command passed: "%s"};
use constant FAIL_INTERNAL  => q{Internal error in IPC::System::Simple: "%s"};
use constant FAIL_TAINT     => q{%s called with tainted argument "%s"};
use constant FAIL_TAINT_ENV => q{%s called with tainted environment $ENV{%s}};
use constant FAIL_SIGNAL    => q{"%s" died to signal "%s" (%d)%s};
use constant FAIL_BADEXIT   => q{"%s" unexpectedly returned exit value %d};

use constant FAIL_UNDEF     => q{%s called with undefined command};

use constant FAIL_POSIX     => q{IPC::System::Simple does not understand the POSIX error '%s'.  Please check http://search.cpan.org/perldoc?IPC::System::Simple to see if there is an updated version.  If not please report this as a bug to http://rt.cpan.org/Public/Bug/Report.html?Queue=IPC-System-Simple};

# On Perl's older than 5.8.x we can't assume that there'll be a
# $^{TAINT} for us to check, so we assume that our args may always
# be tainted.
use constant ASSUME_TAINTED => ($] < 5.008);

use constant EXIT_ANY_CONST => -1;			# Used internally
use constant EXIT_ANY       => [ EXIT_ANY_CONST ];	# Exported

use constant UNDEFINED_POSIX_RE => qr{not (?:defined|a valid) POSIX macro|not implemented on this architecture};

require Exporter;
our @ISA = qw(Exporter);

our @EXPORT_OK = qw( 
    capture  capturex
    run      runx
    system   systemx
    $EXITVAL EXIT_ANY
);

our $VERSION = '1.25'; # VERSION : From dzil
our $EXITVAL = -1;

my @Signal_from_number = split(' ', $Config{sig_name});

# Environment variables we don't want to see tainted.
my @Check_tainted_env = qw(PATH IFS CDPATH ENV BASH_ENV);
if (WINDOWS) {
	push(@Check_tainted_env, 'PERL5SHELL');
}
if (VMS) {
	push(@Check_tainted_env, 'DCL$PATH');
}

# Not all systems implement the WIFEXITED calls, but POSIX
# will always export them (even if they're just stubs that
# die with an error).  Test for the presence of a working
# WIFEXITED and friends, or define our own.

eval { WIFEXITED(0); };

if ($@ =~ UNDEFINED_POSIX_RE) {
        no warnings 'redefine';  ## no critic
	*WIFEXITED   = sub { not $_[0] & 0xff };
	*WEXITSTATUS = sub { $_[0] >> 8  };
	*WIFSIGNALED = sub { $_[0] & 127 };
	*WTERMSIG    = sub { $_[0] & 127 };
} elsif ($@) {
	croak sprintf FAIL_POSIX, $@;
}

# None of the POSIX modules I've found define WCOREDUMP, although
# many systems define it.  Check the POSIX module in the hope that
# it may actually be there.


# TODO: Ideally, $NATIVE_WCOREDUMP should be a constant.

my $NATIVE_WCOREDUMP;

eval { POSIX::WCOREDUMP(1); };

if ($@ =~ UNDEFINED_POSIX_RE) {
	*WCOREDUMP = sub { $_[0] & 128 };
        $NATIVE_WCOREDUMP = 0;
} elsif ($@) {
	croak sprintf FAIL_POSIX, $@;
} else {
	# POSIX actually has it defined!  Huzzah!
	*WCOREDUMP = \&POSIX::WCOREDUMP;
        $NATIVE_WCOREDUMP = 1;
}

sub _native_wcoredump {
    return $NATIVE_WCOREDUMP;
}

# system simply calls run

*system  = \&run;
*systemx = \&runx;

# run is our way of running a process with system() semantics

sub run {

	_check_taint(@_);

	my ($valid_returns, $command, @args) = _process_args(@_);

        # If we have arguments, we really want to call systemx,
        # so we do so.

	if (@args) {
                return systemx($valid_returns, $command, @args);
	}

        # Without arguments, we're calling system, and checking
        # the results.

	# We're throwing our own exception on command not found, so
	# we don't need a warning from Perl.

        {
            # silence 'Statement unlikely to be reached' warning
            no warnings 'exec';             ## no critic
            CORE::system($command,@args);
        }

	return _process_child_error($?,$command,$valid_returns);
}

# runx is just like system/run, but *never* invokes the shell.

sub runx {
    _check_taint(@_);

    my ($valid_returns, $command, @args) = _process_args(@_);

    if (WINDOWS) {
        our $EXITVAL = -1;

        my $pid = _spawn_or_die($command, "$command @args");

        $pid->Wait(INFINITE);	# Wait for process exit.
        $pid->GetExitCode($EXITVAL);
        return _check_exit($command,$EXITVAL,$valid_returns);
    }

    # If system() fails, we throw our own exception.  We don't
    # need to have perl complain about it too.

    no warnings; ## no critic

    CORE::system { $command } $command, @args;

    return _process_child_error($?, $command, $valid_returns);
}

# capture is our way of running a process with backticks/qx semantics

sub capture {
	_check_taint(@_);

	my ($valid_returns, $command, @args) = _process_args(@_);

        if (@args) {
            return capturex($valid_returns, $command, @args);
        }

        if (WINDOWS) {
            # USE_SHELL really means "You may use the shell if you need it."
            return _win32_capture(USE_SHELL, $valid_returns, $command, @args);
        }

	our $EXITVAL = -1;

	my $wantarray = wantarray();

	# We'll produce our own warnings on failure to execute.
	no warnings 'exec';	## no critic

        if ($wantarray) {
                my @results = qx($command);
                _process_child_error($?,$command,$valid_returns);
                return @results;
        } 

        my $results = qx($command);
        _process_child_error($?,$command,$valid_returns);
        return $results;
}

# _win32_capture implements the capture and capurex commands on Win32.
# We need to wrap the whole internals of this sub into
# an if (WINDOWS) block to avoid it being compiled on non-Win32 systems.

sub _win32_capture {
    if (not WINDOWS) {
        croak sprintf(FAIL_INTERNAL, "_win32_capture called when not under Win32");
    } else {

        my ($use_shell, $valid_returns, $command, @args) = @_;

        my $wantarray = wantarray();

        # Perl doesn't support multi-arg open under
        # Windows.  Perl also doesn't provide very good
        # feedback when normal backtails fail, either;
        # it returns exit status from the shell
        # (which is indistinguishable from the command
        # running and producing the same exit status).

        # As such, we essentially have to write our own
        # backticks.

        # We start by dup'ing STDOUT.

        open(my $saved_stdout, '>&', \*STDOUT)  ## no critic
                or croak sprintf(FAIL_PLUMBING, "Can't dup STDOUT", $!);

        # We now open up a pipe that will allow us to	
        # communicate with the new process.

        pipe(my ($read_fh, $write_fh))
                or croak sprintf(FAIL_PLUMBING, "Can't create pipe", $!);

        # Allow CRLF sequences to become "\n", since
        # this is what Perl backticks do.

        binmode($read_fh, ':crlf');

        # Now we re-open our STDOUT to $write_fh...

        open(STDOUT, '>&', $write_fh)  ## no critic
                or croak sprintf(FAIL_PLUMBING, "Can't redirect STDOUT", $!);

        # If we have args, or we're told not to use the shell, then
        # we treat $command as our shell.  Otherwise we grub around
        # in our command to look for a command to run.
        #
        # Note that we don't actually *use* the shell (although in
        # a future version we might).  Being told not to use the shell
        # (capturex) means we treat our command as really being a command,
        # and not a command line.

        my $exe =   @args                      ? $command :
                    (! $use_shell)             ? $command :
                    $command =~ m{^"([^"]+)"}x ? $1       :
                    $command =~ m{(\S+)     }x ? $1       :
                    croak sprintf(FAIL_CMD_BLANK, $command);

        # And now we spawn our new process with inherited
        # filehandles.

        my $err;
        my $pid = eval { 
                _spawn_or_die($exe, "$command @args"); 
        }
        or do {
                $err = $@;
        };

        # Regardless of whether our command ran, we must restore STDOUT.
        # RT #48319
        open(STDOUT, '>&', $saved_stdout)  ## no critic
                or croak sprintf(FAIL_PLUMBING,"Can't restore STDOUT", $!);

        # And now, if there was an actual error , propagate it.
        die $err if defined $err;   # If there's an error from _spawn_or_die

        # Clean-up the filehandles we no longer need...

        close($write_fh)
                or croak sprintf(FAIL_PLUMBING,q{Can't close write end of pipe}, $!);
        close($saved_stdout)
                or croak sprintf(FAIL_PLUMBING,q{Can't close saved STDOUT}, $!);

        # Read the data from our child...

        my (@results, $result);

        if ($wantarray) {
                @results = <$read_fh>;
        } else {
                $result = join("",<$read_fh>);
        }

        # Tidy up our windows process and we're done!

        $pid->Wait(INFINITE);	# Wait for process exit.
        $pid->GetExitCode($EXITVAL);

        _check_exit($command,$EXITVAL,$valid_returns);

        return $wantarray ? @results : $result;

    }
}

# capturex() is just like backticks/qx, but never invokes the shell.

sub capturex {
	_check_taint(@_);

	my ($valid_returns, $command, @args) = _process_args(@_);

	our $EXITVAL = -1;

	my $wantarray = wantarray();

	if (WINDOWS) {
            return _win32_capture(NO_SHELL, $valid_returns, $command, @args);
        }

	# We can't use a multi-arg piped open here, since 5.6.x
	# doesn't like them.  Instead we emulate what 5.8.x does,
	# which is to create a pipe(), set the close-on-exec flag
	# on the child, and the fork/exec.  If the exec fails, the
	# child writes to the pipe.  If the exec succeeds, then
	# the pipe closes without data.

	pipe(my ($read_fh, $write_fh))
		or croak sprintf(FAIL_PLUMBING, "Can't create pipe", $!);

	# This next line also does an implicit fork.
	my $pid = open(my $pipe, '-|');	 ## no critic

	if (not defined $pid) {
		croak sprintf(FAIL_START, $command, $!);
	} elsif (not $pid) {
		# Child process, execs command.

		close($read_fh);

		# TODO: 'no warnings exec' doesn't get rid
		# of the 'unlikely to be reached' warnings.
		# This is a bug in perl / perldiag / perllexwarn / warnings.

		no warnings;   ## no critic

		CORE::exec { $command } $command, @args;

		# Oh no, exec fails!  Send the reason why to
		# the parent.

		print {$write_fh} int($!);
		exit(-1);
	}

	{
		# In parent process.

		close($write_fh);

		# Parent process, check for child error.
		my $error = <$read_fh>;

		# Tidy up our pipes.
		close($read_fh);

		# Check for error.
		if ($error) {
			# Setting $! to our child error number gives
			# us nice looking strings when printed.
			local $! = $error;
			croak sprintf(FAIL_START, $command, $!);
		}
	}

	# Parent process, we don't care about our pid, but we
	# do go and read our pipe.

	if ($wantarray) {
		my @results = <$pipe>;
		close($pipe);
		_process_child_error($?,$command,$valid_returns);
		return @results;
	}

	# NB: We don't check the return status on close(), since
	# on failure it sets $?, which we then inspect for more
	# useful information.

	my $results = join("",<$pipe>);
	close($pipe);
	_process_child_error($?,$command,$valid_returns);
	
	return $results;

}

# Tries really hard to spawn a process under Windows.  Returns
# the pid on success, or undef on error.

sub _spawn_or_die {

	# We need to wrap practically the entire sub in an
	# if block to ensure it doesn't get compiled under non-Win32
	# systems.  Compiling on these systems would not only be a
	# waste of time, but also results in complaints about
	# the NORMAL_PRIORITY_CLASS constant.

	if (not WINDOWS) {
		croak sprintf(FAIL_INTERNAL, "_spawn_or_die called when not under Win32");
	} else {
		my ($orig_exe, $cmdline) = @_;
		my $pid;

		my $exe = $orig_exe;

		# If our command doesn't have an extension, add one.
		$exe .= $Config{_exe} if ($exe !~ m{\.});

		Win32::Process::Create(
			$pid, $exe, $cmdline, 1, NORMAL_PRIORITY_CLASS, "."
		) and return $pid;

		my @path = split(/;/,$ENV{PATH});

		foreach my $dir (@path) {
			my $fullpath = File::Spec->catfile($dir,$exe);

			# We're using -x here on the assumption that stat()
			# is faster than spawn, so trying to spawn a process
			# for each path element will be unacceptably
			# inefficient.

			if (-x $fullpath) {
				Win32::Process::Create(
					$pid, $fullpath, $cmdline, 1,
					NORMAL_PRIORITY_CLASS, "."
				) and return $pid;
			}
		}

		croak sprintf(FAIL_START, $orig_exe, $^E);
	}
}

# Complain on tainted arguments or environment.
# ASSUME_TAINTED is true for 5.6.x, since it's missing ${^TAINT}

sub _check_taint {
	return if not (ASSUME_TAINTED or ${^TAINT});
	my $caller = (caller(1))[3];
	foreach my $var (@_) {
		if (tainted $var) {
			croak sprintf(FAIL_TAINT, $caller, $var);
		}
	}
	foreach my $var (@Check_tainted_env) {
		if (tainted $ENV{$var} ) {
			croak sprintf(FAIL_TAINT_ENV, $caller, $var);
		}
	}

	return;

}

# This subroutine performs the difficult task of interpreting
# $?.  It's not intended to be called directly, as it will
# croak on errors, and its implementation and interface may
# change in the future.

sub _process_child_error {
	my ($child_error, $command, $valid_returns) = @_;
	
	$EXITVAL = -1;

	my $coredump = WCOREDUMP($child_error);

        # There's a bug in perl 5.10.0 where if the system
        # does not provide a native WCOREDUMP, then $? will
        # never contain coredump information.  This code
        # checks to see if we have the bug, and works around
        # it if needed.

        if ($] >= 5.010 and not $NATIVE_WCOREDUMP) {
            $coredump ||= WCOREDUMP( ${^CHILD_ERROR_NATIVE} );
        }

	if ($child_error == -1) {
		croak sprintf(FAIL_START, $command, $!);

	} elsif ( WIFEXITED( $child_error ) ) {
		$EXITVAL = WEXITSTATUS( $child_error );

		return _check_exit($command,$EXITVAL,$valid_returns);

	} elsif ( WIFSIGNALED( $child_error ) ) {
		my $signal_no   = WTERMSIG( $child_error );
		my $signal_name = $Signal_from_number[$signal_no] || "UNKNOWN";

		croak sprintf FAIL_SIGNAL, $command, $signal_name, $signal_no, ($coredump ? " and dumped core" : "");


	} 

	croak sprintf(FAIL_INTERNAL, qq{'$command' ran without exit value or signal});

}

# A simple subroutine for checking exit values.  Results in better
# assurance of consistent error messages, and better forward support
# for new features in I::S::S.

sub _check_exit {
	my ($command, $exitval, $valid_returns) = @_;

	# If we have a single-value list consisting of the EXIT_ANY
	# value, then we're happy with whatever exit value we're given.
	if (@$valid_returns == 1 and $valid_returns->[0] == EXIT_ANY_CONST) {
		return $exitval;
	}

	if (not defined first { $_ == $exitval } @$valid_returns) {
		croak sprintf FAIL_BADEXIT, $command, $exitval;
	}	
	return $exitval;
}


# This subroutine simply determines a list of valid returns, the command
# name, and any arguments that we need to pass to it.

sub _process_args {
	my $valid_returns = [ 0 ];
	my $caller = (caller(1))[3];

	if (not @_) {
		croak "$caller called with no arguments";
	}

	if (ref $_[0] eq "ARRAY") {
		$valid_returns = shift(@_);
	}

	if (not @_) {
		croak "$caller called with no command";
	}

	my $command = shift(@_);

        if (not defined $command) {
                croak sprintf( FAIL_UNDEF, $caller );
        }

	return ($valid_returns,$command,@_);

}

1;

__END__

=head1 NAME

IPC::System::Simple - Run commands simply, with detailed diagnostics

=head1 SYNOPSIS

  use IPC::System::Simple qw(system systemx capture capturex);

  system("some_command");        # Command succeeds or dies!

  system("some_command",@args);  # Succeeds or dies, avoids shell if @args

  systemx("some_command",@args); # Succeeds or dies, NEVER uses the shell


  # Capture the output of a command (just like backticks). Dies on error.
  my $output = capture("some_command");

  # Just like backticks in list context.  Dies on error.
  my @output = capture("some_command");

  # As above, but avoids the shell if @args is non-empty
  my $output = capture("some_command", @args);

  # As above, but NEVER invokes the shell.
  my $output = capturex("some_command", @args);
  my @output = capturex("some_command", @args);

=head1 DESCRIPTION

Calling Perl's in-built C<system()> function is easy, 
determining if it was successful is I<hard>.  Let's face it,
C<$?> isn't the nicest variable in the world to play with, and
even if you I<do> check it, producing a well-formatted error
string takes a lot of work.

C<IPC::System::Simple> takes the hard work out of calling 
external commands.  In fact, if you want to be really lazy,
you can just write:

    use IPC::System::Simple qw(system);

and all of your C<system> commands will either succeed (run to
completion and return a zero exit value), or die with rich diagnostic
messages.

The C<IPC::System::Simple> module also provides a simple replacement
to Perl's backticks operator.  Simply write:

    use IPC::System::Simple qw(capture);

and then use the L</capture()> command just like you'd use backticks.
If there's an error, it will die with a detailed description of what
went wrong.  Better still, you can even use C<capturex()> to run the
equivalent of backticks, but without the shell:

    use IPC::System::Simple qw(capturex);

    my $result = capturex($command, @args);

If you want more power than the basic interface, including the
ability to specify which exit values are acceptable, trap errors,
or process diagnostics, then read on!

=head1 ADVANCED SYNOPSIS

  use IPC::System::Simple qw(
    capture capturex system systemx run runx $EXITVAL EXIT_ANY
  );

  # Run a command, throwing exception on failure

  run("some_command");

  runx("some_command",@args);  # Run a command, avoiding the shell

  # Do the same thing, but with the drop-in system replacement.

  system("some_command");

  systemx("some_command", @args);

  # Run a command which must return 0..5, avoid the shell, and get the
  # exit value (we could also look at $EXITVAL)

  my $exit_value = runx([0..5], "some_command", @args);

  # The same, but any exit value will do.

  my $exit_value = runx(EXIT_ANY, "some_command", @args);

  # Capture output into $result and throw exception on failure

  my $result = capture("some_command");	

  # Check exit value from captured command

  print "some_command exited with status $EXITVAL\n";

  # Captures into @lines, splitting on $/
  my @lines = capture("some_command"); 

  # Run a command which must return 0..5, capture the output into
  # @lines, and avoid the shell.

  my @lines  = capturex([0..5], "some_command", @args);

=head1 ADVANCED USAGE

=head2 run() and system()

C<IPC::System::Simple> provides a subroutine called
C<run>, that executes a command using the same semantics is
Perl's built-in C<system>:

    use IPC::System::Simple qw(run);

    run("cat *.txt");           # Execute command via the shell
    run("cat","/etc/motd");     # Execute command without shell

The primary difference between Perl's in-built system and
the C<run> command is that C<run> will throw an exception on
failure, and allows a list of acceptable exit values to be set.
See L</Exit values> for further information.

In fact, you can even have C<IPC::System::Simple> replace the
default C<system> function for your package so it has the
same behaviour:

    use IPC::System::Simple qw(system);

    system("cat *.txt");  # system now suceeds or dies!

C<system> and C<run> are aliases to each other.

See also L</runx(), systemx() and capturex()> for variants of
C<system()> and C<run()> that never invoke the shell, even with
a single argument.

=head2 capture()

A second subroutine, named C<capture> executes a command with
the same semantics as Perl's built-in backticks (and C<qx()>):

    use IPC::System::Simple qw(capture);

    # Capture text while invoking the shell.
    my $file  = capture("cat /etc/motd");
    my @lines = capture("cat /etc/passwd");

However unlike regular backticks, which always use the shell, C<capture>
will bypass the shell when called with multiple arguments:

    # Capture text while avoiding the shell.
    my $file  = capture("cat", "/etc/motd");
    my @lines = capture("cat", "/etc/passwd");

See also L</runx(), systemx() and capturex()> for a variant of
C<capture()> that never invokes the shell, even with a single
argument.

=head2 runx(), systemx() and capturex()

The C<runx()>, C<systemx()> and C<capturex()> commands are identical
to the multi-argument forms of C<run()>, C<system()> and C<capture()>
respectively, but I<never> invoke the shell, even when called with a
single argument.  These forms are particularly useful when a command's
argument list I<might> be empty, for example:

    systemx($cmd, @args);

The use of C<systemx()> here guarantees that the shell will I<never>
be invoked, even if C<@args> is empty.

=head2 Exception handling

In the case where the command returns an unexpected status, both C<run> and
C<capture> will throw an exception, which if not caught will terminate your
program with an error.

Capturing the exception is easy:

    eval {
        run("cat *.txt");
    };

    if ($@) {
        print "Something went wrong - $@\n";
    }

See the diagnostics section below for more details.

=head3 Exception cases

C<IPC::System::Simple> considers the following to be unexpected,
and worthy of exception:

=over 4

=item *

Failing to start entirely (eg, command not found, permission denied).

=item *

Returning an exit value other than zero (but see below).

=item *

Being killed by a signal.

=item *

Being passed tainted data (in taint mode).

=back

=head2 Exit values

Traditionally, system commands return a zero status for success and a
non-zero status for failure.  C<IPC::System::Simple> will default to throwing
an exception if a non-zero exit value is returned.

You may specify a range of values which are considered acceptable exit
values by passing an I<array reference> as the first argument.  The
special constant C<EXIT_ANY> can be used to allow I<any> exit value
to be returned.

	use IPC::System::Simple qw(run system capture EXIT_ANY);

	run( [0..5], "cat *.txt");             # Exit values 0-5 are OK

	system( [0..5], "cat *.txt");          # This works the same way

	my @lines = capture( EXIT_ANY, "cat *.txt"); # Any exit is fine.

The C<run> and replacement C<system> subroutines returns the exit
value of the process:

	my $exit_value = run( [0..5], "cat *.txt");

	# OR:

	my $exit_value = system( [0..5] "cat *.txt");

	print "Program exited with value $exit_value\n";

=head3 $EXITVAL

The exit value of any command executed by C<IPC::System::Simple>
can always be retrieved from the C<$IPC::System::Simple::EXITVAL>
variable:

This is particularly useful when inspecting results from C<capture>,
which returns the captured text from the command.

	use IPC::System::Simple qw(capture $EXITVAL EXIT_ANY);

	my @enemies_defeated = capture(EXIT_ANY, "defeat_evil", "/dev/mordor");

	print "Program exited with value $EXITVAL\n";

C<$EXITVAL> will be set to C<-1> if the command did not exit normally (eg,
being terminated by a signal) or did not start.  In this situation an
exception will also be thrown.

=head2 WINDOWS-SPECIFIC NOTES

As of C<IPC::System::Simple> v0.06, the C<run> subroutine I<when
called with multiple arguments> will make available the full 32-bit
exit value on Win32 systems.  This is different from the
previous versions of C<IPC::System::Simple> and from Perl's
in-build C<system()> function, which can only handle 8-bit return values.

The C<capture> subroutine always returns the 32-bit exit value under
Windows.  The C<capture> subroutine also never uses the shell,
even when passed a single argument.

Versions of C<IPC::System::Simple> before v0.09 would not search
the C<PATH> environment variable when the multi-argument form of
C<run()> was called.  Versions from v0.09 onwards correctly search
the path provided the command is provided including the extension
(eg, C<notepad.exe> rather than just C<notepad>, or C<gvim.bat> rather
than just C<gvim>).  If no extension is provided, C<.exe> is
assumed.

Signals are not supported on Windows systems.  Sending a signal
to a Windows process will usually cause it to exit with the signal
number used.

=head1 DIAGNOSTICS

=over 4

=item "%s" failed to start: "%s"

The command specified did not even start.  It may not exist, or
you may not have permission to use it.  The reason it could not
start (as determined from C<$!>) will be provided.

=item "%s" unexpectedly returned exit value %d

The command ran successfully, but returned an exit value we did
not expect.  The value returned is reported.

=item "%s" died to signal "%s" (%d) %s

The command was killed by a signal.  The name of the signal
will be reported, or C<UNKNOWN> if it cannot be determined.  The
signal number is always reported.  If we detected that the
process dumped core, then the string C<and dumped core> is
appended.

=item IPC::System::Simple::%s called with no arguments

You attempted to call C<run> or C<capture> but did not provide any
arguments at all.  At the very lease you need to supply a command
to run.

=item IPC::System::Simple::%s called with no command

You called C<run> or C<capture> with a list of acceptable exit values,
but no actual command.

=item IPC::System::Simple::%s called with tainted argument "%s"

You called C<run> or C<capture> with tainted (untrusted) arguments, which is
almost certainly a bad idea.  To untaint your arguments you'll need to pass
your data through a regular expression and use the resulting match variables.
See L<perlsec/Laundering and Detecting Tainted Data> for more information.

=item IPC::System::Simple::%s called with tainted environment $ENV{%s}

You called C<run> or C<capture> but part of your environment was tainted
(untrusted).  You should either delete the named environment
variable before calling C<run>, or set it to an untainted value
(usually one set inside your program).  See
L<perlsec/Cleaning Up Your Path> for more information.

=item Error in IPC::System::Simple plumbing: "%s" - "%s"

Implementing the C<capture> command involves dark and terrible magicks
involving pipes, and one of them has sprung a leak.  This could be due to a
lack of file descriptors, although there are other possibilities.

If you are able to reproduce this error, you are encouraged
to submit a bug report according to the L</Reporting bugs> section below.

=item Internal error in IPC::System::Simple: "%s"

You've found a bug in C<IPC::System::Simple>.  Please check to
see if an updated version of C<IPC::System::Simple> is available.
If not, please file a bug report according to the L</Reporting bugs> section
below.

=item IPC::System::Simple::%s called with undefined command

You've passed the undefined value as a command to be executed.
While this is a very Zen-like action, it's not supported by
Perl's current implementation.

=back

=head1 DEPENDENCIES

This module depends upon L<Win32::Process> when used on Win32
system.  C<Win32::Process> is bundled as a core module in ActivePerl 5.6
and above.

There are no non-core dependencies on non-Win32 systems.

=head1 COMPARISON TO OTHER APIs

Perl provides a range of in-built functions for handling external
commands, and CPAN provides even more.  The C<IPC::System::Simple>
differentiates itself from other options by providing:

=over 4

=item Extremely detailed diagnostics

The diagnostics produced by C<IPC::System::Simple> are designed
to provide as much information as possible.  Rather than requiring
the developer to inspect C<$?>, C<IPC::System::Simple> does the
hard work for you.

If an odd exit status is provided, you're informed of what it is.  If
a signal kills your process, you are informed of both its name and
number.  If tainted data or environment prevents your command from
running, you are informed of exactly which datais 

=item Exceptions on failure

C<IPC::System::Simple> takes an aggressive approach to error handling.
Rather than allow commands to fail silently, exceptions are thrown
when unexpected results are seen.  This allows for easy development
using a try/catch style, and avoids the possibility of accidently
continuing after a failed command.

=item Easy access to exit status

The C<run>, C<system> and C<capture> commands all set C<$EXITVAL>,
making it easy to determine the exit status of a command.
Additionally, the C<system> and C<run> interfaces return the exit
status.

=item Consistent interfaces

When called with multiple arguments, the C<run>, C<system> and
C<capture> interfaces I<never> invoke the shell.  This differs
from the in-built Perl C<system> command which may invoke the
shell under Windows when called with multiple arguments.  It
differs from the in-built Perl backticks operator which always
invokes the shell.

=back

=head1 BUGS

When C<system> is exported, the exotic form C<system { $cmd } @args>
is not supported.  Attemping to use the exotic form is a syntax
error.  This affects the calling package I<only>.  Use C<CORE::system>
if you need it, or consider using the L<autodie> module to replace
C<system> with lexical scope.

Core dumps are only checked for when a process dies due to a
signal.  It is not believed there are any systems where processes
can dump core without dying to a signal.

C<WIFSTOPPED> status is not checked, as perl never spawns processes
with the C<WUNTRACED> option.

Signals are not supported under Win32 systems, since they don't
work at all like Unix signals.  Win32 singals cause commands to
exit with a given exit value, which this modules I<does> capture.

Only 8-bit values are returned when C<run()> or C<system()> 
is called with a single value under Win32.  Multi-argument calls
to C<run()> and C<system()>, as well as the C<runx()> and
C<systemx()> always return the 32-bit Windows return values.

=head2 Reporting bugs

Before reporting a bug, please check to ensure you are using the
most recent version of C<IPC::System::Simple>.  Your problem may
have already been fixed in a new release.

You can find the C<IPC::System::Simple> bug-tracker at
L<http://rt.cpan.org/Public/Dist/Display.html?Name=IPC-System-Simple> .
Please check to see if your bug has already been reported; if
in doubt, report yours anyway.

Submitting a patch and/or failing test case will greatly expedite
the fixing of bugs.

=head1 FEEDBACK

If you find this module useful, please consider rating it on the
CPAN Ratings service at
L<http://cpanratings.perl.org/rate/?distribution=IPC-System-Simple> .

The module author loves to hear how C<IPC::System::Simple> has made
your life better (or worse).  Feedback can be sent to
E<lt>pjf@perltraining.com.auE<gt>.

=head1 SEE ALSO

L<autodie> uses C<IPC::System::Simple> to provide succeed-or-die
replacements to C<system> (and other built-ins) with lexical scope.

L<POSIX>, L<IPC::Run::Simple>, L<perlipc>, L<perlport>, L<IPC::Run>,
L<IPC::Run3>, L<Win32::Process>

=head1 AUTHOR

Paul Fenwick E<lt>pjf@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2008 by Paul Fenwick

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.6.0 or,
at your option, any later version of Perl 5 you may have available.

=for Pod::Coverage WCOREDUMP

=cut
