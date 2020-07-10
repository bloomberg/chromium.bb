package Win32::Console::ANSI;
#
# Copyright (c) 2004-2017 Jean-Louis Morel <jl_morel@bribes.org>
#
# Version 1.11 (24/10/2017)
#
# This program is free software; you can redistribute it and/or modify
# it under the same terms as Perl itself.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either the
# GNU General Public License or the Artistic License for more details.
#

use 5.006;
use strict;
use warnings;
require Exporter;

our @ISA = qw(Exporter);
our $VERSION = '1.11';

use constant  MS_ON      => -1;
use constant  MS_STANDBY => 1;
use constant  MS_OFF     => 2;

use constant  SW_HIDE            => 0;
use constant  SW_NORMAL          => 1;
use constant  SW_SHOWMINIMIZED   => 2;
use constant  SW_SHOWMAXIMIZED   => 3;
use constant  SW_MAXIMIZE        => 3;
use constant  SW_SHOWNOACTIVATE  => 4;
use constant  SW_SHOW            => 5;
use constant  SW_MINIMIZE        => 6;
use constant  SW_SHOWMINNOACTIVE => 7;
use constant  SW_SHOWNA          => 8;
use constant  SW_RESTORE         => 9;
use constant  SW_SHOWDEFAULT     => 10;

our %EXPORT_TAGS = (
'func' => [ qw( Title Cursor CursorSize XYMax SetConsoleSize Cls ScriptCP MinimizeAll SetMonitorState
               ShowConsoleWindow SetCloseButton SetConsoleFullScreen )],
'MS_' => [ qw( SetMonitorState MS_ON MS_OFF MS_STANDBY ) ],
'SW_' => [ qw( MinimizeAll ShowConsoleWindow SW_HIDE SW_NORMAL SW_SHOWMINIMIZED
               SW_SHOWMAXIMIZED SW_MAXIMIZE SW_SHOWNOACTIVATE SW_SHOW SW_MINIMIZE
               SW_SHOWMINNOACTIVE SW_SHOWNA SW_RESTORE SW_SHOWDEFAULT ) ],
'all' => [ qw( Title Cursor CursorSize XYMax SetConsoleSize Cls ScriptCP MinimizeAll SetMonitorState
               ShowConsoleWindow SetCloseButton SetConsoleFullScreen MS_ON MS_OFF MS_STANDBY
               SW_HIDE SW_NORMAL SW_SHOWMINIMIZED SW_SHOWMAXIMIZED
               SW_MAXIMIZE SW_SHOWNOACTIVATE SW_SHOW SW_MINIMIZE SW_SHOWMINNOACTIVE
               SW_SHOWNA SW_RESTORE SW_SHOWDEFAULT )],
);

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} }, @{ $EXPORT_TAGS{'MS_'} },
                   @{ $EXPORT_TAGS{'SW_'} }, @{ $EXPORT_TAGS{'func'} },
);

our @EXPORT = qw(
);

package Win32::Console::ANSI;

require XSLoader;
XSLoader::load('Win32::Console::ANSI', $VERSION);

1;
__END__

# POD documentation

=head1 NAME

Win32::Console::ANSI - Perl extension to emulate ANSI console on Win32 system.

=head1 SYNOPSIS

  use Win32::Console::ANSI;

  print "\e[1;34mThis text is bold blue.\e[0m\n";
  print "This text is normal.\n";
  print "\e[33;45;1mBold yellow on magenta.\e[0m\n";
  print "This text is normal.\n";

With the Term::ANSIColor module one increases the readability:

  use Win32::Console::ANSI;
  use Term::ANSIColor;

  print color 'bold blue';
  print "This text is bold blue.\n";
  print color 'reset';
  print "This text is normal.\n";
  print colored ("Bold yellow on magenta.\n", 'bold yellow on_magenta');
  print "This text is normal.\n";

