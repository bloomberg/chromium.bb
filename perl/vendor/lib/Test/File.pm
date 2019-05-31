package Test::File;
use strict;

use base qw(Exporter);
use vars qw(@EXPORT $VERSION);

use File::Spec;
use Test::Builder;

@EXPORT = qw(
	file_exists_ok file_not_exists_ok
	file_empty_ok file_not_empty_ok file_size_ok file_max_size_ok
	file_min_size_ok file_readable_ok file_not_readable_ok file_writeable_ok
	file_not_writeable_ok file_executable_ok file_not_executable_ok
	file_mode_is file_mode_isnt
	file_mode_has file_mode_hasnt
	file_is_symlink_ok
	symlink_target_exists_ok symlink_target_is
	symlink_target_dangles_ok
	dir_exists_ok dir_contains_ok
	link_count_is_ok link_count_gt_ok link_count_lt_ok
	owner_is owner_isnt
	group_is group_isnt
	file_line_count_is file_line_count_isnt file_line_count_between
	file_contains_like file_contains_unlike
	file_contains_utf8_like file_contains_utf8_unlike
	file_contains_encoded_like file_contains_encoded_unlike
	file_mtime_gt_ok file_mtime_lt_ok file_mtime_age_ok
	);

$VERSION = '1.443';

{
use warnings;
}

my $Test = Test::Builder->new();

=encoding utf8

=head1 NAME

Test::File -- test file attributes

=head1 SYNOPSIS

  use Test::File;

=head1 DESCRIPTION

This modules provides a collection of test utilities for file
attributes.

Some file attributes depend on the owner of the process testing the
file in the same way the file test operators do.  For instance, root
(or super-user or Administrator) may always be able to read files no
matter the permissions.

Some attributes don't make sense outside of Unix, either, so some
tests automatically skip if they think they won't work on the
platform.  If you have a way to make these functions work on Windows,
for instance, please send me a patch. :) IF you want to pretend to be
Windows on a non-Windows machine (for instance, to test C<skip()>),
you can set the C<PRETEND_TO_BE_WINDOWS> environment variable.

The optional NAME parameter for every function allows you to specify a
name for the test.  If not supplied, a reasonable default will be
generated.

=head2 Functions

=cut

sub _normalize
	{
	my $file = shift;
	return unless defined $file;

	return $file =~ m|/|
		? File::Spec->catfile( split m|/|, $file )
		: $file;
	}

sub _win32
	{
	return 0 if $^O eq 'darwin';
	return $ENV{PRETEND_TO_BE_WIN32} if defined $ENV{PRETEND_TO_BE_WIN32};
	return $^O =~ m/Win/;
	}

# returns true if symlinks can't exist
sub _no_symlinks_here { ! eval { symlink("",""); 1 } }

# owner_is and owner_isn't should skip on OS where the question makes no
# sense.  I really don't know a good way to test for that, so I'm going
# to skip on the two OS's that I KNOW aren't multi-user.  I'd love to add
# more if anyone knows of any
#   Note:  I don't have a dos or mac os < 10 machine to test this on
sub _obviously_non_multi_user
	{
	foreach my $os ( qw(dos MacOS) ) { return 1 if $^O eq $os }

	return 0 if $^O eq 'MSWin32';

	eval { my $holder = getpwuid(0) };
	return 1 if $@;

	eval { my $holder = getgrgid(0) };
	return 1 if $@;

	return 0;
	}

=over 4

=item file_exists_ok( FILENAME [, NAME ] )

Ok if the file exists, and not ok otherwise.

=cut

sub file_exists_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename exists";

	my $ok = -e $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag("File [$filename] does not exist");
		$Test->ok(0, $name);
		}
	}

=item file_not_exists_ok( FILENAME [, NAME ] )

Ok if the file does not exist, and not okay if it does exist.

=cut

sub file_not_exists_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename does not exist";

	my $ok = not -e $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag("File [$filename] exists");
		$Test->ok(0, $name);
		}
	}

=item file_empty_ok( FILENAME [, NAME ] )

Ok if the file exists and has empty size, not ok if the
file does not exist or exists with non-zero size.

=cut

sub file_empty_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is empty";

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = -z $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] exists with non-zero size!" );
		$Test->ok(0, $name);
		}
	}

=item file_not_empty_ok( FILENAME [, NAME ] )

Ok if the file exists and has non-zero size, not ok if the
file does not exist or exists with zero size.

=cut

sub file_not_empty_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is not empty";

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = not -z _;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] exists with zero size!" );
		$Test->ok(0, $name);
		}
	}

=item file_size_ok( FILENAME, SIZE [, NAME ]  )

Ok if the file exists and has SIZE size in bytes (exactly), not ok if
the file does not exist or exists with size other than SIZE.

=cut

sub file_size_ok
	{
	my $filename = _normalize( shift );
	my $expected = int shift;
	my $name     = shift || "$filename has right size";

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = ( -s $filename ) == $expected;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		my $actual = -s $filename;
		$Test->diag(
			"File [$filename] has actual size [$actual] not [$expected]!" );

		$Test->ok(0, $name);
		}
	}

=item file_max_size_ok( FILENAME, MAX [, NAME ] )

Ok if the file exists and has size less than or equal to MAX bytes, not
ok if the file does not exist or exists with size greater than MAX
bytes.

