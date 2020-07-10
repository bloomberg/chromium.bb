package Devel::vscode;
use 5.008;
use strict;
use warnings;
no strict;
no warnings;

our $VERSION = '0.02';

our $BREAK_AFTER_FORK = 1;

sub import {
  my ($class, @args) = @_;

  for (@args) {

    if (/^fork=break$/) {

      $BREAK_AFTER_FORK = 1;

    } elsif (/^fork=$/) {

      $BREAK_AFTER_FORK = 0;

    } else {

      die "Unknown option $_";

    }

  }

  return;
}

sub _fork {

  if ($BREAK_AFTER_FORK) {

    my $pid = &CORE::fork;

    return $pid if not defined $pid;
    return $pid if $pid;

    $DB::single = 2;

    return $pid;

  } else {

    &CORE::fork;

  }

};

BEGIN {
  require "perl5db.pl";
}

BEGIN {
  *CORE::GLOBAL::fork = \&_fork;
}

1;

__END__

=head1 NAME

Devel::vscode - Debug with perl-debug in Visual Studio Code

=head1 SYNOPSIS

  % perl -d:vscode example.pl

=head1 DESCRIPTION

This module primarily serves as a namespace registration for the
C<Devel::vscode> namespace for use in the perl-debug extension for
Visual Studio Code. It is not needed to use the extension, and is
only a very thin wrapper around the built-in debugger C<perl5db.pl>.

=head2 FORK OVERRIDE

The only reason to use this as debugger is to enhance your debugging
experience when your code uses C<fork> (see L<perlfunc/fork>). The
extension talks to C<perl5db.pl> over a socket and expects that the
debugger will open a new connection to the extension for newly forked
children.

However, C<perl5db.pl> will only do so when it assumes control over
the child process. That means the Visual Studio Code extension has no
good way to show newly forked children in the user interface until
the child has been stopped. As far as the author of this module is
aware, the following options are available:

=over

=item *

Do something akin to the debugger command C<w $$>. That would create
a global watch expression that would break into the debugger when the
value of C<$$>, the pid of the current process, changes. That would
be right after C<fork> returns in the child process.

The debugger can do this only by enabling C<trace> mode. That however
comes with a runtime performance penalty and would also affect code
that does not use C<fork>. Furthermore, it would mix user defined
watchpoints with a watchpoint set by the extension which may be
confusing for users aswell as the extension itself (what should it
do when the user clears all watch expressions, for instance).

=item *

Override C<CORE::GLOBAL::fork> at compile time. That is the approach
implemented by this module. C<Devel::vscode::_fork> is a wrapper for
C<CORE::fork> that, depending on C<$Devel::vscode::BREAK_AFTER_FORK>,
breaks into the debugger right after C<fork> returns successfully in
child processes. That gives the Visual Studio Code extension a chance
to control this behaviour at runtime, does not affect code that does
not use C<fork>, can be recognised by the extension by looking at the
callstack.

This behaviour is the default and can be configured explicitly using:

  % perl -d:vscode=fork=break

To disable this behaviour:

  % perl -d:vscode=fork=

Note that when the debuggee, or modules it uses, call C<CORE::fork>
directly, they would bypass this wrapper. In that case, C<w $$> is
the only alternative.

=back

=head1 BUG REPORTS

=over

=item * L<https://github.com/hoehrmann/Devel-vscode/issues>

=item * L<mailto:bug-Devel-vscode@rt.cpan.org>

=item * L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Devel-vscode>

=back

=head1 SEE ALSO

* L<https://marketplace.visualstudio.com/items?itemName=mortenhenriksen.perl-debug>

* L<https://github.com/raix/vscode-perl-debug/>

* L<perldebug>

=head1 AUTHOR / COPYRIGHT / LICENSE

  Copyright (c) 2019 Bjoern Hoehrmann <bjoern@hoehrmann.de>.
  This module is licensed under the same terms as Perl itself.

=cut
