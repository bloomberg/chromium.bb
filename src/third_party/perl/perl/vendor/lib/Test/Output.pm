package Test::Output;

use warnings;
use strict;

use Test::Builder;
use Capture::Tiny qw/capture capture_stdout capture_stderr capture_merged/;

use Exporter qw(import);

our %EXPORT_TAGS = (
	stdout => [
      qw(
        stdout_is stdout_isnt stdout_like stdout_unlike
        )
    ],
    stderr => [
      qw(
        stderr_is stderr_isnt stderr_like stderr_unlike
        )
    ],
    output => [
      qw(
        output_is output_isnt output_like output_unlike
        )
    ],
    combined => [
      qw(
        combined_is combined_isnt combined_like combined_unlike
        )
    ],
    functions => [
      qw(
        output_from stderr_from stdout_from combined_from
        )
    ],
    tests => [
      qw(
        output_is output_isnt output_like output_unlike
        stderr_is stderr_isnt stderr_like stderr_unlike
        stdout_is stdout_isnt stdout_like stdout_unlike
        combined_is combined_isnt combined_like combined_unlike
        )
    ],
    all => [
      qw(
        output_is output_isnt output_like output_unlike
        stderr_is stderr_isnt stderr_like stderr_unlike
        stdout_is stdout_isnt stdout_like stdout_unlike
        combined_is combined_isnt combined_like combined_unlike
        output_from stderr_from stdout_from combined_from
        )
    ],
	);

our @EXPORT = keys %{
		{
		map { $_ => 1 }
		map {
			@{ $EXPORT_TAGS{$_} }
			}
		keys %EXPORT_TAGS
		}
	};

my $Test = Test::Builder->new;

=encoding utf8

=head1 NAME

Test::Output - Utilities to test STDOUT and STDERR messages.

=cut

our $VERSION = '1.031';

=head1 SYNOPSIS

    use Test::More tests => 4;
    use Test::Output;

    sub writer {
      print "Write out.\n";
      print STDERR "Error out.\n";
    }

    stdout_is(\&writer,"Write out.\n",'Test STDOUT');

    stderr_isnt(\&writer,"No error out.\n",'Test STDERR');

    combined_is(
                \&writer,
                "Write out.\nError out.\n",
                'Test STDOUT & STDERR combined'
               );

    output_is(
              \&writer,
              "Write out.\n",
              "Error out.\n",
              'Test STDOUT & STDERR'
            );

   # Use bare blocks.

   stdout_is { print "test" } "test", "Test STDOUT";
   stderr_isnt { print "bad test" } "test", "Test STDERR";
   output_is { print 'STDOUT'; print STDERR 'STDERR' }
     "STDOUT", "STDERR", "Test output";

=head1 DESCRIPTION

Test::Output provides a simple interface for testing output sent to C<STDOUT>
or C<STDERR>. A number of different utilities are included to try and be as
flexible as possible to the tester.

Likewise, L<Capture::Tiny> provides a much more robust capture mechanism without
than the original L<Test::Output::Tie>.

=cut

=head1 TESTS

=cut

=head2 STDOUT

=over 4

=item B<stdout_is>

=item B<stdout_isnt>

   stdout_is  ( $coderef, $expected, 'description' );
   stdout_is    { ... } $expected, 'description';
   stdout_isnt( $coderef, $expected, 'description' );
   stdout_isnt  { ... } $expected, 'description';

C<stdout_is()> captures output sent to C<STDOUT> from C<$coderef> and compares
it against C<$expected>. The test passes if equal.

C<stdout_isnt()> passes if C<STDOUT> is not equal to C<$expected>.

=cut