=cut

sub file_max_size_ok
	{
	my $filename = _normalize( shift );
	my $max      = int shift;
	my $name     = shift || "$filename is under $max bytes";

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = ( -s $filename ) <= $max;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		my $actual = -s $filename;
		$Test->diag(
			"File [$filename] has actual size [$actual] " .
			"greater than [$max]!"
			);

		$Test->ok(0, $name);
		}
	}

=item file_min_size_ok( FILENAME, MIN [, NAME ] )

Ok if the file exists and has size greater than or equal to MIN bytes,
not ok if the file does not exist or exists with size less than MIN
bytes.

=cut

sub file_min_size_ok
	{
	my $filename = _normalize( shift );
	my $min      = int shift;
	my $name     = shift || "$filename is over $min bytes";

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = ( -s $filename ) >= $min;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		my $actual = -s $filename;
		$Test->diag(
			"File [$filename] has actual size ".
			"[$actual] less than [$min]!"
			);

		$Test->ok(0, $name);
		}
	}

=item file_line_count_is( FILENAME, COUNT [, NAME ]  )

Ok if the file exists and has COUNT lines (exactly), not ok if the
file does not exist or exists with a line count other than COUNT.

This function uses the current value of C<$/> as the line ending and
counts the lines by reading them and counting how many it read.

=cut

sub _ENOFILE   () { -1 }
sub _ECANTOPEN () { -2 }

sub _file_line_counter
	{
	my $filename = shift;

	return _ENOFILE   unless -e $filename;  # does not exist

	return _ECANTOPEN unless open my( $fh ), "<", $filename;

	my $count = 0;
	while( <$fh> ) { $count++ }

	return $count;
	}

# XXX: lots of cut and pasting here, needs refactoring
# looks like the refactoring might be worse than this though
sub file_line_count_is
	{
	my $filename = _normalize( shift );
	my $expected = shift;
	my $name     = do {
		no warnings 'uninitialized';
		shift || "$filename line count is $expected lines";
		};

	unless( defined $expected && int( $expected ) == $expected )
		{
		no warnings 'uninitialized';
		$Test->diag( "file_line_count_is expects a positive whole number for " .
			"the second argument. Got [$expected]!" );
		return $Test->ok( 0, $name );
		}

	my $got = _file_line_counter( $filename );

	if( $got eq _ENOFILE )
		{
		$Test->diag( "File [$filename] does not exist!" );
		$Test->ok( 0, $name );
		}
	elsif( $got == _ECANTOPEN )
		{
		$Test->diag( "Could not open [$filename]: \$! is [$!]!" );
		$Test->ok( 0, $name );
		}
	elsif( $got == $expected )
		{
		$Test->ok( 1, $name );
		}
	else
		{
		$Test->diag( "Expected [$expected] lines in [$filename], " .
			"got [$got] lines!" );
		$Test->ok( 0, $name );
		}

	}

=item file_line_count_isnt( FILENAME, COUNT [, NAME ]  )

Ok if the file exists and doesn't have exactly COUNT lines, not ok if the
file does not exist or exists with a line count of COUNT. Read that
carefully: the file must exist for this test to pass!

This function uses the current value of C<$/> as the line ending and
counts the lines by reading them and counting how many it read.

=cut

sub file_line_count_isnt
	{
	my $filename = _normalize( shift );
	my $expected = shift;
	my $name     = do {
		no warnings 'uninitialized';
		shift || "$filename line count is not $expected lines";
		};

	unless( defined $expected && int( $expected ) == $expected )
		{
		no warnings 'uninitialized';
		$Test->diag( "file_line_count_is expects a positive whole number for " .
			"the second argument. Got [$expected]!" );
		return $Test->ok( 0, $name );
		}

	my $got = _file_line_counter( $filename );

	if( $got eq _ENOFILE )
		{
		$Test->diag( "File [$filename] does not exist!" );
		$Test->ok( 0, $name );
		}
	elsif( $got == _ECANTOPEN )
		{
		$Test->diag( "Could not open [$filename]: \$! is [$!]!" );
		$Test->ok( 0, $name );
		}
	elsif( $got != $expected )
		{
		$Test->ok( 1, $name );
		}
	else
		{
		$Test->diag( "Expected something other than [$expected] lines in [$filename], " .
			"but got [$got] lines!" );
		$Test->ok( 0, $name );
		}

	}

=item file_line_count_between( FILENAME, MIN, MAX, [, NAME ]  )

Ok if the file exists and has a line count between MIN and MAX, inclusively.

This function uses the current value of C<$/> as the line ending and
counts the lines by reading them and counting how many it read.

=cut

sub file_line_count_between
	{
	my $filename = _normalize( shift );
	my $min      = shift;
	my $max      = shift;

	my $name = do {
		no warnings 'uninitialized';
		shift || "$filename line count is between [$min] and [$max] lines";
		};

	foreach my $ref ( \$min, \$max )
		{
		unless( defined $$ref && int( $$ref ) == $$ref )
			{
			no warnings 'uninitialized';
			$Test->diag( "file_line_count_between expects positive whole numbers for " .
				"the second and third arguments. Got [$min] and [$max]!" );
			return $Test->ok( 0, $name );
			}
		}


	my $got = _file_line_counter( $filename );

	if( $got eq _ENOFILE )
		{
		$Test->diag( "File [$filename] does not exist!" );
		$Test->ok( 0, $name );
		}
	elsif( $got == _ECANTOPEN )
		{
		$Test->diag( "Could not open [$filename]: \$! is [$!]!" );
		$Test->ok( 0, $name );
		}
	elsif( $min <= $got and $got <= $max )
		{
		$Test->ok( 1, $name );
		}
	else
		{
		$Test->diag( "Expected a line count between [$min] and [$max] " .
			"in [$filename], but got [$got] lines!"
			);
		$Test->ok( 0, $name );
		}

	}

=item file_contains_like ( FILENAME, PATTERN [, NAME ] )

Ok if the file exists and its contents (as one big string) match
PATTERN, not ok if the file does not exist, is not readable, or exists
but doesn't match PATTERN.

Since the file contents are read into memory, you should not use this
for large files.  Besides memory consumption, test diagnostics for
failing tests might be difficult to decipher.  However, for short
files this works very well.

Because the entire contents are treated as one large string, you can
make a pattern that tests multiple lines.  Don't forget that you may
need to use the /s modifier for such patterns:

	# make sure file has one or more paragraphs with CSS class X
	file_contains_like($html_file, qr{<p class="X">.*?</p>}s);

Contrariwise, if you need to match at the beginning or end of a line
inside the file, use the /m modifier:

	# make sure file has a setting for foo
	file_contains_like($config_file, qr/^ foo \s* = \s* \w+ $/mx);

If you want to test your file contents against multiple patterns, but
don't want to have the file read in repeatedly, you can pass an
arrayref of patterns instead of a single pattern, like so:

	# make sure our template has rendered correctly
	file_contains_like($template_out,
		[
		qr/^ $title_line $/mx,
		map { qr/^ $_ $/mx } @chapter_headings,
		qr/^ $footer_line $/mx,
		]);

Please note that if you do this, and your file does not exist or is
not readable, you'll only get one test failure instead of a failure
for each pattern.  This could cause your test plan to be off, although
you may not care at that point because your test failed anyway.  If
you do care, either skip the test plan altogether by employing
L<Test::More>'s C<done_testing()> function, or use
L</file_readable_ok> in conjunction with a C<SKIP> block.

Contributed by Buddy Burden C<< <barefoot@cpan.org> >>.

=item file_contains_unlike ( FILENAME, PATTERN [, NAME ] )

Ok if the file exists and its contents (as one big string) do B<not>
match PATTERN, not ok if the file does not exist, is not readable, or
exists but matches PATTERN.

All notes and caveats for L</file_contains_like> apply to this
function as well.

Contributed by Buddy Burden C<< <barefoot@cpan.org> >>.

=item file_contains_utf8_like ( FILENAME, PATTERN [, NAME ] )

The same as C<file_contains_like>, except the file is opened as UTF-8.

=item file_contains_utf8_unlike ( FILENAME, PATTERN [, NAME ] )

The same as C<file_contains_unlike>, except the file is opened as UTF-8.

=item file_contains_encoded_like ( FILENAME, ENCODING, PATTERN [, NAME ] )

The same as C<file_contains_like>, except the file is opened with ENCODING

=item file_contains_encoded_unlike ( FILENAME, ENCODING, PATTERN [, NAME ] )

The same as C<file_contains_unlike>, except the file is opened with ENCODING.

=cut

sub file_contains_like
	{
		local $Test::Builder::Level = $Test::Builder::Level + 1;
		_file_contains(like => "contains", undef, @_);
	}

sub file_contains_unlike
	{
		local $Test::Builder::Level = $Test::Builder::Level + 1;
		_file_contains(unlike => "doesn't contain", undef, @_);
	}

sub file_contains_utf8_like
	{
		local $Test::Builder::Level = $Test::Builder::Level + 1;
		_file_contains(like => "contains", 'UTF-8', @_);
	}
sub file_contains_utf8_unlike
	{
		local $Test::Builder::Level = $Test::Builder::Level + 1;
		_file_contains(unlike => "doesn't contain", 'UTF-8', @_);
	}

sub file_contains_encoded_like
	{
		local $Test::Builder::Level = $Test::Builder::Level + 1;
		my $filename = shift;
		my $encoding = shift;
		_file_contains(like => "contains", $encoding, $filename, @_);
	}
sub file_contains_encoded_unlike
	{
		local $Test::Builder::Level = $Test::Builder::Level + 1;
		my $filename = shift;
		my $encoding = shift;
		_file_contains(unlike => "doesn't contain", $encoding, $filename, @_);
	}

sub _file_contains
	{
	my $method   = shift;
	my $verb     = shift;
	my $encoding = shift;
	my $filename = _normalize( shift );
	my $patterns = shift;
	my $name     = shift;

	my (@patterns, %patterns);
	if (ref $patterns eq 'ARRAY')
		{
		@patterns = @$patterns;
		%patterns = map { $_ => $name || "$filename $verb $_" } @patterns;
		}
		else
		{
		@patterns = ($patterns);
		%patterns = ( $patterns => $name || "$filename $verb $patterns" );
		}

	# for purpose of checking the file's existence, just use the first
	# test name as the name
	$name = $patterns{$patterns[0]};

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	unless( -r $filename )
		{
		$Test->diag( "File [$filename] is not readable!" );
		return $Test->ok(0, $name);
		}

	# do the slurp
	my $file_contents;
	{
	unless (open(FH, $filename))
		{
		$Test->diag( "Could not open [$filename]: \$! is [$!]!" );
		return $Test->ok( 0, $name );
		}

	if (defined $encoding) {
		binmode FH, ":encoding($encoding)";
	}

	local $/ = undef;
	$file_contents = <FH>;
	close FH;
	}

	foreach my $p (@patterns)
		{
		$Test->$method($file_contents, $p, $patterns{$p});
		}
	}

