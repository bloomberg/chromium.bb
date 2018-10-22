@rem = '--*-Perl-*--
@echo off
if "%OS%" == "Windows_NT" goto WinNT
"%~dp0perl.exe" -x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofperl
:WinNT
"%~dp0perl.exe" -x -S %0 %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofperl
if %errorlevel% == 9009 echo You do not have Perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofperl
@rem ';
#!perl -w
#line 15
use strict;
use File::Find::Rule;
use File::Spec::Functions qw(catdir);

# bootstrap extensions
for (@INC) {
    my $dir = catdir($_, qw( File Find Rule ) );
    next unless -d $dir;
    my @pm = find( name => '*.pm', maxdepth => 1,
                   exec => sub { (my $name = $_) =~ s/\.pm$//;
                                 eval "require File::Find::Rule::$name"; },
                   in => $dir );
}

# what directories are we searching in?
my @where;
while (@ARGV) {
    local $_ = shift @ARGV;
    if (/^-/) {
        unshift @ARGV, $_;
        last;
    }
    push @where, $_;
}

# parse arguments, build a rule object
my $rule = new File::Find::Rule;
while (@ARGV) {
    my $clause = shift @ARGV;

    unless ( $clause =~ s/^-// && $rule->can( $clause ) ) {
        # not a known rule - complain about this
        die "unknown option '$clause'\n"
    }

    # it was the last switch
    unless (@ARGV) {
        $rule->$clause();
        next;
    }

    # consume the parameters
    my $param = shift @ARGV;

    if ($param =~ /^-/) {
        # it's the next switch - put it back, and add one with no params
        unshift @ARGV, $param;
        $rule->$clause();
        next;
    }

    if ($param eq '(') {
        # multiple values - just look for the closing parenthesis
        my @p;
        while (@ARGV) {
            my $val = shift @ARGV;
            last if $val eq ')';
            push @p, $val;
        }
        $rule->$clause( @p );
        next;
    }

    # a single argument
    $rule->$clause( $param );
}

# add a print rule so things happen faster
$rule->exec( sub { print "$_[2]\n"; return; } );

# profit
$rule->in( @where ? @where : '.' );
exit 0;

__END__

=head1 NAME

findrule - command line wrapper to File::Find::Rule

=head1 USAGE

  findrule [path...] [expression]

=head1 DESCRIPTION

C<findrule> mostly borrows the interface from GNU find(1) to provide a
command-line interface onto the File::Find::Rule heirarchy of modules.

The syntax for expressions is the rule name, preceded by a dash,
followed by an optional argument.  If the argument is an opening
parenthesis it is taken as a list of arguments, terminated by a
closing parenthesis.

Some examples:

 find -file -name ( foo bar )

files named C<foo> or C<bar>, below the current directory.

 find -file -name foo -bar

files named C<foo>, that have pubs (for this is what our ficticious
C<bar> clause specifies), below the current directory.

 find -file -name ( -bar )

files named C<-bar>, below the current directory.  In this case if
we'd have omitted the parenthesis it would have parsed as a call to
name with no arguments, followed by a call to -bar.

=head2 Supported switches

I'm very slack.  Please consult the File::Find::Rule manpage for now,
and prepend - to the commands that you want.

=head2 Extra bonus switches

findrule automatically loads all of your installed File::Find::Rule::*
extension modules, so check the documentation to see what those would be.

=head1 AUTHOR

Richard Clamp <richardc@unixbeard.net> from a suggestion by Tatsuhiko Miyagawa

=head1 COPYRIGHT

Copyright (C) 2002 Richard Clamp.  All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 SEE ALSO

L<File::Find::Rule>

=cut

__END__
:endofperl
