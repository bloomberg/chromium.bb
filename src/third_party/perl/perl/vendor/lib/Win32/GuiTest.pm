#
# $Id: GuiTest.pm,v 1.7 2010/10/31 19:06:51 int32 Exp $
#

=head1 NAME

Win32::GuiTest - Perl GUI Test Utilities.

=head1 SYNOPSIS

  use Win32::GuiTest qw(FindWindowLike GetWindowText 
    SetForegroundWindow SendKeys);

  $Win32::GuiTest::debug = 0; # Set to "1" to enable verbose mode

  my @windows = FindWindowLike(0, "^Microsoft Excel", "^XLMAIN\$");
  for (@windows) {
      print "$_>\t'", GetWindowText($_), "'\n";
      SetForegroundWindow($_);
      SendKeys("%fn~a{TAB}b{TAB}{BS}{DOWN}");
  }

=head1 INSTALLATION

    // This batch file comes with MS Visual Studio.  Running
    // it first might help with various compilation problems.
    vcvars32.bat 

    perl makefile.pl
    nmake
    nmake test
    nmake install

    See more details in the DEVELOPMENT section elswhere in this document.

You can get the most recent release from 
L<http://www.sourceforge.net/projects/winguitest>. The package will
contain Win32-GuiTest.ppd file and Win32-GuiTest.tar.gz file, 
which is all that you need to use
ppm. If you put those 2 files in C:\TEMP directory, the installation
should look as follows.  Enter PPM (Perl Package Manager) from the 
command-line and type commands as below

    C:\TEMP>ppm
    PPM interactive shell (2.0) - type 'help' for available commands.
    PPM> install C:\temp\win32-guitest.ppd
    Install package 'C:\temp\win32-guitest.ppd?' (y/N): Y
    Retrieving package 'C:\temp\win32-guitest.ppd'...
    Writing C:\Perl\site\lib\auto\Win32\GuiTest\.packlist
    PPM>

I extracted them to 'c:\temp', please use the directory where you extracted 
the files instead.


=head1 DESCRIPTION

Most GUI test scripts I have seen/written for Win32 use some variant of Visual
Basic (e.g. MS-VB or MS-Visual Test). The main reason is the availability of
the SendKeys function.

A nice way to drive Win32 programs from a test script is to use OLE Automation
(ActiveX Scripting), but not all Win32 programs support this interface. That is
where SendKeys comes handy.

Some time ago Al Williams published a Delphi version in Dr. Dobb's
(http://www.drdobbs.com/keys-to-the-kingdom/184410429). I ported it to C and
packaged it using h2xs...

The tentative name for this module is Win32::GuiTest (mostly because I plan to
include more GUI testing functions).

I've created a Yahoo Group for the module that you can join at
   http://groups.yahoo.com/group/perlguitest/join

Also, an initial version of a script recording application has been written to use with this 
module.  A copy of it may be found with this distribution (Recorder\Win32GuiTest.exe)
or can be obtained at http://sourceforge.net/projects/winguitest

If the documentation of these functions is not satisfactory, you can 
try running a search on http://msdn.microsoft.com/ using the name of the function. 
Some of these functions are described there.

This distribution of the module - the one you are looking at now - has
its own CVS repository at http://sourceforge.net/projects/winguitest
Patches to both the code and the documentation are welcome.

=cut

package Win32::GuiTest;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $debug %EXPORT_TAGS);

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = ();

%EXPORT_TAGS=(
    FUNC => [ qw(
        AllocateVirtualBuffer
        CheckButton
        ClientToScreen
        EnableWindow
        FindWindowLike
        FreeVirtualBuffer
        GetActiveWindow
        GetAsyncKeyState
        GetCaretPos
        GetChildDepth
        GetChildWindows
        GetClassName
        GetComboContents
        GetComboText
        GetCursorPos
        GetDesktopWindow
        GetFocus
        GetForegroundWindow
        GetListContents
        GetListText
        GetListViewContents
        GetMenu
        GetMenuItemCount
        GetMenuItemID
        GetMenuItemIndex
        GetMenuItemInfo
        GetParent
        GetPopupHandle
        GetScreenRes
        GetSubMenu
        GetSystemMenu
        GetTabItems
        GetTreeViewSelPath
        GetWindow
        GetWindowID
        GetWindowLong
        GetWindowRect
        GetWindowText
        IsCheckedButton
        IsChild
        IsGrayedButton
        IsKeyPressed
        IsListViewItemSel
        IsTabItemSel
        IsWindow
        IsWindowEnabled
        IsWindowStyle
        IsWindowStyleEx
        IsWindowVisible
        MenuSelect
        MouseClick
        MouseMoveAbsPix
        MouseMoveWheel
        NormToScreen
        PostMessage
        PushButton
        PushChildButton
        PushChildById
        ReadFromVirtualBuffer
        ScreenToClient
        ScreenToNorm
        SelComboItem
        SelComboItemText
        SelComboString
        SelListViewItem
        SelListViewItemText
        SelTabItem
        SelTabItemText
        SelTreeViewItemPath
        SelectTabItem
        SendKeys
        SendLButtonDown
        SendLButtonUp
        SendMButtonDown
        SendMButtonUp
        SendMessage
        SendMouse
        SendMouseMoveAbs
        SendMouseMoveRel
        SendRawKey
        SendRButtonDown
        SendRButtonUp
        SetActiveWindow
        SetFocus
        SetForegroundWindow
        SetWindowPos
        ShowWindow
        TabCtrl_GetCurFocus
        TabCtrl_GetItemCount
        TabCtrl_SetCurFocus
        TabCtrl_SetCurSel
	UnicodeSemantics
	VkKeyScan
        WMGetText
        WMSetText
        WaitForReady
        WaitWindow
        WaitWindowLike
        WindowFromPoint
        WriteToVirtualBuffer
    )],
    VARS => [ qw(
        $debug
    )],
    SW => [ qw(
        SW_HIDE SW_SHOWNORMAL SW_NORMAL SW_SHOWMINIMIZED SW_SHOWMAXIMIZED
        SW_MAXIMIZE SW_SHOWNOACTIVATE SW_SHOW SW_MINIMIZE SW_SHOWMINNOACTIVE
        SW_SHOWNA SW_RESTORE SW_SHOWDEFAULT SW_FORCEMINIMIZE SW_MAX
    )],
    LVS => [ qw(
        LVS_AUTOARRANGE LVS_ICON LVS_LIST LVS_SMALLICON
    )],
    VK => [ qw(
        VK_LBUTTON VK_RBUTTON VK_CANCEL VK_MBUTTON VK_BACK VK_TAB VK_CLEAR
        VK_RETURN VK_SHIFT VK_CONTROL VK_MENU VK_PAUSE VK_CAPITAL VK_KANA
        VK_HANGEUL VK_HANGUL VK_JUNJA VK_FINAL VK_HANJA VK_KANJI VK_ESCAPE
        VK_CONVERT VK_NONCONVERT VK_ACCEPT VK_MODECHANGE VK_SPACE VK_PRIOR VK_NEXT
        VK_END VK_HOME VK_LEFT VK_UP VK_RIGHT VK_DOWN VK_SELECT VK_PRINT VK_EXECUTE
        VK_SNAPSHOT VK_INSERT VK_DELETE VK_HELP VK_LWIN VK_RWIN VK_APPS VK_NUMPAD0
        VK_NUMPAD1 VK_NUMPAD2 VK_NUMPAD3 VK_NUMPAD4 VK_NUMPAD5 VK_NUMPAD6
        VK_NUMPAD7 VK_NUMPAD8 VK_NUMPAD9 VK_MULTIPLY VK_ADD VK_SEPARATOR
        VK_SUBTRACT VK_DECIMAL VK_DIVIDE VK_F1 VK_F2 VK_F3 VK_F4 VK_F5 VK_F6 VK_F7
        VK_F8 VK_F9 VK_F10 VK_F11 VK_F12 VK_F13 VK_F14 VK_F15 VK_F16 VK_F17 VK_F18
        VK_F19 VK_F20 VK_F21 VK_F22 VK_F23 VK_F24 VK_NUMLOCK VK_SCROLL VK_LSHIFT
        VK_RSHIFT VK_LCONTROL VK_RCONTROL VK_LMENU VK_RMENU VK_PROCESSKEY VK_ATTN
        VK_CRSEL VK_EXSEL VK_EREOF VK_PLAY VK_ZOOM VK_NONAME VK_PA1 VK_OEM_CLEAR
        KEYEVENTF_EXTENDEDKEY KEYEVENTF_KEYUP
    )],
);
#getLW

@EXPORT_OK= ();
{ my $ref;
    foreach $ref (  values(%EXPORT_TAGS)  ) {
        push( @EXPORT_OK, @$ref );
    }
}
$EXPORT_TAGS{ALL}= \@EXPORT_OK;
                             
$VERSION = '1.63';

$debug = 0;

bootstrap Win32::GuiTest $VERSION;

require "Win32/GuiTest/GuiTest.pc";

=head2 Functions

=over 4

=item $debug

When set enables the verbose mode.

=item SendKeys($keys[,$delay])

Sends keystrokes to the active window as if typed at the keyboard using the
optional delay between key-up and key-down messages (default is 25 ms and
should be OK for most uses). 

The keystrokes to send are specified in KEYS. There are several
characters that have special meaning. This allows sending control codes 
and modifiers:

	~ means ENTER
	+ means SHIFT 
	^ means CTRL 
	% means ALT

The parens allow character grouping. You may group several characters, so
that a specific keyboard modifier applies to all of them. Groups can
be enclosed in groups.

E.g. SendKeys("ABC") is equivalent to SendKeys("+(abc)")

The curly braces are used to quote special characters (SendKeys("{+}{{}")
sends a '+' and a '{'). You can also use them to specify certain named actions:

	Name          Action

	{BACKSPACE}   Backspace
	{BS}          Backspace
	{BKSP}        Backspace
	{BREAK}       Break
	{CAPS}        Caps Lock
	{DELETE}      Delete
	{DOWN}        Down arrow
	{END}         End
	{ENTER}       Enter (same as ~)
	{ESCAPE}      Escape
	{HELP}        Help key
	{HOME}        Home
	{INSERT}      Insert
	{LEFT}        Left arrow
	{NUMLOCK}     Num lock
	{PGDN}        Page down
	{PGUP}        Page up
	{PRTSCR}      Print screen
	{RIGHT}       Right arrow
	{SCROLL}      Scroll lock
	{TAB}         Tab
	{UP}          Up arrow
	{PAUSE}       Pause
        {F1}          Function Key 1
        ...           ...
        {F24}         Function Key 24
        {SPC}         Spacebar
        {SPACE}       Spacebar
        {SPACEBAR}    Spacebar
        {LWI}         Left Windows Key
        {RWI}         Right Windows Key 
        {APP}         Open Context Menu Key

or supply a number that will be treated as a VK code. Note that a single-digit
number will be treated as a character, so prepend these with '0'.

All these named actions take an optional integer argument, like in {RIGHT 5}. 
For all of them, except PAUSE, the argument means a repeat count. For PAUSE
it means the number of milliseconds SendKeys should pause before proceding.

In this implementation, SendKeys always returns after sending the keystrokes.
There is no way to tell if an application has processed those keys when the
function returns.

Unicode characters in C<$keys> are translated into set of ALT+NUMPAD keystrokes.
Note that not all applications can understand unicode input.

=cut

my %vk = (
	BAC    => VK_BACK(),
	BS     => VK_BACK(),
	BKS    => VK_BACK(),
	BRE    => VK_CANCEL(),
	CAP    => VK_CAPITAL(),
	DEL    => VK_DELETE(),
	DOW    => VK_DOWN(),
	'END'  => VK_END(),
	ENT    => VK_RETURN(),
	ESC    => VK_ESCAPE(),
	HEL    => VK_HELP(),
	HOM    => VK_HOME(),
	INS    => VK_INSERT(),
	LEF    => VK_LEFT(),
	NUM    => VK_NUMLOCK(),
	PGD    => VK_NEXT(),
	PGU    => VK_PRIOR(),
	PRT    => VK_SNAPSHOT(),
	RIG    => VK_RIGHT(),
	SCR    => VK_SCROLL(),
	TAB    => VK_TAB(),
	UP     => VK_UP(),
	F1     => VK_F1(),
	F2     => VK_F2(),
	F3     => VK_F3(),
	F4     => VK_F4(),
	F5     => VK_F5(),
	F6     => VK_F6(),
	F7     => VK_F7(),
	F8     => VK_F8(),
	F9     => VK_F9(),
	F10    => VK_F10(),
	F11    => VK_F11(),
	F12    => VK_F12(),
	F13    => VK_F13(),
	F14    => VK_F14(),
	F15    => VK_F15(),
	F16    => VK_F16(),
	F17    => VK_F17(),
	F18    => VK_F18(),
	F19    => VK_F19(),
	F20    => VK_F20(),
	F21    => VK_F21(),
	F22    => VK_F22(),
	F23    => VK_F23(),
	F24    => VK_F24(),
	SPC    => VK_SPACE(),
	SPA    => VK_SPACE(),
	LWI    => VK_LWIN(),
	RWI    => VK_RWIN(),
	APP    => VK_APPS(),
);

my %vk_flags = (
	'+'            => VK_SHIFT(),
	'%'            => VK_MENU(),
	'^'            => VK_CONTROL(),
);

my @digits = (
	VK_INSERT(), VK_END(), VK_DOWN(), VK_NEXT(), VK_LEFT(),
	VK_CLEAR(), VK_RIGHT(), VK_HOME(), VK_UP(), VK_PRIOR(),
);

sub wrap_meta
{
	my @k = @vk_flags{ keys %{$_[0]} };
	%{shift()} = ();
	return 
		( map { $_ => 0 } @k),
		@_,
		( map { $_ => KEYEVENTF_KEYUP() } @k)
		;
}

sub alt_sequence
{(
	VK_MENU(),	0,
	( map {
		$digits[$_], 0,
		$digits[$_], KEYEVENTF_KEYUP(),
	} split '', ord shift),
	VK_MENU(),	KEYEVENTF_KEYUP(),
)}

sub make_charcode
{
	my ( $flags, $c) = @_;
	my $scan = VkKeyScan( ord $c );
	$flags-> {'+'}++ if $scan & 0x100;
	$flags-> {'^'}++ if $scan & 0x200;
	$flags-> {'%'}++ if $scan & 0x400;
	$scan &= 0xff;
	return ( 
		$scan, 0 , 
		$scan, KEYEVENTF_KEYUP()
	);
}

sub find_vkey
{
	my $c = shift;
	if ( $c =~ /^\d+$/) {
	      $c = 0 + $c;
	} elsif ( $c =~ /^0x([a-f0-9]+)$/i) {
	      $c = hex $1;
	} else {
		substr( $c, 3) = '' if length($c) > 3;
		return undef unless exists $vk{$c};
		$c = $vk{$c};
	}
	return $c;
}

sub parse_keys
{
	my $level = $_[1] || 0;
	my ( @ret, %flags);
	{
		# grouping
		$_[0] =~ m/\G\(/gcs and do {
			push @ret, wrap_meta( \%flags, parse_keys( $_[0], $level + 1));
			redo;
		};
		$level > 0 and $_[0] =~ m/\G\)/gcs and return @ret; # single ) is ok

		# shift keys
		$_[0] =~ m/\G([+^%])/gcs and do {
			$flags{$1}++;
			redo;
		};

		# single character
		$_[0] =~ m/\G(?:\{(.)(?:\s(\d+))?\}|([^~{]))/gcs and do {
			my $c   = defined($3) ? $3 : $1;
			my $rep = defined($2) ? $2 : 1;
			my @r   = ( utf8::is_utf8($c) and ord($c) > 0x7f) ?
				alt_sequence( $c) :
				make_charcode( \%flags, $c)
				;
			push @ret, (wrap_meta( \%flags, @r)) x $rep;
			redo;
		};

		# enter
		$_[0] =~ m/\G\~/gcs and do {
			push @ret, wrap_meta(
				\%flags,
				VK_RETURN(), 0,
				VK_RETURN(), KEYEVENTF_KEYUP(),
			);
			redo;
		};

		# virtual key
		$_[0] =~ m/\G\{(\w+)(?:\s(\d+))?\}/gcs and do {
			my $k;
			my $c   = $1;
			my $rep = defined($2) ? $2 : 1;
			if ( $c =~ /^PAU/) {
				push @ret, undef, sub { select(undef, undef, undef, $rep / 1000 ) };
				%flags = ();
				redo;
			} elsif ( not defined( $k = find_vkey( $c))) {
				warn "GuiTest: invalid virtual key {$c}\n";
				%flags = ();
				redo;
			}
			push @ret, (wrap_meta(
				\%flags, 
				$k, 0,
				$k, KEYEVENTF_KEYUP()
			)) x $rep;
			redo;
		};

		# end
		$_[0] =~ m/\G$/gcs and return @ret;

		warn 
			"GuiTest: cannot parse key sequence beginning at ", 
			substr($_[0], pos($_[0])) ,
			"\n";
	};
}

sub SendKeys
{
	my ( $keys, $delay) = @_;
	$delay = 25 unless defined $delay;
	$delay /= 1000;
	my @k = parse_keys($keys);
	for ( my $i = 0; $i < @k; $i+=2) {
		if ( defined $k[$i]) {
			DbgShow("SendRawKey(@k[$i,$i+1])\n");
			SendRawKey( @k[$i,$i+1]);
			select(undef,undef,undef,$delay);
		} else {
			$k[$i+1]-> ();
		}
	}
}

=item SendMouse($command)

This function emulates mouse input.  The COMMAND parameter is a string
containing one or more of the following substrings:

        {LEFTDOWN}    left button down
        {LEFTUP}      left button up
        {MIDDLEDOWN}  middle button down
	{MIDDLEUP}    middle button up
	{RIGHTDOWN}   right button down
	{RIGHTUP}     right button up
	{LEFTCLICK}   left button single click
	{MIDDLECLICK} middle button single click
	{RIGHTCLICK}  right button single click
	{ABSx,y}      move to absolute coordinate ( x, y )
        {RELx,y}      move to relative coordinate ( x, y )

Note: Absolute mouse coordinates range from 0 to 65535.
      Relative coordinates can be positive or negative.
      If you need pixel coordinates you can use MouseMoveAbsPix.

Also equivalent low-level functions are available:

    SendLButtonUp()
    SendLButtonDown()
    SendMButtonUp()
    SendMButtonDown()
    SendRButtonUp()
    SendRButtonDown()
    SendMouseMoveRel(x,y)
    SendMouseMoveAbs(x,y)

=cut

sub SendMouse {
    my $command = shift;

    # Split out each command block enclosed in curly braces.
    my @list = ( $command =~ /{(.+?)}/g );
    my $item;

    foreach $item ( @list ) {
        if ( $item =~ /leftdown/i )      { SendLButtonDown (); }
        elsif ( $item =~ /leftup/i )	 { SendLButtonUp   (); }
        elsif ( $item =~ /middledown/i ) { SendMButtonDown (); }
        elsif ( $item =~ /middleup/i )	 { SendMButtonUp   (); }
        elsif ( $item =~ /rightdown/i )	 { SendRButtonDown (); }
        elsif ( $item =~ /rightup/i )	 { SendRButtonUp   (); }
        elsif ( $item =~ /leftclick/i )	{
            SendLButtonDown ();
            SendLButtonUp ();
        }
        elsif ( $item =~ /middleclick/i ) {
            SendMButtonDown ();
            SendMButtonUp ();
        }
        elsif ( $item =~ /rightclick/i ) {
            SendRButtonDown ();
            SendRButtonUp ();
        }
        elsif ( $item =~ /abs(-?\d+),(-?\d+)/i ) { SendMouseMoveAbs($1,$2); }
        elsif ( $item =~ /rel(-?\d+),(-?\d+)/i ) { SendMouseMoveRel($1,$2); }
        else  { warn "GuiTest: Unmatched mouse command! \n"; }
    }
}


=item MouseMoveAbsPix($x,$y)

Move the mouse cursor to the screen pixel indicated as parameter.

  # Moves to x=200, y=100 in pixel coordinates.
  MouseMoveAbsPix(200, 100);

=cut

=item MouseMoveWheel($change)

  Positive or negative value to direct mouse wheel movement.

=cut

=item FindWindowLike($window,$titleregex,$classregex,$childid,$maxlevel) 

Finds the window handles of the windows matching the specified parameters and
returns them as a list. 

You may specify the handle of the window to search under. The routine 
searches through all of this windows children and their children recursively.
If 'undef' then the routine searches through all windows. There is also a 
regexp used to match against the text in the window caption and another regexp
used to match against the text in the window class. If you pass a child ID 
number, the functions will only match windows with this id. In each case 
undef matches everything.

=cut 

sub FindWindowLike {

    my %arg;
    my $hWndStart;
    my $windowre;
    my $classre;
    my $ID;
    my $maxlevel;
    my $oo_mode = 0; # Object oriented mode

    if( defined($_[0]) and $_[ 0 ] eq "Win32::GuiTest" )  # Object
    {
        shift(@_);
        %arg       = @_;
   	    $hWndStart = $arg{ '-parent' } || GetDesktopWindow(); # Where to start
        $windowre  = $arg{ '-titleregex' }; # Regexp
        $classre   = $arg{ '-classregex' }; # Regexp
        $ID        = $arg{ '-childid' }; # Control/Op. ID
        $maxlevel  = $arg{ '-maxlevel' };
        $oo_mode   = 1;
    }elsif( defined($_[0]) and ref( $_[ 0 ] eq "Win32::GuiTest::Window" ) )  # Object
    {
        shift(@_);
        %arg       = @_;
   	    $hWndStart = $arg{ '-parent' } || $_[ 0 ]->{ '-handle' }; 
                                            # Where to start
        $windowre  = $arg{ '-titleregex' }; # Regexp
        $classre   = $arg{ '-classregex' }; # Regexp
        $ID        = $arg{ '-childid' }; # Control/Op. ID
        $maxlevel  = $arg{ '-maxlevel' };
        $oo_mode   = 1;
    }
    else{                     #Static
        $hWndStart  = shift || GetDesktopWindow(); # Where to start
    	$windowre   = shift; # Regexp
    	$classre    = shift; # Regexp
    	$ID         = shift; # Control/Op. ID
    	$maxlevel   = shift;
    }

    my @found;

    #DbgShow("Children < @hwnds >\n");
    for my $hwnd (GetChildWindows($hWndStart)) {
        next if $maxlevel && GetChildDepth($hWndStart, $hwnd) > $maxlevel;

        # Get the window text and class name:
        my $sWindowText = GetWindowText($hwnd);
        my $sClassname  = GetClassName($hwnd);

	#DbgShow("($hwnd, $sWindowText, $sClassname) has ". scalar @children .
        #        " children < @children >\n");

        # If window is a child get the ID:                                                                                                      `
        my $sID;
        if (GetParent($hwnd) != 0) {
            $sID = GetWindowLong($hwnd, GWL_ID());
        }

        DbgShow("Using window pattern ($windowre)\n") if $windowre;
        DbgShow("Using class pattern ($classre)\n") if $classre;

	if ((!$windowre || $sWindowText =~ /$windowre/) &&
            (!$classre  || $sClassname =~ /$classre/))
        {
            DbgShow("Matched $1\n") if $1;
			# If ID not supplied OR child window ID equals
			# the one supplied.
            if ((not defined($ID)) || (defined($sID) && $sID == $ID)) {
                # If find a match add handle to array:
				push @found, $hwnd;
            }
            DbgShow("Window Found(" .
                "Text  : '$sWindowText'" .
		" Class : '$sClassname'" .
		" Handle: '$hwnd')\n");
        }
    }

    #DbgShow("FindWin found < @found >\n");
    if( not $oo_mode ){
        return @found;
    }else{
        my @found_obj = ();
        foreach my $wnd ( @found ){
            push( @found_obj, 
                  Win32::GuiTest::Window->new( '-handle' => $wnd ) );
        }
        return @found_obj;
    }
}

sub DbgShow {
    my $string = shift;
    print $string if $debug;
}

=item GetWindowID($window)

    Returns the control Id of the specified window.

=cut

sub GetWindowID {
    return GetWindowLong(shift, GWL_ID());
}

=item PushButton($button[,$delay])

Equivalent to

    PushChildButton(GetForegroundWindow, BUTTON, DELAY)

=cut

sub PushButton {
    my $button = shift;
    my $delay  = shift;

    return PushChildButton(GetForegroundWindow(), $button, $delay);
}

=item PushChildButton($parent,$button[,$delay])

Allows generating a mouse click on a particular button.

parent - the parent window of the button

button - either the text in a button (e.g. "Yes") or the control ID
of a button.

delay - the time (0.25 means 250 ms) to wait between the mouse down
and the mouse up event. This is useful for debugging.

=cut

sub MatchTitleOrId {
    my $wnd = shift;
    my $regex = shift;
    my $title = GetWindowText($wnd);
    my $id = GetWindowID($wnd);
    return $title =~ /$regex/i ||
        $regex =~ /^\d+$/ && $regex == $id;
}

sub PushChildButton {
    my $parent = shift;
    my $button = shift;
    my $delay  = shift;
    $delay = 0 unless defined($delay);
    for my $child (GetChildWindows($parent)) {
        # Is correct text or correct window ID?
	if (MatchTitleOrId($child, $button) && IsWindowEnabled($child)) {
	    # Need to use PostMessage.  SendMessage won't return when certain dialogs come up.
	    PostMessage($child, WM_LBUTTONDOWN(), 0, 0);
	    # Allow for user to see that button is being pressed by waiting some ms.
	    select(undef, undef, undef, $delay) if $delay;
          PostMessage($child, WM_LBUTTONUP(), 0, 0);
	    return(1);
	}
    }
    return(0);
}

=item PushChildById( $parent, $button, $level, $delay )

Allows pushing a button, which control id is eqaul to a given parameter.
C<PushChildButton> tries to match parameter against control id or
caption.
PushChildById matches only against control id. Secondly, PushChildById
allows specifying search depth in the windows hierarchy tree.
The default is 2, which means that only direct children will be
pushed.

=cut

sub PushChildById 
{
    my $parent = shift;
    my $button = shift;
    my $level = shift;
    my $delay = shift;
    $level = 2 unless defined( $level );
    $delay = 0 unless defined($ delay );
    my @buttons = FindWindowLike( $parent, undef, undef, $button, $level );
    PostMessage($buttons[ 0 ], WM_LBUTTONDOWN(), 0, 0);
    # Allow for user to see that button is being pressed by waiting some ms.
    select(undef, undef, undef, $delay) if $delay;
    PostMessage($buttons[ 0 ], WM_LBUTTONUP(), 0, 0);
    return;
}


=item WaitWindowLike($parent,$wndtitle,$wndclass,$wndid,$depth,$wait)

Function which allows one to wait for a window to appear
vs. using hard waits (e.g. sleep 2).

parent   - Where to start (parent window)

wndtitle - Regexp for the window title

wndclass - Regexp for the window class name

wndid    - Numeric Window or Control ID

depth    - How deep should we search before we stop

wait     - How many seconds should we wait before giving up

=cut

sub WaitWindowLike {
    my $parent   = shift; # Where to start
    my $wndtitle = shift; # Regexp
    my $wndclass = shift; # Regexp
    my $wndid    = shift; # Window/Control ID
    my $depth    = shift; # How deep before we stop
    my $wait     = shift || 10; # Default to 10 seconds.

    # For each second of $wait, look for window 
    # twice (2lookups * 500ms = 1 second).
    for (my $i = 0; $i < ($wait * 2); $i++) {
	my @windows =
            FindWindowLike($parent, $wndtitle, $wndclass, $wndid, $depth);
	if (@windows && IsWindow($windows[0])) {
            # Window found, return pass indicator.
            return $windows[0];
        }
	# 500ms intervals, so we don't bog down the system. If you 
	# change this, you will have to change ($wait * 2) above in
	# order to represent seconds correctly.
	select(undef, undef, undef, 0.50);
    }

    # Window not found, return fail indicator.
    return 0;
}

=item WaitWindow($wndtitle,[$wait])

Minimal version of WaitWindowLike. Only requires the window title
regexp. You can also specify the wait timeout in seconds.

wndtitle - Regexp for the window title

wait     - How many seconds should we wait before giving up

=cut

sub WaitWindow {
    my $wndtitle = shift; # Regexp
    my $wait     = shift || 10; # Default to 10 seconds.

    return WaitWindowLike(0, $wndtitle, "", undef, undef, $wait);
}

=item IsWindowStyle($window, $style)

    Determines if a window has the specified style.  See sample
    script for more details.

=cut    

# Checks for a specified window style
sub IsWindowStyle {
    my $hwnd = shift;
    my $style = shift;

    my $rs = GetWindowLong($hwnd, GWL_STYLE());
    # Check bitmasked return value for the style.
    return ($rs & $style);
}

=item IsWindowStyleEx($window, $exstyle)

    Determines if a window has the specified extended
    style.  See sample script for more details.

=cut    

# Checks for a specified extended window style
sub IsWindowStyleEx {
    my $hwnd = shift;
    my $style = shift;

    my $rs = GetWindowLong($hwnd, GWL_EXSTYLE());
    # Check bitmasked return value for the style.
    return ($rs & $style);
}


# $window = [Control Caption] or [Window ID]
# $item = 0 based tab item identifier
# $parent = Parent window.  Default is foreground window.
sub SelectTabItem {
    my $window = shift;
    my $item = shift;
    my $parent = shift || GetForegroundWindow();

    foreach my $child (GetChildWindows($parent)) {
        # Is correct text or correct window ID?
        if (MatchTitleOrId($child, $window)) {
            PostMessage($child, TCM_SETCURFOCUS(), $item, 0);
            # Success
            return(1);
        }
    }
    # Failure
    return(0);
}

sub FindAndCheck {
    my $window = shift;
    my $parent = shift || GetForegroundWindow();

    foreach my $child (GetChildWindows($parent)) {
        my $childtext = GetWindowText($child);
        my $childid = GetWindowID($child);
        # Is correct text or correct window ID?
        if (MatchTitleOrId($child, $window)) {
            CheckButton($child);
            last;
        }
    }
}

=item GetMenu

Using the corresponding library function (see MSDN) it returns a MenuID number

=item GetMenuItemIndex($curr, $menu);

$curr is a MenuId and $menu is the (localized !) name of the menu including the hot
key:  "Rep&eate"  
Returns the index of the menu item (-1 if not found) 

=item GetMenuItemCount($menu)

Returns the number of elements in the given menu.

=item MenuSelect($menupath,$window,$menu)

Allows selecting a menu programmatically.

Simple Examples:
    # Exit foreground application through application menu.
    MenuSelect("&File|E&xit");

    # Exit foreground application through system menu
    MenuSelect("&Close", 0, GetSystemMenu(GetForegroundWindow(), FALSE));

=item GetMenuItemInfo($menuHndl, $cnt)

Receives a menu handler (one we got from GetMenu or GetSubMenu) and
a number (which is the location of the item within the given menu).

Returns a hash of which there are currently 2 keys:
type can be either "string" or "separator"  - this is the type of the menu item
text is the visible text of the menu item (provided only for "string" type)

WARNING: This is an experimental function. Its behavior might change.

=cut

sub MenuSelect {
    my $item = shift;
    my $hwnd = shift || GetForegroundWindow();
    my $menu = shift || GetMenu($hwnd);
    return MenuSelectItem($hwnd, $menu, $item);
}

# Function: MenuSelectItem
# Parameters: hwnd (Window Handle), hpm (Parent/Root Menu), item (Menu Item Path)
# Returns: FALSE on failure, TRUE on success
# 
sub MenuSelectItem {
    my $hwnd = shift;
    my $start  = shift;
    my $items = shift;

    my $mi = -1;
    my $curr = $start;
    DbgShow "'$items'\n";
    my @menus = split('\|', $items);
    foreach my $menu (@menus) {
        DbgShow "'$menu'\n";
        # Look for menu item in current menu level
        $mi = GetMenuItemIndex($curr, $menu);
        return 0 if $mi == -1; # Error, item not found
        # Go to next menu level
        my $next = GetSubMenu($curr, $mi);
        last unless $next; # No more sub menus, we are already in correct menu.  Jump
	 		   # out and select menu item
        # Change current menu handle for the next sub menu
        $curr = $next;
    }
    # Do what we came for, select the menu item
    if ($curr != GetSystemMenu($hwnd, 0)) {
        # Regular Menu
	PostMessage($hwnd, WM_COMMAND(), GetMenuItemID($curr, $mi), 0);
    } else {
        # System Menu
	PostMessage($hwnd, WM_SYSCOMMAND(), GetMenuItemID($curr, $mi), 0);
    }
	
    return 1;
}

=item MouseClick($window [,$parent] [,$x_offset] [,$y_offset] [,$button] [,$delay])

Allows one to easily interact with an application through mouse emulation.

window = Regexp for a Window caption / Child caption, or just a Child ID.

parent = Handle to parent window.  Default is foreground window.  Use
GetDesktopWindow() return value for this if clicking on an application
title bar.

x_offset = Offset for X axis.  Default is 0.

y_offset = Offset for Y axis.  Default is 0.

button = {LEFT}, {MIDDLE}, {RIGHT}.  Default is {LEFT}

delay = Default is 0.  0.50 = 500 ms.  Delay between button down and
button up.

Simple Examples:

    # Click on CE button if its parent window is in foreground.
    MouseClick('^CE$');

    # Right click on CE button if its parent window is in foreground
    MouseClick('^CE$', undef, undef, undef, '{RIGHT}');

    # Click on 8 button window under the specified parent window; where
    # [PARENTHWND] will be replaced by a parent handle variable.
    MouseClick('8', [PARENTHWND]);

    # Click on Calculator parent window itself
    MouseClick('Calculator', GetDesktopWindow());

=cut

sub MouseClick {
    my $window = shift or return(0);
    my $parent = shift || GetForegroundWindow();
    my $x_off = shift || 0;
    my $y_off = shift || 0;
    my $button = shift || '{LEFT}';
    my $delay = shift || 0;

    # Ensure button variable looks ok
    ($button =~ /^\{\D+\}$/) or return(0);
    # Strike } from $button for purposes below
    $button =~ s/\}$//;

    foreach my $child (GetChildWindows($parent)) {
        # Is correct text or window ID?
        if (MatchTitleOrId($child, $window)) {
            my ($x, $y) = GetWindowRect($child);
            # Move mouse to window, +1 for curved windows
            MouseMoveAbsPix(($x + 1) + $x_off, ($y + 1) + $y_off);
            # Press button
            SendMouse($button.'DOWN}');
            select(undef, undef, undef, $delay) if $delay;
            # Release button
            SendMouse($button.'UP}');
            # Success
            return(1);
        }
    }

    # Failure
    return(0);
}

=item $buf_str = AllocateVirtualBuffer( $hwnd, $size )

Allocates memory in the address space of the process, which is an owner of
a window identified by $hwnd. Returns a reference to a hash, which has 2 elements:

=over 8

=item ptr - address of the allocated memory

=item process - process handle (in the Win32 meaning, as returned by Win32 OpenProcess
API function

=back

=cut

sub AllocateVirtualBuffer
{
    my $hwnd = shift;
    my $size = shift;
    my ( $ptr, $process ) = AllocateVirtualBufferImp( $hwnd, $size );
    my $ret = {};
    $ret->{ 'ptr' } = $ptr;
    $ret->{ 'process' } = $process;

    return $ret;
}

=item $value = ReadFromVirtualBuffer( $buf_str, $size )

Read from a memory in the address space of the other process.
C<$buf_str> is a reference to a hash returned by AllocateVirtualBuffer.

Returns read value.

=cut

sub ReadFromVirtualBuffer
{
    my $buf_str = shift;
    my $size    = shift;
    my $value = ReadFromVirtualBufferImp( $buf_str->{ 'process' },
                                          $buf_str->{ 'ptr' },
                                          $size );
    return $value;
}

=item WriteToVirtualBuffer( $buf_str, $value )

Write to a memory in the address space of the other process.
C<$buf_str> is a reference to a hash returned by AllocateVirtualBuffer.
C<$value> is a value to be copied.

=cut

sub WriteToVirtualBuffer
{
    my $buf_str = shift;
    my $value   = shift;
    WriteToVirtualBufferImp( $buf_str->{ 'process' },
                             $buf_str->{ 'ptr' },
                             $value );
}

=item FreeVirtualBuffer( $buf_str )

Frees memory allocated by AllocateVirtualBuffer

=cut

sub FreeVirtualBuffer
{
    my $buf_str = shift;
    FreeVirtualBufferImp( $buf_str->{ 'process' },
                          $buf_str->{ 'ptr' } );

}



=item $text = WMGetText($hwnd) *

Sends a WM_GETTEXT to a window and returns its contents

=item $set = WMSetText(hwnd,text) *

Sends a WM_SETTEXT to a window setting its contents

=item ($x,$y) = GetCursorPos() *

Retrieves the cursor's position,in screen coordinates as (x,y) array. 

=item GetCaretPos()

Retrieves the caret's position, in client coordinates as (x,y) array. (Like Windows function)

=item HWND SetFocus(hWnd)

Sets the keyboard focus to the specified window

=item HWND GetDesktopWindow() *

Returns a handle to the desktop window

=item HWND GetWindow(hwnd,uCmd) *

=item SV * GetWindowText(hwnd) *

Get the text name of the window as shown on the top of it.
Beware, this is text depends on localization.

=item $class = GetClassName(hwnd) *

Using the same Windows library function returns the name
of the class wo which the specified window belongs.

See MSDN for more details.

You can also check out MSDN to see an overview of the Window Classes.

=item HWND GetParent(hwnd) *

A library function (see MSDN) to return the WindowID of the parent window.
See MSDN for the special cases.

=item long GetWindowLong(hwnd,index) *

=item BOOL SetForegroundWindow(hWnd) *

See corresponding Windows functions.

=item @wnds = GetChildWindows(hWnd)

Using EnumChildWindows library function (see MSDN) it returns the WindowID 
of each child window. If the children have their own children the function
returns them too until the tree ends.

=item BOOL IsChild(hWndParent,hWnd) *

Using the corresponding library function (see MSDN) it returns true
if the second window is an immediate child or a descendant window of
the first window.

=item $depth = GetChildDepth(hAncestor,hChild)

Using the GetParent library function in a loop, returns the distance
between an ancestor window and a child (descendant) window.

Features/bugs:
If the given "ancsetor" is not really an ancestor, the return value is the distance of child from the root window (0)
If you supply the same id for both the ancestor and the child you get 1.
If the ancestor you are checking is not 0 then the distance given is 1 larger than it should be.

see eg\get_child_depth.pl

=item $res = SendMessage(hWnd,Msg,wParam,lParam) *

This is a library function (see MSDN) used by a number of the functions provided by
Win32::GuiTest. It sends the specified message to a window or windows.
HWnd is the WindowID or HWND_BROADCAST to send message to all top level windows.
     Message is not sent to child windows. (If I understand this correctly this means
     it is sent to all the immediate children of the root window (0).
Msg  the message
wParam additional parameter
lParam additioanl parameter

It is most likely you won't use this directly but through one of the functions
implemented already in Win32::GuiTest.

See the guitest.xs for some examples.


=item $res = PostMessage(hwnd,msg,wParam,lParam) *

See corresponding Windows library function in MSDN.

=item CheckButton(hwnd) 

=item UnCheckButton(hwnd) 

=item GrayOutButton(hwnd)

=item BOOL IsCheckedButton(hwnd)

=item BOOL IsGrayedButton(hwnd)

The names say it.  Works on radio buttons and
checkboxes.  For regular buttons, use IsWindowEnabled.

=item BOOL IsWindow(hwnd) *

=item ($x,$y) = ScreenToClient(hwnd,x,y) *

=item ($x,$y) = ClientToScreen(hwnd,x,y) *

=item ($x,$y) = GetCaretPos(hwnd) *A

=item HWND SetFocus(hWnd) *A

=item HWND GetFocus(hwnd) *A

=item HWND GetActiveWindow(hwnd) *A

=item HWND GetForegroundWindow() *

=item HWND SetActiveWindow(hwnd) *A

=item BOOL EnableWindow(hwnd,fEnable) *

=item BOOL IsWindowEnabled(hwnd)*

=item BOOL IsWindowVisible(hwnd)*

=item BOOL ShowWindow(hwnd,nCmdShow) *A

See corresponding Windows functions.

=item ($x,$y) = ScreenToNorm(x,y) 

Returns normalised coordinates of given point (0-FFFF as a fraction of screen 
resolution)

=item ($x,$y) = NormToScreen(x,y)

The opposite transformation

=item ($x,$y) = GetScreenRes()

Returns screen resolution

=item HWND WindowFromPoint(x, y)

=item ($l,$t,$r,$b) = GetWindowRect(hWnd) *

=item ($l,$t,$r,$b) = GetClientRect(hWnd) *

See corresponding Windows functions.

=cut

=item SelComboItem($window, $index)

Selects an item in the combo box based off an index (zero-based).

=item SelComboItemText($window, $txt)

Selects an item in the combo box based off text (case insensitive).

=item $txt = GetComboText(hwnd,index)

=item $txt = GetListText(hwnd,index)

=item @lst = GetComboContents(hWnd)

=item @lst = GetListContents(hWnd)

Fetch the contents of the list and combo boxes.

=item GetAsyncKeyState($key)

=item IsKeyPressed($key)

Wrapper around the GetAsyncKeyState API function. Returns TRUE if the user presses the 
specified key.

    IsKeyPressed("ESC");
    IsKeyPressed("A");
    IsKeyPressed("DOWN"); 
    IsKeyPressed( VK_DOWN);

=cut

sub IsKeyPressed
{
	my $c = shift;
	if ( $c =~ /^.$/) {
		$c = ord uc $c;	
	} elsif ( not defined( $c = find_vkey( $c))) {
		return 0;
	}
	return GetAsyncKeyState( $c);
}

=pod

=item SendRawKey($virtualkey,$flags)

Wrapper around keybd_event. Allows sending low-level keys. The first argument is any of the VK_* constants. The second argument can be 0, KEYEVENTF_EXTENDEDKEY, KEYEVENTF_KEYUP or a combination of them.

    KEYEVENTF_EXTENDEDKEY - Means it is an extended key (i.e. to distinguish between arrow keys on the numeric keypad and elsewhere). 
    KEYEVENTF_KEYUP       - Means keyup. Unspecified means keydown.

   #Example
   use Win32::GuiTest qw/:FUNC :VK/;

   while (1) {
       SendRawKey(VK_DOWN, KEYEVENTF_EXTENDEDKEY); 
       SendKeys "{PAUSE 200}";
   }

=item VkKeyScan(int)

=item C<GetListViewContents($handle)>

    Return the items of the list view with C<$handle> as a list, each
	element of which is a reference to an array containing the values
	in each column.

=cut

sub GetListViewContents
{
	my $lv = shift;
	my @res;
	my $item_count = GetListViewItemCount($lv);
	my $header = GetListViewHeader($lv);
	my $column_count = $header ? GetHeaderColumnCount($header) : 1;
	# Items are numbered from 0
	for my $item_index (0..$item_count-1)
	{
		# Sub-items are numbered from 1
		push @res, [map(GetListViewItem($lv, $item_index, $_),
						0..$column_count - 1)];
	}
	return @res;
}

=item SelListViewItem($window, $idx, [$multi_select])

    Selects an item in the list view based off an index (zero-based).

	# Select first item, clears out any previous selections.
	SelListViewItem($win, 0);
	# Select an *additional* item.
	SelListViewItem($win, 1, 1);

=cut

=item SelListViewItemText($window, $txt, [$multi_select])

    Selects an item in the list view based off text (case insensitive).

	# Select first item, clears out any previous selections.
	SelListViewItemText($win, 'Temp');
	# Select an *additional* item.
	SelListViewItemText($win, 'cabs', 1);

=cut

=item IsListViewItemSel($window, $txt)

   Determines if the specified list view item is selected.

=cut

=item GetTabItems($window)

    Returns a list of a tab control's labels.

=cut

=item SelTabItem($window, $idx)

    Selects a tab based off an index (zero-based).

=cut

=item SelTabItemText($window, $txt)

    Selects a tab based off text label (case insensitive).

=cut

=item IsTabItemSel($window, $txt)

   Determines if the specified tab item is selected.

=cut

=item SelTreeViewItemPath($window, $path)

    Selects a tree view item based off a "path" (case insensitive).

    # Select Machine item and Processors sub-item.
    SelTreeViewItemPath($window, "Machine|Processors");

    SelTreeViewItemPath($window, "Item");

=cut

sub SelTreeViewItemPath
{
    my $hwnd = shift;
    my $path = shift;
    my $max_buf = shift;
    my $delay = shift;

    if( !$max_buf ){
        $max_buf = 124;
    }
    if( !$delay ){
        $delay = 0.50;
    }
    
    my @parts = split( /\|/, $path );
 
    my $tvitem;
    eval{
        $tvitem = AllocateVirtualBuffer( $hwnd, 50 );
    };
    if( $@ ){
        die "Allocation failed with message ---> $@";
    }
    
    my $text_buf = AllocateVirtualBuffer( $hwnd, $max_buf );
 
    my $hItem = SendMessage( $hwnd, TVM_GETNEXTITEM(), TVGN_ROOT(), 0 );
    $hItem = TVPathWalk( $hwnd, 
                         $tvitem, 
                         $text_buf, 
                         $hItem, 
                         $max_buf, 
                         $delay, 
                         @parts );
    SendMessage( $hwnd, TVM_SELECTITEM(), TVGN_CARET(), $hItem );
    FreeVirtualBuffer( $tvitem );
    FreeVirtualBuffer( $text_buf );
    return;
}

#
# Helper method. Used by SelTreeViewItemPath to find an item in the tree
# view.
#
sub TVPathWalk
{
    my $hwnd     = shift;
    my $tvitem   = shift;
    my $text_buf = shift;
    my $hItem    = shift;
    my $max_buf  = shift;
    my $delay    = shift;
    my @parts    = @_;
    SendMessage( $hwnd, TVM_ENSUREVISIBLE(), 0, $hItem );
    while( $hItem != 0 ){
        my $str_long = pack( "L L L L L L L L L L", 
            0x41, #mask (TVIF_TEXT | TVIF_CHILDREN)
            $hItem, #hItem
            0, #state
            0, #stateMask
            $text_buf->{ 'ptr' }, #pszText
            100, #cchTextMax
            0, #iImage
            0, #iSelectedImage
            0, #cChildren
            0  #lParam 
                            );

        WriteToVirtualBuffer( $tvitem, $str_long );
        SendMessage( $hwnd, TVM_GETITEM(), 0, $tvitem->{ 'ptr' } ); 
        my $text = ReadFromVirtualBuffer( $text_buf, $max_buf );
        $text =~ s/\0.+$//;
        my $struct = ReadFromVirtualBuffer( $tvitem, 40 );
        my @fields = unpack( "L10", $struct );
        if( $text eq $parts[ 0 ] ){
            SendMessage( $hwnd, TVM_EXPAND(), TVE_EXPAND(), $hItem );
            #
            # Give the node some time to expand...
            #
            select(undef, undef, undef, $delay) if $delay;
            if( @parts == 1 ){
                return $hItem;
            }
            if( $fields[ 8 ] ){
                my $hChild = SendMessage( $hwnd, 
                                       TVM_GETNEXTITEM(),
                                       TVGN_CHILD(),
                                       $hItem );
                shift( @parts );
                return TVPathWalk( $hwnd, 
                                   $tvitem, 
                                   $text_buf, 
                                   $hChild, 
                                   $max_buf, 
                                   $delay, 
                                   @parts );
            }
        }else{
            $hItem = SendMessage( $hwnd, 
                                  TVM_GETNEXTITEM(), 
                                  TVGN_NEXT(), 
                                  $hItem );
        }
    }
    return 0;
}    

=item GetTreeViewSelPath($window)

   Returns a string containing the path (i.e., "parent|child") of
   the currently selected tree view item.

   $oldpath = GetTreeViewSelPath($window);
   SelTreeViewItemPath($window, "Parent|Child");
   SelTreeViewItemPath($window, $oldpath);

=cut

=item $hpopup = GetPopupHandle($hwnd, $x, $y, [$wait])

   This function gets the handle of a popup window generated by
   right-clicking at the $x and $y screen coordinates (absolute). An
   optional delay can be entered which will wait the given number of
   milliseconds after the right-click for the window to appear (default
   is 50). Zero is returned when no popup menu is found.

=back

=head2 DibSect

A class to manage a Windows DIB section. Currently limited in functionality to 
24-bit images. Pulled from old code into GuiTest when I (jurasz@imb.uni-karlsruhe.de) 
needed to create several grayscale screen dumps.

Possible future extenstions: other color resolutions, loading, comparison of bitmaps,
getting from clipboard.

Synopsis:

  $ds = new Win32::GuiTest::DibSect;
  $ds->CopyWindow($w);
  $ds->ToGrayScale();
  $ds->SaveAs("bla.bmp");
  $ds->ToClipboard();

=over 8  

=item bool DibSect::CopyClient(hwnd,[rect])

Copy a client area of given window (or possibly its subset) into a given DibSect.
The rectangle may be optionally passed as a reference to 4-element array.
To get the right result make sure the window you want to copy is not obscured by 
others.

=item bool DibSect::CopyWindow(hwnd)

Copy the window rectangle. Equivalent to 

  $ds->CopyClient(GetDesktopWindow(), \@{[GetWindowRect($w)]});

=item bool DibSect::SaveAs(szFile)

Save the current contents of the DIB section in a given file. With 24-bit 
resolution it can grow quite big, so I immediately convert them to PNG (direct 
writing of PNG seemed to complicated to implement).

=item bool DibSect::Invert()

Invert the colors in a current DIB section.

=item bool DibSect::ToGrayScale()

Convert the DibSection to the gray scale. Note that it is still encoded as 24-bit
BMP for simplicity.

=item bool DibSect::ToClipboard()

Copies the DibSect to clipboard (as an old-fashioned metafile), so that it can 
be further processed with your favourite image processing software, for example 
automatically using SendKeys.

=item bool DibSect::Destroy()

Destroys the contents of the DIB section.

=back

=cut


package Win32::GuiTest::Window;

sub new
{
    my ( $proto, %params ) = @_;
    
    my $class = ref( $proto ) || $proto;
    my $self = {};

    $self->{ '-handle' } = $params{ '-handle' };
    bless( $self, $class );
    return $self;
}

sub SetForegroundWindow
{
    my $self = shift;
    Win32::GuiTest::SetForegroundWindow( $self->{ '-handle' } );
}

sub GetWindowRect
{
    my $self = shift;
    return Win32::GuiTest::GetWindowRect( $self->{ '-handle' } );
}


1;
__END__

=head1 UNICODE SUPPORT

Currently (2007) there's no consensus about unicode input in Perl, so
the module declares function C<UnicodeSemantics> that sets whether 
information queried from windows should use A or W syscalls. The
function that support this differentiation, and produce different
results depending on value set to C<UnicodeSemantics> is:

C<GetWindowText>, and all its callers, - FindWindowLike, WaitWindow,
WaitWindowLike

C<SendKeys> translated unicode characters into set of ALT+NUMPAD keystrokes.
Note that not all applications can understand unicode input.

=over

=item UnicodeSemantics [BOOL]

If a boolean parameter is set, changes the semantics flag for functions
that return results of either A or W syscalls. If the parameter is
not set, returns the current value of the flag.

=back


=head1 DEVELOPMENT

If you would like to participate in the development of this module there are 
several thing that need to be done. For some of them you only need Perl
and the latest source of the module from CVS for others you'll also need to 
have a C++ compiler.

To get the latest source code you need a CVS client and then do the following:

 cvs -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/winguitest login
 cvs -z3 -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/winguitest co Win32-GuiTest

See more detailed explanations here http://sourceforge.net/projects/winguitest/

=head2 cygwin

g++ needs to be installed

  perl Makefile.PL
  make
  make test
  make install

=head2 MSVC environment

To setup a development environment for compiling the C++ code you can either buy
Visual Studio with Visual C++ or you can download a few things free of charge from 
Microsoft. There might be other ways too we have not explored.

The instructions to get the free environment are here:

From http://www.microsoft.com/ download and install:

 1) Microsoft .NET Framework Version 1.1 Redistributable Package
 2) .NET Framework SDK Version 1.1


This is not enough as there are a number of header files and libraries that are 
not included in these distributions. You can get them from Microsoft in two additional
downloads. For these you will have to be using Internet Explorer.
Visit 

  http://www.microsoft.com/msdownload/platformsdk/sdkupdate/

and install 

 1) Core SDK
 2) Microsoft Data Access Components 2.7


Before you can compile you'll have to open a command prompt and execute the
C<sdkvars.bat> script from the.NET SDK that will set a number of environment
variables. In addition you'll have to run the C<setenv.bat> you got with the 
Core SDK (and located in C:\Program Files\Microsoft SDK) with the appropriate
parameters. For me this was /XP32 /RETAIL


In order to finish the packaging you'll also need the tar, gzip and zip utilities from 

 http://gnuwin32.sourceforge.net/packages.html

I have not tried it yet.

After this you will probably be able to do the normal cycle:

 perl makefile.pl
 nmake
 nmake test

 or run

 perl makedist.pl

=head1 SEE ALSO

Module's documentation is available at L<http://www.piotrkaluski.com/files/winguitest/docs/index.html>.

=head1 TODO

Here are a few items where help would be welcome. 

=head2 Perl only

 Improve Tests
 Improve documentation
 Add more examples and explain them

=head2 C++ compiler needed

 Add more calls to the C++ backend
 Fix current calls

 32bit custom controls (some already implemented)
 Possibly Java interfaces
 Retreive the list of the menu of a given window.

=cut

=head1 COPYRIGHT

The SendKeys function is based on the Delphi sourcecode
published by Al Williams  E<lt>http://www.al-williams.com/awc/E<gt> 
in Dr.Dobbs  E<lt>http://www.drdobbs.com/keys-to-the-kingdom/184410429E<gt>.

Copyright (c) 1998-2002 Ernesto Guisado, (c) 2004 Dennis K. Paulsen. All rights 
reserved. This program is free software; You may distribute it and/or modify it
under the same terms as Perl itself.

=head1 AUTHORS

Ernesto Guisado (erngui@acm.org), http://triumvir.org

Jarek Jurasz (jurasz@imb.uni-karlsruhe.de), http://www.uni-karlsruhe.de/~gm07 wrote 
DibSect and some other pieces (see C<Changes> for details).

Dennis K. Paulsen (ctrondlp@cpan.org) wrote various pieces (See C<Changes> for
details).

Dmitry Karasik (dmitry@karasik.eu.org) added support for unicode and cygwin/mingw.

=head1 CREDITS

Thanks very much to:

=over 4

=item Johannes Maehner

=item Ben Shern

=item Phill Wolf

=item Mauro

=item Sohrab Niramwalla

=item Frank van Dijk

=item Jarek Jurasz

=item Wilson P. Snyder II

=item Rudi Farkas

=item Paul Covington

=item Piotr Kaluski

=item ...and more...

for code, suggestions and bug fixes.

=back

=cut