sub stdout_is (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my $stdout = stdout_from($test);

  my $ok = ( $stdout eq $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDOUT is:\n$stdout\nnot:\n$expected\nas expected");

  return $ok;
}

sub stdout_isnt (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my $stdout = stdout_from($test);

  my $ok = ( $stdout ne $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDOUT:\n$stdout\nmatching:\n$expected\nnot expected");

  return $ok;
}

=item B<stdout_like>

=item B<stdout_unlike>

   stdout_like  ( $coderef, qr/$expected/, 'description' );
   stdout_like    { ... } qr/$expected/, 'description';
   stdout_unlike( $coderef, qr/$expected/, 'description' );
   stdout_unlike  { ... } qr/$expected/, 'description';

C<stdout_like()> captures the output sent to C<STDOUT> from C<$coderef> and compares
it to the regex in C<$expected>. The test passes if the regex matches.

C<stdout_unlike()> passes if STDOUT does not match the regex.

=back

=cut

sub stdout_like (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  unless ( my $regextest = _chkregex( 'stdout_like' => $expected ) ) {
    return $regextest;
  }

  my $stdout = stdout_from($test);

  my $ok = ( $stdout =~ $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDOUT:\n$stdout\ndoesn't match:\n$expected\nas expected");

  return $ok;
}

sub stdout_unlike (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  unless ( my $regextest = _chkregex( 'stdout_unlike' => $expected ) ) {
    return $regextest;
  }

  my $stdout = stdout_from($test);

  my $ok = ( $stdout !~ $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDOUT:\n$stdout\nmatches:\n$expected\nnot expected");

  return $ok;
}

=head2 STDERR

=over 4

=item B<stderr_is>

=item B<stderr_isnt>

   stderr_is  ( $coderef, $expected, 'description' );
   stderr_is    {... } $expected, 'description';

   stderr_isnt( $coderef, $expected, 'description' );
   stderr_isnt  {... } $expected, 'description';

C<stderr_is()> is similar to C<stdout_is>, except that it captures C<STDERR>. The
test passes if C<STDERR> from C<$coderef> equals C<$expected>.

C<stderr_isnt()> passes if C<STDERR> is not equal to C<$expected>.

=cut

sub stderr_is (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my $stderr = stderr_from($test);

  my $ok = ( $stderr eq $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDERR is:\n$stderr\nnot:\n$expected\nas expected");

  return $ok;
}

sub stderr_isnt (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my $stderr = stderr_from($test);

  my $ok = ( $stderr ne $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDERR:\n$stderr\nmatches:\n$expected\nnot expected");

  return $ok;
}

=item B<stderr_like>

=item B<stderr_unlike>

   stderr_like  ( $coderef, qr/$expected/, 'description' );
   stderr_like   { ...} qr/$expected/, 'description';
   stderr_unlike( $coderef, qr/$expected/, 'description' );
   stderr_unlike  { ...} qr/$expected/, 'description';

C<stderr_like()> is similar to C<stdout_like()> except that it compares the regex
C<$expected> to C<STDERR> captured from C<$codref>. The test passes if the regex
matches.

C<stderr_unlike()> passes if C<STDERR> does not match the regex.

=back

=cut

sub stderr_like (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  unless ( my $regextest = _chkregex( 'stderr_like' => $expected ) ) {
    return $regextest;
  }

  my $stderr = stderr_from($test);

  my $ok = ( $stderr =~ $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDERR:\n$stderr\ndoesn't match:\n$expected\nas expected");

  return $ok;
}

sub stderr_unlike (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  unless ( my $regextest = _chkregex( 'stderr_unlike' => $expected ) ) {
    return $regextest;
  }

  my $stderr = stderr_from($test);

  my $ok = ( $stderr !~ $expected );

  $Test->ok( $ok, $description )
    || $Test->diag("STDERR:\n$stderr\nmatches:\n$expected\nnot expected");

  return $ok;
}

=head2 COMBINED OUTPUT

=over 4

=item B<combined_is>

=item B<combined_isnt>

   combined_is   ( $coderef, $expected, 'description' );
   combined_is   {... } $expected, 'description';
   combined_isnt ( $coderef, $expected, 'description' );
   combined_isnt {... } $expected, 'description';

C<combined_is()> directs C<STDERR> to C<STDOUT> then captures C<STDOUT>. This is
equivalent to UNIXs C<< 2>&1 >>. The test passes if the combined C<STDOUT>
and C<STDERR> from $coderef equals $expected.

C<combined_isnt()> passes if combined C<STDOUT> and C<STDERR> are not equal
to C<$expected>.

=cut

sub combined_is (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my $combined = combined_from($test);

  my $ok = ( $combined eq $expected );

  $Test->ok( $ok, $description )
    || $Test->diag(
    "STDOUT & STDERR are:\n$combined\nnot:\n$expected\nas expected");

  return $ok;
}

sub combined_isnt (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my $combined = combined_from($test);

  my $ok = ( $combined ne $expected );

  $Test->ok( $ok, $description )
    || $Test->diag(
    "STDOUT & STDERR:\n$combined\nmatching:\n$expected\nnot expected");

  return $ok;
}

=item B<combined_like>

=item B<combined_unlike>

   combined_like   ( $coderef, qr/$expected/, 'description' );
   combined_like   { ...} qr/$expected/, 'description';
   combined_unlike ( $coderef, qr/$expected/, 'description' );
   combined_unlike { ...} qr/$expected/, 'description';

C<combined_like()> is similar to C<combined_is()> except that it compares a regex
(C<$expected)> to C<STDOUT> and C<STDERR> captured from C<$codref>. The test passes if
the regex matches.

C<combined_unlike()> passes if the combined C<STDOUT> and C<STDERR> does not match
the regex.

=back

=cut

sub combined_like (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  unless ( my $regextest = _chkregex( 'combined_like' => $expected ) ) {
    return $regextest;
  }

  my $combined = combined_from($test);

  my $ok = ( $combined =~ $expected );

  $Test->ok( $ok, $description )
    || $Test->diag(
    "STDOUT & STDERR:\n$combined\ndon't match:\n$expected\nas expected");

  return $ok;
}

sub combined_unlike (&$;$$) {
  my $test        = shift;
  my $expected    = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  unless ( my $regextest = _chkregex( 'combined_unlike' => $expected ) ) {
    return $regextest;
  }

  my $combined = combined_from($test);

  my $ok = ( $combined !~ $expected );

  $Test->ok( $ok, $description )
    || $Test->diag(
    "STDOUT & STDERR:\n$combined\nmatching:\n$expected\nnot expected");

  return $ok;
}

=head2 OUTPUT

=over 4

=item B<output_is>

=item B<output_isnt>

   output_is  ( $coderef, $expected_stdout, $expected_stderr, 'description' );
   output_is    {... } $expected_stdout, $expected_stderr, 'description';
   output_isnt( $coderef, $expected_stdout, $expected_stderr, 'description' );
   output_isnt  {... } $expected_stdout, $expected_stderr, 'description';

The C<output_is()> function is a combination of the C<stdout_is()> and C<stderr_is()>
functions. For example:

  output_is(sub {print "foo"; print STDERR "bar";},'foo','bar');

is functionally equivalent to

  stdout_is(sub {print "foo";},'foo')
    && stderr_is(sub {print STDERR "bar";'bar');

except that C<$coderef> is only executed once.

Unlike C<stdout_is()> and C<stderr_is()> which ignore STDERR and STDOUT
respectively, C<output_is()> requires both C<STDOUT> and C<STDERR> to match in order
to pass. Setting either C<$expected_stdout> or C<$expected_stderr> to C<undef>
ignores C<STDOUT> or C<STDERR> respectively.

  output_is(sub {print "foo"; print STDERR "bar";},'foo',undef);

is the same as

  stdout_is(sub {print "foo";},'foo')

C<output_isnt()> provides the opposite function of C<output_is()>. It is a
combination of C<stdout_isnt()> and C<stderr_isnt()>.

  output_isnt(sub {print "foo"; print STDERR "bar";},'bar','foo');

is functionally equivalent to

  stdout_is(sub {print "foo";},'bar')
    && stderr_is(sub {print STDERR "bar";'foo');

As with C<output_is()>, setting either C<$expected_stdout> or C<$expected_stderr> to
C<undef> ignores the output to that facility.

  output_isnt(sub {print "foo"; print STDERR "bar";},undef,'foo');

is the same as

  stderr_is(sub {print STDERR "bar";},'foo')

=cut

sub output_is (&$$;$$) {
  my $test        = shift;
  my $expout      = shift;
  my $experr      = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my ( $stdout, $stderr ) = output_from($test);

  my $ok = 1;
  my $diag;

  if ( defined($experr) && defined($expout) ) {
    unless ( $stdout eq $expout ) {
      $ok = 0;
      $diag .= "STDOUT is:\n$stdout\nnot:\n$expout\nas expected";
    }
    unless ( $stderr eq $experr ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR is:\n$stderr\nnot:\n$experr\nas expected";
    }
  }
  elsif ( defined($expout) ) {
    $ok = ( $stdout eq $expout );
    $diag .= "STDOUT is:\n$stdout\nnot:\n$expout\nas expected";
  }
  elsif ( defined($experr) ) {
    $ok = ( $stderr eq $experr );
    $diag .= "STDERR is:\n$stderr\nnot:\n$experr\nas expected";
  }
  else {
    unless ( $stdout eq '' ) {
      $ok = 0;
      $diag .= "STDOUT is:\n$stdout\nnot:\n\nas expected";
    }
    unless ( $stderr eq '' ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR is:\n$stderr\nnot:\n\nas expected";
    }
  }

  $Test->ok( $ok, $description ) || $Test->diag($diag);

  return $ok;
}

sub output_isnt (&$$;$$) {
  my $test        = shift;
  my $expout      = shift;
  my $experr      = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my ( $stdout, $stderr ) = output_from($test);

  my $ok = 1;
  my $diag;

  if ( defined($experr) && defined($expout) ) {
    if ( $stdout eq $expout ) {
      $ok = 0;
      $diag .= "STDOUT:\n$stdout\nmatching:\n$expout\nnot expected";
    }
    if ( $stderr eq $experr ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR:\n$stderr\nmatching:\n$experr\nnot expected";
    }
  }
  elsif ( defined($expout) ) {
    $ok = ( $stdout ne $expout );
    $diag = "STDOUT:\n$stdout\nmatching:\n$expout\nnot expected";
  }
  elsif ( defined($experr) ) {
    $ok = ( $stderr ne $experr );
    $diag = "STDERR:\n$stderr\nmatching:\n$experr\nnot expected";
  }
  else {
    if ( $stdout eq '' ) {
      $ok   = 0;
      $diag = "STDOUT:\n$stdout\nmatching:\n\nnot expected";
    }
    if ( $stderr eq '' ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR:\n$stderr\nmatching:\n\nnot expected";
    }
  }

  $Test->ok( $ok, $description ) || $Test->diag($diag);

  return $ok;
}

=item B<output_like>

=item B<output_unlike>

  output_like  ( $coderef, $regex_stdout, $regex_stderr, 'description' );
  output_like  { ... } $regex_stdout, $regex_stderr, 'description';
  output_unlike( $coderef, $regex_stdout, $regex_stderr, 'description' );
  output_unlike { ... } $regex_stdout, $regex_stderr, 'description';

C<output_like()> and C<output_unlike()> follow the same principles as C<output_is()>
and C<output_isnt()> except they use a regular expression for matching.

C<output_like()> attempts to match C<$regex_stdout> and C<$regex_stderr> against
C<STDOUT> and C<STDERR> produced by $coderef. The test passes if both match.

  output_like(sub {print "foo"; print STDERR "bar";},qr/foo/,qr/bar/);

The above test is successful.

Like C<output_is()>, setting either C<$regex_stdout> or C<$regex_stderr> to
C<undef> ignores the output to that facility.

  output_like(sub {print "foo"; print STDERR "bar";},qr/foo/,undef);

is the same as

  stdout_like(sub {print "foo"; print STDERR "bar";},qr/foo/);

C<output_unlike()> test pass if output from C<$coderef> doesn't match
C<$regex_stdout> and C<$regex_stderr>.

=back

=cut

sub output_like (&$$;$$) {
  my $test        = shift;
  my $expout      = shift;
  my $experr      = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my ( $stdout, $stderr ) = output_from($test);

  my $ok = 1;

  unless (
    my $regextest = _chkregex(
      'output_like_STDERR' => $experr,
      'output_like_STDOUT' => $expout
    )
    )
  {
    return $regextest;
  }

  my $diag;
  if ( defined($experr) && defined($expout) ) {
    unless ( $stdout =~ $expout ) {
      $ok = 0;
      $diag .= "STDOUT:\n$stdout\ndoesn't match:\n$expout\nas expected";
    }
    unless ( $stderr =~ $experr ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR:\n$stderr\ndoesn't match:\n$experr\nas expected";
    }
  }
  elsif ( defined($expout) ) {
    $ok = ( $stdout =~ $expout );
    $diag .= "STDOUT:\n$stdout\ndoesn't match:\n$expout\nas expected";
  }
  elsif ( defined($experr) ) {
    $ok = ( $stderr =~ $experr );
    $diag .= "STDERR:\n$stderr\ndoesn't match:\n$experr\nas expected";
  }
  else {
    unless ( $stdout eq '' ) {
      $ok = 0;
      $diag .= "STDOUT is:\n$stdout\nnot:\n\nas expected";
    }
    unless ( $stderr eq '' ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR is:\n$stderr\nnot:\n\nas expected";
    }
  }

  $Test->ok( $ok, $description ) || $Test->diag($diag);

  return $ok;
}

sub output_unlike (&$$;$$) {
  my $test        = shift;
  my $expout      = shift;
  my $experr      = shift;
  my $options     = shift if ( ref( $_[0] ) );
  my $description = shift;

  my ( $stdout, $stderr ) = output_from($test);

  my $ok = 1;

  unless (
    my $regextest = _chkregex(
      'output_unlike_STDERR' => $experr,
      'output_unlike_STDOUT' => $expout
    )
    )
  {
    return $regextest;
  }

  my $diag;
  if ( defined($experr) && defined($expout) ) {
    if ( $stdout =~ $expout ) {
      $ok = 0;
      $diag .= "STDOUT:\n$stdout\nmatches:\n$expout\nnot expected";
    }
    if ( $stderr =~ $experr ) {
      $diag .= "\n" unless ($ok);
      $ok = 0;
      $diag .= "STDERR:\n$stderr\nmatches:\n$experr\nnot expected";
    }
  }
  elsif ( defined($expout) ) {
    $ok = ( $stdout !~ $expout );
    $diag .= "STDOUT:\n$stdout\nmatches:\n$expout\nnot expected";
  }
  elsif ( defined($experr) ) {
    $ok = ( $stderr !~ $experr );
    $diag .= "STDERR:\n$stderr\nmatches:\n$experr\nnot expected";
  }

  $Test->ok( $ok, $description ) || $Test->diag($diag);

  return $ok;
}

=head1 EXPORTS

By default, all subroutines are exported by default.

=over 4

=item * :stdout - the subs with C<stdout> in the name.

=item * :stderr - the subs with C<stderr> in the name.

=item * :functions - the subs with C<_from> at the end.

=item * :output - the subs with C<output> in the name.

=item * :combined - the subs with C<combined> in the name.

=item * :tests - everything that outputs TAP

=item * :all - everything (which is the same as the default)

=back

=head1 FUNCTIONS

=cut

=head2 stdout_from

  my $stdout = stdout_from($coderef)
  my $stdout = stdout_from { ... };

stdout_from() executes $coderef and captures STDOUT.

=cut

sub stdout_from (&) {
  my $test = shift;

  my $stdout = capture_stdout {
    select( ( select(STDOUT), $| = 1 )[0] );
    $test->()
  };

  return $stdout;
}

=head2 stderr_from

  my $stderr = stderr_from($coderef)
  my $stderr = stderr_from { ... };

C<stderr_from()> executes C<$coderef> and captures C<STDERR>.

=cut

sub stderr_from (&) {
  my $test = shift;

  # XXX why is this here and not in output_from or combined_from -- xdg, 2012-05-13
  local $SIG{__WARN__} = sub { print STDERR @_ }
    if $] < 5.008;

  my $stderr = capture_stderr {
    select( ( select(STDERR), $| = 1 )[0] );
    $test->()
  };

  return $stderr;
}

=head2 output_from

  my ($stdout, $stderr) = output_from($coderef)
  my ($stdout, $stderr) = output_from {...};

C<output_from()> executes C<$coderef> one time capturing both C<STDOUT> and C<STDERR>.

=cut

sub output_from (&) {
  my $test = shift;

  my ($stdout, $stderr) = capture {
    select( ( select(STDOUT), $| = 1 )[0] );
    select( ( select(STDERR), $| = 1 )[0] );
    $test->();
  };

  return ( $stdout, $stderr );
}

=head2 combined_from

  my $combined = combined_from($coderef);
  my $combined = combined_from {...};

C<combined_from()> executes C<$coderef> one time combines C<STDOUT> and C<STDERR>, and
captures them. C<combined_from()> is equivalent to using C<< 2>&1 >> in UNIX.

=cut

sub combined_from (&) {
  my $test = shift;

  my $combined = capture_merged {
    select( ( select(STDOUT), $| = 1 )[0] );
    select( ( select(STDERR), $| = 1 )[0] );
    $test->();
  };

  return $combined;
}

sub _chkregex {
  my %regexs = @_;

  foreach my $test ( keys(%regexs) ) {
    next unless ( defined( $regexs{$test} ) );

    my $usable_regex = $Test->maybe_regex( $regexs{$test} );
    unless ( defined($usable_regex) ) {
      my $ok = $Test->ok( 0, $test );

      $Test->diag("'$regexs{$test}' doesn't look much like a regex to me.");
#       unless $ok;

      return $ok;
    }
  }
  return 1;
}

=head1 AUTHOR

Currently maintained by brian d foy, C<bdfoy@cpan.org>.

Shawn Sorichetti, C<< <ssoriche@cpan.org> >>

=head1 SOURCE AVAILABILITY

This module is in Github:

	http://github.com/briandfoy/test-output/tree/master

=head1 BUGS

Please report any bugs or feature requests to
C<bug-test-output@rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org>.  I will be notified, and then you'll automatically
be notified of progress on your bug as I make changes.

=head1 ACKNOWLEDGEMENTS

Thanks to chromatic whose TieOut.pm was the basis for capturing output.

Also thanks to rjbs for his help cleaning the documentation, and pushing me to
L<Sub::Exporter>. (This feature has been removed since it uses none of
L<Sub::Exporter>'s strengths).

Thanks to David Wheeler for providing code block support and tests.

Thanks to Michael G Schwern for the solution to combining C<STDOUT> and C<STDERR>.

=head1 COPYRIGHT & LICENSE

Copyright 2005-2013 Shawn Sorichetti, All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

1;    # End of Test::Output
