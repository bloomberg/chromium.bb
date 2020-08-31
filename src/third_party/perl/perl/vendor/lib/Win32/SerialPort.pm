package Win32::SerialPort;

use strict;
use warnings;

use Win32;
use Win32API::CommPort qw( :STAT :PARAM 0.20 );

use Carp;

our $VERSION = '0.22';

require Exporter;

our @ISA = qw( Exporter Win32API::CommPort );

our @EXPORT= qw();
our @EXPORT_OK= @Win32API::CommPort::EXPORT_OK;
our %EXPORT_TAGS = %Win32API::CommPort::EXPORT_TAGS;

# parameters that must be included in a "save" and "checking subs"

my %validate =	(
   ALIAS     => "alias",
   BAUD      => "baudrate",
   BINARY    => "binary",
   DATA      => "databits",
   E_MSG     => "error_msg",
   EOFCHAR   => "eof_char",
   ERRCHAR   => "error_char",
   EVTCHAR   => "event_char",
   HSHAKE    => "handshake",
   PARITY    => "parity",
   PARITY_EN => "parity_enable",
   RCONST    => "read_const_time",
   READBUF   => "set_read_buf",
   RINT      => "read_interval",
   RTOT      => "read_char_time",
   STOP      => "stopbits",
   U_MSG     => "user_msg",
   WCONST    => "write_const_time",
   WRITEBUF  => "set_write_buf",
   WTOT      => "write_char_time",
   XOFFCHAR  => "xoff_char",
   XOFFLIM   => "xoff_limit",
   XONCHAR   => "xon_char",
   XONLIM    => "xon_limit",
   intr      => "is_stty_intr",
   quit      => "is_stty_quit",
   s_eof     => "is_stty_eof",
   eol       => "is_stty_eol",
   erase     => "is_stty_erase",
   s_kill    => "is_stty_kill",
   bsdel     => "stty_bsdel",
   clear     => "is_stty_clear",
   echo      => "stty_echo",
   echoe     => "stty_echoe",
   echok     => "stty_echok",
   echonl    => "stty_echonl",
   echoke    => "stty_echoke",
   echoctl   => "stty_echoctl",
   istrip    => "stty_istrip",
   icrnl     => "stty_icrnl",
   ocrnl     => "stty_ocrnl",
   opost     => "stty_opost",
   igncr     => "stty_igncr",
   inlcr     => "stty_inlcr",
   onlcr     => "stty_onlcr",
   isig      => "stty_isig",
   icanon    => "stty_icanon",
   DVTYPE    => "devicetype",
   HNAME     => "hostname",
   HADDR     => "hostaddr",
   DATYPE    => "datatype",
   CFG_1     => "cfg_param_1",
   CFG_2     => "cfg_param_2",
   CFG_3     => "cfg_param_3",
);

# parameters supported by the stty method

my %opts = (
   "intr"     => "is_stty_intr:argv_char",
   "quit"     => "is_stty_quit:argv_char",
   "eof"      => "is_stty_eof:argv_char",
   "eol"      => "is_stty_eol:argv_char",
   "erase"    => "is_stty_erase:argv_char",
   "kill"     => "is_stty_kill:argv_char",
   "echo"     => "stty_echo:1",
   "-echo"    => "stty_echo:0",
   "echoe"    => "stty_echoe:1",
   "-echoe"   => "stty_echoe:0",
   "echok"    => "stty_echok:1",
   "-echok"   => "stty_echok:0",
   "echonl"   => "stty_echonl:1",
   "-echonl"  => "stty_echonl:0",
   "echoke"   => "stty_echoke:1",
   "-echoke"  => "stty_echoke:0",
   "echoctl"  => "stty_echoctl:1",
   "-echoctl" => "stty_echoctl:0",
   "istrip"   => "stty_istrip:1",
   "-istrip"  => "stty_istrip:0",
   "icrnl"    => "stty_icrnl:1",
   "-icrnl"   => "stty_icrnl:0",
   "ocrnl"    => "stty_ocrnl:1",
   "-ocrnl"   => "stty_ocrnl:0",
   "igncr"    => "stty_igncr:1",
   "-igncr"   => "stty_igncr:0",
   "inlcr"    => "stty_inlcr:1",
   "-inlcr"   => "stty_inlcr:0",
   "onlcr"    => "stty_onlcr:1",
   "-onlcr"   => "stty_onlcr:0",
   "opost"    => "stty_opost:1",
   "-opost"   => "stty_opost:0",
   "isig"     => "stty_isig:1",
   "-isig"    => "stty_isig:0",
   "icanon"   => "stty_icanon:1",
   "-icanon"  => "stty_icanon:0",
   "parenb"   => "parity_enable:1",
   "-parenb"  => "parity_enable:0",
   "inpck"    => "parity_enable:1",
   "-inpck"   => "parity:none",
   "cs5"      => "databits:5",
   "cs6"      => "databits:6",
   "cs7"      => "databits:7",
   "cs8"      => "databits:8",
   "cstopb"   => "stopbits:2",
   "-cstopb"  => "stopbits:1",
   "parodd"   => "parity:odd",
   "-parodd"  => "parity:even",
   "clocal"   => "handshake:none",
   "-clocal"  => "handshake:dtr",
   "crtscts"  => "handshake:rts",
   "-crtscts" => "handshake:none",
   "ixon"     => "handshake:xoff",
   "-ixon"    => "handshake:none",
   "ixoff"    => "handshake:xoff",
   "-ixoff"   => "handshake:none",
   "start"    => "xon_char:argv_char",
   "stop"     => "xoff_char:argv_char",
);

#### Package variable declarations ####

my @binary_opt = (0, 1);
my @byte_opt = (0, 255);

my $cfg_file_sig="Win32::SerialPort_Configuration_File -- DO NOT EDIT --\n";

my $Verbose = 0;

    # test*.t only - suppresses default messages
sub set_test_mode_active {
    return unless (@_ == 2);
    Win32API::CommPort->set_no_messages($_[1]);
	# object not defined but :: upsets strict
    return (keys %validate);
}

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $device = shift;
    my $alias = $device;
    if ($device =~ /^COM\d+$/io) {
	$device = '\\\\.\\' . $device;
	# required COM10 and beyond, done for all to facilitate testing
    }
    my @new_cmd = ($device);
    my $quiet = shift;
    if ($quiet) {
        push @new_cmd, 1;
    }
    my $self  = $class->SUPER::new(@new_cmd);

    unless ($self) {
        return 0 if ($quiet);
        return;
    }

    # "private" data
    $self->{"_DEBUG"}    	= 0;
    $self->{U_MSG}     		= 0;
    $self->{E_MSG}     		= 0;
    $self->{OFS}		= "";
    $self->{ORS}		= "";
    $self->{"_T_INPUT"}		= "";
    $self->{"_LOOK"}		= "";
    $self->{"_LASTLOOK"}	= "";
    $self->{"_LASTLINE"}	= "";
    $self->{"_CLASTLINE"}	= "";
    $self->{"_SIZE"}		= 1;
    $self->{"_LMATCH"}		= "";
    $self->{"_LPATT"}		= "";
    $self->{"_PROMPT"}		= "";
    $self->{"_MATCH"}		= [];
    $self->{"_CMATCH"}		= [];
    @{ $self->{"_MATCH"} }	= "\n";
    @{ $self->{"_CMATCH"} }	= "\n";
    $self->{DVTYPE}		= "none";
    $self->{HNAME}		= "localhost";
    $self->{HADDR}		= 0;
    $self->{DATYPE}		= "raw";
    $self->{CFG_1}		= "none";
    $self->{CFG_2}		= "none";
    $self->{CFG_3}		= "none";

    # user settable options for lookfor (the "stty" collection)
    # defaults like RedHat linux unless indicated
	# char to abort nextline subroutine
    $self->{intr}	= "\cC";	# MUST be single char

	# char to abort perl
    $self->{quit}	= "\cD";	# MUST be single char

	# end_of_file char (linux typ: "\cD")
    $self->{s_eof}	= "\cZ";	# MUST be single char

	# end_of_line char
    $self->{eol}	= "\cJ";	# MUST be single char

	# delete one character from buffer (backspace)
    $self->{erase}	= "\cH";	# MUST be single char

	# clear line buffer
    $self->{s_kill}	= "\cU";	# MUST be single char

	# written after erase character
    $self->{bsdel}	= "\cH \cH";

	# written after kill character
    my $space76 = " "x76;
    $self->{clear}	= "\r$space76\r";	# 76 spaces

	# echo every character
    $self->{echo}	= 0;

	# echo erase character with bsdel string
    $self->{echoe}	= 1;

	# echo \n after kill character
    $self->{echok}	= 1;

	# echo \n
    $self->{echonl}	= 0;

	# echo clear string after kill character
    $self->{echoke}	= 1;	# linux console yes, serial no

	# echo "^Char" for control chars
    $self->{echoctl}	= 0;	# linux console yes, serial no

	# strip input to 7-bits
    $self->{istrip}	= 0;

	# map \r to \n on input
    $self->{icrnl}	= 0;

	# map \r to \n on output
    $self->{ocrnl}	= 0;

	# ignore \r on input
    $self->{igncr}	= 0;

	# map \n to \r on input
    $self->{inlcr}	= 0;

	# map \n to \r\n on output
    $self->{onlcr}	= 1;

	# enable output mapping
    $self->{opost}	= 0;

	# enable quit and intr characters
    $self->{isig}	= 0;	# linux actually SUPPORTS signals

	# enable erase and kill characters
    $self->{icanon}	= 0;

    my $token;
    my @bauds = $self->are_baudrate;
    foreach $token (@bauds) { $opts{$token} = "baudrate:$token"; }

    # initialize (in CommPort) and write_settings need these defined
    $self->{"_N_U_MSG"} 	= 0;
    $self->{"_N_E_MSG"}		= 0;
    $self->{"_N_ALIAS"}		= 0;
    $self->{"_N_intr"}		= 0;
    $self->{"_N_quit"}		= 0;
    $self->{"_N_s_eof"}		= 0;
    $self->{"_N_eol"}		= 0;
    $self->{"_N_erase"}		= 0;
    $self->{"_N_s_kill"}	= 0;
    $self->{"_N_bsdel"}		= 0;
    $self->{"_N_clear"}		= 0;
    $self->{"_N_echo"}		= 0;
    $self->{"_N_echoe"}		= 0;
    $self->{"_N_echok"}		= 0;
    $self->{"_N_echonl"}	= 0;
    $self->{"_N_echoke"}	= 0;
    $self->{"_N_echoctl"}	= 0;
    $self->{"_N_istrip"}	= 0;
    $self->{"_N_icrnl"}		= 0;
    $self->{"_N_ocrnl"}		= 0;
    $self->{"_N_opost"}		= 0;
    $self->{"_N_igncr"}		= 0;
    $self->{"_N_inlcr"}		= 0;
    $self->{"_N_onlcr"}		= 0;
    $self->{"_N_isig"}		= 0;
    $self->{"_N_icanon"}	= 0;
    $self->{"_N_DVTYPE"}	= 0;
    $self->{"_N_HNAME"}		= 0;
    $self->{"_N_HADDR"}		= 0;
    $self->{"_N_DATYPE"}	= 0;
    $self->{"_N_CFG_1"}		= 0;
    $self->{"_N_CFG_2"}		= 0;
    $self->{"_N_CFG_3"}		= 0;

    $self->{ALIAS} 	= $alias;	# so "\\.\+++" can be changed
    $self->{DEVICE} 	= $device;	# clone so NAME stays in CommPort

    ($self->{MAX_RXB}, $self->{MAX_TXB}) = $self->buffer_max;

    bless ($self, $class);
    return $self;
}


sub stty_intr {
    my $self = shift;
    if (@_ == 1) { $self->{intr} = shift; }
    return if (@_);
    return $self->{intr};
}

sub stty_quit {
    my $self = shift;
    if (@_ == 1) { $self->{quit} = shift; }
    return if (@_);
    return $self->{quit};
}

sub is_stty_eof {
    my $self = shift;
    if (@_ == 1) { $self->{s_eof} = chr(shift); }
    return if (@_);
    return ord($self->{s_eof});
}

sub is_stty_eol {
    my $self = shift;
    if (@_ == 1) { $self->{eol} = chr(shift); }
    return if (@_);
    return ord($self->{eol});
}

sub is_stty_quit {
    my $self = shift;
    if (@_ == 1) { $self->{quit} = chr(shift); }
    return if (@_);
    return ord($self->{quit});
}

sub is_stty_intr {
    my $self = shift;
    if (@_ == 1) { $self->{intr} = chr(shift); }
    return if (@_);
    return ord($self->{intr});
}

sub is_stty_erase {
    my $self = shift;
    if (@_ == 1) { $self->{erase} = chr(shift); }
    return if (@_);
    return ord($self->{erase});
}

sub is_stty_kill {
    my $self = shift;
    if (@_ == 1) { $self->{s_kill} = chr(shift); }
    return if (@_);
    return ord($self->{s_kill});
}

sub is_stty_clear {
    my $self = shift;
    my @chars;
    if (@_ == 1) {
	@chars = split (//, shift);
	for (@chars) {
	    $_ = chr ( ord($_) - 32 );
	}
        $self->{clear} = join("", @chars);
        return $self->{clear};
    }
    return if (@_);
    @chars = split (//, $self->{clear});
    for (@chars) {
        $_ = chr ( ord($_) + 32 );
    }
    my $permute = join("", @chars);
    return $permute;
}

sub stty_eof {
    my $self = shift;
    if (@_ == 1) { $self->{s_eof} = shift; }
    return if (@_);
    return $self->{s_eof};
}

sub stty_eol {
    my $self = shift;
    if (@_ == 1) { $self->{eol} = shift; }
    return if (@_);
    return $self->{eol};
}

sub stty_erase {
    my $self = shift;
    if (@_ == 1) {
        my $tmp = shift;
	return unless (length($tmp) == 1);
	$self->{erase} = $tmp;
    }
    return if (@_);
    return $self->{erase};
}

sub stty_kill {
    my $self = shift;
    if (@_ == 1) {
        my $tmp = shift;
	return unless (length($tmp) == 1);
	$self->{s_kill} = $tmp;
    }
    return if (@_);
    return $self->{s_kill};
}

sub stty_bsdel {
    my $self = shift;
    if (@_ == 1) { $self->{bsdel} = shift; }
    return if (@_);
    return $self->{bsdel};
}

sub stty_clear {
    my $self = shift;
    if (@_ == 1) { $self->{clear} = shift; }
    return if (@_);
    return $self->{clear};
}

sub stty_echo {
    my $self = shift;
    if (@_ == 1) { $self->{echo} = yes_true ( shift ) }
    return if (@_);
    return $self->{echo};
}

sub stty_echoe {
    my $self = shift;
    if (@_ == 1) { $self->{echoe} = yes_true ( shift ) }
    return if (@_);
    return $self->{echoe};
}

sub stty_echok {
    my $self = shift;
    if (@_ == 1) { $self->{echok} = yes_true ( shift ) }
    return if (@_);
    return $self->{echok};
}

sub stty_echonl {
    my $self = shift;
    if (@_ == 1) { $self->{echonl} = yes_true ( shift ) }
    return if (@_);
    return $self->{echonl};
}

sub stty_echoke {
    my $self = shift;
    if (@_ == 1) { $self->{echoke} = yes_true ( shift ) }
    return if (@_);
    return $self->{echoke};
}

sub stty_echoctl {
    my $self = shift;
    if (@_ == 1) { $self->{echoctl} = yes_true ( shift ) }
    return if (@_);
    return $self->{echoctl};
}

sub stty_istrip {
    my $self = shift;
    if (@_ == 1) { $self->{istrip} = yes_true ( shift ) }
    return if (@_);
    return $self->{istrip};
}

sub stty_icrnl {
    my $self = shift;
    if (@_ == 1) { $self->{icrnl} = yes_true ( shift ) }
    return if (@_);
    return $self->{icrnl};
}

sub stty_ocrnl {
    my $self = shift;
    if (@_ == 1) { $self->{ocrnl} = yes_true ( shift ) }
    return if (@_);
    return $self->{ocrnl};
}

sub stty_opost {
    my $self = shift;
    if (@_ == 1) { $self->{opost} = yes_true ( shift ) }
    return if (@_);
    return $self->{opost};
}

sub stty_igncr {
    my $self = shift;
    if (@_ == 1) { $self->{igncr} = yes_true ( shift ) }
    return if (@_);
    return $self->{igncr};
}

sub stty_inlcr {
    my $self = shift;
    if (@_ == 1) { $self->{inlcr} = yes_true ( shift ) }
    return if (@_);
    return $self->{inlcr};
}

sub stty_onlcr {
    my $self = shift;
    if (@_ == 1) { $self->{onlcr} = yes_true ( shift ) }
    return if (@_);
    return $self->{onlcr};
}

sub stty_isig {
    my $self = shift;
    if (@_ == 1) { $self->{isig} = yes_true ( shift ) }
    return if (@_);
    return $self->{isig};
}

sub stty_icanon {
    my $self = shift;
    if (@_ == 1) { $self->{icanon} = yes_true ( shift ) }
    return if (@_);
    return $self->{icanon};
}

sub is_prompt {
    my $self = shift;
    if (@_ == 1) { $self->{"_PROMPT"} = shift; }
    return if (@_);
    return $self->{"_PROMPT"};
}

sub are_match {
    my $self = shift;
    my $pat;
    my $re_next = 0;
    if (@_) {
	@{ $self->{"_MATCH"} } = @_;
	@{ $self->{"_CMATCH"} } = ();
	while ($pat = shift) {
	    if ($re_next) {
		$re_next = 0;
	        eval 'push (@{ $self->{"_CMATCH"} }, qr/$pat/)';
	   } else {
	        push (@{ $self->{"_CMATCH"} }, $pat);
	   }
	   if ($pat eq "-re") {
		$re_next++;
	    }
	}
    }
    return @{ $self->{"_MATCH"} };
}


# parse values for start/restart
sub get_start_values {
    return unless (@_ == 2);
    my $self = shift;
    my $filename = shift;

    unless ( open CF, "<$filename" ) {
        carp "can't open file: $filename";
        return;
    }
    my ($signature, $name, @values) = <CF>;
    close CF;

    unless ( $cfg_file_sig eq $signature ) {
        carp "Invalid signature in $filename: $signature";
        return;
    }
    chomp $name;
    unless ( $self->{DEVICE} eq $name ) {
        carp "Invalid Port DEVICE=$self->{DEVICE} in $filename: $name";
        return;
    }
    if ($Verbose or not $self) {
        print "signature = $signature";
        print "name = $name\n";
        if ($Verbose) {
            print "values:\n";
            foreach (@values) { print "    $_"; }
        }
    }
    my $item;
    my $key;
    my $value;
    my $gosub;
    my $fault = 0;
    no strict 'refs';		# for $gosub
    foreach $item (@values) {
        chomp $item;
        ($key, $value) = split (/,/, $item);
        if ($value eq "") { $fault++ }
        else {
            $gosub = $validate{$key};
            unless (defined &$gosub ($self, $value)) {
    	        carp "Invalid parameter for $key=$value   ";
    	        return;
	    }
        }
    }
    use strict 'refs';
    if ($fault) {
        carp "Invalid value in $filename";
        undef $self;
        return;
    }
    1;
}

sub restart {
    return unless (@_ == 2);
    my $self = shift;
    my $filename = shift;

    unless ( $self->init_done ) {
        carp "Can't restart before Port has been initialized";
        return;
    }
    get_start_values($self, $filename);
    write_settings($self);
}

sub start {
    my $proto = shift;
    my $class = ref($proto) || $proto;

    return unless (@_);
    my $filename = shift;

    unless ( open CF, "<$filename" ) {
        carp "can't open file: $filename";
        return;
    }
    my ($signature, $name, @values) = <CF>;
    close CF;

    unless ( $cfg_file_sig eq $signature ) {
        carp "Invalid signature in $filename: $signature";
        return;
    }
    chomp $name;
    my $self  = new ($class, $name);
    if ($Verbose or not $self) {
        print "signature = $signature";
        print "class = $class\n";
        print "name = $name\n";
        if ($Verbose) {
            print "values:\n";
            foreach (@values) { print "    $_"; }
        }
    }
    if ($self) {
        if ( get_start_values($self, $filename) ) {
            write_settings ($self);
	}
        else {
            carp "Invalid value in $filename";
            undef $self;
            return;
        }
    }
    return $self;
}

sub write_settings {
    my $self = shift;
    my @items = keys %validate;

    # initialize returns number of faults
    if ( $self->initialize(@items) ) {
        unless (nocarp) {
            carp "write_settings failed, closing port";
	    $self->close;
	}
        return;
    }

    $self->update_DCB;
    if ($Verbose) {
        print "writing settings to $self->{ALIAS}\n";
    }
    1;
}

sub save {
    my $self = shift;
    my $item;
    my $getsub;
    my $value;

    return unless (@_);
    unless ($self->init_done) {
        carp "can't save until init_done";
	return;
    }

    my $filename = shift;
    unless ( open CF, ">$filename" ) {
        carp "can't open file: $filename";
        return;
    }
    print CF "$cfg_file_sig";
    print CF "$self->{DEVICE}\n";
	# used to "reopen" so must be DEVICE=NAME

    no strict 'refs';		# for $gosub
    while (($item, $getsub) = each %validate) {
        chomp $getsub;
	$value = scalar &$getsub($self);
        print CF "$item,$value\n";
    }
    use strict 'refs';
    close CF;
    if ($Verbose) {
        print "wrote file $filename for $self->{ALIAS}\n";
    }
    1;
}

##### tied FileHandle support

sub TIEHANDLE {
    my $proto = shift;
    my $class = ref($proto) || $proto;

    return unless (@_);

    my $self = start($class, shift);
    return $self;
}

# WRITE this, LIST
#      This method will be called when the handle is written to via the
#      syswrite function.

sub WRITE {
    return if (@_ < 3);
    my $self = shift;
    my $buf = shift;
    my $len = shift;
    my $offset = 0;
    if (@_) { $offset = shift; }
    my $out2 = substr($buf, $offset, $len);
    return unless ($self->post_print($out2));
    return length($out2);
}

# PRINT this, LIST
#      This method will be triggered every time the tied handle is printed to
#      with the print() function. Beyond its self reference it also expects
#      the list that was passed to the print function.

sub PRINT {
    my $self = shift;
    return unless (@_);
    my $ofs = $, ? $, : "";
    if ($self->{OFS}) { $ofs = $self->{OFS}; }
    my $ors = $\ ? $\ : "";
    if ($self->{ORS}) { $ors = $self->{ORS}; }
    my $output = join($ofs,@_);
    $output .= $ors;
    return $self->post_print($output);
}

sub output_field_separator {
    my $self = shift;
    my $prev = $self->{OFS};
    if (@_) { $self->{OFS} = shift; }
    return $prev;
}

sub output_record_separator {
    my $self = shift;
    my $prev = $self->{ORS};
    if (@_) { $self->{ORS} = shift; }
    return $prev;
}

sub post_print {
    my $self = shift;
    return unless (@_);
    my $output = shift;
    if ($self->stty_opost) {
	if ($self->stty_ocrnl) { $output =~ s/\r/\n/osg; }
	if ($self->stty_onlcr) { $output =~ s/\n/\r\n/osg; }
    }
    my $to_do = length($output);
    my $done = 0;
    my $written = 0;
    while ($done < $to_do) {
        my $out2 = substr($output, $done);
        $written = $self->write($out2);
	if (! defined $written) {
	    $^E = 1121; # ERROR_COUNTER_TIMEOUT
            return;
        }
	return 0 unless ($written);
	$done += $written;
    }
    $^E = 0;
    1;
}

# PRINTF this, LIST
#      This method will be triggered every time the tied handle is printed to
#      with the printf() function. Beyond its self reference it also expects
#      the format and list that was passed to the printf function.

sub PRINTF {
    my $self = shift;
    my $fmt = shift;
    return unless ($fmt);
    return unless (@_);
    my $output = sprintf($fmt, @_);
    $self->PRINT($output);
}

# READ this, LIST
#      This method will be called when the handle is read from via the read
#      or sysread functions.

sub READ {
    return if (@_ < 3);
    my $buf = \$_[1];
    my ($self, $junk, $len, $offset) = @_;
    unless (defined $offset) { $offset = 0; }
    my $done = 0;
    my $count_in = 0;
    my $string_in = "";
    my $in2 = "";
    my $bufsize = $self->internal_buffer;

    while ($done < $len) {
	my $size = $len - $done;
        if ($size > $bufsize) { $size = $bufsize; }
	($count_in, $string_in) = $self->read($size);
	if ($count_in) {
            $in2 .= $string_in;
	    $done += $count_in;
	    $^E = 0;
	}
	elsif ($done) {
	    $^E = 0;
	    last;
	}
        else {
	    $^E = 1121; # ERROR_COUNTER_TIMEOUT
	    last;
        }
    }
    my $tail = substr($$buf, $offset + $done);
    my $head = substr($$buf, 0, $offset);
    if ($self->{icrnl}) { $in2 =~ tr/\r/\n/; }
    if ($self->{inlcr}) { $in2 =~ tr/\n/\r/; }
    if ($self->{igncr}) { $in2 =~ s/\r//gos; }
    $$buf = $head.$in2.$tail;
    return $done if ($done);
    return;
}

# READLINE this
#      This method will be called when the handle is read from via <HANDLE>.
#      The method should return undef when there is no more data.

sub READLINE {
    my $self = shift;
    return if (@_);
    my $gotit = "";
    my $match = "";
    my $was;

    if (wantarray) {
	my @lines;
        for (;;) {
            $was = $self->reset_error;
	    if ($was) {
	        $^E = 1117; # ERROR_IO_DEVICE
		return @lines if (@lines);
                return;
	    }
            if (! defined ($gotit = $self->streamline($self->{"_SIZE"}))) {
	        $^E = 1121; # ERROR_COUNTER_TIMEOUT
		return @lines if (@lines);
                return;
            }
	    $match = $self->matchclear;
            if ( ($gotit ne "") || ($match ne "") ) {
	        $^E = 0;
		$gotit .= $match;
                push (@lines, $gotit);
		return @lines if ($gotit =~ /$self->{"_CLASTLINE"}/s);
            }
        }
    }
    else {
        for (;;) {
            $was = $self->reset_error;
	    if ($was) {
	        $^E = 1117; # ERROR_IO_DEVICE
                return;
	    }
            if (! defined ($gotit = $self->lookfor($self->{"_SIZE"}))) {
	        $^E = 1121; # ERROR_COUNTER_TIMEOUT
                return;
            }
	    $match = $self->matchclear;
            if ( ($gotit ne "") || ($match ne "") ) {
	        $^E = 0;
                return $gotit.$match;  # traditional <HANDLE> behavior
            }
        }
    }
}

# GETC this
#      This method will be called when the getc function is called.

sub GETC {
    my $self = shift;
    my ($count, $in) = $self->read(1);
    if ($count == 1) {
	$^E = 0;
        return $in;
    }
    else {
	$^E = 1121; # ERROR_COUNTER_TIMEOUT
        return;
    }
}

# CLOSE this
#      This method will be called when the handle is closed via the close
#      function.

sub CLOSE {
    my $self = shift;
    my $success = $self->close;
    if ($Verbose) { printf "CLOSE result:%d\n", $success; }
    return $success;
}

# DESTROY this
#      As with the other types of ties, this method will be called when the
#      tied handle is about to be destroyed. This is useful for debugging and
#      possibly cleaning up.

sub DESTROY {
    my $self = shift;
    if ($Verbose) { print "SerialPort::DESTROY called.\n"; }
    $self->SUPER::DESTROY();
}

###############

sub alias {
    my $self = shift;
    if (@_) { $self->{ALIAS} = shift; }	# should return true for legal names
    return $self->{ALIAS};
}

sub user_msg {
    my $self = shift;
    if (@_) { $self->{U_MSG} = yes_true ( shift ) }
    return wantarray ? @binary_opt : $self->{U_MSG};
}

sub error_msg {
    my $self = shift;
    if (@_) { $self->{E_MSG} = yes_true ( shift ) }
    return wantarray ? @binary_opt : $self->{E_MSG};
}

sub device { ## readonly for test suite
    my $self = shift;
    return $self->{DEVICE};
}

sub devicetype {
    my $self = shift;
    if (@_) { $self->{DVTYPE} = shift; } # return true for legal names
    return $self->{DVTYPE};
}

sub hostname {
    my $self = shift;
    if (@_) { $self->{HNAME} = shift; }	# return true for legal names
    return $self->{HNAME};
}

sub hostaddr {
    my $self = shift;
    if (@_) { $self->{HADDR} = shift; }	# return true for assigned port
    return $self->{HADDR};
}

sub datatype {
    my $self = shift;
    if (@_) { $self->{DATYPE} = shift; } # return true for legal types
    return $self->{DATYPE};
}

sub cfg_param_1 {
    my $self = shift;
    if (@_) { $self->{CFG_1} = shift; }	# return true for legal param
    return $self->{CFG_1};
}

sub cfg_param_2 {
    my $self = shift;
    if (@_) { $self->{CFG_2} = shift; }	# return true for legal param
    return $self->{CFG_2};
}

sub cfg_param_3 {
    my $self = shift;
    if (@_) { $self->{CFG_3} = shift; }	# return true for legal param
    return $self->{CFG_3};
}

sub baudrate {
    my $self = shift;
    if (@_) {
	unless ( defined $self->is_baudrate( shift ) ) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Could not set baudrate on $self->{ALIAS}";
            }
	    return;
        }
    }
    return wantarray ? $self->are_baudrate : $self->is_baudrate;
}

sub status {
    my $self		= shift;
    my $ok		= 0;
    my $fmask		= 0;
    my $v1		= $Verbose | $self->{"_DEBUG"};
    my $v2		= $v1 | $self->{U_MSG};
    my $v3		= $v1 | $self->{E_MSG};

    my @stat = $self->is_status;
    return unless (scalar @stat);
    $fmask=$stat[ST_BLOCK];
    if ($v1) { printf "BlockingFlags= %lx\n", $fmask; }
    if ($v2 && $fmask) {
        printf "Waiting for CTS\n"		if ($fmask & BM_fCtsHold);
        printf "Waiting for DSR\n"		if ($fmask & BM_fDsrHold);
        printf "Waiting for RLSD\n"		if ($fmask & BM_fRlsdHold);
        printf "Waiting for XON\n"		if ($fmask & BM_fXoffHold);
        printf "Waiting, XOFF was sent\n"	if ($fmask & BM_fXoffSent);
        printf "End_of_File received\n"		if ($fmask & BM_fEof);
        printf "Character waiting to TX\n"	if ($fmask & BM_fTxim);
    }
    $fmask=$stat[ST_ERROR];
    if ($v1) { printf "Error_BitMask= %lx\n", $fmask; }
    if ($v3 && $fmask) {
        # only prints if error is new (API resets each call)
        printf "Invalid MODE or bad HANDLE\n"	if ($fmask & CE_MODE);
        printf "Receive Overrun detected\n"	if ($fmask & CE_RXOVER);
        printf "Buffer Overrun detected\n"	if ($fmask & CE_OVERRUN);
        printf "Parity Error detected\n"	if ($fmask & CE_RXPARITY);
        printf "Framing Error detected\n"	if ($fmask & CE_FRAME);
        printf "Break Signal detected\n"	if ($fmask & CE_BREAK);
        printf "Transmit Buffer is full\n"	if ($fmask & CE_TXFULL);
    }
    return @stat;
}

sub handshake {
    my $self = shift;
    if (@_) {
	unless ( $self->is_handshake(shift) ) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Could not set handshake on $self->{ALIAS}";
            }
	    return;
        }
    }
    return wantarray ? $self->are_handshake : $self->is_handshake;
}

sub parity {
    my $self = shift;
    if (@_) {
	unless ( $self->is_parity(shift) ) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Could not set parity on $self->{ALIAS}";
            }
	    return;
        }
    }
    return wantarray ? $self->are_parity : $self->is_parity;
}

sub databits {
    my $self = shift;
    if (@_) {
	unless ( $self->is_databits(shift) ) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Could not set databits on $self->{ALIAS}";
            }
	    return;
        }
    }
    return wantarray ? $self->are_databits : $self->is_databits;
}

sub stopbits {
    my $self = shift;
    if (@_) {
	unless ( $self->is_stopbits(shift) ) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Could not set stopbits on $self->{ALIAS}";
            }
	    return;
        }
    }
    return wantarray ? $self->are_stopbits : $self->is_stopbits;
}

# single value for save/start
sub set_read_buf {
    my $self = shift;
    if (@_) {
        return unless (@_ == 1);
        my $rbuf = int shift;
        return unless (($rbuf > 0) and ($rbuf <= $self->{MAX_RXB}));
        $self->is_read_buf($rbuf);
    }
    return $self->is_read_buf;
}

# single value for save/start
sub set_write_buf {
    my $self = shift;
    if (@_) {
        return unless (@_ == 1);
        my $wbuf = int shift;
        return unless (($wbuf >= 0) and ($wbuf <= $self->{MAX_TXB}));
        $self->is_write_buf($wbuf);
    }
    return $self->is_write_buf;
}

sub buffers {
    my $self = shift;

    if (@_ == 2) {
        my $rbuf = shift;
        my $wbuf = shift;
	unless (defined set_read_buf ($self, $rbuf)) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Can't set read buffer on $self->{ALIAS}";
            }
	    return;
        }
	unless (defined set_write_buf ($self, $wbuf)) {
            if ($self->{U_MSG} or $Verbose) {
                carp "Can't set write buffer on $self->{ALIAS}";
            }
	    return;
        }
	$self->is_buffers($rbuf, $wbuf) || return;
    }
    elsif (@_) { return; }
    return wantarray ? $self->are_buffers : 1;
}

sub read {
    return unless (@_ == 2);
    my $self = shift;
    my $wanted = shift;
    my $ok     = 0;
    my $result = "";
    return unless ($wanted > 0);

    my $got = $self->read_bg ($wanted);

    if ($got != $wanted) {
        ($ok, $got, $result) = $self->read_done(1);	# block until done
    }
    else { ($ok, $got, $result) = $self->read_done(0); }
    print "read=$got\n" if ($Verbose);
    return ($got, $result);
}

sub lookclear {
    my $self = shift;
    if (nocarp && (@_ == 1)) {
        $self->{"_T_INPUT"} = shift;
    }
    $self->{"_LOOK"}	 = "";
    $self->{"_LASTLOOK"} = "";
    $self->{"_LMATCH"}	 = "";
    $self->{"_LPATT"}	 = "";
    return if (@_);
    1;
}

sub linesize {
    my $self = shift;
    if (@_) {
	my $val = int shift;
	return if ($val < 0);
        $self->{"_SIZE"} = $val;
    }
    return $self->{"_SIZE"};
}

sub lastline {
    my $self = shift;
    if (@_) {
        $self->{"_LASTLINE"} = shift;
	eval '$self->{"_CLASTLINE"} = qr/$self->{"_LASTLINE"}/';
    }
    return $self->{"_LASTLINE"};
}

sub matchclear {
    my $self = shift;
    my $found = $self->{"_LMATCH"};
    $self->{"_LMATCH"}	 = "";
    return if (@_);
    return $found;
}

sub lastlook {
    my $self = shift;
    return if (@_);
    return ( $self->{"_LMATCH"}, $self->{"_LASTLOOK"},
	     $self->{"_LPATT"}, $self->{"_LOOK"} );
}

sub lookfor {
    my $self = shift;
    my $size = 0;
    if (@_) { $size = shift; }
    my $loc = "";
    my $count_in = 0;
    my $string_in = "";
    $self->{"_LMATCH"}	 = "";
    $self->{"_LPATT"}	 = "";

    if ( ! $self->{"_LOOK"} ) {
        $loc = $self->{"_LASTLOOK"};
    }

    if ($size) {
        my ($bbb, $iii, $ooo, $eee) = status($self);
	if ($iii > $size) { $size = $iii; }
	($count_in, $string_in) = $self->read($size);
	return unless ($count_in);
        $loc .= $string_in;
    }
    else {
	$loc .= $self->input;
    }

    if ($loc ne "") {
	if ($self->{icrnl}) { $loc =~ tr/\r/\n/; }
	my $n_char;
	my $mpos;
	my $erase_is_bsdel = 0;
	my $nl_after_kill = "";
	my $clear_after_kill = 0;
	my $echo_ctl = 0;
	my $lookbuf;
	my $re_next = 0;
	my $got_match = 0;
	my $pat;
	my $lf_erase = "";
	my $lf_kill = "";
	my $lf_eof = "";
	my $lf_quit = "";
	my $lf_intr = "";
	my $nl_2_crnl = 0;
	my $cr_2_nl = 0;

	if ($self->{opost}) {
	    $nl_2_crnl = $self->{onlcr};
	    $cr_2_nl = $self->{ocrnl};
	}

	if ($self->{echo}) {
	    $erase_is_bsdel = $self->{echoe};
	    if ($self->{echok}) {
	        $nl_after_kill = $self->{onlcr} ? "\r\n" : "\n";
	    }
	    $clear_after_kill = $self->{echoke};
	    $echo_ctl = $self->{echoctl};
	}

	if ($self->{icanon}) {
	    $lf_erase = $self->{erase};
	    $lf_kill = $self->{s_kill};
	    $lf_eof = $self->{s_eof};
	}

	if ($self->{isig}) {
	    $lf_quit = $self->{quit};
	    $lf_intr = $self->{intr};
	}

	my @loc_char = split (//, $loc);
	while (defined ($n_char = shift @loc_char)) {
##	    printf STDERR "0x%x ", ord($n_char);
	    if ($n_char eq $lf_erase) {
	        if ($erase_is_bsdel && (length $self->{"_LOOK"}) ) {
		    $mpos = chop $self->{"_LOOK"};
	            $self->write($self->{bsdel});
	            if ($echo_ctl && (($mpos lt "@")|($mpos eq chr(127)))) {
	                $self->write($self->{bsdel});
		    }
		}
	    }
	    elsif ($n_char eq $lf_kill) {
		$self->{"_LOOK"} = "";
	        $self->write($self->{clear}) if ($clear_after_kill);
	        $self->write($nl_after_kill);
	        $self->write($self->{"_PROMPT"});
	    }
	    elsif ($n_char eq $lf_intr) {
		$self->{"_LOOK"}     = "";
		$self->{"_LASTLOOK"} = "";
		return;
	    }
	    elsif ($n_char eq $lf_quit) {
		exit;
	    }
	    else {
		$mpos = ord $n_char;
		if ($self->{istrip}) {
		    if ($mpos > 127) { $n_char = chr($mpos - 128); }
		}
                $self->{"_LOOK"} .= $n_char;
##	        print $n_char;
	        if ($cr_2_nl) { $n_char =~ s/\r/\n/os; }
	        if ($nl_2_crnl) { $n_char =~ s/\n/\r\n/os; }
	        if (($mpos < 32)  && $echo_ctl &&
			($mpos != is_stty_eol($self))) {
		    $n_char = chr($mpos + 64);
	            $self->write("^$n_char");
		}
		elsif (($mpos == 127) && $echo_ctl) {
	            $self->write("^.");
		}
		elsif ($self->{echonl} && ($n_char =~ "\n")) {
		    # also writes "\r\n" for onlcr
	            $self->write($n_char);
		}
		elsif ($self->{echo}) {
		    # also writes "\r\n" for onlcr
	            $self->write($n_char);
		}
		$lookbuf = $self->{"_LOOK"};
		if (($lf_eof ne "") and ($lookbuf =~ /$lf_eof$/)) {
		    $self->{"_LOOK"}     = "";
		    $self->{"_LASTLOOK"} = "";
		    return $lookbuf;
		}
		$count_in = 0;
		foreach $pat ( @{ $self->{"_CMATCH"} } ) {
		    if ($pat eq "-re") {
			$re_next++;
		        $count_in++;
			next;
		    }
		    if ($re_next) {
			$re_next = 0;
			# always at $lookbuf end when processing single char
		        if ( $lookbuf =~ s/$pat//s ) {
		            $self->{"_LMATCH"} = $&;
			    $got_match++;
			}
		    }
		    elsif (($mpos = index($lookbuf, $pat)) > -1) {
			$got_match++;
			$lookbuf = substr ($lookbuf, 0, $mpos);
		        $self->{"_LMATCH"} = $pat;
		    }
		    if ($got_match) {
		        $self->{"_LPATT"} = $self->{"_MATCH"}[$count_in];
		        if (scalar @loc_char) {
		            $self->{"_LASTLOOK"} = join("", @loc_char);
##		            print ".$self->{\"_LASTLOOK\"}.";
                        }
		        else {
		            $self->{"_LASTLOOK"} = "";
		        }
		        $self->{"_LOOK"}     = "";
		        return $lookbuf;
                    }
		    $count_in++;
		}
	    }
	}
    }
    return "";
}

sub streamline {
    my $self = shift;
    my $size = 0;
    if (@_) { $size = shift; }
    my $loc = "";
    my $mpos;
    my $count_in = 0;
    my $string_in = "";
    my $re_next = 0;
    my $got_match = 0;
    my $best_pos = 0;
    my $pat;
    my $match = "";
    my $before = "";
    my $after = "";
    my $best_match = "";
    my $best_before = "";
    my $best_after = "";
    my $best_pat = "";
    $self->{"_LMATCH"}	 = "";
    $self->{"_LPATT"}	 = "";

    if ( ! $self->{"_LOOK"} ) {
        $loc = $self->{"_LASTLOOK"};
    }

    if ($size) {
        my ($bbb, $iii, $ooo, $eee) = status($self);
	if ($iii > $size) { $size = $iii; }
	($count_in, $string_in) = $self->read($size);
	return unless ($count_in);
        $loc .= $string_in;
    }
    else {
	$loc .= $self->input;
    }

    if ($loc ne "") {
        $self->{"_LOOK"} .= $loc;
	$count_in = 0;
	foreach $pat ( @{ $self->{"_CMATCH"} } ) {
	    if ($pat eq "-re") {
		$re_next++;
		$count_in++;
		next;
	    }
	    if ($re_next) {
		$re_next = 0;
	        if ( $self->{"_LOOK"} =~ /$pat/s ) {
		    ( $match, $before, $after ) = ( $&, $`, $' );
		    $got_match++;
        	    $mpos = length($before);
        	    if ($mpos) {
        	        next if ($best_pos && ($mpos > $best_pos));
			$best_pos = $mpos;
			$best_pat = $self->{"_MATCH"}[$count_in];
			$best_match = $match;
			$best_before = $before;
			$best_after = $after;
	    	    } else {
		        $self->{"_LPATT"} = $self->{"_MATCH"}[$count_in];
		        $self->{"_LMATCH"} = $match;
	                $self->{"_LASTLOOK"} = $after;
		        $self->{"_LOOK"}     = "";
		        return $before;
		        # pattern at start will be best
		    }
		}
	    }
	    elsif (($mpos = index($self->{"_LOOK"}, $pat)) > -1) {
		$got_match++;
		$before = substr ($self->{"_LOOK"}, 0, $mpos);
        	if ($mpos) {
        	    next if ($best_pos && ($mpos > $best_pos));
		    $best_pos = $mpos;
		    $best_pat = $pat;
		    $best_match = $pat;
		    $best_before = $before;
		    $mpos += length($pat);
		    $best_after = substr ($self->{"_LOOK"}, $mpos);
	    	} else {
	            $self->{"_LPATT"} = $pat;
		    $self->{"_LMATCH"} = $pat;
		    $before = substr ($self->{"_LOOK"}, 0, $mpos);
		    $mpos += length($pat);
	            $self->{"_LASTLOOK"} = substr ($self->{"_LOOK"}, $mpos);
		    $self->{"_LOOK"}     = "";
		    return $before;
		    # match at start will be best
		}
	    }
	    $count_in++;
	}
	if ($got_match) {
	    $self->{"_LPATT"} = $best_pat;
	    $self->{"_LMATCH"} = $best_match;
            $self->{"_LASTLOOK"} = $best_after;
	    $self->{"_LOOK"}     = "";
	    return $best_before;
        }
    }
    return "";
}

sub input {
    return unless (@_ == 1);
    my $self = shift;
    my $result = "";
    # some USB ports can generate config data (that we want to ignore)
    if (nocarp) {	# only relevant for test mode
	$result .= $self->{"_T_INPUT"};
	$self->{"_T_INPUT"} = "";
	return $result;
    }
    my $ok     = 0;
    my $got_p = " "x4;
    my ($bbb, $wanted, $ooo, $eee) = status($self);
    return "" if ($eee);
    return "" unless $wanted;

    my $got = $self->read_bg ($wanted);

    if ($got != $wanted) {
        	# block if unexpected happens
        ($ok, $got, $result) = $self->read_done(1);	# block until done
    }
    else { ($ok, $got, $result) = $self->read_done(0); }
###    print "input: got= $got   result=$result\n";
    return $got ? $result : "";
}

sub write {
    return unless (@_ == 2);
    my $self = shift;
    my $wbuf = shift;
    my $ok = 1;

    return 0 if ($wbuf eq "");
    my $lbuf = length ($wbuf);

    my $written = $self->write_bg ($wbuf);

    if ($written != $lbuf) {
        ($ok, $written) = $self->write_done(1);	# block until done
    }
    if ($Verbose) {
	print "wbuf=$wbuf\n";
	print "lbuf=$lbuf\n";
	print "written=$written\n";
    }
    return unless ($ok);
    return $written;
}

sub transmit_char {
    my $self = shift;
    return unless (@_ == 1);
    my $v = int shift;
    return if (($v < 0) or ($v > 255));
    return unless $self->xmit_imm_char ($v);
    return wantarray ? @byte_opt : 1;
}

sub xon_char {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > 255));
        $self->is_xon_char($v);
    }
    return wantarray ? @byte_opt : $self->is_xon_char;
}

sub xoff_char {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > 255));
        $self->is_xoff_char($v);
    }
    return wantarray ? @byte_opt : $self->is_xoff_char;
}

sub eof_char {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > 255));
        $self->is_eof_char($v);
    }
    return wantarray ? @byte_opt : $self->is_eof_char;
}

sub event_char {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > 255));
        $self->is_event_char($v);
    }
    return wantarray ? @byte_opt : $self->is_event_char;
}

sub error_char {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > 255));
        $self->is_error_char($v);
    }
    return wantarray ? @byte_opt : $self->is_error_char;
}

sub xon_limit {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > SHORTsize));
        $self->is_xon_limit($v);
    }
    return wantarray ? (0, SHORTsize) : $self->is_xon_limit;
}

sub xoff_limit {
    my $self = shift;
    if (@_ == 1) {
	my $v = int shift;
	return if (($v < 0) or ($v > SHORTsize));
        $self->is_xoff_limit($v);
    }
    return wantarray ? (0, SHORTsize) : $self->is_xoff_limit;
}

sub read_interval {
    my $self = shift;
    if (@_) {
	return unless defined $self->is_read_interval( shift );
    }
    return wantarray ? (0, LONGsize) : $self->is_read_interval;
}

sub read_char_time {
    my $self = shift;
    if (@_) {
	return unless defined $self->is_read_char_time( shift );
    }
    return wantarray ? (0, LONGsize) : $self->is_read_char_time;
}

sub read_const_time {
    my $self = shift;
    if (@_) {
	return unless defined $self->is_read_const_time( shift );
    }
    return wantarray ? (0, LONGsize) : $self->is_read_const_time;
}

sub write_const_time {
    my $self = shift;
    if (@_) {
	return unless defined $self->is_write_const_time( shift );
    }
    return wantarray ? (0, LONGsize) : $self->is_write_const_time;
}

sub write_char_time {
    my $self = shift;
    if (@_) {
	return unless defined $self->is_write_char_time( shift );
    }
    return wantarray ? (0, LONGsize) : $self->is_write_char_time;
}


  # true/false parameters

sub binary {
    my $self = shift;
    if (@_) {
	return unless defined $self->is_binary( shift );
    }
    return $self->is_binary;
}

sub parity_enable {
    my $self = shift;
    if (@_) {
        if ( $self->can_parity_enable ) {
            $self->is_parity_enable( shift );
        }
        elsif ($self->{U_MSG}) {
            carp "Can't set parity enable on $self->{ALIAS}";
        }
    }
    return $self->is_parity_enable;
}

sub modemlines {
    return unless (@_ == 1);
    my $self = shift;
    my $result = $self->is_modemlines;
    if ($Verbose) {
        print "CTS is ON\n"		if ($result & MS_CTS_ON);
        print "DSR is ON\n"		if ($result & MS_DSR_ON);
        print "RING is ON\n"		if ($result & MS_RING_ON);
        print "RLSD is ON\n"		if ($result & MS_RLSD_ON);
    }
    return $result;
}

sub stty {
    my $ob = shift;
    my $token;
    if (@_) {
	my $ok = 1;
        no strict 'refs'; # for $gosub
        while ($token = shift) {
            if (exists $opts{$token}) {
                ## print "    $opts{$token}\n";
                my ($gosub, $value) = split (':', $opts{$token});
                if ($value eq "argv_char") { $value = &argv_char(shift); }
		if (defined $value) {
                    &$gosub($ob, $value);
		} else {
                    nocarp or carp "bad value for parameter $token\n";
		    $ok = 0;
		}
            }
            else {
                nocarp or carp "parameter $token not found\n";
		$ok = 0;
            }
        }
        use strict 'refs';
	return $ok;
    }
    else {
	my @settings; # array returned by ()
        my $current = $ob->baudrate;
        push @settings, "$current";

        push @settings, "intr";
        push @settings, cntl_char($ob->stty_intr);
        push @settings, "quit";
        push @settings, cntl_char($ob->stty_quit);
        push @settings, "erase";
        push @settings, cntl_char($ob->stty_erase);
        push @settings, "kill";
        push @settings, cntl_char($ob->stty_kill);
        push @settings, "eof";
        push @settings, cntl_char($ob->stty_eof);
        push @settings, "eol";
        push @settings, cntl_char($ob->stty_eol);
        push @settings, "start";
        push @settings, cntl_char(chr $ob->xon_char);
        push @settings, "stop";
        push @settings, cntl_char(chr $ob->xoff_char);
	# "stop" is last CHAR type

        push @settings, ($ob->stty_echo ? "" : "-")."echo";
        push @settings, ($ob->stty_echoe ? "" : "-")."echoe";
        push @settings, ($ob->stty_echok ? "" : "-")."echok";
        push @settings, ($ob->stty_echonl ? "" : "-")."echonl";
        push @settings, ($ob->stty_echoke ? "" : "-")."echoke";
        push @settings, ($ob->stty_echoctl ? "" : "-")."echoctl";
        push @settings, ($ob->stty_istrip ? "" : "-")."istrip";
        push @settings, ($ob->stty_icrnl ? "" : "-")."icrnl";
        push @settings, ($ob->stty_ocrnl ? "" : "-")."ocrnl";
        push @settings, ($ob->stty_igncr ? "" : "-")."igncr";
        push @settings, ($ob->stty_inlcr ? "" : "-")."inlcr";
        push @settings, ($ob->stty_onlcr ? "" : "-")."onlcr";
        push @settings, ($ob->stty_opost ? "" : "-")."opost";
        push @settings, ($ob->stty_isig ? "" : "-")."isig";
        push @settings, ($ob->stty_icanon ? "" : "-")."icanon";

        $current = $ob->databits;
        push @settings, "cs$current";
        push @settings, (($ob->stopbits == 2) ? "" : "-")."cstopb";

        $current = $ob->handshake;
        push @settings, (($current eq "dtr") ? "" : "-")."clocal";
        push @settings, (($current eq "rts") ? "" : "-")."crtscts";
        push @settings, (($current eq "xoff") ? "" : "-")."ixoff";
        push @settings, (($current eq "xoff") ? "" : "-")."ixon";

        my $parity = $ob->parity;
        if    ($parity eq "none")  {
            push @settings, "-parenb";
            push @settings, "-parodd";
            push @settings, "-inpck";
        }
        else {
    	    $current = $ob->is_parity_enable;
            push @settings, ($current ? "" : "-")."parenb";
            push @settings, (($parity eq "odd") ? "" : "-")."parodd";
            push @settings, ($current ? "" : "-")."inpck";
            # mark and space not supported
        }
        return @settings;
    }
}

sub cntl_char {
    my $n_char = shift;
    return "<undef>" unless (defined $n_char);
    my $pos = ord $n_char;
    if ($pos < 32) {
        $n_char = "^".chr($pos + 64);
    }
    if ($pos > 126) {
        $n_char = sprintf "0x%x", $pos;
    }
    return $n_char;
}

sub argv_char {
    my $n_char = shift;
    return unless (defined $n_char);
    my $pos = $n_char;
    if ($n_char =~ s/^\^//) {
        $pos = ord($n_char) - 64;
    }
    elsif ($n_char =~ s/^0x//) {
        $pos = hex($n_char);
    }
    elsif ($n_char =~ /^0/) {
        $pos = oct($n_char);
    }
    ## print "pos = $pos\n";
    return $pos;
}

sub debug {
    my $self = shift || '';	# call be called as sub without object
    return @binary_opt if (wantarray);
    if (ref($self))  {
        if (@_) {
	    $self->{"_DEBUG"} = yes_true ( shift );
	    $self->debug_comm($self->{"_DEBUG"});
	}
        else {
	    my $tmp = $self->{"_DEBUG"};
            nocarp || carp "Debug level: $self->{ALIAS} = $tmp";
	    $self->debug_comm($tmp);
        }
        return $self->{"_DEBUG"};
    } else {
	if ($self =~ m/Port/io) {
	    # cover the case when someone uses the pseudohash calling style
	    # $obj->debug() on an unblessed $obj (old test cases do that)
	    $self = shift || '';
	}
	if ($self ne '') {
        	$Verbose = yes_true ($self);
	}
        nocarp || carp "SerialPort Debug Class = $Verbose";
	Win32API::CommPort::debug_comm($Verbose);
        return $Verbose;
    }
}

sub close {
    my $self = shift;

    return unless (defined $self->{ALIAS});

    if ($Verbose or $self->{"_DEBUG"}) {
        carp "Closing $self " . $self->{ALIAS};
    }
    my $success = $self->SUPER::close;
    $self->{DEVICE} = undef;
    $self->{ALIAS} = undef;
    if ($Verbose) {
        printf "SerialPort close result:%d\n", $success;
    }
    return $success;
}

1;

__END__

=pod

=head1 NAME

Win32::SerialPort - User interface to Win32 Serial API calls

=head1 SYNOPSIS

  require 5.003;
  use Win32::SerialPort qw( :STAT 0.19 );

=head2 Constructors

  $PortObj = new Win32::SerialPort ($PortName, $quiet)
       || die "Can't open $PortName: $^E\n";    # $quiet is optional

  $PortObj = start Win32::SerialPort ($Configuration_File_Name)
       || die "Can't start $Configuration_File_Name: $^E\n";

  $PortObj = tie (*FH, 'Win32::SerialPort', $Configuration_File_Name)
       || die "Can't tie using $Configuration_File_Name: $^E\n";


=head2 Configuration Utility Methods

  $PortObj->alias("MODEM1");

     # before using start, restart, or tie
  $PortObj->save($Configuration_File_Name)
       || warn "Can't save $Configuration_File_Name: $^E\n";

     # after new, must check for failure
  $PortObj->write_settings || undef $PortObj;
  print "Can't change Device_Control_Block: $^E\n" unless ($PortObj);

     # rereads file to either return open port to a known state
     # or switch to a different configuration on the same port
  $PortObj->restart($Configuration_File_Name)
       || warn "Can't reread $Configuration_File_Name: $^E\n";

     # "app. variables" saved in $Configuration_File, not used internally
  $PortObj->devicetype('none');     # CM11, CM17, 'weeder', 'modem'
  $PortObj->hostname('localhost');  # for socket-based implementations
  $PortObj->hostaddr(0);            # false unless specified
  $PortObj->datatype('raw');        # in case an application needs_to_know
  $PortObj->cfg_param_1('none');    # null string '' hard to save/restore
  $PortObj->cfg_param_2('none');    # 3 spares should be enough for now
  $PortObj->cfg_param_3('none');    # one may end up as a log file path

     # specials for test suite only
  @necessary_param = Win32::SerialPort->set_test_mode_active(1);
  $PortObj->lookclear("loopback to next 'input' method");
  $name = $PortObj->device();        # readonly for test suite

=head2 Configuration Parameter Methods

     # most methods can be called three ways:
  $PortObj->handshake("xoff");           # set parameter
  $flowcontrol = $PortObj->handshake;    # current value (scalar)
  @handshake_opts = $PortObj->handshake; # permitted choices (list)

     # similar
  $PortObj->baudrate(9600);
  $PortObj->parity("odd");
  $PortObj->databits(8);
  $PortObj->stopbits(1);

     # range parameters return (minimum, maximum) in list context
  $PortObj->xon_limit(100);      # bytes left in buffer
  $PortObj->xoff_limit(100);     # space left in buffer
  $PortObj->xon_char(0x11);
  $PortObj->xoff_char(0x13);
  $PortObj->eof_char(0x0);
  $PortObj->event_char(0x0);
  $PortObj->error_char(0);       # for parity errors

  $PortObj->buffers(4096, 4096);  # read, write
	# returns current in list context

  $PortObj->read_interval(100);    # max time between read char (milliseconds)
  $PortObj->read_char_time(5);     # avg time between read char
  $PortObj->read_const_time(100);  # total = (avg * bytes) + const
  $PortObj->write_char_time(5);
  $PortObj->write_const_time(100);

     # true/false parameters (return scalar context only)

  $PortObj->binary(T);		# just say Yes (Win 3.x option)
  $PortObj->parity_enable(F);	# faults during input
  $PortObj->debug(0);

=head2 Operating Methods

  ($BlockingFlags, $InBytes, $OutBytes, $LatchErrorFlags) = $PortObj->status
	|| warn "could not get port status\n";

  if ($BlockingFlags) { warn "Port is blocked"; }
  if ($BlockingFlags & BM_fCtsHold) { warn "Waiting for CTS"; }
  if ($LatchErrorFlags & CE_FRAME) { warn "Framing Error"; }
        # The API resets errors when reading status, $LatchErrorFlags
	# is all $ErrorFlags seen since the last reset_error

Additional useful constants may be exported eventually. If the only fault
action desired is a message, B<status> provides I<Built-In> BitMask processing:

  $PortObj->error_msg(1);  # prints hardware messages like "Framing Error"
  $PortObj->user_msg(1);   # prints function messages like "Waiting for CTS"

  ($count_in, $string_in) = $PortObj->read($InBytes);
  warn "read unsuccessful\n" unless ($count_in == $InBytes);

  $count_out = $PortObj->write($output_string);
  warn "write failed\n"		unless ($count_out);
  warn "write incomplete\n"	if ( $count_out != length($output_string) );

  if ($string_in = $PortObj->input) { PortObj->write($string_in); }
     # simple echo with no control character processing

  $PortObj->transmit_char(0x03);	# bypass buffer (and suspend)

  $ModemStatus = $PortObj->modemlines;
  if ($ModemStatus & $PortObj->MS_RLSD_ON) { print "carrier detected"; }

=head2 Methods used with Tied FileHandles

  $PortObj = tie (*FH, 'Win32::SerialPort', $Configuration_File_Name)
       || die "Can't tie: $^E\n";            ## TIEHANDLE ##

  print FH "text";                           ## PRINT     ##
  $char = getc FH;                           ## GETC      ##
  syswrite FH, $out, length($out), 0;        ## WRITE     ##
  $line = <FH>;                              ## READLINE  ##
  @lines = <FH>;                             ## READLINE  ##
  printf FH "received: %s", $line;           ## PRINTF    ##
  read (FH, $in, 5, 0) or die "$^E";         ## READ      ##
  sysread (FH, $in, 5, 0) or die "$^E";      ## READ      ##
  close FH || warn "close failed";           ## CLOSE     ##
  undef $PortObj;
  untie *FH;                                 ## DESTROY   ##

  $PortObj->linesize(10);		# with READLINE
  $PortObj->lastline("_GOT_ME_");	# with READLINE, list only

  $old_ors = $PortObj->output_record_separator("RECORD");	# with PRINT
  $old_ofs = $PortObj->output_field_separator("COMMA");		# with PRINT

=head2 Destructors

  $PortObj->close || warn "close failed";
      # passed to CommPort to release port to OS - needed to reopen
      # close will not usually DESTROY the object
      # also called as: close FH || warn "close failed";


  undef $PortObj;
      # preferred unless reopen expected since it triggers DESTROY
      # calls $PortObj->close but does not confirm success
      # MUST precede untie - do all three IN THIS SEQUENCE before re-tie.

  untie *FH;

=head2 Methods for I/O Processing

  $PortObj->are_match("text", "\n");	# possible end strings
  $PortObj->lookclear;			# empty buffers
  $PortObj->write("Feed Me:");		# initial prompt
  $PortObj->is_prompt("More Food:");	# new prompt after "kill" char

  my $gotit = "";
  my $match1 = "";
  until ("" ne $gotit) {
      $gotit = $PortObj->lookfor;	# poll until data ready
      die "Aborted without match\n" unless (defined $gotit);
      last if ($gotit);
      $match1 = $PortObj->matchclear;   # match is first thing received
      last if ($match1);
      sleep 1;				# polling sample time
  }

  printf "gotit = %s\n", $gotit;		# input BEFORE the match
  my ($match, $after, $pattern, $instead) = $PortObj->lastlook;
      # input that MATCHED, input AFTER the match, PATTERN that matched
      # input received INSTEAD when timeout without match

  if ($match1) {
      $match = $match1;
  }
  printf "lastlook-match = %s  -after = %s  -pattern = %s\n",
                           $match,      $after,        $pattern;

  $gotit = $PortObj->lookfor($count);	# block until $count chars received

  $PortObj->are_match("-re", "pattern", "text");
      # possible match strings: "pattern" is a regular expression,
      #                         "text" is a literal string

  $gotit = $PortObj->streamline;	# poll until data ready
  $gotit = $PortObj->streamline($count);# block until $count chars received
      # fast alternatives to lookfor with no character processing

  $PortObj->stty_intr("\cC");	# char to abort lookfor method
  $PortObj->stty_quit("\cD");	# char to abort perl
  $PortObj->stty_eof("\cZ");	# end_of_file char
  $PortObj->stty_eol("\cJ");	# end_of_line char
  $PortObj->stty_erase("\cH");	# delete one character from buffer (backspace)
  $PortObj->stty_kill("\cU");	# clear line buffer

  $PortObj->is_stty_intr(3);	# ord(char) to abort lookfor method
  $qc = $PortObj->is_stty_quit;	# ($qc == 4) for "\cD"
  $PortObj->is_stty_eof(26);
  $PortObj->is_stty_eol(10);
  $PortObj->is_stty_erase(8);
  $PortObj->is_stty_kill(21);

  my $air = " "x76;
  $PortObj->stty_clear("\r$air\r");	# written after kill character
  $PortObj->is_stty_clear;		# internal version for config file
  $PortObj->stty_bsdel("\cH \cH");	# written after erase character

  $PortObj->stty_echo(0);	# echo every character
  $PortObj->stty_echoe(1);	# if echo erase character with bsdel string
  $PortObj->stty_echok(1);	# if echo \n after kill character
  $PortObj->stty_echonl(0);	# if echo \n
  $PortObj->stty_echoke(1);	# if echo clear string after kill character
  $PortObj->stty_echoctl(0);	# if echo "^Char" for control chars
  $PortObj->stty_istrip(0);	# strip input to 7-bits
  $PortObj->stty_icrnl(0);	# map \r to \n on input
  $PortObj->stty_ocrnl(0);	# map \r to \n on output
  $PortObj->stty_igncr(0);	# ignore \r on input
  $PortObj->stty_inlcr(0);	# map \n to \r on input
  $PortObj->stty_onlcr(1);	# map \n to \r\n on output
  $PortObj->stty_opost(0);	# enable output mapping
  $PortObj->stty_isig(0);	# enable quit and intr characters
  $PortObj->stty_icanon(0);	# enable erase and kill characters

  $PortObj->stty("-icanon");	# disable eof, erase and kill char, Unix-style
  @stty_all = $PortObj->stty();	# get all the parameters, Perl-style

=head2 Capability Methods inherited from Win32API::CommPort

These return scalar context only.

  can_baud            can_databits           can_stopbits
  can_dtrdsr          can_handshake          can_parity_check
  can_parity_config   can_parity_enable      can_rlsd
  can_16bitmode       is_rs232               is_modem
  can_rtscts          can_xonxoff            can_xon_char
  can_spec_char       can_interval_timeout   can_total_timeout
  buffer_max          can_rlsd_config        can_ioctl

=head2 Operating Methods inherited from Win32API::CommPort

  write_bg            write_done             read_bg
  read_done           reset_error            suspend_tx
  resume_tx           dtr_active             rts_active
  break_active        xoff_active            xon_active
  purge_all           purge_rx               purge_tx
  pulse_rts_on        pulse_rts_off          pulse_dtr_on
  pulse_dtr_off       ignore_null            ignore_no_dsr
  subst_pe_char       abort_on_error         output_xoff
  output_dsr          output_cts             tx_on_xoff
  input_xoff          get_tick_count


=head1 DESCRIPTION


This module uses Win32API::CommPort for raw access to the API calls and
related constants.  It provides an object-based user interface to allow
higher-level use of common API call sequences for dealing with serial
ports.

Uses features of the Win32 API to implement non-blocking I/O, serial
parameter setting, event-loop operation, and enhanced error handling.

To pass in C<NULL> as the pointer to an optional buffer, pass in C<$null=0>.
This is expected to change to an empty list reference, C<[]>, when Perl
supports that form in this usage.

=head2 Initialization

The primary constructor is B<new> with a F<PortName> (as the Registry
knows it) specified. This will create an object, and get the available
options and capabilities via the Win32 API. The object is a superset
of a B<Win32API::CommPort> object, and supports all of its methods.
The port is not yet ready for read/write access. First, the desired
I<parameter settings> must be established. Since these are tuning
constants for an underlying hardware driver in the Operating System,
they are all checked for validity by the methods that set them. The
B<write_settings> method writes a new I<Device Control Block> to the
driver. The B<write_settings> method will return true if the port is
ready for access or C<undef> on failure. Ports are opened for binary
transfers. A separate C<binmode> is not needed. The USER must release
the object if B<write_settings> does not succeed.

Version 0.15 adds an optional C<$quiet> parameter to B<new>. Failure
to open a port prints a error message to STDOUT by default. Since only
one application at a time can "own" the port, one source of failure was
"port in use". There was previously no way to check this without getting
a "fail message". Setting C<$quiet> disables this built-in message. It
also returns 0 instead of C<undef> if the port is unavailable (still FALSE,
used for testing this condition - other faults may still return C<undef>).
Use of C<$quiet> only applies to B<new>.

=over 8

Certain parameters I<MUST> be set before executing B<write_settings>.
Others will attempt to deduce defaults from the hardware or from other
parameters. The I<Required> parameters are:

=item baudrate

Any legal value.

=item parity

One of the following: "none", "odd", "even", "mark", "space".
If you select anything except "none", you will need to set B<parity_enable>.

=item databits

An integer from 5 to 8.

=item stopbits

Legal values are 1, 1.5, and 2. But 1.5 only works with 5 databits, 2 does
not work with 5 databits, and other combinations may not work on all
hardware if parity is also used.

=back

The B<handshake> setting is recommended but no longer required. Select one
of the following: "none", "rts", "xoff", "dtr".

Some individual parameters (eg. baudrate) can be changed after the
initialization is completed. These will be validated and will
update the I<Device Control Block> as required. The B<save>
method will write the current parameters to a file that B<start, tie,> and
B<restart> can use to reestablish a functional setup.

  $PortObj = new Win32::SerialPort ($PortName, $quiet)
       || die "Can't open $PortName: $^E\n";    # $quiet is optional

  $PortObj->user_msg(ON);
  $PortObj->databits(8);
  $PortObj->baudrate(9600);
  $PortObj->parity("none");
  $PortObj->stopbits(1);
  $PortObj->handshake("rts");
  $PortObj->buffers(4096, 4096);

  $PortObj->write_settings || undef $PortObj;

  $PortObj->save($Configuration_File_Name);

  $PortObj->baudrate(300);
  $PortObj->restart($Configuration_File_Name);	# back to 9600 baud

  $PortObj->close || die "failed to close";
  undef $PortObj;				# frees memory back to perl

The F<PortName> maps to both the Registry I<Device Name> and the
I<Properties> associated with that device. A single I<Physical> port
can be accessed using two or more I<Device Names>. But the options
and setup data will differ significantly in the two cases. A typical
example is a Modem on port "COM2". Both of these F<PortNames> open
the same I<Physical> hardware:

  $P1 = new Win32::SerialPort ("COM2");

  $P2 = new Win32::SerialPort ("\\\\.\\Nanohertz Modem model K-9");

$P1 is a "generic" serial port. $P2 includes all of $P1 plus a variety
of modem-specific added options and features. The "raw" API calls return
different size configuration structures in the two cases. Win32 uses the
"\\.\" prefix to identify "named" devices. Since both names use the same
I<Physical> hardware, they can not both be used at the same time. The OS
will complain. Consider this A Good Thing. Use B<alias> to convert the
name used by "built-in" messages.

  $P2->alias("FIDO");

Beginning with version 0.20, the prefix is added automatically to device
names that match the regular expression "^COM\d+$" so that COM10, COM11,
etc. do not require separate handling. A corresponding alias is created.
Hence, for the first constructor above:

  $alias = $P1->alias;    # $alias = "COM1"
  $device = $P1->device:  # $device = "\\.\COM1" 

The second constructor, B<start> is intended to simplify scripts which
need a constant setup. It executes all the steps from B<new> to
B<write_settings> based on a previously saved configuration. This
constructor will return C<undef> on a bad configuration file or failure
of a validity check. The returned object is ready for access.

  $PortObj2 = start Win32::SerialPort ($Configuration_File_Name)
       || die;

The third constructor, B<tie>, combines the B<start> with Perl's
support for tied FileHandles (see I<perltie>). Win32::SerialPort
implements the complete set of methods: TIEHANDLE, PRINT, PRINTF,
WRITE, READ, GETC, READLINE, CLOSE, and DESTROY. Tied FileHandle
support was new with Version 0.14.

  $PortObj2 = tie (*FH, 'Win32::SerialPort', $Configuration_File_Name)
       || die;

The implementation attempts to mimic STDIN/STDOUT behaviour as closely
as possible: calls block until done, data strings that exceed internal
buffers are divided transparently into multiple calls, and B<stty_onlcr>
and B<stty_ocrnl> are applied to output data (WRITE, PRINT, PRINTF) when
B<stty_opost> is true. In Version 0.17, the output separators C<$,> and
C<$\> are also applied to PRINT if set. Since PRINTF is treated internally
as a single record PRINT, C<$\> will be applied. Output separators are not
applied to WRITE (called as C<syswrite FH, $scalar, $length, [$offset]>).

The B<output_record_separator> and B<output_field_separator> methods can set
I<Port-FileHandle-Specific> versions of C<$,> and C<$\> if desired.
The input_record_separator C<$/> is not explicitly supported - but an
identical function can be obtained with a suitable B<are_match> setting.
Record separators are experimental in Version 0.17. They are not saved
in the configuration_file.

The tied FileHandle methods may be combined with the Win32::SerialPort
methods for B<read, input>, and B<write> as well as other methods. The
typical restrictions against mixing B<print> with B<syswrite> do not
apply. Since both B<(tied) read> and B<sysread> call the same C<$ob-E<gt>READ>
method, and since a separate C<$ob-E<gt>read> method has existed for some
time in Win32::SerialPort, you should always use B<sysread> with the
tied interface. Beginning in Version 0.17, B<sysread> checks the input
against B<stty_icrnl>, B<stty_inlcr>, and B<stty_igncr>. With B<stty_igncr>
active, the B<sysread> returns the count of all characters received including
and C<\r> characters subsequently deleted.

Because all the tied methods block, they should ALWAYS be used with
timeout settings and are not suitable for background operations and
polled loops. The B<sysread> method may return fewer characters than
requested when a timeout occurs. The method call is still considered
successful. If a B<sysread> times out after receiving some characters,
the actual elapsed time may be as much as twice the programmed limit.
If no bytes are received, the normal timing applies.

=head2 Configuration and Capability Methods

Starting in Version 0.18, a number of I<Application Variables> are saved
in B<$Configuration_File>. These parameters are not used internally. But
methods allow setting and reading them. The intent is to facilitate the
use of separate I<configuration scripts> to create the files. Then an
application can use B<start> as the Constructor and not bother with
command line processing or managing its own small configuration file.
The default values and number of parameters is subject to change.

  $PortObj->devicetype('none');
  $PortObj->hostname('localhost');  # for socket-based implementations
  $PortObj->hostaddr(0);            # a "false" value
  $PortObj->datatype('raw');        # 'record' is another possibility
  $PortObj->cfg_param_1('none');
  $PortObj->cfg_param_2('none');    # 3 spares should be enough for now
  $PortObj->cfg_param_3('none');

The Win32 Serial Comm API provides extensive information concerning
the capabilities and options available for a specific port (and
instance). "Modem" ports have different capabilties than "RS-232"
ports - even if they share the same Hardware. Many traditional modem
actions are handled via TAPI. "Fax" ports have another set of options -
and are accessed via MAPI. Yet many of the same low-level API commands
and data structures are "common" to each type ("Modem" is implemented
as an "RS-232" superset). In addition, Win95 supports a variety of
legacy hardware (e.g fixed 134.5 baud) while WinNT has hooks for ISDN,
16-data-bit paths, and 256Kbaud.

=over 8

Binary selections will accept as I<true> any of the following:
C<("YES", "Y", "ON", "TRUE", "T", "1", 1)> (upper/lower/mixed case)
Anything else is I<false>.

There are a large number of possible configuration and option parameters.
To facilitate checking option validity in scripts, most configuration
methods can be used in three different ways:

=item method called with an argument

The parameter is set to the argument, if valid. An invalid argument
returns I<false> (undef) and the parameter is unchanged. The function
will also I<carp> if B<$user_msg> is I<true>. After B<write_settings>,
the port will be updated immediately if allowed. Otherwise, the value
will be applied when B<write_settings> is called.

=item method called with no argument in scalar context

The current value is returned. If the value is not initialized either
directly or by default, return "undef" which will parse to I<false>.
For binary selections (true/false), return the current value. All
current values from "multivalue" selections will parse to I<true>.
Current values may differ from requested values until B<write_settings>.
There is no way to see requests which have not yet been applied.
Setting the same parameter again overwrites the first request. Test
the return value of the setting method to check "success".

=item method called with no argument in list context

Return a list consisting of all acceptable choices for parameters with
discrete choices. Return a list C<(minimum, maximum)> for parameters
which can be set to a range of values. Binary selections have no need
to call this way - but will get C<(0,1)> if they do. Beginning in
Version 0.16, Binary selections inherited from Win32API::CommPort may
not return anything useful in list context. The null list C<(undef)>
will be returned for failed calls in list context (e.g. for an invalid
or unexpected argument).

=item Asynchronous (Background) I/O

The module handles Polling (do if Ready), Synchronous (block until
Ready), and Asynchronous Modes (begin and test if Ready) with the timeout
choices provided by the API. No effort has yet been made to interact with
Windows events. But background I/O has been used successfully with the
Perl Tk modules and callbacks from the event loop.

=item Timeouts

The API provides two timing models. The first applies only to reading and
essentially determines I<Read Not Ready> by checking the time between
consecutive characters. The B<ReadFile> operation returns if that time
exceeds the value set by B<read_interval>. It does this by timestamping
each character. It appears that at least one character must by received in
I<every> B<read> I<call to the API> to initialize the mechanism. The timer
is then reset by each succeeding character. If no characters are received,
the read will block indefinitely.

Setting B<read_interval> to C<0xffffffff> will do a non-blocking read.
The B<ReadFile> returns immediately whether or not any characters are
actually read. This replicates the behavior of the API.

The other model defines the total time allowed to complete the operation.
A fixed overhead time is added to the product of bytes and per_byte_time.
A wide variety of timeout options can be defined by selecting the three
parameters: fixed, each, and size.

Read_Total = B<read_const_time> + (B<read_char_time> * bytes_to_read)

Write_Total = B<write_const_time> + (B<write_char_time> * bytes_to_write)

When reading a known number of characters, the I<Read_Total> mechanism is
recommended. This mechanism I<MUST> be used with I<tied FileHandles> because
the tie methods can make multiple internal API calls in response to a single
B<sysread> or B<READLINE>. The I<Read_Interval> mechanism is suitable for
a B<read> method that expects a response of variable or unknown size. You
should then also set a long I<Read_Total> timeout as a "backup" in case
no bytes are received.

=back

=head2 Exports

Nothing is exported by default.  Nothing is currently exported. Optional
tags from Win32API::CommPort are passed through.

=over 4

=item :PARAM

Utility subroutines and constants for parameter setting and test:

	LONGsize	SHORTsize	nocarp		yes_true
	OS_Error	internal_buffer

=item :STAT

Serial communications constants from Win32API::CommPort. Included are the
constants for ascertaining why a transmission is blocked:

	BM_fCtsHold	BM_fDsrHold	BM_fRlsdHold	BM_fXoffHold
	BM_fXoffSent	BM_fEof		BM_fTxim	BM_AllBits

Which incoming bits are active:

	MS_CTS_ON	MS_DSR_ON	MS_RING_ON	MS_RLSD_ON

What hardware errors have been detected:

	CE_RXOVER	CE_OVERRUN	CE_RXPARITY	CE_FRAME
	CE_BREAK	CE_TXFULL	CE_MODE

Offsets into the array returned by B<status:>

	ST_BLOCK	ST_INPUT	ST_OUTPUT	ST_ERROR

=back

=head2 Stty Emulation

Nothing wrong with dreaming! A subset of stty options is available
through a B<stty> method. The purpose is support of existing serial
devices which have embedded knowledge of Unix communication line and
login practices. It is also needed by Tom Christiansen's Perl Power Tools
project. This is new and experimental in Version 0.15. The B<stty> method
returns an array of "traditional stty values" when called with no
arguments. With arguments, it sets the corresponding parameters.

  $ok = $PortObj->stty("-icanon");	# equivalent to stty_icanon(0)
  @stty_all = $PortObj->stty();		# get all the parameters, Perl-style
  $ok = $PortObj->stty("cs7",19200);	# multiple parameters
  $ok = $PortObj->stty(@stty_save);	# many parameters

The distribution includes a demo script, stty.plx, which gives details
of usage. Not all Unix parameters are currently supported. But the array
will contain all those which can be set. The order in C<@stty_all> will
match the following pattern:

  baud,			# numeric, always first
  "intr", character,	# the parameters which set special characters
  "name", character, ...
  "stop", character,	# "stop" will always be the last "pair"
  "parameter",		# the on/off settings
  "-parameter", ...

Version 0.13 added the primitive functions required to implement this
feature. A number of methods named B<stty_xxx> do what an
I<experienced stty user> would expect.
Unlike B<stty> on Unix, the B<stty_xxx> operations apply only to I/O
processed via the B<lookfor> method or the I<tied FileHandle> methods.
The B<read, input, read_done, write> methods all treat data as "raw".


        The following stty functions have related SerialPort functions:
        ---------------------------------------------------------------
        stty (control)		SerialPort		Default Value
        ----------------	------------------      -------------
        parenb inpck		parity_enable		from port

        parodd			parity			from port

        cs5 cs6 cs7 cs8		databits		from port

        cstopb			stopbits		from port

        clocal crtscts		handshake		from port
        ixon ixoff		handshake		from port

        time			read_const_time		from port

        110 300 600 1200 2400	baudrate		from port
        4800 9600 19200 38400	baudrate

        75 134.5 150 1800	fixed baud only - not selectable

        g, "stty < /dev/x"	start, save		none

        sane			restart			none



        stty (input)		SerialPort		Default Value
        ----------------	------------------      -------------
	istrip			stty_istrip		off

	igncr			stty_igncr		off

	inlcr			stty_inlcr		off

	icrnl			stty_icrnl		on

        parmrk			error_char		from port (off typ)



        stty (output)		SerialPort		Default Value
        ----------------	------------------      -------------
	ocrnl			stty_ocrnl		off if opost

	onlcr			stty_onlcr		on if opost

	opost			stty_opost		off



        stty (local)		SerialPort		Default Value
        ----------------	------------------      -------------
        raw			read, write, input	none

        cooked			lookfor			none

	echo			stty_echo		off

	echoe			stty_echoe		on if echo

	echok			stty_echok		on if echo

	echonl			stty_echonl		off

	echoke			stty_echoke		on if echo

	echoctl			stty_echoctl		off

	isig			stty_isig		off

	icanon			stty_icanon		off



        stty (char)		SerialPort		Default Value
        ----------------	------------------      -------------
	intr			stty_intr		"\cC"
				is_stty_intr		3

	quit			stty_quit		"\cD"
				is_stty_quit		4

	erase			stty_erase		"\cH"
				is_stty_erase		8

	(erase echo)		stty_bsdel		"\cH \cH"

	kill			stty_kill		"\cU"
				is_stty_kill		21

	(kill echo)		stty_clear		"\r {76}\r"
				is_stty_clear		"-@{76}-"

	eof			stty_eof		"\cZ"
				is_stty_eof		26

	eol			stty_eol		"\cJ"
				is_stty_eol		10

        start			xon_char		from port ("\cQ" typ)
				is_xon_char		17

        stop			xoff_char		from port ("\cS" typ)
				is_xoff_char		19



        The following stty functions have no equivalent in SerialPort:
        --------------------------------------------------------------
        [-]hup		[-]ignbrk	[-]brkint	[-]ignpar
        [-]tostop	susp		0		50
	134		200		exta		extb
	[-]cread	[-]hupcl

The stty function list is taken from the documentation for IO::Stty by
Austin Schutz.

=head2 Lookfor and I/O Processing

Many of the B<stty_xxx> methods support features which are necessary for
line-oriented input (such as command-line handling). These include methods
which select control-keys to delete characters (B<stty_erase>) and lines
(B<stty_kill>), define input boundaries (B<stty_eol, stty_eof>), and abort
processing (B<stty_intr, stty_quit>). These keys also have B<is_stty_xxx>
methods which convert the key-codes to numeric equivalents which can be
saved in the configuration file.

Some communications programs have a different but related need - to collect
(or discard) input until a specific pattern is detected. For lines, the
pattern is a line-termination. But there are also requirements to search
for other strings in the input such as "username:" and "password:". The
B<lookfor> method provides a consistant mechanism for solving this problem.
It searches input character-by-character looking for a match to any of the
elements of an array set using the B<are_match> method. It returns the
entire input up to the match pattern if a match is found. If no match
is found, it returns "" unless an input error or abort is detected (which
returns undef).

The actual match and the characters after it (if any) may also be viewed
using the B<lastlook> method. In Version 0.13, the match test included
a C<s/$pattern//s> test which worked fine for literal text but returned
the I<Regular Expression> that matched when C<$pattern> contained any Perl
metacharacters. That was probably a bug - although no one reported it.

In Version 0.14, B<lastlook> returns both the input and the pattern from
the match test. It also adopts the convention from Expect.pm that match
strings are literal text (tested using B<index>) unless preceeded in the
B<are_match> list by a B<"-re",> entry. The default B<are_match> list
is C<("\n")>, which matches complete lines.

   my ($match, $after, $pattern, $instead) = $PortObj->lastlook;
     # input that MATCHED, input AFTER the match, PATTERN that matched
     # input received INSTEAD when timeout without match ("" if match)

   $PortObj->are_match("text1", "-re", "pattern", "text2");
     # possible match strings: "pattern" is a regular expression,
     #                         "text1" and "text2" are literal strings

The I<Regular Expression> handling in B<lookfor> is still
experimental. Please let me know if you use it (or can't use it), so
I can confirm bug fixes don't break your code. For literal strings,
C<$match> and C<$pattern> should be identical. The C<$instead> value
returns the internal buffer tested by the match logic. A successful
match or a B<lookclear> resets it to "" - so it is only useful for error
handling such as timeout processing or reporting unexpected responses.

The B<lookfor> method is designed to be sampled periodically (polled). Any
characters after the match pattern are saved for a subsequent B<lookfor>.
Internally, B<lookfor> is implemented using the nonblocking B<input> method
when called with no parameter. If called with a count, B<lookfor> calls
C<$PortObj-E<gt>read(count)> which blocks until the B<read> is I<Complete> or
a I<Timeout> occurs. The blocking alternative should not be used unless a
fault time has been defined using B<read_interval, read_const_time, and
read_char_time>. It exists mostly to support the I<tied FileHandle>
functions B<sysread, getc,> and B<E<lt>FHE<gt>>.

The internal buffers used by B<lookfor> may be purged by the B<lookclear>
method (which also clears the last match). For testing, B<lookclear> can
accept a string which is "looped back" to the next B<input>. This feature
is enabled only when C<set_test_mode_active(1)>. Normally, B<lookclear>
will return C<undef> if given parameters. It still purges the buffers and
last_match in that case (but nothing is "looped back"). You will want
B<stty_echo(0)> when exercising loopback.

Version 0.15 adds a B<matchclear> method. It is designed to handle the
"special case" where the match string is the first character(s) received
by B<lookfor>. In this case, C<$lookfor_return == "">, B<lookfor> does
not provide a clear indication that a match was found. The B<matchclear>
returns the same C<$match> that would be returned by B<lastlook> and
resets it to "" without resetting any of the other buffers. Since the
B<lookfor> already searched I<through> the match, B<matchclear> is used
to both detect and step-over "blank" lines.

The character-by-character processing used by B<lookfor> to support the
I<stty emulation> is fine for interactive activities and tasks which
expect short responses. But it has too much "overhead" to handle fast
data streams.  Version 0.15 adds a B<streamline> method which is a fast,
line-oriented alternative with no echo support or input handling except
for pattern searching. Exact benchmarks will vary with input data and
patterns, but my tests indicate B<streamline> is 10-20 times faster then
B<lookfor> when uploading files averaging 25-50 characters per line.
Since B<streamline> uses the same internal buffers, the B<lookclear,
lastlook, are_match, and matchclear> methods act the same in both cases.
In fact, calls to B<streamline> and B<lookfor> can be interleaved if desired
(e.g. an interactive task that starts an upload and returns to interactive
activity when it is complete).

Beginning in Version 0.15, the B<READLINE> method supports "list context".
A tied FileHandle can slurp in a whole file with an "@lines = E<lt>FHE<gt>"
construct. In "scalar context", B<READLINE> calls B<lookfor>. But it calls
B<streamline> in "list context". Both contexts also call B<matchclear>
to detect "empty" lines and B<reset_error> to detect hardware problems.
The existance of a hardware fault is reported with C<$^E>, although the
specific fault is only reported when B<error_msg> is true.

There are two additional methods for supporting "list context" input:
B<lastline> sets an "end_of_file" I<Regular Expression>, and B<linesize>
permits changing the "packet size" in the blocking read operation to allow
tuning performance to data characteristics. These two only apply during
B<READLINE>. The default for B<linesize> is 1. There is no default for
the B<lastline> method.

In Version 0.15, I<Regular Expressions> set by B<are_match> and B<lastline>
will be pre-compiled using the I<qr//> construct.
This doubled B<lookfor> and B<streamline> speed in my tests with
I<Regular Expressions> - but actual improvements depend on both patterns
and input data.

The functionality of B<lookfor> includes a limited subset of the capabilities
found in Austin Schutz's I<Expect.pm> for Unix (and Tcl's expect which it
resembles). The C<$before, $match, $pattern, and $after> return values are
available if someone needs to create an "expect" subroutine for porting a
script. When using multiple patterns, there is one important functional
difference: I<Expect.pm> looks at each pattern in turn and returns the first
match found; B<lookfor> and B<streamline> test all patterns and return the
one found I<earliest> in the input if more than one matches.

Because B<lookfor> can be used to manage a command-line environment much
like a Unix serial login, a number of "stty-like" methods are included to
handle the issues raised by serial logins. One issue is dissimilar line
terminations. This is addressed by the following methods:

  $PortObj->stty_icrnl;		# map \r to \n on input
  $PortObj->stty_igncr;		# ignore \r on input
  $PortObj->stty_inlcr;		# map \n to \r on input
  $PortObj->stty_ocrnl;		# map \r to \n on output
  $PortObj->stty_onlcr;		# map \n to \r\n on output
  $PortObj->stty_opost;		# enable output mapping

The default specifies a raw device with no input or output processing.
In Version 0.14, the default was a device which sends "\r" at the end
of a line, requires "\r\n" to terminate incoming lines, and expects the
"host" to echo every keystroke. Many "dumb terminals" act this way and
the defaults were similar to Unix defaults. But some users found this
ackward and confusing.

Sometimes, you want perl to echo input characters back to the serial
device (and other times you don't want that).

  $PortObj->stty_echo;		# echo every character
  $PortObj->stty_echoe;		# if echo erase with bsdel string (default)
  $PortObj->stty_echok;		# if echo \n after kill character (default)
  $PortObj->stty_echonl;	# echo \n even if stty_echo(0)
  $PortObj->stty_echoke;	# if echo clear string after kill (default)
  $PortObj->stty_echoctl;	# if echo "^Char" for control chars

  $PortObj->stty_istrip;	# strip input to 7-bits

  my $air = " "x76;		# overwrite entire line with spaces
  $PortObj->stty_clear("\r$air\r");	# written after kill character
  $PortObj->is_prompt("PROMPT:");	# need to write after kill
  $PortObj->stty_bsdel("\cH \cH");	# written after erase character

  # internal method that permits clear string with \r in config file
  my $plus32 = "@"x76;		# overwrite line with spaces (ord += 32)
  $PortObj->is_stty_clear("-$plus32-");	# equivalent to stty_clear


=head1 NOTES

The object returned by B<new> or B<start> is NOT a I<FileHandle>. You
will be disappointed if you try to use it as one. If you need a
I<FileHandle>, you must use B<tie> as the constructor.

e.g. the following is WRONG!!____C<print $PortObj "some text";>

You need something like this:

        # construct
    $tie_ob = tie(*FOO,'Win32::SerialPort', $cfgfile)
                 or die "Can't start $cfgfile\n";

    print FOO "enter char: "; # destination is FileHandle, not Object
    my $in = getc FOO;
    syswrite FOO, "$in\n", 2, 0;
    print FOO "enter line: ";
    $in = <FOO>;
    printf FOO "received: %s\n", $in;
    print FOO "enter 5 char: ";
    sysread (FOO, $in, 5, 0) or die;
    printf FOO "received: %s\n", $in;

        # destruct
    close FOO || print "close failed\n";
    undef $tie_ob;	# Don't forget this one!!
    untie *FOO;

Always include the C<undef $tie_ob> before the B<untie>. See the I<Gotcha>
description in I<perltie>.

An important note about Win32 filenames. The reserved device names such
as C< COM1, AUX, LPT1, CON, PRN > can NOT be used as filenames. Hence
I<"COM2.cfg"> would not be usable for B<$Configuration_File_Name>.

Thanks to Ken White for initial testing on NT.

There is a linux clone of this module implemented using I<POSIX.pm>.
It also runs on AIX and Solaris, and will probably run on other POSIX
systems as well. It does not currently support the complete set of methods -
although portability of user programs is excellent for the calls it does
support. It is available from CPAN as I<Device::SerialPort>.

There is an emulator for testing application code without hardware.
It is available from CPAN as I<Test::Device::SerialPort>. But it also
works fine with the Win32 version.

=head1 KNOWN LIMITATIONS

Since everything is (sometimes convoluted but still pure) Perl, you can
fix flaws and change limits if required. But please file a bug report if
you do. This module has been tested with each of the binary perl versions
for which Win32::API is supported: AS builds 315, 316, 500-509 and GS
5.004_02. It has only been tested on Intel hardware.

Although the B<lookfor, stty_xxx, and Tied FileHandle> mechanisms are
considered stable, they have only been tested on a small subset of possible
applications. While "\r" characters may be included in the clear string
using B<is_stty_clear> internally, "\n" characters may NOT be included
in multi-character strings if you plan to save the strings in a configuration
file (which uses "\n" as an internal terminator).

=over 4

=item Tutorial

With all the options, this module needs a good tutorial. It doesn't
have a complete one yet. A I<"How to get started"> tutorial appeared
B<The Perl Journal #13> (March 1999). The demo programs in the distribution
are a good starting point for additional examples.

=item Buffers

The size of the Win32 buffers are selectable with B<buffers>. But each read
method currently uses a fixed internal buffer of 4096 bytes. This can be
changed in the Win32API::CommPort source and read with B<internal_buffer>.
The XS version will support dynamic buffer sizing. Large operations are
automatically converted to multiple smaller ones by the B<tied FileHandle>
methods.

=item Modems

Lots of modem-specific options are not supported. The same is true of
TAPI, MAPI. I<API Wizards> are welcome to contribute.

=item API Options

Lots of options are just "passed through from the API". Some probably
shouldn't be used together. The module validates the obvious choices when
possible. For something really fancy, you may need additional API
documentation. Available from I<Micro$oft Pre$$>.

=back

=head1 BUGS

On Win32, a port must B<close> before it can be reopened again by the same
process. If a physical port can be accessed using more than one name (see
above), all names are treated as one. The perl script can also be run
multiple times within a single batch file or shell script.

On NT, a B<read_done> or B<write_done> returns I<False> if a background
operation is aborted by a purge. Win9x returns I<True>.

A few NT systems seem to set B<can_parity_enable> true, but do not actually
support setting B<parity_enable>. This may be a characteristic of certain
third-party serial drivers.

__Please send comments and bug reports to wcbirthisel@alum.mit.edu.

=head1 AUTHORS

Bill Birthisel, wcbirthisel@alum.mit.edu, http://members.aol.com/Bbirthisel/.

Tye McQueen contributed but no longer supports these modules.

=head1 SEE ALSO

Win32API::CommPort - the low-level API calls which support this module

Win32API::File I<when available>

Win32::API - Aldo Calpini's "Magic", http://www.divinf.it/dada/perl/

Perltoot.xxx - Tom (Christiansen)'s Object-Oriented Tutorial

Expect.pm - Austin Schutz's adaptation of TCL's "expect" for Unix Perls

=head1 COPYRIGHT

Copyright (C) 2010, Bill Birthisel. All rights reserved.

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head2 COMPATIBILITY

Most of the code in this module has been stable since version 0.12.
Version 0.20 adds explicit support for COM10++ and USB - although the
functionality existed before. Perl ports before 5.6.0 are no longer
supported for test or install. The modules themselves work with 5.003.
1 April 2010.

=cut