=item file_readable_ok( FILENAME [, NAME ] )

Ok if the file exists and is readable, not ok
if the file does not exist or is not readable.

=cut

sub file_readable_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is readable";

	my $ok = -r $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] is not readable!" );
		$Test->ok(0, $name);
		}
	}

=item file_not_readable_ok( FILENAME [, NAME ] )

Ok if the file exists and is not readable, not ok
if the file does not exist or is readable.

=cut

sub file_not_readable_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is not readable";

	my $ok = not -r $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] is readable!" );
		$Test->ok(0, $name);
		}
	}

=item file_writeable_ok( FILENAME [, NAME ] )

Ok if the file exists and is writeable, not ok
if the file does not exist or is not writeable.

=cut

sub file_writeable_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is writeable";

	my $ok = -w $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] is not writeable!" );
		$Test->ok(0, $name);
		}
	}

=item file_not_writeable_ok( FILENAME [, NAME ] )

Ok if the file exists and is not writeable, not ok
if the file does not exist or is writeable.

=cut

sub file_not_writeable_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is not writeable";

	my $ok = not -w $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag("File [$filename] is writeable!");
		$Test->ok(0, $name);
		}
	}

=item file_executable_ok( FILENAME [, NAME ] )

Ok if the file exists and is executable, not ok
if the file does not exist or is not executable.

This test automatically skips if it thinks it is on a
Windows platform.

=cut

sub file_executable_ok
	{
	if( _win32() )
		{
		$Test->skip( "file_executable_ok doesn't work on Windows!" );
		return;
		}

	my $filename = _normalize( shift );
	my $name     = shift || "$filename is executable";

	my $ok = -x $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag("File [$filename] is not executable!");
		$Test->ok(0, $name);
		}
	}

=item file_not_executable_ok( FILENAME [, NAME ] )

Ok if the file exists and is not executable, not ok
if the file does not exist or is executable.

This test automatically skips if it thinks it is on a
Windows platform.

=cut

sub file_not_executable_ok
	{
	if( _win32() )
		{
		$Test->skip( "file_not_executable_ok doesn't work on Windows!" );
		return;
		}

	my $filename = _normalize( shift );
	my $name     = shift || "$filename is not executable";

	my $ok = not -x $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag("File [$filename] is executable!");
		$Test->ok(0, $name);
		}
	}

=item file_mode_is( FILENAME, MODE [, NAME ] )

Ok if the file exists and the mode matches, not ok
if the file does not exist or the mode does not match.

This test automatically skips if it thinks it is on a
Windows platform.

Contributed by Shawn Sorichetti C<< <ssoriche@coloredblocks.net> >>

=cut

sub file_mode_is
	{
	if( _win32() )
		{
		$Test->skip( "file_mode_is doesn't work on Windows!" );
		return;
		}

	my $filename = _normalize( shift );
	my $mode     = shift;

	my $name     = shift || sprintf("%s mode is %04o", $filename, $mode);

	my $ok = -e $filename && ((stat($filename))[2] & 07777) == $mode;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag(sprintf("File [%s] mode is not %04o!", $filename, $mode) );
		$Test->ok(0, $name);
		}
	}

=item file_mode_isnt( FILENAME, MODE [, NAME ] )

Ok if the file exists and mode does not match, not ok
if the file does not exist or mode does match.

This test automatically skips if it thinks it is on a
Windows platform.

Contributed by Shawn Sorichetti C<< <ssoriche@coloredblocks.net> >>

=cut

sub file_mode_isnt
	{
	if( _win32() )
		{
		$Test->skip( "file_mode_isnt doesn't work on Windows!" );
		return;
		}

	my $filename = _normalize( shift );
	my $mode     = shift;

	my $name     = shift || sprintf("%s mode is not %04o",$filename,$mode);

	my $ok = not (-e $filename && ((stat($filename))[2] & 07777) == $mode);

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag(sprintf("File [%s] mode is %04o!",$filename,$mode));
		$Test->ok(0, $name);
		}
	}

=item file_mode_has( FILENAME, MODE [, NAME ] )

Ok if the file exists and has all the bits in mode turned on, not ok
if the file does not exist or the mode does not match.  That is,
C<< FILEMODE & MODE == MODE >> must be true.

This test automatically skips if it thinks it is on a
Windows platform.

Contributed by Ricardo Signes C<< <rjbs@cpan.org> >>

=cut

sub file_mode_has
	{
	if( _win32() )
		{
		$Test->skip( "file_mode_has doesn't work on Windows!" );
		return;
		}

	my $filename = _normalize( shift );
	my $mode     = shift;

	my $name     = shift || sprintf("%s mode has all bits of %04o", $filename, $mode);

	my $present = -e $filename;
	my $gotmode = $present ? (stat($filename))[2] : undef;
	my $ok      = $present && ($gotmode & $mode) == $mode;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		my $missing = ($gotmode ^ $mode) & $mode;
		$Test->diag(sprintf("File [%s] mode is missing component %04o!", $filename, $missing) );
		$Test->ok(0, $name);
		}
	}

=item file_mode_hasnt( FILENAME, MODE [, NAME ] )

Ok if the file exists and has all the bits in mode turned off, not ok
if the file does not exist or the mode does not match.  That is,
C<< FILEMODE & MODE == 0 >> must be true.

This test automatically skips if it thinks it is on a
Windows platform.

Contributed by Ricardo Signes C<< <rjbs@cpan.org> >>

=cut

sub file_mode_hasnt
	{
	if( _win32() )
		{
		$Test->skip( "file_mode_hasnt doesn't work on Windows!" );
		return;
		}

	my $filename = _normalize( shift );
	my $mode     = shift;

	my $name     = shift || sprintf("%s mode has no bits of %04o", $filename, $mode);

	my $present = -e $filename;
	my $gotmode = $present ? (stat($filename))[2] : undef;
	my $ok      = $present && ($gotmode & $mode) == 0;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		my $bad = $gotmode & $mode;
		$Test->diag(sprintf("File [%s] mode has forbidden component %04o!", $filename, $bad) );
		$Test->ok(0, $name);
		}
	}

=item file_is_symlink_ok( FILENAME [, NAME ] )

Ok if FILENAME is a symlink, even if it points to a non-existent
file. This test automatically skips if the operating system does
not support symlinks. If the file does not exist, the test fails.

=cut

sub file_is_symlink_ok
	{
	if( _no_symlinks_here() )
		{
		$Test->skip(
			"file_is_symlink_ok doesn't work on systems without symlinks!" );
		return;
		}

	my $file = shift;
	my $name = shift || "$file is a symlink";

	if( -l $file )
		{
		$Test->ok(1, $name)
		}
	else
		{
		$Test->diag( "File [$file] is not a symlink!" );
		$Test->ok(0, $name);
		}
	}

=item symlink_target_exists_ok( SYMLINK [, TARGET] [, NAME ] )

Ok if FILENAME is a symlink and it points to a existing file. With the
optional TARGET argument, the test fails if SYMLINK's target is not
TARGET. This test automatically skips if the operating system does not
support symlinks. If the file does not exist, the test fails.

=cut

sub symlink_target_exists_ok
	{
	if( _no_symlinks_here() )
		{
		$Test->skip(
			"symlink_target_exists_ok doesn't work on systems without symlinks!"
			);
		return;
		}

	my $file = shift;
	my $dest = shift || readlink( $file );
	my $name = shift || "$file is a symlink";

	unless( -l $file )
		{
		$Test->diag( "File [$file] is not a symlink!" );
		return $Test->ok( 0, $name );
		}

	unless( -e $dest )
		{
		$Test->diag( "Symlink [$file] points to non-existent target [$dest]!" );
		return $Test->ok( 0, $name );
		}

	my $actual = readlink( $file );
	unless( $dest eq $actual )
		{
		$Test->diag(
			"Symlink [$file] points to\n" .
			"         got: $actual\n" .
			"    expected: $dest\n"
			);
		return $Test->ok( 0, $name );
		}

	$Test->ok( 1, $name );
	}

=item symlink_target_dangles_ok( SYMLINK [, NAME ] )

Ok if FILENAME is a symlink and if it doesn't point to a existing
file. This test automatically skips if the operating system does not
support symlinks. If the file does not exist, the test fails.

=cut

sub symlink_target_dangles_ok
	{
	if( _no_symlinks_here() )
		{
		$Test->skip(
			"symlink_target_dangles_ok doesn't work on systems without symlinks!" );
		return;
		}

	my $file = shift;
	my $dest = readlink( $file );
	my $name = shift || "$file is a symlink";

	unless( -l $file )
		{
		$Test->diag( "File [$file] is not a symlink!" );
		return $Test->ok( 0, $name );
		}

	if( -e $dest )
		{
		$Test->diag(
			"Symlink [$file] points to existing file [$dest] but shouldn't!" );
		return $Test->ok( 0, $name );
		}

	$Test->ok( 1, $name );
	}

=item symlink_target_is( SYMLINK, TARGET [, NAME ] )

Ok if FILENAME is a symlink and if points to TARGET. This test
automatically skips if the operating system does not support symlinks.
If the file does not exist, the test fails.

=cut

sub symlink_target_is
	{
	if( _no_symlinks_here() )
		{
		$Test->skip(
			"symlink_target_is doesn't work on systems without symlinks!" );
		return;
		}

	my $file = shift;
	my $dest = shift;
	my $name = shift || "symlink $file points to $dest";

	unless( -l $file )
		{
		$Test->diag( "File [$file] is not a symlink!" );
		return $Test->ok( 0, $name );
		}

	my $actual_dest = readlink( $file );
	my $link_error  = $!;

	unless( defined $actual_dest )
		{
		$Test->diag( "Symlink [$file] does not have a defined target!" );
		$Test->diag( "readlink error: $link_error" ) if defined $link_error;
		return $Test->ok( 0, $name );
		}

	if( $dest eq $actual_dest )
		{
		$Test->ok( 1, $name );
		}
	else
		{
		$Test->ok( 0, $name );
		$Test->diag("       got: $actual_dest" );
		$Test->diag("  expected: $dest" );
		}
	}

=item symlink_target_is_absolute_ok( SYMLINK [, NAME ] )

Ok if FILENAME is a symlink and if its target is an absolute path.
This test automatically skips if the operating system does not support
symlinks. If the file does not exist, the test fails.

=cut

sub symlink_target_is_absolute_ok
	{
	if( _no_symlinks_here() )
		{
		$Test->skip(
			"symlink_target_exists_ok doesn't work on systems without symlinks" );
		return;
		}

	my( $from, $from_base, $to, $to_base, $name ) = @_;
	my $link     = readlink( $from );
	my $link_err = defined( $link ) ? '' : $!; # $! doesn't always get reset
	my $link_abs = abs_path( rel2abs($link, $from_base) );
	my $to_abs   = abs_path( rel2abs($to, $to_base) );

	if (defined( $link_abs ) && defined( $to_abs ) && $link_abs eq $to_abs) {
		$Test->ok( 1, $name );
		}
	else {
		$Test->ok( 0, $name );
		$link     ||= 'undefined';
		$link_abs ||= 'undefined';
		$to_abs   ||= 'undefined';

		$Test->diag("    link: $from");
		$Test->diag("     got: $link");
		$Test->diag("    (abs): $link_abs");
		$Test->diag("  expected: $to");
		$Test->diag("    (abs): $to_abs");
		$Test->diag("  readlink() error: $link_err") if ($link_err);
		}
	}

=item dir_exists_ok( DIRECTORYNAME [, NAME ] )

Ok if the file exists and is a directory, not ok if the file doesn't exist, or exists but isn't a
directory.

Contributed by Buddy Burden C<< <barefoot@cpan.org> >>.

=cut

sub dir_exists_ok
	{
	my $filename = _normalize( shift );
	my $name     = shift || "$filename is a directory";

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = -d $filename;

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] exists but is not a directory!" );
		$Test->ok(0, $name);
		}
	}

=item dir_contains_ok( DIRECTORYNAME, FILENAME [, NAME ] )

Ok if the directory exists and contains the file, not ok if the directory doesn't exist, or exists
but doesn't contain the file.

Contributed by Buddy Burden C<< <barefoot@cpan.org> >>.

=cut

sub dir_contains_ok
	{
	my $dirname  = _normalize( shift );
	my $filename = _normalize( shift );
	my $name     = shift || "directory $dirname contains file $filename";

	unless( -d $dirname )
		{
		$Test->diag( "Directory [$dirname] does not exist!" );
		return $Test->ok(0, $name);
		}

	my $ok = -e File::Spec->catfile($dirname, $filename);

	if( $ok )
		{
		$Test->ok(1, $name);
		}
	else
		{
		$Test->diag( "File [$filename] does not exist in directory $dirname!" );
		$Test->ok(0, $name);
		}
	}

=item link_count_is_ok( FILE, LINK_COUNT [, NAME ] )

Ok if the link count to FILE is LINK_COUNT. LINK_COUNT is interpreted
as an integer. A LINK_COUNT that evaluates to 0 returns Ok if the file
does not exist.


=cut

sub link_count_is_ok
	{
	my $file   = shift;
	my $count  = int( 0 + shift );

	my $name   = shift || "$file has a link count of [$count]";

	my $actual = ( stat $file )[3];

	unless( $actual == $count )
		{
		$Test->diag(
			"File [$file] points has [$actual] links: expected [$count]!" );
		return $Test->ok( 0, $name );
		}

	$Test->ok( 1, $name );
	}

=item link_count_gt_ok( FILE, LINK_COUNT [, NAME ] )

Ok if the link count to FILE is greater than LINK_COUNT. LINK_COUNT is
interpreted as an integer. A LINK_COUNT that evaluates to 0 returns Ok
if the file has at least one link.

=cut

sub link_count_gt_ok
	{
	my $file   = shift;
	my $count  = int( 0 + shift );

	my $name   = shift || "$file has a link count of [$count]";

	my $actual = (stat $file )[3];

	unless( $actual > $count )
		{
		$Test->diag(
			"File [$file] points has [$actual] links: ".
			"expected more than [$count]!" );
		return $Test->ok( 0, $name );
		}

	$Test->ok( 1, $name );
	}

=item link_count_lt_ok( FILE, LINK_COUNT [, NAME ] )

Ok if the link count to FILE is less than LINK_COUNT. LINK_COUNT is
interpreted as an integer. A LINK_COUNT that evaluates to 0 returns Ok
if the file has at least one link.

=cut

sub link_count_lt_ok
	{
	my $file   = shift;
	my $count  = int( 0 + shift );

	my $name   = shift || "$file has a link count of [$count]";

	my $actual = (stat $file )[3];

	unless( $actual < $count )
		{
		$Test->diag(
			"File [$file] points has [$actual] links: ".
			"expected less than [$count]!" );
		return $Test->ok( 0, $name );
		}

	$Test->ok( 1, $name );
	}


