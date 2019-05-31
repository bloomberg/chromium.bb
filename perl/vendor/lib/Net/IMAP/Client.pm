package Net::IMAP::Client;

use vars qw[$VERSION];
$VERSION = '0.9505';

use strict;
use warnings;

use List::Util qw( min max first );
use List::MoreUtils qw( each_array );
use IO::Socket::INET ();
use IO::Socket::SSL ();
use Socket qw( SO_KEEPALIVE );

use Net::IMAP::Client::MsgSummary ();

our $READ_BUFFER = 4096;
my %UID_COMMANDS = map { $_ => 1 } qw( COPY FETCH STORE SEARCH SORT THREAD );
my %DEFAULT_ARGS = (
    uid_mode => 1,
    timeout  => 90,
    server   => '127.0.0.1',
    port     => undef,
    user     => undef,
    pass     => undef,
    ssl      => 0,
    ssl_verify_peer => 1,
    socket   => undef,
    _cmd_id  => 0,
    ssl_options => {},
);

sub new {
    my ($class, %args) = @_;

    my $self = { map {
        $_ => exists $args{$_} ? $args{$_} : $DEFAULT_ARGS{$_}
    } keys %DEFAULT_ARGS };

    bless $self, $class;

    $self->{notifications} = [];
    eval {
        $self->{greeting} = $self->_socket_getline;
    };
    return $@ ? undef : $self;
}

sub DESTROY {
    my ($self) = @_;
    eval {
        $self->quit
          if $self->{socket}->opened;
    };
}

sub uid_mode {
    my ($self, $val) = @_;
    if (defined($val)) {
        return $self->{uid_mode} = $val;
    } else {
        return $self->{uid_mode};
    }
}

### imap utilities ###

sub login {
    my ($self, $user, $pass) = @_;
    $user ||= $self->{user};
    $pass ||= $self->{pass};
    $self->{user} = $user;
    $self->{pass} = $pass;
    _string_quote($user);
    _string_quote($pass);
    my ($ok) = $self->_tell_imap(LOGIN => "$user $pass");
    return $ok;
}

sub logout {
    my ($self) = @_;
    $self->_send_cmd('LOGOUT');
    $self->_get_socket->close;
    return 1;
}

*quit = \&logout;

sub capability {
    my ($self, $requirement) = @_;
    my $capability = $self->{capability};
    unless ($capability) {
        my ($ok, $lines) = $self->_tell_imap('CAPABILITY');
        if ($ok) {
            my $line = $lines->[0][0];
            if ($line =~ /^\*\s+CAPABILITY\s+(.*?)\s*$/) {
                $capability = $self->{capability} = [ split(/\s+/, $1) ];
            }
        }
    }
    if ($requirement && $capability) {
        return first { $_ =~ $requirement } @$capability;
    }
    return $capability;
}

sub status {
    my $self = shift;
    my $a;
    my $wants_one = undef;
    if (ref($_[0]) eq 'ARRAY') {
        my @tmp = @{$_[0]};
        $a = \@tmp;
    } else {
        $a = [ shift ];
        $wants_one = 1;
    }
    foreach (@$a) {
        _string_quote($_);
        $_ = "STATUS $_ (MESSAGES RECENT UNSEEN UIDNEXT UIDVALIDITY)";
    }
    my $results = $self->_tell_imap2(@$a);

    # remove "NO CLIENT BUG DETECTED" lines as they serve no
    # purpose beyond the religious zeal of IMAP implementors
    for my $row (@$results) {
        if (@{$row->[1]} > 1) {
            $row->[1] = [ grep { $_->[0] !~ /NO CLIENT BUG DETECTED: STATUS on selected mailbox:/ } @{$row->[1]} ];
        }
    }

    my %ret;
    my $name;
    foreach my $i (@$results) {
        if ($i->[0]) {          # was successful?
            my $tokens = _parse_tokens($i->[1]->[0]);
            $name = $tokens->[2];
            $tokens = $tokens->[3];
            my %tmp = @$tokens;
            $tmp{name} = $name;
            $ret{$name} = \%tmp;
        }
    }
    return $wants_one 
        ? (defined $name and $ret{$name})  # avoid data on undef key
        : \%ret;
}

sub select {
	my ($self, $folder) = @_;
	$self->_select_or_examine($folder, 'SELECT');
}
sub examine {
	my ($self, $folder) = @_;
	$self->_select_or_examine($folder, 'EXAMINE');
}

sub _select_or_examine {
    my ($self, $folder, $operation) = @_;
    my $quoted = $folder;
    _string_quote($quoted);
    my ($ok, $lines) = $self->_tell_imap($operation => $quoted);
    if ($ok) {
        $self->{selected_folder} = $folder;
        my %info = ();
        foreach my $tmp (@$lines) {
            my $line = $tmp->[0];
            if ($line =~ /^\*\s+(\d+)\s+EXISTS/i) {
                $info{messages} = $1 + 0;
            } elsif ($line =~ /^\*\s+FLAGS\s+\((.*?)\)/i) {
                $info{flags} = [ split(/\s+/, $1) ];
            } elsif ($line =~ /^\*\s+(\d+)\s+RECENT/i) {
                $info{recent} = $1 + 0;
            } elsif ($line =~ /^\*\s+OK\s+\[(.*?)\s+(.*?)\]/i) {
                my ($flag, $value) = ($1, $2);
                if ($value =~ /\((.*?)\)/) {
                    $info{sflags}->{$flag} = [split(/\s+/, $1)];
                } else {
                    $info{sflags}->{$flag} = $value;
                }
            }
        }
        $self->{FOLDERS} ||= {};
        $self->{FOLDERS}{$folder} = \%info;
    }
    return $ok;
}

sub separator {
    my ($self) = @_;
    my $sep = $self->{separator};
    if (!$sep) {
        my ($ok, $lines) = $self->_tell_imap(LIST => '"" ""');
        if ($ok) {
            my $tokens = _parse_tokens($lines->[0]);
            $sep = $self->{separator} = $tokens->[3];
        } else {
            $sep = undef;
        }
    }
    return $sep;
}

sub folders {
    my ($self) = @_;
    my ($ok, $lines) = $self->_tell_imap(LIST => '"" "*"');
    if ($ok) {
        my @ret = map { _parse_tokens($_)->[4] } @$lines;
        return wantarray ? @ret : \@ret;
    }
    return undef;
}

sub _mk_namespace {
    my ($ns) = @_;
    if ($ns) {
        foreach my $i (@$ns) {
            $i = {
                prefix => $i->[0],
                sep    => $i->[1],
            };
        }
    }
    return $ns;
}

sub namespace {
    my ($self) = @_;
    my ($ok, $lines) = $self->_tell_imap('NAMESPACE');
    if ($ok) {
        my $ret = _parse_tokens($lines->[0]);
        splice(@$ret, 0, 2);
        return {
            personal => _mk_namespace($ret->[0]),
            other    => _mk_namespace($ret->[1]),
            shared   => _mk_namespace($ret->[2]),
        };
    }
}

sub folders_more {
    my ($self) = @_;
    my ($ok, $lines) = $self->_tell_imap(LIST => '"" "*"');
    if ($ok) {
        my %ret = map {
            my $tokens = _parse_tokens($_);
            my $flags = $tokens->[2];
            my $sep   = $tokens->[3];
            my $name  = $tokens->[4];
            ( $name, { flags => $flags, sep => $sep } );
        } @$lines;
        return \%ret;
    }
    return undef;
}

sub noop {
    my ($self) = @_;
    my ($ok) = $self->_tell_imap('NOOP', undef, 1);
    return $ok;
}

