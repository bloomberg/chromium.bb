# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Log::Report;
use vars '$VERSION';
$VERSION = '1.28';

use base 'Exporter';

use warnings;
use strict;

use List::Util         qw/first/;
use Scalar::Util       qw/blessed/;

use Log::Report::Util;
my $lrm = 'Log::Report::Message';

### if you change anything here, you also have to change Log::Report::Minimal
my @make_msg   = qw/__ __x __n __nx __xn N__ N__n N__w __p __px __np __npx/;
my @functions  = qw/report dispatcher try textdomain/;
my @reason_functions = qw/trace assert info notice warning
   mistake error fault alert failure panic/;

our @EXPORT_OK = (@make_msg, @functions, @reason_functions);

sub _whats_needed(); sub dispatcher($@); sub textdomain(@);
sub trace(@); sub assert(@); sub info(@); sub notice(@); sub warning(@);
sub mistake(@); sub error(@); sub fault(@); sub alert(@); sub failure(@);
sub panic(@);
sub __($); sub __x($@); sub __n($$$@); sub __nx($$$@); sub __xn($$$@);
sub N__($); sub N__n($$); sub N__w(@);
sub __p($$); sub __px($$@); sub __np($$$$); sub __npx($$$$@);

#
# Some initiations
#

my $reporter     = {};
my $default_mode = 0;
my @nested_tries;

# we can only load these after Log::Report has compiled, because
# they use this module themselves as well.

require Log::Report::Die;
require Log::Report::Domain;
require Log::Report::Message;
require Log::Report::Exception;
require Log::Report::Dispatcher;
require Log::Report::Dispatcher::Try;

textdomain 'log-report';

my $default_dispatcher = dispatcher PERL => 'default', accept => 'NOTICE-';


sub report($@)
{   my $opts = ref $_[0] eq 'HASH' ? +{ %{ (shift) } } : {};
    my $reason = shift;
    my $stop = exists $opts->{is_fatal} ? $opts->{is_fatal} : is_fatal $reason;

    my @disp;
    if(defined(my $try = $nested_tries[-1]))
    {   push @disp, @{$reporter->{needs}{$reason}||[]}
            unless $stop || $try->hides($reason);
        push @disp, $try if $try->needs($reason);
    }
    else
    {   @disp = @{$reporter->{needs}{$reason} || []};
    }

    is_reason $reason
        or error __x"token '{token}' not recognized as reason", token=>$reason;

    # return when no-one needs it: skip unused trace() fast!
    @disp || $stop
        or return;

    $opts->{errno} ||= $!+0 || $? || 1
        if use_errno($reason) && !defined $opts->{errno};

    if(my $to = delete $opts->{to})
    {   # explicit destination, still disp may not need it.
        if(ref $to eq 'ARRAY')
        {   my %disp = map +($_->name => $_), @disp;
            @disp    = grep defined, @disp{@$to};
        }
        else
        {   @disp    = grep $_->name eq $to, @disp;
        }
        @disp || $stop
            or return;
    }

    my $message = shift;

    unless(Log::Report::Dispatcher->can('collectLocation'))
    {   # internal Log::Report error can result in "deep recursions".
        eval "require Carp"; Carp::confess($message);
    }
    $opts->{location} ||= Log::Report::Dispatcher->collectLocation;

    my $exception;
    if(!blessed $message)
    {   # untranslated message into object
        @_%2 and error __x"odd length parameter list with '{msg}'", msg => $message;
        $message  = $lrm->new(_prepend => $message, @_);
    }
    elsif($message->isa('Log::Report::Exception'))
    {   $exception = $message;
        $message   = $exception->message;
    }
    elsif($message->isa('Log::Report::Message'))
    {   @_==0 or error __x"a message object is reported with more parameters";
    }
    else
    {   # foreign object
        my $text = "$message";  # hope stringification is overloaded
		$text    =~ s/\s*$//gs;
        @_%2 and error __x"odd length parameter list with object '{msg}'",
             msg => $text;
        $message = $lrm->new(_prepend => $text, @_);
    }

    if(my $to = $message->to)
    {   @disp = grep $_->name eq $to, @disp;
        @disp or return;
    }

    my $domain = $message->domain;
    if(my $filters = $reporter->{filters})
    {
      DISPATCHER:
        foreach my $d (@disp)
        {   my ($r, $m) = ($reason, $message);
            foreach my $filter (@$filters)
            {   next if keys %{$filter->[1]} && !$filter->[1]{$d->name};
                ($r, $m) = $filter->[0]->($d, $opts, $r, $m, $domain);
                $r or next DISPATCHER;
            }
            $d->log($opts, $r, $m, $domain);
        }
    }
    else
    {   $_->log($opts, $reason, $message, $domain) for @disp;
    }

    if($stop)
    {   # $^S = $EXCEPTIONS_BEING_CAUGHT; parse: undef, eval: 1, else 0
        (defined($^S) ? $^S : 1) or exit($opts->{errno} || 0);

        $! = $opts->{errno} || 0;
        $@ = $exception || Log::Report::Exception->new(report_opts => $opts
          , reason => $reason, message => $message);
        die;   # $@->PROPAGATE() will be called, some eval will catch this
    }

    @disp;
}


my %disp_actions = map +($_ => 1), qw/
  close find list disable enable mode needs filter active-try do-not-reopen
  /;

my $reopen_disp = 1;

sub dispatcher($@)
{   if(! $disp_actions{$_[0]})
    {   my ($type, $name) = (shift, shift);

        # old dispatcher with same name will be closed in DESTROY
        my $disps = $reporter->{dispatchers};
 
        if(!$reopen_disp)
        {   my $has = first {$_->name eq $name} @$disps;
            if(defined $has && $has ne $default_dispatcher)
            {   my $default = $name eq 'default'
                   ? ' (refreshing configuration instead)' : '';
                trace "not reopening $name$default";
                return $has;
            }
        }

        my @disps = grep $_->name ne $name, @$disps;
        trace "reopening dispatcher $name" if @disps != @$disps;

        my $disp = Log::Report::Dispatcher
           ->new($type, $name, mode => $default_mode, @_);

        push @disps, $disp if $disp;
        $reporter->{dispatchers} = \@disps;

        _whats_needed;
        return $disp ? ($disp) : undef;
    }

    my $command = shift;
    if($command eq 'list')
    {   mistake __"the 'list' sub-command doesn't expect additional parameters"
           if @_;
        my @disp = @{$reporter->{dispatchers}};
        push @disp, $nested_tries[-1] if @nested_tries;
        return @disp;
    }
    if($command eq 'needs')
    {   my $reason = shift || 'undef';
        error __"the 'needs' sub-command parameter '{reason}' is not a reason"
            unless is_reason $reason;
        my $disp = $reporter->{needs}{$reason};
        return $disp ? @$disp : ();
    }
    if($command eq 'filter')
    {   my $code = shift;
        error __"the 'filter' sub-command needs a CODE reference"
            unless ref $code eq 'CODE';
        my %names = map +($_ => 1), @_;
        push @{$reporter->{filters}}, [ $code, \%names ];
        return ();
    }
    if($command eq 'active-try')
    {   return $nested_tries[-1];
    }
    if($command eq 'do-not-reopen')
    {   $reopen_disp = 0;
        return ();
    }

    my $mode     = $command eq 'mode' ? shift : undef;

    my $all_disp = @_==1 && $_[0] eq 'ALL';
    my $disps    = $reporter->{dispatchers};
    my @disps;
    if($all_disp) { @disps = @$disps }
    else
    {   # take the dispatchers in the specified order.  Both lists
        # are small, so O(x²) is small enough
        for my $n (@_) { push @disps, grep $_->name eq $n, @$disps }
    }

    error __"only one dispatcher name accepted in SCALAR context"
        if @disps > 1 && !wantarray && defined wantarray;

    if($command eq 'close')
    {   my %kill = map +($_->name => 1), @disps;
        @$disps  = grep !$kill{$_->name}, @$disps;
        $_->close for @disps;
    }
    elsif($command eq 'enable')  { $_->_disabled(0) for @disps }
    elsif($command eq 'disable') { $_->_disabled(1) for @disps }
    elsif($command eq 'mode')
    {    Log::Report::Dispatcher->defaultMode($mode) if $all_disp;
         $_->_set_mode($mode) for @disps;
    }

    # find does require reinventarization
    _whats_needed if $command ne 'find';

    wantarray ? @disps : $disps[0];
}

END { $_->close for @{$reporter->{dispatchers}} }

# _whats_needed
# Investigate from all dispatchers which reasons will need to be
# passed on. After dispatchers are added, enabled, or disabled,
# this method shall be called to re-investigate the back-ends.

sub _whats_needed()
{   my %needs;
    foreach my $disp (@{$reporter->{dispatchers}})
    {   push @{$needs{$_}}, $disp for $disp->needs;
    }
    $reporter->{needs} = \%needs;
}


sub try(&@)
{   my $code = shift;

    @_ % 2
      and report {location => [caller 0]}, PANIC =>
          __x"odd length parameter list for try(): forgot the terminating ';'?";

	unshift @_, mode => 'DEBUG'
        if $reporter->{needs}{TRACE};

    my $disp = Log::Report::Dispatcher::Try->new(TRY => 'try', @_);
    push @nested_tries, $disp;

    # user's __DIE__ handlers would frustrate the exception mechanism
    local $SIG{__DIE__};

    my ($ret, @ret);
    if(!defined wantarray)  { eval { $code->() } } # VOID   context
    elsif(wantarray) { @ret = eval { $code->() } } # LIST   context
    else             { $ret = eval { $code->() } } # SCALAR context

    my $err  = $@;
    pop @nested_tries;

    my $is_exception = blessed $err && $err->isa('Log::Report::Exception');
    if(!$is_exception && $err && !$disp->wasFatal)
    {   # Decode exceptions which do not origin from Log::Report reports
        ($err, my($opts, $reason, $text)) = blessed $err
           ? Log::Report::Die::exception_decode($err)
           : Log::Report::Die::die_decode($err, on_die => $disp->die2reason);

        $disp->log($opts, $reason, __$text);
    }

    $disp->died($err)
        if $is_exception ? $err->isFatal : $err;

    $@ = $disp;

    wantarray ? @ret : $ret;
}

#------------

sub trace(@)   {report TRACE   => @_}
sub assert(@)  {report ASSERT  => @_}
sub info(@)    {report INFO    => @_}
sub notice(@)  {report NOTICE  => @_}
sub warning(@) {report WARNING => @_}
sub mistake(@) {report MISTAKE => @_}
sub error(@)   {report ERROR   => @_}
sub fault(@)   {report FAULT   => @_}
sub alert(@)   {report ALERT   => @_}
sub failure(@) {report FAILURE => @_}
sub panic(@)   {report PANIC   => @_}

#-------------


sub __($)
{   my ($cpkg, $fn, $linenr) = caller;
    $lrm->new
      ( _msgid  => shift
      , _domain => pkg2domain($cpkg)
      , _use    => "$fn line $linenr"
      );
} 


# label "msgid" added before first argument
sub __x($@)
{   my ($cpkg, $fn, $linenr) = caller;
    @_%2 or error __x"even length parameter list for __x at {where}",
        where => "$fn line $linenr";

    my $msgid = shift;
    $lrm->new
      ( _msgid  => $msgid
      , _expand => 1
      , _domain => pkg2domain($cpkg)
      , _use    => "$fn line $linenr"
      , @_
      );
} 


sub __n($$$@)
{   my ($single, $plural, $count) = (shift, shift, shift);
    my ($cpkg, $fn, $linenr) = caller;
    $lrm->new
     ( _msgid  => $single
     , _plural => $plural
     , _count  => $count
     , _domain => pkg2domain($cpkg)
     , _use    => "$fn line $linenr"
     , @_
     );
}


sub __nx($$$@)
{   my ($single, $plural, $count) = (shift, shift, shift);
    my ($cpkg, $fn, $linenr) = caller;
    $lrm->new
     ( _msgid  => $single
     , _plural => $plural
     , _count  => $count
     , _expand => 1
     , _domain => pkg2domain($cpkg)
     , _use    => "$fn line $linenr"
     , @_
     );
}


sub __xn($$$@)   # repeated for prototype
{   my ($single, $plural, $count) = (shift, shift, shift);
    my ($cpkg, $fn, $linenr) = caller;
    $lrm->new
     ( _msgid  => $single
     , _plural => $plural
     , _count  => $count
     , _expand => 1
     , _domain => pkg2domain($cpkg)
     , _use    => "$fn line $linenr"
     , @_
     );
}


sub N__($) { $_[0] }


sub N__n($$) {@_}


sub N__w(@) {split " ", $_[0]}


#-------------

sub __p($$) { __($_[0])->_msgctxt($_[1]) }
sub __px($$@)
{   my ($ctxt, $msgid) = (shift, shift);
    __x($msgid, @_)->_msgctxt($ctxt);
}

sub __np($$$$)
{   my ($ctxt, $msgid, $plural, $count) = @_;
    __n($msgid, $msgid, $plural, $count)->_msgctxt($ctxt);
}

sub __npx($$$$@)
{   my ($ctxt, $msgid, $plural, $count) = splice @_, 0, 4;
    __nx($msgid, $msgid, $plural, $count, @_)->_msgctxt($ctxt);
}

#-------------

sub import(@)
{   my $class = shift;

    if($INC{'Log/Report/Minimal.pm'})
    {    my ($pkg, $fn, $line) = caller;   # do not report on LR:: modules
         if(index($pkg, 'Log::Report::') != 0)
         {   # @pkgs empty during release testings of L::R distributions
             my @pkgs = Log::Report::Optional->usedBy;
             die "Log::Report loaded too late in $fn line $line, "
               . "put in $pkg before ", (join ',', @pkgs) if @pkgs;
         }
    }

    my $to_level   = ($_[0] && $_[0] =~ m/^\+\d+$/ ? shift : undef) || 0;
    my $textdomain = @_%2 ? shift : undef;
    my %opts       = @_;

    my ($pkg, $fn, $linenr) = caller $to_level;
    my $domain;

    if(defined $textdomain)
    {   pkg2domain $pkg, $textdomain, $fn, $linenr;
        $domain = textdomain $textdomain;
    }

    ### Log::Report options

    if(exists $opts{mode})
    {   $default_mode = delete $opts{mode} || 0;
        Log::Report::Dispatcher->defaultMode($default_mode);
        dispatcher mode => $default_mode, 'ALL';
    }

    my @export;
    if(my $in = delete $opts{import})
    {   push @export, ref $in eq 'ARRAY' ? @$in : $in;
    }
    else
    {   push @export, @functions, @make_msg;

        my $syntax = delete $opts{syntax} || 'SHORT';
        if($syntax eq 'SHORT')
        {   push @export, @reason_functions
        }
        elsif($syntax ne 'REPORT' && $syntax ne 'LONG')
        {   error __x"syntax flag must be either SHORT or REPORT, not `{flag}' in {fn} line {line}"
              , flag => $syntax, fn => $fn, line => $linenr;
        }
    }

    if(my $msg_class = delete $opts{message_class})
    {   $msg_class->isa($lrm)
            or error __x"message_class {class} does not extend {base}"
                 , base => $lrm, class => $msg_class;
        $lrm = $msg_class;
    }

    $class->export_to_level(1+$to_level, undef, @export);

    ### Log::Report::Domain configuration

    if(!%opts) { }
    elsif($domain)
    {   $domain->configure(%opts, where => [$pkg, $fn, $linenr ]) }
    else
    {   error __x"no domain for configuration options in {fn} line {line}"
          , fn => $fn, line => $linenr;
    }
}

# deprecated, since we have a ::Domain object in 1.00
sub translator($;$$$$)
{   # replaced by (textdomain $domain)->configure

    my ($class, $name) = (shift, shift);
    my $domain = textdomain $name
        or error __x"textdomain `{domain}' for translator not defined"
             , domain => $name;

    @_ or return $domain->translator;

    my ($translator, $pkg, $fn, $line) = @_;
    ($pkg, $fn, $line) = caller    # direct call, not via import
        unless defined $pkg;

    $translator->isa('Log::Report::Translator')
        or error __x"translator must be a {pkg} object for {domain}"
              , pkg => 'Log::Report::Translator', domain => $name;

    $domain->configure(translator => $translator, where => [$pkg, $fn, $line]);
}


sub textdomain(@)
{   if(@_==1 && blessed $_[0])
    {   my $domain = shift;
        $domain->isa('Log::Report::Domain') or panic;
        return $reporter->{textdomains}{$domain->name} = $domain;
    }

    if(@_==2)
    {    # used for 'maintenance' and testing
    	return delete $reporter->{textdomains}{$_[0]} if $_[1] eq 'DELETE';
    	return $reporter->{textdomains}{$_[0]} if $_[1] eq 'EXISTS';
    }

    my $name   = (@_%2 ? shift : pkg2domain((caller)[0])) || 'default';
    my $domain = $reporter->{textdomains}{$name}
        ||= Log::Report::Domain->new(name => $name);

    $domain->configure(@_, where => [caller]) if @_;
    $domain;
}

#--------------

sub needs(@)
{   my $thing = shift;
    my $self  = ref $thing ? $thing : $reporter;
    first {$self->{needs}{$_}} @_;
}


1;