# owner_is, owner_isnt, group_is and group_isnt are almost
# identical in the beginning, so I'm writing a skeleton they can all use.
# I can't think of a better name...
sub _dm_skeleton
	{
	no warnings 'uninitialized';

	if( _obviously_non_multi_user() )
		{
		my $calling_sub = (caller(1))[3];
		$Test->skip( $calling_sub . " only works on a multi-user OS!" );
		return 'skip';
		}

	my $filename      = _normalize( shift );
	my $testing_for   = shift;
	my $name          = shift;

	unless( defined $filename )
		{
		$Test->diag( "File name not specified!" );
		return $Test->ok( 0, $name );
		}

	unless( -e $filename )
		{
		$Test->diag( "File [$filename] does not exist!" );
		return $Test->ok( 0, $name );
		}

	return;
	}

=item owner_is( FILE , OWNER [, NAME ] )

Ok if FILE's owner is the same as OWNER.  OWNER may be a text user name
or a numeric userid.  Test skips on Dos, and Mac OS <= 9.
If the file does not exist, the test fails.

Contributed by Dylan Martin

=cut

sub owner_is
	{
	my $filename      = shift;
	my $owner         = shift;
	my $name          = shift || "$filename belongs to $owner";

	my $err = _dm_skeleton( $filename, $owner, $name );
	return if( defined( $err ) && $err eq 'skip' );
	return $err if defined($err);

	my $owner_uid = _get_uid( $owner );
	unless( defined $owner_uid )
		{
		$Test->diag("User [$owner] does not exist on this system!");
		return $Test->ok( 0, $name );
		}

	my $file_uid = ( stat $filename )[4];

	unless( defined $file_uid )
		{
		$Test->skip("stat failed to return owner uid for $filename!");
		return;
		}

	return $Test->ok( 1, $name ) if $file_uid == $owner_uid;

	my $real_owner = ( getpwuid $file_uid )[0];
	unless( defined $real_owner )
		{
		$Test->diag("File does not belong to $owner!");
		return $Test->ok( 0, $name );
		}

	$Test->diag( "File [$filename] belongs to $real_owner ($file_uid), ".
			"not $owner ($owner_uid)!" );
	return $Test->ok( 0, $name );
	}

=item owner_isnt( FILE, OWNER [, NAME ] )

Ok if FILE's owner is not the same as OWNER.  OWNER may be a text user name
or a numeric userid.  Test skips on Dos and Mac OS <= 9.  If the file
does not exist, the test fails.

Contributed by Dylan Martin

=cut

sub owner_isnt
	{
	my $filename      = shift;
	my $owner         = shift;
	my $name          = shift || "$filename doesn't belong to $owner";

	my $err = _dm_skeleton( $filename, $owner, $name );
	return if( defined( $err ) && $err eq 'skip' );
	return $err if defined($err);

	my $owner_uid = _get_uid( $owner );
	unless( defined $owner_uid )
		{
		return $Test->ok( 1, $name );
		}

	my $file_uid  = ( stat $filename )[4];

	#$Test->diag( "owner_isnt: $owner_uid $file_uid" );
	return $Test->ok( 1, $name ) if $file_uid != $owner_uid;

	$Test->diag( "File [$filename] belongs to $owner ($owner_uid)!" );
	return $Test->ok( 0, $name );
	}

=item group_is( FILE , GROUP [, NAME ] )

Ok if FILE's group is the same as GROUP.  GROUP may be a text group name or
a numeric group id.  Test skips on Dos, Mac OS <= 9 and any other operating
systems that do not support getpwuid() and friends.  If the file does not
exist, the test fails.

Contributed by Dylan Martin

=cut

sub group_is
	{
	my $filename      = shift;
	my $group         = shift;
	my $name          = ( shift || "$filename belongs to group $group" );

	my $err = _dm_skeleton( $filename, $group, $name );
	return if( defined( $err ) && $err eq 'skip' );
	return $err if defined($err);

	my $group_gid = _get_gid( $group );
	unless( defined $group_gid )
		{
		$Test->diag("Group [$group] does not exist on this system!");
		return $Test->ok( 0, $name );
		}

	my $file_gid  = ( stat $filename )[5];

	unless( defined $file_gid )
		{
		$Test->skip("stat failed to return group gid for $filename!");
		return;
		}

	return $Test->ok( 1, $name ) if $file_gid == $group_gid;

	my $real_group = ( getgrgid $file_gid )[0];
	unless( defined $real_group )
		{
		$Test->diag("File does not belong to $group!");
		return $Test->ok( 0, $name );
		}

	$Test->diag( "File [$filename] belongs to $real_group ($file_gid), ".
			"not $group ($group_gid)!" );

	return $Test->ok( 0, $name );
	}

=item group_isnt( FILE , GROUP [, NAME ] )

Ok if FILE's group is not the same as GROUP.  GROUP may be a text group name or
a numeric group id.  Test skips on Dos, Mac OS <= 9 and any other operating
systems that do not support getpwuid() and friends.  If the file does not
exist, the test fails.

Contributed by Dylan Martin

=cut

sub group_isnt
	{
	my $filename      = shift;
	my $group         = shift;
	my $name          = shift || "$filename does not belong to group $group";

	my $err = _dm_skeleton( $filename, $group, $name );
	return if( defined( $err ) && $err eq 'skip' );
	return $err if defined($err);

	my $group_gid = _get_gid( $group );
	my $file_gid  = ( stat $filename )[5];

	unless( defined $file_gid )
		{
		$Test->skip("stat failed to return group gid for $filename!");
		return;
		}

	return $Test->ok( 1, $name ) if $file_gid != $group_gid;

	$Test->diag( "File [$filename] belongs to $group ($group_gid)!" );
		return $Test->ok( 0, $name );
	}

sub _get_uid
	{
	my $owner = shift;
	my $owner_uid;

	if ($owner =~ /^\d+/)
		{
		$owner_uid = $owner;
		$owner = ( getpwuid $owner )[0];
		}
	else
		{
		$owner_uid = (getpwnam($owner))[2];
		}

	$owner_uid;
	}

sub _get_gid
	{
	my $group = shift;
	my $group_uid;

	if ($group =~ /^\d+/)
		{
		$group_uid = $group;
		$group = ( getgrgid $group )[0];
		}
	else
		{
		$group_uid = (getgrnam($group))[2];
		}

	$group_uid;
	}


=item file_mtime_age_ok( FILE [, WITHIN_SECONDS ] [, NAME ] )

Ok if FILE's modified time is WITHIN_SECONDS inclusive of the system's current time.
This test uses stat() to obtain the mtime. If the file does not exist the test
returns failure. If stat() fails, the test is skipped.

=cut

sub file_mtime_age_ok
	{
	my $filename    = shift;
	my $within_secs = int shift || 0;
	my $name        = shift || "$filename mtime within $within_secs seconds of current time";

	my $time        = time();

	my $filetime = _stat_file($filename, 9);

	return if ( $filetime == -1 ); #skip

	return $Test->ok(1, $name) if ( $filetime + $within_secs > $time-1  );

	$Test->diag( "Filename [$filename] mtime [$filetime] is not $within_secs seconds within current system time [$time].");
	return $Test->ok(0, $name);
	}

=item file_mtime_gt_ok( FILE, UNIXTIME [, NAME ] )

Ok if FILE's mtime is > UNIXTIME. This test uses stat() to get the mtime. If stat() fails
this test is skipped. If FILE does not exist, this test fails.

=cut

sub file_mtime_gt_ok
	{
	my $filename    = shift;
	my $time        = int shift;
	my $name        = shift || "$filename mtime is less than unix timestamp $time";

	my $filetime = _stat_file($filename, 9);

	return if ( $filetime == -1 ); #skip

	return $Test->ok(1, $name) if ( $filetime > $time );

	$Test->diag( "Filename [$filename] mtime [$filetime] not greater than $time" );
	$Test->ok(0, $name);
  }

=item file_mtime_lt_ok( FILE, UNIXTIME, [, NAME ] )

Ok if FILE's modified time is < UNIXTIME.  This test uses stat() to get the mtime. If stat() fails
this test is skipped. If FILE does not exist, this test fails.

=cut

sub file_mtime_lt_ok
	{
	my $filename = shift;
	my $time = int shift;
	my $name = shift || "$filename mtime less than unix timestamp $time";

  # gets mtime
	my $filetime = _stat_file($filename, 9);

	return if ( $filetime == -1 ); #skip

	return $Test->ok(1, $name) if ( $filetime < $time );

	$Test->diag( "Filename [$filename] mtime [$filetime] not less than $time" );
	$Test->ok(0, $name);
	}

# private function to safely stat a file
#
# Arugments:
# filename     file to perform on
# attr_pos     pos of the array returned from stat we want to compare. perldoc -f stat
#
# Returns:
# -1        - stat failed
#  0        - failure (file doesn't exist etc)
#  filetime - on success, time requested provided by stat
#
sub _stat_file
	{
	my $filename    = _normalize( shift );
	my $attr_pos    = shift;

	unless( defined $filename )
		{
		$Test->diag( "Filename not specified!" );
		return 0;
		}

	unless( -e $filename )
		{
		$Test->diag( "Filename [$filename] does not exist!" );
		return 0;
		}

	my $filetime = ( stat($filename) )[$attr_pos];

	unless( $filetime )
		{
		$Test->diag( "stat of $filename failed" );
		return -1; #skip on stat failure
		}

	return $filetime;
	}

=back

=head1 TO DO

* check properties for other users (readable_by_root, for instance)

* check times

* check number of links to file

* check path parts (directory, filename, extension)

=head1 SEE ALSO

L<Test::Builder>,
L<Test::More>

=head1 SOURCE AVAILABILITY

This module is in Github:

	git://github.com/briandfoy/test-file.git

=head1 AUTHOR

brian d foy, C<< <bdfoy@cpan.org> >>

=head1 CREDITS

Shawn Sorichetti C<< <ssoriche@coloredblocks.net> >> provided
some functions.

Tom Metro helped me figure out some Windows capabilities.

Dylan Martin added C<owner_is> and C<owner_isnt>.

David Wheeler added C<file_line_count_is>.

Buddy Burden C<< <barefoot@cpan.org> >> provided C<dir_exists_ok>,
C<dir_contains_ok>, C<file_contains_like>, and
C<file_contains_unlike>.

xmikew C<< <https://github.com/xmikew> >> provided the C<mtime_age>
stuff.

=head1 COPYRIGHT AND LICENSE

Copyright © 2002-2016, brian d foy <bdfoy@cpan.org>. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

"The quick brown fox jumped over the lazy dog";