sub seq_to_uid {
    my ($self, @seq_ids) = @_;
    my $ids = join(',', @seq_ids);

    my $save_uid_mode = $self->uid_mode;
    $self->uid_mode(0);
    my ($ok, $lines) = $self->_tell_imap(FETCH => "$ids UID", 1);
    $self->uid_mode($save_uid_mode);

    if ($ok) {
        my %ret = map {
            $_->[0] =~ /^\*\s+(\d+)\s+FETCH\s*\(\s*UID\s+(\d+)/
              && ( $1, $2 );
        } @$lines;
        return \%ret;
    }
    return undef;
}

sub search {
    my ($self, $criteria, $sort, $charset) = @_;

    $charset ||= 'UTF-8';

    my $cmd = $sort ? 'SORT' : 'SEARCH';
    if ($sort) {
        if (ref($sort) eq 'ARRAY') {
            $sort = uc '(' . join(' ', @$sort) . ')';
        } elsif ($sort !~ /^\(/) {
            $sort = uc "($sort)";
        }
        $sort =~ s/\s*$/ /;
        $sort =~ s/\^/REVERSE /g;
    } else {
        $charset = "CHARSET $charset";
        $sort = '';
    }

    if (ref($criteria) eq 'HASH') {
        my @a;
        while (my ($key, $val) = each %$criteria) {
            my $quoted = $val;
			# don't quote range
			_string_quote($quoted) unless uc $key eq 'UID';
            push @a, uc $key, $quoted;
        }
        $criteria = '(' . join(' ', @a) . ')';
    }

    my ($ok, $lines) = $self->_tell_imap($cmd => "$sort$charset $criteria", 1);
    if ($ok) {
        # it makes no sense to employ the full token parser here
        # read past progress messages lacking initial '*'
		foreach my $line (@{$lines->[0]}) {
			if ($line =~ s/^\*\s+(?:SEARCH|SORT)\s+//ig) {
				$line =~ s/\s*$//g;
				return [ map { $_ + 0 } split(/\s+/, $line) ];
			}
		}
    }

    return undef;
}

sub get_rfc822_body {
    my ($self, $msg) = @_;
    my $wants_many = undef;
    if (ref($msg) eq 'ARRAY') {
        $msg = join(',', @$msg);
        $wants_many = 1;
    }
    my ($ok, $lines) = $self->_tell_imap(FETCH => "$msg RFC822", 1);
    if ($ok) {
        my @ret = map { $_->[1] } @$lines;
        return $wants_many ? \@ret : $ret[0];
    }
    return undef;
}

sub get_part_body {
    my ($self, $msg, $part) = @_;
    $part = "BODY[$part]";
    my ($ok, $lines) = $self->_tell_imap(FETCH => "$msg $part", 1);
    if ($ok) {
        # it can contain FLAGS notification, i.e. \Seen flag becomes set
        my $tokens = _parse_tokens($lines->[0], 1);
        my %hash = @{$tokens->[3]};
        if ($hash{FLAGS}) {
            $self->_handle_notification($tokens);
        }
        return $hash{$part};
    }
    return undef;
}

sub get_parts_bodies {
    my ($self, $msg, $parts) = @_;
    my $tmp = join(' ', map { "BODY[$_]" } @$parts);
    my ($ok, $lines) = $self->_tell_imap(FETCH => "$msg ($tmp)", 1);
    if ($ok) {
        # it can contain FLAGS notification, i.e. \Seen flag becomes set
        my $tokens = _parse_tokens($lines->[0], 1);
        my %hash = @{$tokens->[3]};
        if ($hash{FLAGS}) {
            $self->_handle_notification($tokens);
        }
        my %ret = map {( $_, $hash{"BODY[$_]"} )} @$parts;
        return \%ret;
    }
    return undef;
}

sub get_summaries {
    my ($self, $msg, $headers) = @_;
    if (!$msg) {
        $msg = '1:*';
    } elsif (ref $msg eq 'ARRAY') {
        $msg = join(',', @$msg);
    }
    if ($headers) {
        $headers = " BODY.PEEK[HEADER.FIELDS ($headers)]";
    } else {
        $headers = '';
    }
    my ($ok, $lp) = $self->_tell_imap(FETCH => qq[$msg (UID FLAGS INTERNALDATE RFC822.SIZE ENVELOPE BODYSTRUCTURE$headers)], 1);
    if ($ok) {
        my @ret;
        foreach (@$lp) {
            my $summary;
            my $tokens = _parse_tokens($_); ## in form: [ '*', ID, 'FETCH', [ tokens ]]
            if ($tokens->[2] eq 'FETCH') {
                my %hash = @{$tokens->[3]};
                if ($hash{ENVELOPE}) {
                    # full fetch
                    $summary = Net::IMAP::Client::MsgSummary->new(\%hash, undef, !!$headers);
                    $summary->{seq_id} = $tokens->[1];
                } else {
                    # 'FETCH' (probably FLAGS) notification!
                    $self->_handle_notification($tokens);
                }
            } else {
                # notification!
                $self->_handle_notification($tokens);
            }
            push @ret, $summary
              if $summary;
        }
        return \@ret;
    } else {
        return undef;
    }
}

sub fetch {
    my ($self, $msg, $keys) = @_;
    my $wants_many = undef;
    if (ref $msg eq 'ARRAY') {
        $msg = join(',', @$msg);
        $wants_many = 1;
    }
    if (ref $keys eq 'ARRAY') {
        $keys = join(' ', @$keys);
    }
    my ($ok, $lp) = $self->_tell_imap(FETCH => qq[$msg ($keys)], 1);
    if ($ok) {
        my @ret;
        foreach (@$lp) {
            my $tokens = _parse_tokens($_)->[3];
            push @ret, { @$tokens };
        }
        return $wants_many || @ret > 1 ? \@ret : $ret[0];
    }
}

sub create_folder {
    my ($self, $folder) = @_;
    my $quoted = $folder;
    _string_quote($quoted);
    my ($ok) = $self->_tell_imap(CREATE => $quoted);
    return $ok;
}

# recursively removes any subfolders!
sub delete_folder {
    my ($self, $folder) = @_;
    my $quoted = $folder . $self->separator . '*';
    _string_quote($quoted);
    my ($ok, $lines) = $self->_tell_imap(LIST => qq{"" $quoted});
    if ($ok) {
        my @subfolders;
        foreach my $line (@$lines) {
            my $tokens = _parse_tokens($line);
            push @subfolders, $tokens->[4];
        }
        @subfolders = sort { length($b) - length($a) } @subfolders;
        foreach (@subfolders) {
            _string_quote($_);
            ($ok) = $self->_tell_imap(DELETE => $_);
        }
        $quoted = $folder;
        _string_quote($quoted);
        ($ok) = $self->_tell_imap(DELETE => $quoted);
    }
    return $ok;
}

sub append {
    my ($self, $folder, $rfc822, $flags, $date) = @_;
    die 'message body passed to append() must be a SCALAR reference'
        unless ref $rfc822 eq 'SCALAR';
    my $quoted = $folder;
    _string_quote($quoted);
    my $args = [ "$quoted " ];
    if ($flags) {
        # my @tmp = @$flags;
        # $quoted = join(' ', map { _string_quote($_) } @tmp);
        # push @$args, "($quoted) ";
        push @$args, '(' . join(' ', @$flags) . ') ';
    }
    if ($date) {
        my $tmp = $date;
        _string_quote($tmp);
        push @$args, "$tmp ";
    }
    push @$args, $rfc822;
    my ($ok) = $self->_tell_imap(APPEND => $args, 1);
    return $ok;
}

sub copy {
    my ($self, $msg, $folder) = @_;
    my $quoted = $folder;
    _string_quote($quoted);
    if (ref $msg eq 'ARRAY') {
        $msg = join(',', @$msg);
    }
    my ($ok) = $self->_tell_imap(COPY => "$msg $quoted", 1);
    return $ok;
}

sub get_flags {
    my ($self, $msg) = @_;
    my $wants_many = undef;
    if (ref($msg) eq 'ARRAY') {
        $msg = join(',', @$msg);
        $wants_many = 1;
    }
    my ($ok, $lines) = $self->_tell_imap(FETCH => "$msg (UID FLAGS)", 1);
    if ($ok) {
        my %ret = map {
            my $tokens = _parse_tokens($_)->[3];
            my %hash = @$tokens;
            $hash{UID} => $hash{FLAGS};
        } @$lines;
        return $wants_many ? \%ret : $ret{$msg};
    }
    return undef;
}

sub get_threads {
    my ($self, $algo, $msg) = @_;
    $algo ||= "REFERENCES";
    my ($ok, $lines) = $self->_tell_imap(THREAD => "$algo UTF-8 ALL");
    if ($ok) {
        my $result = $lines->[0][0];
        $result =~ s/^\*\s+THREAD\s+//;
        my $parsed = _parse_tokens([ $result ]);
        if ($msg) {
            (my $left = $result) =~ s/\b$msg\b.*$//;
            my $thr = 0;
            my $par = 0;
            for (my $i = 0; $i < length($left); ++$i) {
                my $c = substr($left, $i, 1);
                if ($c eq '(') {
                    $par++;
                } elsif ($c eq ')') {
                    $par--;
                    if ($par == 0) {
                        $thr++;
                    }
                }
            }
            $parsed = $parsed->[$thr];
        }
        return $parsed;
    }
    return $ok;
}

sub _store_helper {
    my ($self, $msg, $flags, $cmd) = @_;
    if (ref $msg eq 'ARRAY') {
        $msg = join(',', @$msg);
    }
    unless (ref $flags) {
        $flags = [ $flags ];
    }
    $flags = '(' . join(' ', @$flags) . ')';
    $self->_tell_imap(STORE => "$msg $cmd $flags", 1);
}

sub store {
    my ($self, $msg, $flags) = @_;
    $self->_store_helper($msg, $flags, 'FLAGS');
}

sub add_flags {
    my ($self, $msg, $flags) = @_;
    $self->_store_helper($msg, $flags, '+FLAGS');
}

sub del_flags {
    my ($self, $msg, $flags) = @_;
    $self->_store_helper($msg, $flags, '-FLAGS');
}

sub delete_message {
    my ($self, $msg) = @_;
    $self->add_flags($msg, '\\Deleted');
}

sub expunge {
    my ($self) = @_;
    my ($ok, $lines) = $self->_tell_imap('EXPUNGE' => undef, 1);
    if ($ok && $lines && @$lines) {
        my $ret = $lines->[0][0];
        if ($ret =~ /^\*\s+(\d+)\s+EXPUNGE/) {
            return $1 + 0;
        }
    }
    return $ok ? -1 : undef;
}

sub last_error {
    my ($self) = @_;
    $self->{_error} =~ s/\s+$//s; # remove trailing carriage return
    return $self->{_error};
}

sub notifications {
    my ($self) = @_;
    my $tmp = $self->{notifications};
    $self->{notifications} = [];
    return wantarray ? @$tmp : $tmp;
}

##### internal stuff #####

sub _get_port {
    my ($self) = @_;
    return $self->{port} || ($self->{ssl} ? 993 : 143);
}

sub _get_timeout {
    my ($self) = @_;
    return $self->{timeout} || 90;
}

sub _get_server {
    my ($self) = @_;
    return $self->{server};
}

sub _get_ssl_config {
    my ($self) = @_;
    if (!$self->{ssl_verify_peer}
         || !$self->{ssl_ca_path}
         && !$self->{ssl_ca_file}
         && $^O ne 'linux') {
        return SSL_verify_mode => IO::Socket::SSL::SSL_VERIFY_NONE;
    }

    my %ssl_config = ( SSL_verify_mode => IO::Socket::SSL::SSL_VERIFY_PEER );

    if ($^O eq 'linux' && !$self->{ssl_ca_path} && !$self->{ssl_ca_file}) {
        $ssl_config{SSL_ca_path} = '/etc/ssl/certs/';
		-d $ssl_config{SSL_ca_path} 
			or die "$ssl_config{SSL_ca_path}: SSL certification directory not found";
    }
    $ssl_config{SSL_ca_path} = $self->{ssl_ca_path} if $self->{ssl_ca_path};
    $ssl_config{SSL_ca_file} = $self->{ssl_ca_file} if $self->{ssl_ca_file};

    return %ssl_config;
}
sub _get_socket {
    my ($self) = @_;
     my $socket = $self->{socket} ||= ($self->{ssl} ? 'IO::Socket::SSL' : 'IO::Socket::INET')->new(
			( ( %{$self->{ssl_options}} ) x !!$self->{ssl} ), 
                PeerAddr => $self->_get_server,
                PeerPort => $self->_get_port,
                Timeout  => $self->_get_timeout,
                Proto    => 'tcp',
                Blocking => 1,
                $self->_get_ssl_config,
            ) or die "failed connect or ssl handshake: $!,$IO::Socket::SSL::SSL_ERROR";
    $socket->sockopt(SO_KEEPALIVE, 1);
    return $socket;
}

sub _get_next_id {
    return ++$_[0]->{_cmd_id};
}

sub _socket_getline {
    local $/ = "\r\n";
    return $_[0]->_get_socket->getline;
}

sub _socket_write {
    my $self = shift;
    # open LOG, '>>:raw', '/tmp/net-imap-client.log';
    # print LOG @_;
    # close LOG;
    $self->_get_socket->write(@_);
}

sub _send_cmd {
    my ($self, $cmd, $args) = @_;

    local $\;
    use bytes;
    my $id   = $self->_get_next_id;
    if ($self->uid_mode && exists($UID_COMMANDS{$cmd})) {
        $cmd = "UID $cmd";
    }
    my @literals = ();
    if (ref $args eq 'ARRAY') {
        # may contain literals
        foreach (@$args) {
            if (ref $_ eq 'SCALAR') {
                push @literals, $_;
                $_ = '{' . length($$_) . "}\r\n";
            }
        }
        $args = join('', @$args);
    }
    my $socket = $self->_get_socket;
    if (@literals == 0) {
        $cmd = "NIC$id $cmd" . ($args ? " $args" : '') . "\r\n";
        $self->_socket_write($cmd);
    } else {
        $cmd = "NIC$id $cmd ";
        $self->_socket_write($cmd);
        my @split = split(/\r\n/, $args);

        my $ea = each_array(@split, @literals);
        while (my ($tmp, $lit) = $ea->()) {
            $self->_socket_write($tmp . "\r\n");
            my $line = $self->_socket_getline;
            # print STDERR "$line - $tmp\n";
            if ($line =~ /^\+/) {
                $self->_socket_write($$lit);
            } else {
                $self->{_error} = "Expected continuation, got: $line";
                # XXX: it's really bad if we get here, what to do?
                return undef;
            }
        }
        $self->_socket_write("\r\n"); # end of command!
    }
    $socket->flush;
    return "NIC$id";
}

sub _read_literal {
    my ($self, $count) = @_;

    my $buf;
    my @lines = ();
    my $sock = $self->_get_socket;
    # print STDERR "\033[1;31m ::: Reading $count bytes ::: \033[0m \n";
    while ($count > 0) {
        my $read = $sock->read($buf, min($count, $READ_BUFFER));
        # print STDERR "GOT $read / $buf";
        $count -= $read;
        last if !$read;
        push @lines, $buf;
    }

    $buf = join('', @lines);
    return \$buf;
}

sub _cmd_ok {
    my ($self, $res, $id) = @_;
    $id ||= $self->{_cmd_id};

    if ($res =~ /^NIC$id\s+OK/i) {
        return 1;
    } elsif ($res =~ /^NIC$id\s+(?:NO|BAD)(?:\s+(.+))?/i) {
        my $error = $1 || 'unknown error';
        $self->{_error} = $error;
        return 0;
    }
    return undef;
}

sub _cmd_ok2 {
    my ($self, $res) = @_;

    if ($res =~ /^(NIC\d+)\s+OK/i) {
        my $id = $1;
        return ($id, 1);
    } elsif ($res =~ /^(NIC\d+)\s+(?:NO|BAD)(?:\s+(.+))?/i) {
        my $id = $1;
        my $error = $2 || 'unknown error';
        $self->{_error} = $error;
        return ($id, 0, $error);
    }
    return ();
}

sub _reconnect_if_needed {
    my ($self, $force) = @_;
    if ($force || !$self->_get_socket->connected) {
        $self->{socket} = undef;
        $self->{greeting} = $self->_socket_getline;
        if ($self->login) {
            if ($self->{selected_folder}) {
                $self->select($self->{selected_folder});
            }
            return 1;
        }
        return undef;
    }
    return 0;
}

sub _tell_imap {
    my ($self, $cmd, $args, $do_notf) = @_;

    $cmd = uc $cmd;

    my ($lineparts, $ok, $res);

  RETRY1: {
        $self->_send_cmd($cmd, $args);
        redo RETRY1 if $self->_reconnect_if_needed;
    }

    $lineparts = [];      # holds results in boxes
    my $accumulator = []; # box for collecting results
    while ($res = $self->_socket_getline) {
        # print STDERR ">>>>$res<<<<<\n";

        if ($res =~ /^\*/) {

            # store previous box and start a new one

            push @$lineparts, $accumulator if @$accumulator;
            $accumulator = []; 
        }
        if ($res =~ /(.*)\{(\d+)\}\r\n/) {
            my ($line, $len) = ($1, $2 + 0);
            push @$accumulator,
              $line,
                $self->_read_literal($len);
        } else {
            $ok = $self->_cmd_ok($res);
            if (defined($ok)) {
                last;
            } else {
                push @$accumulator, $res;
            }
        }
    }
    # store last box
    push @$lineparts, $accumulator if @$accumulator;

    unless (defined $res) {
        goto RETRY1 if $self->_reconnect_if_needed(1);
    }

    if ($do_notf) {
        no warnings 'uninitialized';
        for (my $i = scalar(@$lineparts); --$i >= 0;) {
            my $line = $lineparts->[$i];

            # 1. notifications don't contain literals
            last if scalar(@$line) != 1;

            my $text = $line->[0];

            # 2. FETCH notifications only contain FLAGS.  We make a
            #    promise never to FETCH flags alone intentionally.

            # 3. Other notifications will have a first token different
            #    from the running command

            if ( $text =~ /^\*\s+\d+\s+FETCH\s*\(\s*FLAGS\s*\([^\)]*?\)\)/
                   || $text !~ /^\*\s+(?:\d+\s+)?$cmd/ ) {
                my $tokens = _parse_tokens($line);
                if ($self->_handle_notification($tokens, 1)) {
                    splice @$lineparts, $i, 1;
                }
                next;
            }

            last;
        }
    }

    return wantarray ? ($ok, $lineparts) : $ok ? $lineparts : undef;
}

# Variant of the above method that sends multiple commands.  After
# sending all commands to the server, it waits until all results are
# returned and puts them in an array, in the order the commands were
# sent.
sub _tell_imap2 {
    my ($self, @cmd) = @_;

    my %results;
    my @ids;

  RETRY2: {
        @ids = ();
        foreach (@cmd) {
            push @ids, $self->_send_cmd($_);
            redo RETRY2 if $self->_reconnect_if_needed;
        }
    }

    %results = ();
    for (0..$#cmd) {
        my $lineparts = [];
        my $accumulator = [];
        my $res;
        while ($res = $self->_socket_getline) {
            # print STDERR "2: $res";
            if ($res =~ /^\*/) {
                push @$lineparts, $accumulator if @$accumulator;
                $accumulator = []; 
            }
            if ($res =~ /(.*)\{(\d+)\}\r\n/) {
                my ($line, $len) = ($1, $2);
                push @$accumulator,
                  $line,
                    $self->_read_literal($len);
            } else {
                my ($cmdid, $ok, $error) = $self->_cmd_ok2($res);
                if (defined($ok)) {
                    $results{$cmdid} = [ $ok, $lineparts, $error ];
                    last;
                } else {
                    push @$accumulator, $res;
                }
            }
        }
        push @$lineparts, $accumulator if @$accumulator;
        unless (defined $res) {
            goto RETRY2 if $self->_reconnect_if_needed(1);
        }
    }

    my @ret = @results{@ids};
    return \@ret;
}

sub _string_quote {
    $_[0] =~ s/\\/\\\\/g;
    $_[0] =~ s/\"/\\\"/g;
    $_[0] = "\"$_[0]\"";
}

sub _string_unquote {
    if ($_[0] =~ s/^"//g) {
        $_[0] =~ s/"$//g;
        $_[0] =~ s/\\\"/\"/g;
        $_[0] =~ s/\\\\/\\/g;
    }
}

##### parse imap response #####
#
# This is probably the simplest/dumbest way to parse the IMAP output.
# Nevertheless it seems to be very stable and fast.
#
# $input is an array ref containing IMAP output.  Normally it will
# contain only one entry -- a line of text -- but when IMAP sends
# literal data, we read it separately (see _read_literal) and store it
# as a scalar reference, therefore it can be like this:
#
#    [ '* 11 FETCH (RFC822.TEXT ', \$DATA, ')' ]
#
# so that's why the routine looks a bit more complicated.
#
# It returns an array of tokens.  Literal strings are dereferenced so
# for the above text, the output will be:
#
#    [ '*', '11', 'FETCH', [ 'RFC822.TEXT', $DATA ] ]
#
# note that lists are represented as arrays.
#
sub _parse_tokens {
    my ($input, $no_deref) = @_;

    my @tokens = ();
    my @stack = (\@tokens);

    while (my $text = shift @$input) {
        if (ref $text) {
            push @{$stack[-1]}, ($no_deref ? $text : $$text);
            next;
        }
        while (1) {
            $text =~ m/\G\s+/gc;
            if ($text =~ m/\G[([]/gc) {
                my $sub = [];
                push @{$stack[-1]}, $sub;
                push @stack, $sub;
            } elsif ($text =~ m/\G(BODY\[[a-zA-Z0-9._() -]*\])/gc) {
                push @{$stack[-1]}, $1; # let's consider this an atom too
            } elsif ($text =~ m/\G[])]/gc) {
                pop @stack;
            } elsif ($text =~ m/\G\"((?:\\.|[^\"\\])*)\"/gc) {
                my $str = $1;
                # unescape
                $str =~ s/\\\"/\"/g;
                $str =~ s/\\\\/\\/g;
                push @{$stack[-1]}, $str; # found string
            } elsif ($text =~ m/\G(\d+)/gc) {
                push @{$stack[-1]}, $1 + 0; # found numeric
            } elsif ($text =~ m/\G([a-zA-Z0-9_\$\\.+\/*&-]+)/gc) {
                my $atom = $1;
                if (lc $atom eq 'nil') {
                    $atom = undef;
                }
                push @{$stack[-1]}, $atom; # found atom
            } else {
                last;
            }
        }
    }

    return \@tokens;
}

sub _handle_notification {
    my ($self, $tokens, $reverse) = @_;

    no warnings 'uninitialized';
    my $not;

    my $sf = $self->{selected_folder};
    if ($sf) { # otherwise we shouldn't get any notifications, but whatever
        $sf = $self->{FOLDERS}{$sf};
        if ($tokens->[2] eq 'FETCH') {
            my %data = @{$tokens->[3]};
            if (my $flags = $data{FLAGS}) {
                $not = { seq   => $tokens->[1] + 0,
                         flags => $flags };
                if (first { $_ eq '\\Deleted' } @$flags) {
                    --$sf->{messages};
                    $not->{deleted} = 1;
                }
                if ($data{UID}) {
                    $not->{uid} = $data{UID};
                }
            }

        } elsif ($tokens->[2] eq 'EXISTS') {
            $sf->{messages} = $tokens->[1] + 0;
            $not = { messages => $tokens->[1] + 0 };

        } elsif ($tokens->[2] eq 'EXPUNGE') {
            --$sf->{messages};
            $not = { seq => $tokens->[1] + 0, destroyed => 1 };

        } elsif ($tokens->[2] eq 'RECENT') {
            $sf->{recent} = $tokens->[1] + 0;
            $not = { recent => $tokens->[1] + 0 };

        } elsif ($tokens->[1] eq 'FLAGS') {
            $sf->{flags} = $tokens->[2];
            $not = { flags => $tokens->[2] };

        } elsif ($tokens->[1] eq 'OK') {
            $sf->{sflags}{$tokens->[2][0]} = $tokens->[2][1];
        }
    }

    if (defined $not) {
        $not->{folder} = $self->{selected_folder};
        if ($reverse) {
            unshift @{$self->{notifications}}, $not;
        } else {
            push @{$self->{notifications}}, $not;
        }
        return 1;
    }

    return 0;
}

1;

__END__

=pod

=encoding utf8

=head1 NAME

Net::IMAP::Client - Not so simple IMAP client library

=head1 SYNOPSIS

    use Net::IMAP::Client;

    my $imap = Net::IMAP::Client->new(

        server => 'mail.you.com',
        user   => 'USERID',
        pass   => 'PASSWORD',
        ssl    => 1,                              # (use SSL? default no)
        ssl_verify_peer => 1,                     # (use ca to verify server, default yes)
        ssl_ca_file => '/etc/ssl/certs/certa.pm', # (CA file used for verify server) or
      # ssl_ca_path => '/etc/ssl/certs/',         # (CA path used for SSL)
        port   => 993                             # (but defaults are sane)

    ) or die "Could not connect to IMAP server";

    # everything's useless if you can't login
    $imap->login or
      die('Login failed: ' . $imap->last_error);

    # let's see what this server knows (result cached on first call)
    my $capab = $imap->capability;
       # or
    my $knows_sort = $imap->capability( qr/^sort/i );

    # get list of folders
    my @folders = $imap->folders;

    # get total # of messages, # of unseen messages etc. (fast!)
    my $status = $imap->status(@folders); # hash ref!

    # select folder
    $imap->select('INBOX');

    # get folder hierarchy separator (cached at first call)
    my $sep = $imap->separator;

    # fetch all message ids (as array reference)
    my $messages = $imap->search('ALL');

    # fetch all ID-s sorted by subject
    my $messages = $imap->search('ALL', 'SUBJECT');
       # or
    my $messages = $imap->search('ALL', [ 'SUBJECT' ]);

    # fetch ID-s that match criteria, sorted by subject and reverse date
    my $messages = $imap->search({
        FROM    => 'foo',
        SUBJECT => 'bar',
    }, [ 'SUBJECT', '^DATE' ]);

    # fetch message summaries (actually, a lot more)
    my $summaries = $imap->get_summaries([ @msg_ids ]);

    foreach (@$summaries) {
        print $_->uid, $_->subject, $_->date, $_->rfc822_size;
        print join(', ', @{$_->from}); # etc.
    }

    # fetch full message
    my $data = $imap->get_rfc822_body($msg_id);
    print $$data; # it's reference to a scalar

    # fetch full messages
    my @msgs = $imap->get_rfc822_body([ @msg_ids ]);
    print $$_ for (@msgs);

    # fetch single attachment (message part)
    my $data = $imap->get_part_body($msg_id, '1.2');

    # fetch multiple attachments at once
    my $hash = $imap->get_parts_bodies($msg_id, [ '1.2', '1.3', '2.2' ]);
    my $part1_2 = $hash->{'1.2'};
    my $part1_3 = $hash->{'1.3'};
    my $part2_2 = $hash->{'2.2'};
    print $$part1_2;              # need to dereference it

    # copy messages between folders
    $imap->select('INBOX');
    $imap->copy(\@msg_ids, 'Archive');

    # delete messages ("Move to Trash")
    $imap->copy(\@msg_ids, 'Trash');
    $imap->add_flags(\@msg_ids, '\\Deleted');
    $imap->expunge;

=head1 DESCRIPTION

Net::IMAP::Client provides methods to access an IMAP server.  It aims
to provide a simple and clean API, while employing a rigorous parser
for IMAP responses in order to create Perl data structures from them.
The code is simple, clean and extensible.

It started as an effort to improve L<Net::IMAP::Simple> but then I
realized that I needed to change a lot of code and API so I started it
as a fresh module.  Still, the design is influenced by
Net::IMAP::Simple and I even stole a few lines of code from it ;-)
(very few, honestly).

This software was developed for creating a web-based email (IMAP)
client: www.xuheki.com.  Xhueki uses Net::IMAP::Client.

=head1 API REFERENCE

Unless otherwise specified, if a method fails it returns I<undef> and
you can inspect the error by calling $imap->last_error.  For a
successful call most methods will return a meaningful value but
definitely not I<undef>.

=head2 new(%args)  # constructor

    my $imap = Net::IMAP::Client->new(%args);

Pass to the constructor a hash of arguments that can contain:

=over

=item - B<server> (STRING)

Host name or IP of the IMAP server.

=item - B<user> (STRING)

User ID (I<only "clear" login is supported for now!>)

=item - B<pass> (STRING)

Password

=item - B<ssl> (BOOL, optional, default FALSE)

Pass a true value if you want to use IO::Socket::SSL

=item - B<ssl_verify_peer> (BOOL, optional, default TRUE)

Pass a false value if you do not want to use SSL CA to verify server

only need when you set ssl to true

=item - B<ssl_ca_file> (STRING, optional)

Pass a file path which used as CA file to verify server

at least one of ssl_ca_file and ssl_ca_path is needed for ssl verify
 server

=item -B<ssl_ca_path> (STRING, optional)

Pass a dir which will be used as CA file search dir, found CA file
will be used to verify server

On linux, by default is '/etc/ssl/certs/'

at least one of ssl_ca_file and ssl_ca_path is needed for ssl verify
 server

=item - B<ssl_options> (HASHREF, optional)

Optional arguments to be passed to the L<IO::Socket::SSL> object.

=item - B<uid_mode> (BOOL, optional, default TRUE)

Whether to use UID command (see RFC3501).  Recommended.

=item - B<socket> (IO::Handle, optional)

If you already have a socket connected to the IMAP server, you can
pass it here.

=back

The B<ssl_ca_file> and B<ssl_ca_path> only need when you set
B<ssl_verify_peer> to TRUE.

If you havn't apply an B<ssl_ca_file> and B<ssl_ca_path>, on linux,
the B<ssl_ca_path> will use the value '/etc/ssl/certs/', on other
platform B<ssl_verify_peer> will be disabled.

The constructor doesn't login to the IMAP server -- you need to call
$imap->login for that.

=head2 last_error

Returns the last error from the IMAP server.

=head2 login($user, $pass)

Login to the IMAP server.  You can pass $user and $pass here if you
wish; if not passed, the values used in constructor will be used.

Returns I<undef> if login failed.

=head2 logout / quit

Send EXPUNGE and LOGOUT then close connection.  C<quit> is an alias
for C<logout>.

=head2 noop

"Do nothing" method that calls the IMAP "NOOP" command.  It returns a
true value upon success, I<undef> otherwise.

This method fetches any notifications that the server might have for
us and you can get them by calling $imap->notifications.  See the
L</notifications()> method.

=head2 capability() / capability(qr/^SOMETHING/)

With no arguments, returns an array of all capabilities advertised by
the server.  If you're interested in a certain capability you can pass
a RegExp.  E.g. to check if this server knows 'SORT', you can do this:

    if ($imap->capability(/^sort$/i)) {
        # speaks it
    }

This data is cached, the server will be only hit once.

=head2 select($folder)

Selects the current IMAP folder.  On success this method also records
some information about the selected folder in a hash stored in
$self->{FOLDERS}{$folder}.  You might want to use Data::Dumper to find
out exactly what, but at the time of this writing this is:

=over

=item - B<messages>

Total number of messages in this folder

=item - B<flags>

Flags available for this folder (as array ref)

=item - B<recent>

Total number of recent messages in this folder

=item - B<sflags>

Various other flags here, such as PERMANENTFLAGS of UIDVALIDITY.  You
might want to take a look at RFC3501 at this point. :-p

=back

This method is basically stolen from Net::IMAP::Simple.

=head2 examine($folder)

Selects the current IMAP folder in read-only (EXAMINE) mode.
Otherwise identical to select.
 
=head2 status($folder), status(\@folders)

Returns the status of the given folder(s).

If passed an array ref, the return value is a hash ref mapping folder
name to folder status (which are hash references in turn).  If passed
a single folder name, it returns the status of that folder only.

    my $inbox = $imap->status('INBOX');
    print $inbox->{UNSEEN}, $inbox->{MESSAGES};
    print Data::Dumper::Dumper($inbox);

    my $all = $imap->status($imap->folders);
    while (my ($name, $status) = each %$all) {
        print "$name : $status->{MESSAGES}/$status->{UNSEEN}\n";
    }

This method is designed to be very fast when passed multiple folders.
It's I<a lot> faster to call:

    $imap->status(\@folders);

than:

    $imap->status($_) foreach (@folders);

because it sends all the STATUS requests to the IMAP server before it
starts receiving the answers.  In my tests with my remote IMAP server,
for 40 folders this method takes 0.6 seconds, compared to 6+ seconds
when called individually for each folder alone.

=head2 separator

Returns the folder hierarchy separator.  This is provided as a result
of the following IMAP command:

    FETCH "" "*"

I don't know of any way to change this value on a server so I have to
assume it's a constant.  Therefore, this method caches the result and
it won't hit the server a second time on subsequent calls.

=head2 folders

Returns a list of all folders available on the server.  In scalar
context it returns a reference to an array, i.e.:

    my @a = $imap->folders;
    my $b = $imap->folders;
    # now @a == @$b;

=head2 folders_more

Returns an hash reference containing more information about folders.
It maps folder name to an hash ref containing the following:

  - flags -- folder flags (array ref; i.e. [ '\\HasChildren' ])
  - sep   -- one character containing folder hierarchy separator
  - name  -- folder name (same as the key -- thus redundant)

=head2 namespace

Returns an hash reference containing the namespaces for this server
(see RFC 2342).  Since the RFC defines 3 possible types of namespaces,
the hash contains the following keys:

 - `personal' -- the personal namespace
 - `other' -- "other users" namespace
 - `shared' -- shared namespace

Each one can be I<undef> if the server returned "NIL", or an array
reference.  If an array reference, each element is in the form:

 {
    sep    => '.',
    prefix => 'INBOX.'
 }

(I<sep> is the separator for this hierarchy, and I<prefix> is the
prefix).

=head2 seq_to_uid(@sequence_ids)

I recommend usage of UID-s only (see L</uid_mode>) but this isn't
always possible.  Even when C<uid_mode> is on, the server will
sometimes return notifications that only contain message sequence
numbers.  To convert these to UID-s you can use this method.

On success it returns an hash reference which maps sequence numbers to
message UID-s.  Of course, on failure it returns I<undef>.

=head2 search($criteria, $sort, $charset)

Executes the "SEARCH" or "SORT" IMAP commands (depending on wether
$sort is I<undef>) and returns the results as an array reference
containing message ID-s.

Note that if you use C<$sort> and the IMAP server doesn't have this
capability, this method will fail.  Use L</capability> to investigate.

=over

=item - B<$criteria>

Can be a string, in which case it is passed literally to the IMAP
command (which can be "SEARCH" or "SORT").

It can also be an hash reference, in which case keys => values are
collected into a string and values are properly quoted, i.e.:

   { subject => 'foo',
     from    => 'bar' }

will translate to:

   'SUBJECT "foo" FROM "bar"'

which is a valid IMAP SEARCH query.

If you want to retrieve all messages (no search criteria) then pass
'ALL' here.

=item - B<$sort>

Can be a string or an array reference.  If it's an array, it will
simply be joined with a space, so for instance passing the following
is equivalent:

    'SUBJECT DATE'
    [ 'SUBJECT', 'DATE' ]

The SORT command in IMAP allows you to prefix a sort criteria with
'REVERSE' which would mean descending sorting; this module will allow
you to prefix it with '^', so again, here are some equivalent
constructs:

    'SUBJECT REVERSE DATE'
    'SUBJECT ^DATE'
    [ 'SUBJECT', 'REVERSE', 'DATE' ]
    [ 'subject', 'reverse date' ]
    [ 'SUBJECT', '^DATE' ]

It'll also uppercase whatever you passed here.

If you omit $sort (or pass I<undef>) then this method will use the
SEARCH command.  Otherwise it uses the SORT command.

=item - B<$charset>

The IMAP SORT recommendation [2] requires a charset declaration for
SORT, but not for SEARCH.  Interesting, huh?

Our module is a bit more paranoid and it will actually add charset for
both SORT and SEARCH.  If $charset is omitted (or I<undef>) the it
will default to "UTF-8", which, supposedly, is supported by all IMAP
servers.

=back

=head2 get_rfc822_body($msg_id)

Fetch and return the full RFC822 body of the message.  B<$msg_id> can
be a scalar but also an array of ID-s.  If it's an array, then all
bodies of those messages will be fetched and the return value will be
a list or an array reference (depending how you call it).

Note that the actual data is returned as a reference to a scalar, to
speed things up.

Examples:

    my $data = $imap->get_rfc822_body(10);
    print $$data;   # need to dereference it

    my @more = $imap->get_rfc822_body([ 11, 12, 13 ]);
    print $$_ foreach @more;

        or

    my $more = $imap->get_rfc822_body([ 11, 12, 13 ]);
    print $$_ foreach @$more;

=head2 get_part_body($msg_id, $part_id)

Fetches and returns the body of a certain part of the message.  Part
ID-s look like '1' or '1.1' or '2.3.1' etc. (see RFC3501 [1], "FETCH
Command").

=head3 Scalar reference

Note that again, this data is returned as a reference to a scalar
rather than the scalar itself.  This decision was taken purely to save
some time passing around potentially large data from Perl subroutines.

=head3 Undecoded

One other thing to note is that the data is not decoded.  One simple
way to decode it is use Email::MIME::Encodings, i.e.:

    use Email::MIME::Encodings;
    my $summary = $imap->get_summaries(10)->[0];
    my $part = $summary->get_subpart('1.1');
    my $body = $imap->get_part_body('1.1');
    my $cte = $part->transfer_encoding;  # Content-Transfer-Encoding
    $body = Email::MIME::Encodings::decode($cte, $$body);

    # and now you should have the undecoded (perhaps binary) data.

See get_summaries below.

=head2 get_parts_bodies($msg_id, \@part_ids)

Similar to get_part_body, but this method is capable to retrieve more
parts at once.  It's of course faster than calling get_part_body for
each part alone.  Returns an hash reference which maps part ID to part
body (the latter is a reference to a scalar containing the actual
data).  Again, the data is not unencoded.

    my $parts = $imap->get_parts_bodies(10, [ '1.1', '1.2', '2.1' ]);
    print ${$parts->{'1.1'}};

=head2 get_summaries($msg, $headers) / get_summaries(\@msgs, $headers)

(C<$headers> is optional).

Fetches, parses and returns "message summaries".  $msg can be an array
ref, or a single id.  The return value is always an array reference,
even if a single message is queried.

If $headers is passed, it must be a string containing name(s) of the
header fields to fetch (space separated).  Example:

    $imap->get_summaries([1, 2, 3], 'References X-Original-To')

The result contains L<Net::IMAP::Client::MsgSummary> objects.  The
best way to understand the result is to actually call this function
and use Data::Dumper to see its structure.

Following is the output for a pretty complicated message, which
contains an HTML part with an embedded image and an attached message.
The attached message in turn contains an HTML part and an embedded
message.

  bless( {
    'message_id' => '<48A71D17.1000109@foobar.com>',
    'date' => 'Sat, 16 Aug 2008 21:31:51 +0300',
    'to' => [
        bless( {
            'at_domain_list' => undef,
            'name' => undef,
            'mailbox' => 'kwlookup',
            'host' => 'foobar.com'
        }, 'Net::IMAP::Client::MsgAddress' )
    ],
    'cc' => undef,
    'from' => [
        bless( {
            'at_domain_list' => undef,
            'name' => 'Mihai Bazon',
            'mailbox' => 'justme',
            'host' => 'foobar.com'
        }, 'Net::IMAP::Client::MsgAddress' )
    ],
    'flags' => [
        '\\Seen',
        'NonJunk',
        'foo_bara'
    ],
    'uid' => '11',
    'subject' => 'test with message attachment',
    'rfc822_size' => '12550',
    'in_reply_to' => undef,
    'bcc' => undef,
    'internaldate' => '16-Aug-2008 21:29:23 +0300',
    'reply_to' => [
        bless( {
            'at_domain_list' => undef,
            'name' => 'Mihai Bazon',
            'mailbox' => 'justme',
            'host' => 'foobar.com'
        }, 'Net::IMAP::Client::MsgAddress' )
    ],
    'sender' => [
        bless( {
            'at_domain_list' => undef,
            'name' => 'Mihai Bazon',
            'mailbox' => 'justme',
            'host' => 'foobar.com'
        }, 'Net::IMAP::Client::MsgAddress' )
    ],
    'parts' => [
        bless( {
            'part_id' => '1',
            'parts' => [
                bless( {
                    'parameters' => {
                        'charset' => 'UTF-8'
                    },
                    'subtype' => 'html',
                    'part_id' => '1.1',
                    'encoded_size' => '365',
                    'cid' => undef,
                    'type' => 'text',
                    'description' => undef,
                    'transfer_encoding' => '7bit'
                }, 'Net::IMAP::Client::MsgSummary' ),
                bless( {
                    'disposition' => {
                        'inline' => {
                            'filename' => 'someimage.png'
                        }
                    },
                    'language' => undef,
                    'encoded_size' => '4168',
                    'description' => undef,
                    'transfer_encoding' => 'base64',
                    'parameters' => {
                        'name' => 'someimage.png'
                    },
                    'subtype' => 'png',
                    'part_id' => '1.2',
                    'type' => 'image',
                    'cid' => '<part1.02030404.05090202@foobar.com>',
                    'md5' => undef
                }, 'Net::IMAP::Client::MsgSummary' )
            ],
            'multipart_type' => 'related'
        }, 'Net::IMAP::Client::MsgSummary' ),
        bless( {
            'message_id' => '<48A530CE.3050807@foobar.com>',
            'date' => 'Fri, 15 Aug 2008 10:31:26 +0300',
            'encoded_size' => '6283',
            'to' => [
                bless( {
                    'at_domain_list' => undef,
                    'name' => undef,
                    'mailbox' => 'kwlookup',
                    'host' => 'foobar.com'
                }, 'Net::IMAP::Client::MsgAddress' )
            ],
            'subtype' => 'rfc822',
            'cc' => undef,
            'from' => [
                bless( {
                    'at_domain_list' => undef,
                    'name' => 'Mihai Bazon',
                    'mailbox' => 'justme',
                    'host' => 'foobar.com'
                }, 'Net::IMAP::Client::MsgAddress' )
            ],
            'subject' => 'Test with images',
            'in_reply_to' => undef,
            'description' => undef,
            'transfer_encoding' => '7bit',
            'parameters' => {
                'name' => 'Attached Message'
            },
            'bcc' => undef,
            'part_id' => '2',
            'sender' => [
                bless( {
                    'at_domain_list' => undef,
                    'name' => 'Mihai Bazon',
                    'mailbox' => 'justme',
                    'host' => 'foobar.com'
                }, 'Net::IMAP::Client::MsgAddress' )
            ],
            'reply_to' => [
                bless( {
                    'at_domain_list' => undef,
                    'name' => 'Mihai Bazon',
                    'mailbox' => 'justme',
                    'host' => 'foobar.com'
                }, 'Net::IMAP::Client::MsgAddress' )
            ],
            'parts' => [
                bless( {
                    'parameters' => {
                        'charset' => 'UTF-8'
                    },
                    'subtype' => 'html',
                    'part_id' => '2.1',
                    'encoded_size' => '344',
                    'cid' => undef,
                    'type' => 'text',
                    'description' => undef,
                    'transfer_encoding' => '7bit'
                }, 'Net::IMAP::Client::MsgSummary' ),
                bless( {
                    'disposition' => {
                        'inline' => {
                            'filename' => 'logo.png'
                        }
                    },
                    'language' => undef,
                    'encoded_size' => '4578',
                    'description' => undef,
                    'transfer_encoding' => 'base64',
                    'parameters' => {
                        'name' => 'logo.png'
                    },
                    'subtype' => 'png',
                    'part_id' => '2.2',
                    'type' => 'image',
                    'cid' => '<part1.02060209.09080406@foobar.com>',
                    'md5' => undef
                }, 'Net::IMAP::Client::MsgSummary' )
            ],
            'cid' => undef,
            'type' => 'message',
            'multipart_type' => 'related'
        }, 'Net::IMAP::Client::MsgSummary' )
    ],
    'multipart_type' => 'mixed'
  }, 'Net::IMAP::Client::MsgSummary' );

As you can see, the parser retrieves all data, including from the
embedded messages.

There are many other modules you can use to fetch such information.
L<Email::Simple> and L<Email::MIME> are great.  The only problem is
that you have to have fetched already the full (RFC822) body of the
message, which is impractical over IMAP.  When you want to quickly
display a folder summary, the only practical way is to issue a FETCH
command and retrieve only those headers that you are interested in
(instead of full body).  C<get_summaries> does exactly that (issues a
FETCH (FLAGS INTERNALDATE RFC822.SIZE ENVELOPE BODYSTRUCTURE)).  It's
acceptably fast even for huge folders.

=head2 fetch($msg_id, $attributes)

This is a low level interface to FETCH.  It calls the imap FETCH
command and returns a somewhat parsed hash of the results.

C<$msg_id> can be a single message ID or an array of IDs.  If a single
ID is given, the return value will be a hash reference containing the
requested values.  If C<$msg_id> is an array, even if it contains a
single it, then the return value will be an array of hashes.

C<$attributes> is a string of attributes to FETCH, separated with a
space, or an array (ref) of attributes.

Examples:

# retrieve the UID of the most recent message

    my $last_uid = $imap->fetch('*', 'UID')->{UID};

# fetch the flags of the first message

    my $flags = $imap->fetch(1, 'FLAGS')->{FLAGS};

# fetch flags and some headers (Subject and From)

    my $headers = 'BODY[HEADER.FIELDS (Subject From)]';
    my $results = $imap->fetch([1, 2, 3], "FLAGS $headers");
    foreach my $hash (@$results) {
        print join(" ", @{$hash->{FLAGS}}), "\n";
        print $hash->{$headers}, "\n";
    }

=head2 notifications()

The IMAP server may send various notifications upon execution of
commands.  They are collected in an array which is returned by this
method (returns an array ref in scalar context, or a list otherwise).
It clears the notifications queue so on second call it will return an
empty array (unless new notifications were collected in the meantime).

Each element in this array (notification) is a hash reference
containing one or more or the following:

  - seq       : the *sequence number* of the changed message
  - uid       : UID of the changed message (NOT ALWAYS available!)
  - flags     : new flags for this message
  - deleted   : when the \Deleted flag was set for this message
  - messages  : new number of messages in this folder
  - recent    : number of recent messages in this folder
  - flags     : new flags of this folder (seq is missing)
  - destroyed : when this message was expunged
  - folder    : the name of the selected folder

C<folder> is always present.  C<seq> is present when a message was
changed some flags (in which case you have C<flags>) or was expunged
(in which case C<destroyed> is true).  When C<flags> were changed and
the B<\Deleted> flag is present, you also get C<deleted> true.

C<seq> is a message sequence number.  Pretty dumb, I think it's
preferable to work with UID-s, but that's what the IMAP server
reports.  In some cases the UID I<might> be readily available (i.e. my
IMAP server sends notifications in the same body as a response to,
say, a FETCH BODY command), but when it's not, you have to rely on
seq_to_uid().  B<Note> that when C<destroyed> is true, the message has
been B<expunged>; there is no way in this case to retrieve the UID so
you have to rely solely on C<seq> in order to update your caches.

When C<flags> is present but no C<seq>, it means that the list of
available flags for the C<folder> has changed.

You get C<messages> upon an "EXISTS" notification, which usually means
"you have new mail".  It indicates the total number of messages in the
folder, not just "new" messages.  I've yet to come up with a good way
to measure the number of new/unseen messages, other than calling
C<status($folder)>.

I rarely got C<recent> from my IMAP server in my tests; if more
clients are simultaneously logged in as the same IMAP user, only one
of them will receive "RECENT" notifications; others will have to rely
on "EXISTS" to tell when new messages have arrived.  Therefore I can
only say that "RECENT" is useless and I advise you to ignore it.

=head2 append($folder, \$rfc822, $flags, $date)

Appends a message to the given C<$folder>.  You must pass the full
RFC822 body in C<$rfc822>.  C<$flags> and C<$date> are optional.  If
you pass C<$flags>, it must be an array of strings specifying the
initial flags of the appended message.  If I<undef>, the message will
be appended with an empty flag set, which amongst other things means
that it will be regarded as an C<\Unseen> message.

C<$date> specifies the INTERNALDATE of the appended messge.  If
I<undef> it will default to the current date/time.  B<NOTE:> this
functionality is not tested; C<$date> should be in a format understood
by IMAP.

=head2 get_flags($msg_id) / get_flags(\@msg_ids)

Returns the flags of one or more messages.  The return value is an
array (reference) if one message ID was passed, or a hash reference if
an array (of one or more) message ID-s was passed.

When an array was passed, the returned hash will map each message UID
to an array of flags.

=head2 store($msg, $flag) / store(\@msgs, \@flags)

Resets FLAGS of the given message(s) to the given flag(s).  C<$msg>
can be an array of ID-s (or UID-s), or a single (U)ID.  C<$flags> can
be a single string, or an array reference as well.

Note that the folder where these messages reside must have been
already selected.

Examples:

    $imap->store(10, '\\Seen');
    $imap->store([11, 12], '\\Deleted');
    $imap->store(13, [ '\\Seen', '\\Answered' ]);

The IMAP specification defines certain reserved flags (they all start
with a backslash).  For example, a message with the flag C<\Deleted>
should be regarded as deleted and will be permanently discarded by an
EXPUNGE command.  Although, it is possible to "undelete" a message by
removing this flag.

The following reserved flags are defined by the IMAP spec:

    \Seen
    \Answered
    \Flagged
    \Deleted
    \Draft
    \Recent

The C<\Recent> flag is considered "read-only" -- you cannot add or
remove it manually; the server itself will do this as appropriate.

=head2 add_flags($msg, $flag) / add_flags(\@msgs, \@flags)

Like store() but it doesn't reset all flags -- it just specifies which
flags to B<add> to the message.

=head2 del_flags($msg, $flag) / del_flags(\@msgs, \@flags)

Like store() / add_flags() but it B<removes> flags.

=head2 delete_message($msg) / delete_message(\@msgs)

Stores the \Deleted flag on the given message(s).  Equivalent to:

    $imap->add_flags(\@msgs, '\\Deleted');

=head2 expunge()

Permanently removes messages that have the C<\Deleted> flag set from
the current folder.

=head2 copy($msg, $folder) / copy(\@msg_ids, $folder)

Copies message(s) from the selected folder to the given C<$folder>.
You can pass a single message ID, or an array of message ID-s.

=head2 create_folder($folder)

Creates the folder with the given name.

=head2 delete_folder($folder)

Deletes the folder with the given name.  This works a bit different
from the IMAP specs.  The IMAP specs says that any subfolders should
remain intact.  This method actually deletes subfolders recursively.
Most of the time, this is What You Want.

Note that all messages in C<$folder>, as well as in any subfolders,
are permanently lost.

=head2 get_threads($algorithm, $msg_id)

Returns a "threaded view" of the current folder.  Both arguments are
optional.

C<$algorithm> should be I<undef>, "REFERENCES" or "SUBJECT".  If
undefined, "REFERENCES" is assumed.  This selects the threading
algorithm, as per IMAP THREAD AND SORT extensions specification.  I
only tested "REFERENCES".

C<$msg_id> can be undefined, or a message ID.  If it's undefined, then
a threaded view of the whole folder will be returned.  If you pass a
message ID, then this method will return the top-level thread that
contains the message.

The return value is an array which actually represents threads.
Elements of this array are message ID-s, or other arrays (which in
turn contain message ID-s or other arrays, etc.).  The first element
in an array will represent the start of the thread.  Subsequent
elements are child messages or subthreads.

An example should help (FIXME).

=head1 TODO

=over

=item - authentication schemes other than plain text (B<help wanted>)

=item - better error handling?

=back

=head1 SEE ALSO

L<Net::IMAP::Simple>, L<Mail::IMAPClient>, L<Mail::IMAPTalk>

L<Email::Simple>, L<Email::MIME>

RFC3501 [1] is a must read if you want to do anything fancier than
what this module already supports.

=head1 REFERENCES

[1] http://ietfreport.isoc.org/rfc/rfc3501.txt

[2] http://ietfreport.isoc.org/all-ids/draft-ietf-imapext-sort-20.txt

=head1 AUTHOR

Mihai Bazon, <mihai.bazon@gmail.com>
    http://www.xuheki.com/
    http://www.dynarchlib.com/
    http://www.bazon.net/mishoo/

=head1 COPYRIGHT

Copyright (c) Mihai Bazon 2008.  All rights reserved.

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 DISCLAIMER OF WARRANTY

BECAUSE THIS SOFTWARE IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT
WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER
PARTIES PROVIDE THE SOFTWARE "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
SOFTWARE IS WITH YOU. SHOULD THE SOFTWARE PROVE DEFECTIVE, YOU ASSUME
THE COST OF ALL NECESSARY SERVICING, REPAIR, OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE SOFTWARE AS PERMITTED BY THE ABOVE LICENCE, BE LIABLE
TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL, OR
CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE
SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING
RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A
FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER SOFTWARE), EVEN IF
SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.

=cut