And even more with Term::ANSIScreen:

  use Win32::Console::ANSI;
  use Term::ANSIScreen qw/:color :cursor :screen/;

  locate 1, 1; print "@ This is (1,1)", savepos;
  print locate(24,60), "@ This is (24,60)"; loadpos;
  print down(2), clline, "@ This is (3,16)\n";
  color 'black on white'; clline;
  print "This line is black on white.\n";
  print color 'reset'; print "This text is normal.\n";
  print colored ("This text is bold blue.\n", 'bold blue');
  print "This text is normal.\n";
  print colored ['bold blue'], "This text is bold blue.\n";
  print "This text is normal.\n";

=head1 DESCRIPTION

Windows NT/2000/XP does not support ANSI escape sequences in Win32 Console
applications. This module emulates an ANSI console for the script that uses
it and also converts the characters from Windows code page to DOS code page
(the so-called ANSI to OEM conversion). This conversion permits the display
of the accented characters in the console like in the Windows- based editor
used to type the script.

=head2 Escape sequences for Cursor Movement

=over

=item * \e[#A

CUU: CUrsor Up: Moves the cursor up by the specified number of lines without
changing columns. If the cursor is already on the top line, this sequence
is ignored. \e[A is equivalent to \e[1A.

=item * \e[#B

CUD: CUrsor Down: Moves the cursor down by the specified number of lines
without changing columns. If the cursor is already on the bottom line,
this sequence is ignored. \e[B is equivalent to \e[1B.

=item * \e[#C

CUF: CUrsor Forward: Moves the cursor forward by the specified number of
columns without changing lines. If the cursor is already in the
rightmost column, this sequence is ignored. \e[C is equivalent to \e[1C.

=item * \e[#D

CUB: CUrsor Backward: Moves the cursor back by the specified number of
columns without changing lines. If the cursor is already in the leftmost
column, this sequence is ignored. \e[D is equivalent to \e[1D.

=item * \e[#E

CNL: Cursor Next Line: Moves the cursor down the indicated # of rows, to
column 1. \e[E is equivalent to \e[1E.

=item * \e[#F

CPL: Cursor Preceding Line: Moves the cursor up the indicated # of rows,
to column 1. \e[F is equivalent to \e[1F.

=item * \e[#G

CHA: Cursor Horizontal Absolute: Moves the cursor to indicated column in
current row. \e[G is equivalent to \e[1G.

=item * \e[#;#H

CUP: CUrsor Position: Moves the cursor to the specified position. The first #
specifies the line number, the second # specifies the column.
If you do not specify a position, the cursor moves to the
home position: the upper-left corner of the screen (line 1, column 1).

=item * \e[#;#f

HVP: Horizontal and Vertical Position.
Works in the same way as the preceding escape sequence.

=item * \e[s

SCP: Save Cursor Position: Saves the current cursor position. You can move
the cursor to the saved cursor position by using the Restore Cursor
Position sequence.

=item * \e[u

RCP: Restore Cursor Position: Returns the cursor to the position stored
by the Save Cursor Position sequence.

=back

=head2 Escape sequences for Display Edition

=over

=item * \e[#J

ED: Erase Display:

=over

=item * \e[0J

Clears the screen from cursor to end of display. The cursor position is unchanged.

=item * \e[1J

Clears the screen from start to cursor. The cursor position is unchanged.

=item * \e[2J

Clears the screen and moves the cursor to the home position (line 1, column 1).

=back

\e[J is equivalent to \e[0J. (Some terminal/emulators respond to \e[J as if
it were \e[2J. Here, the default is 0; it is the norm)

=item * \e[#K

EL: Erase Line:

=over

=item * \e[0K

Clears all characters from the cursor position to the end of the line
(including the character at the cursor position).
The cursor position is unchanged.

=item * \e[1K

Clears all characters from start of line to the cursor position.
(including the character at the cursor position).
The cursor position is unchanged.

=item * \e[2K

Clears all characters of the whole line.
The cursor position is unchanged.

=back

\e[K is equivalent to \e[0K. (Some terminal/emulators respond to \e[K as if
it were \e[2K. Here, the default is 0; it is the norm)

=item * \e[#L

IL: Insert Lines: The cursor line and all lines below it move down # lines,
leaving blank space. The cursor position is unchanged. The bottommost #
lines are lost. \e[L is equivalent to \e[1L.

=item * \e[#M

DL: Delete Line: The block of # lines at and below the cursor are  deleted;
all  lines below  them  move up # lines to fill in the gap, leaving # blank
lines at the bottom of the screen.  The cursor position is unchanged.
\e[M is equivalent to \e[1M.

=item * \e[#\@

ICH: Insert CHaracter: The cursor character and all characters to the right
of it move right # columns, leaving behind blank space.
The cursor position is unchanged. The rightmost # characters on the line are lost.
\e[\@ is equivalent to \e[1\@.

=item * \e[#P

DCH: Delete CHaracter: The block of # characters at and to the right of the
cursor are deleted; all characters to the right of it move left # columns,
leaving behind blank space. The cursor position is unchanged.
\e[P is equivalent to \e[1P.

=back

=head2 Escape sequences for Set Graphics Rendition

=over

=item * \e[#;...;#m

SGM: Set Graphics Mode: Calls the graphics functions specified by the
following values. These specified functions remain active until the next
occurrence of this escape sequence. Graphics mode changes the colors and
attributes of text (such as bold and underline) displayed on the
screen.

=over

=item * Text attributes

       0    All attributes off
       1    Bold on
       4    Underscore on
       7    Reverse video on
       8    Concealed on

       21   Bold off
       24   Underscore off
       27   Reverse video off
       28   Concealed off

=item * Foreground colors

       30    Black
       31    Red
       32    Green
       33    Yellow
       34    Blue
       35    Magenta
       36    Cyan
       37    White
       
       39    Default foreground color

=item * Background colors

       40    Black
       41    Red
       42    Green
       43    Yellow
       44    Blue
       45    Magenta
       46    Cyan
       47    White
       
       49    Default background color

=item * Bright / high-intensity foreground colors

       90    Bright Black (aka Dark Gray)
       91    Bright Red
       92    Bright Green
       93    Bright Yellow
       94    Bright Blue
       95    Bright Magenta
       96    Bright Cyan
       97    Bright White

=item * Bright / high-intensity background colors

      100    Bright Black (aka Dark Gray)
      101    Bright Red
      102    Bright Green
      103    Bright Yellow
      104    Bright Blue
      105    Bright Magenta
      106    Bright Cyan
      107    Bright White

=back

\e[m is equivalent to \e[0m.

=back

=head2 Escape sequences for Select Character Set

=over

=item * \e(U

Selects null mapping - straight to character from the codepage of the console.

=item * \e(K

Selects Windows to DOS mapping. This is the default mapping. It is useful
because one types the script with a Windows-based editor (using a Windows
codepage) and the script prints its messages on the console using another
codepage: without translation, the characters with a code greatest than 127
are different and the printed messages may be not readable.

The conversion is done by the Windows internal functions.
If a character cannot be represented in the console code page it is
replaced by a question mark character.


=item * \e(#X

This escape sequence is I<not> standard! It is an experimental one, just for
fun :-)

If (I<and only if>) the console uses a Unicode police, it is possible to
change its codepage with this escape sequence. No effect with an ordinary
"Raster Font". (For Windows NT/2000/XP the currently available Unicode
console font is the Lucida Console TrueType font.)
# is the number of the codepage needed, 855 for cp855 for instance.

=back


=head2 Extra escape sequences for Set Cursor Visibility

The two following escape sequences are not in the ANSI standard.
These are private escape sequences introduced by DEC for the VT-300
series of video terminals.

=over

=item * \e[?25h

DECTCEM: DEC Text Cursor Enable Mode: Shows the cursor.

=item * \e[?25l

DECTCEM: DEC Text Cursor Enable Mode: Hides the cursor. (Note: the trailing character is lowercase L.)

=back


=head1 AUXILIARY FUNCTIONS

Because the module exports no symbols into the callers namespace, it is
necessary to import the names of the functions before using them.

=over

=item * Cls( );

Clears the screen with the current background color, and set cursor to
(1,1).

=item * $old_title = Title( [$new_title] );

Gets and sets the title bar of the current console window. With no
argument, the title is not modified.

  use Win32::Console::ANSI qw/ Title /;
  for (reverse 0..5) {
    Title "Count down ... $_";
    sleep 1;
  }

=item * ($old_x, $old_y) = Cursor( [$new_x, $new_y] );

Gets and sets the cursor position (the upper-left corner of the screen is
at (1, 1)). With no arguments, the cursor position is not modified. If one
of the two coordinates $new_x or $new_y is 0, the corresponding
coordinate does not change.

  use Win32::Console::ANSI qw/ Cursor /;
  ($x, $y) = Cursor();     # reads cursor position
  Cursor(5, 8);            # puts the cursor at column 5, line 8
  Cursor(5, 0);            # puts the cursor at column 5, line doesn't change
  Cursor(0, 8);            # puts the cursor at line 8, column doesn't change
  Cursor(0, 0);            # the cursor doesn't change a position (useless!)
  ($x, $y) = Cursor(5, 8); # reads cursor position AND puts cursor at (5, 8)

=item * ($Xmax, $Ymax) = XYMax( );

Gets the maximum cursor position.
If C<($x, $y) = Cursor()> we have always C<1 E<lt>= $x E<lt>= $Xmax> and
C<1 E<lt>= $y E<lt>= $Ymax>.

=item * $old_size = CursorSize( [$new_size] );

Gets and sets the cursor size i.e. the percentage of the character cell that
is filled by the cursor. This value is between 1 and 100. The cursor appearance
varies, ranging from completely filling the cell to showing up as a horizontal
line at the bottom of the cell. With no argument, the cursor size is not modified.

=item * $success = SetConsoleSize( $width, $height );

Sets the new size, in columns and rows, of the screen buffer.
The specified C<$width> and C<$height> cannot be less than the width and height
of the screen buffer's window. The specified dimensions also cannot be less
than the minimum size allowed by the system.

If the function succeeds, the return value is nonzero.
If the function fails, the return value is zero and the extended error
message is in C<$^E>.

=item * ShowConsoleWindow( $state )

Sets the console window's show state.

The parameter C<$state> can be one of the following values:

=over

=item * SW_HIDE

Hides the console window and activates another window.

=item * SW_MAXIMIZE

Maximizes the console window.

=item * SW_MINIMIZE

Minimizes the console window and activates the next top-level window in the
Z order.

=item * SW_RESTORE

Activates and displays the console window. If the console window is
minimized or maximized, the system restores it to its original size and
position. An application should specify this flag when restoring a
minimized console window.

=item * SW_SHOW

Activates the console window and displays it in its current size and
position.

=item * SW_SHOWDEFAULT

Sets the show state based on the SW_ value specified in the STARTUPINFO
structure passed to the CreateProcess function by the program that started
the application.

=item * SW_SHOWMAXIMIZED

Activates the console window and displays it as a maximized window.

=item * SW_SHOWMINIMIZED

Activates the window and displays it as a minimized window.

=item * SW_SHOWMINNOACTIVE

Displays the console window as a minimized window. This value is similar to
SW_SHOWMINIMIZED, except the window is not activated.

=item * SW_SHOWNA

Displays the console window in its current size and position. This value is
similar to SW_SHOW, except the window is not activated.

=item * SW_SHOWNOACTIVATE

Displays the console window in its most recent size and position. This value is
similar to SW_NORMAL, except the window is not activated.

=item * SW_NORMAL

Activates and displays the console window. If the window is minimized or
maximized, the system restores it to its original size and position.

=back

If the console window was previously visible, the return value is nonzero.

If the console window was previously hidden, the return value is zero.

=item * MinimizeAll( )

Minimizes all the windows on the desktop.

Example:

    #!/usr/bin/perl -w
    use strict;
    use Win32::Console::ANSI qw/ :SW_ /;

    MinimizeAll();
    sleep 2;
    ShowConsoleWindow(SW_SHOWMAXIMIZED);
    sleep 2;
    ShowConsoleWindow(SW_HIDE);
    sleep 2;
    ShowConsoleWindow(SW_RESTORE);

=item * $sucess = SetCloseButton( $state )

C<SetCloseButton( 0 )> disables the close button C<[x]> of the console
window and deletes the CLOSE menu item from the console menu system.

C<SetCloseButton( 1 )> enables the close button C<[x]> of the console window
and restores the CLOSE menu item from the console menu system.

If the function succeeds, the return value is nonzero else, the return value
is zero.

For obvious reasons, the button is re-established and the menu restored at
the end of the script.

Example:

    #!/usr/bin/perl -w
    use strict;
    use Win32::Console::ANSI qw/ SetCloseButton /;

    $SIG{INT}='IGNORE';   # no Ctrl-C interrupt
    SetCloseButton(0);    # no close button

    print "No close button, no Ctrl-C interrupt\n",
          "  Press [Enter]...\n";
    do { $_ = <STDIN> } until defined;

    $SIG{INT}='';         # Ctrl-C interrupt
    print "No close button, Ctrl-C interrupt enabled\n",
          "  Press [Enter]...\n";
    do { $_ = <STDIN> } until defined;

    SetCloseButton(1);      # restore close button
    print "Close button available\n  Press [Enter]...\n";
    do { $_ = <STDIN> } until defined;

=item * $success = SetConsoleFullScreen( $mode )

Sets the console in full-screen or windowed mode. This function works only
on WinXP/Vista...

C<SetConsoleFullScreen( 1 )> sets the console in full-screen mode.

C<SetConsoleFullScreen( 0 )> sets the console in windowed mode.

If the function succeeds, the return value is nonzero. If the function
fails, the return value is zero and the extended error message is in
C<$^E>).

=item * SetMonitorState( $state )

Sets the monitor state (on / off / standby).

The parameter C<$state> can be one of the following constants:

=over

=item * MS_ON

The display is being turn-on.

=item * MS_STANDBY

The display is going to low power.

=item * MS_OFF

The display is being shut off.

=back

Example:

    #!/usr/bin/perl -w
    use strict;
    use Win32::Console::ANSI qw/ :MS_ /;

    SetMonitorState(MS_STANDBY);
    sleep 10;                    # standby for 10 sec
    SetMonitorState(MS_ON);


=item * $old_ACP = ScriptCP( [$new_ACP] );

Sets the codepage of the script and return the old value.

=back

=head1 EXPORTS

Nothing by default;
the function names and constants must be explicitly exported.

=head2 Export Tags:

=over

=item * :func

exports all the functions.

=item * :MS_

exports C<SetMonitorState> and the C<MS_*> constants.

=item * :SW_

exports C<MinimizeAll>, C<ShowConsoleWindow> and the C<SW_*> constants

=item * :all

exports all.

=back

=head1 CAVEATS

Due to DOS-console limitations, the blink mode (text attributes 5 and 25) is
not implemented.

If you use an integrated environment for developing your program you can see
strange results on the console controlled by your IDE. The IDE catches and
processes the output of your program so your program does not see a "real"
console. In this case test your program in a separate console.

=head1 SEE ALSO

L<Win32::Console>, L<Term::ANSIColor>, L<Term::ANSIScreen>.

=head1 AUTHOR

J-L Morel E<lt>jl_morel@bribes.orgE<gt>

Home page: http://www.bribes.org/perl/wANSIConsole.html

Report bug: http://rt.cpan.org/Public/Dist/Display.html?Name=Win32-Console-ANSI

=head1 COPYRIGHT

Copyright (c) 2003-2017 J-L Morel. All rights reserved.

This program is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut
