# Copyrights 2007-2019 by [Mark Overmeer <markov@cpan.org>].
#  For other contributors see ChangeLog.
# See the manual pages for details on the licensing terms.
# Pod stripped from pm file by OODoc 2.02.
# This code is part of distribution Log-Report. Meta-POD processed with
# OODoc into POD and HTML manual-pages.  See README.md
# Copyright Mark Overmeer.  Licensed under the same terms as Perl itself.

package Dancer2::Plugin::LogReport;
use vars '$VERSION';
$VERSION = '1.28';


use warnings;
use strict;

BEGIN { use Log::Report () }  # require very early   XXX MO: useless?

use Dancer2::Plugin;
use Dancer2::Plugin::LogReport::Message;
use Log::Report  'log-report', syntax => 'REPORT',
    message_class => 'Dancer2::Plugin::LogReport::Message';

use Scalar::Util qw/blessed refaddr/;

my %_all_dsls;  # The DSLs for each app within the Dancer application
my $_settings;


# "use" import
sub import
{   my $class = shift;

     # Import Log::Report into the caller. Import options get passed through
     my $level = $Dancer2::VERSION > 0.166001 ? '+1' : '+2';
     Log::Report->import($level, @_, syntax => 'LONG');
 
     # Ensure the overridden import method is called (from Exporter::Tiny)
     # note this does not (currently) pass options through.
     my $caller = caller;
     $class->SUPER::import( {into => $caller} );
}

my %session_messages;
# The default reasons that a message will be displayed to the end user
my @default_reasons = qw/NOTICE WARNING MISTAKE ERROR FAULT ALERT FAILURE PANIC/;
my $hide_real_message; # Used to hide the real message to the end user
my $messages_variable = $_settings->{messages_key} || 'messages';


# Dancer2 import
on_plugin_import
{   # The DSL for the particular app that is loading the plugin
    my $dsl      = shift;  # capture global singleton
    $_all_dsls{refaddr($dsl->app)} = $dsl;

    my $settings = $_settings = plugin_setting;

    # Any exceptions in routes should not be happening. Therefore,
    # raise to PANIC.
    $dsl->app->add_hook(
        Dancer2::Core::Hook->new(
            name => 'core.app.route_exception',
            code => sub {
                my ($app, $error) = @_;
                report 'PANIC' => $error;
            },
        ),
    );


    if($settings->{handle_http_errors})
    {   # Need after_error for HTTP errors (eg 404) so as to
        # be able to change the forwarding location
        $dsl->app->add_hook(
            Dancer2::Core::Hook->new(
                name => 'after_error',
                code => sub {
                    my $error = shift;
                    my $msg = __($error->status . ": "
                      . Dancer2::Core::HTTP->status_message($error->status));

                    #XXX This doesn't work at the moment. The DSL at this point
                    # doesn't seem to respond to changes in the session or
                    # forward requests
                    _forward_home($msg);
                },
            ),
        );
    }

    $dsl->app->add_hook(
        Dancer2::Core::Hook->new(
            name => 'after_layout_render',
            code => sub {
                my $session = $dsl->app->session;
                $session->write($messages_variable => []);
            },
        ),
    );

    # Define which messages are saved to the session for later display
    # to the user. This can be configured in the config file, or we
    # choose some sensible defaults.
    my $sm = $settings->{session_messages} // \@default_reasons;
    $session_messages{$_} = 1
        for ref $sm eq 'ARRAY' ? @$sm : $sm;

    # In a production server, we don't want the end user seeing (unexpected)
    # exception messages, for both security and usability. If we detect
    # that this is a production server (show_errors is 0), then we change
    # the specific error to a generic error, when displayed to the user.
    # The message can be customised in the config file.
    my $fatal_error_message = $settings->{fatal_error_message}
       // "An unexpected error has occurred";

    unless($dsl->app->config->{show_errors})
    {   $hide_real_message->{$_} = $fatal_error_message
            for qw/FAULT ALERT FAILURE PANIC/;
    }

    if(my $forward_template = $settings->{forward_template})
    {   # Add a route for the specified template
        $dsl->app->add_route
          ( method => 'get'
          , regexp => qr!^/\Q$forward_template\E$!,
          , code   => sub { shift->app->template($forward_template) }
          );
        # Forward to that new route
        $settings->{forward_url} = $forward_template;
    }

    # This is so that all messages go into the session, to be displayed
    # on the web page (if required)
    dispatcher CALLBACK => 'error_handler'
      , callback => \&_error_handler
      , mode     => 'DEBUG'
        unless dispatcher find => 'error_handler';

    Log::Report::Dispatcher->addSkipStack( sub { $_[0][0] =~
        m/ ^ Dancer2\:\:(?:Plugin|Logger)
         | ^ Dancer2\:\:Core\:\:Role\:\:DSL
         | ^ Dancer2\:\:Core\:\:App
         /x
    });

};    # ";" required!


sub process($$)
{   my ($dsl, $coderef) = @_;
    try { $coderef->() } hide => 'ALL', on_die => 'PANIC';
	my $e = $@;  # fragile
    $e->reportAll(is_fatal => 0);
    $e->success || 0;
}

register process => \&process;



my @user_fatal_handlers;

plugin_keywords fatal_handler => sub {
    my ($plugin, $sub) = @_;
    push @user_fatal_handlers, $sub;
};

sub _get_dsl()
{   # Similar trick to Log::Report::Dispatcher::collectStack(), this time to
    # work out which Dancer app we were called from. We then use that app's
    # DSL. If we use the wrong DSL, then the request object will not be
    # available and we won't be able to forward if needed

    package DB;
    use Scalar::Util qw/blessed refaddr/;

    my (@ret, $ref, $i);
    do { @ret = caller ++$i }
    until !@ret
     || (    blessed $DB::args[0]
          && blessed $DB::args[0] eq 'Dancer2::Core::App'
          && ( $ref = refaddr $DB::args[0] )
        )
     || (    blessed $DB::args[1]
          && blessed $DB::args[1] eq 'Dancer2::Core::App'
          && ( $ref = refaddr $DB::args[1] )
        );
    $ref ? $_all_dsls{$ref} : undef;
}


sub _message_add($)
{   my $msg = shift;

    return
        if ! $session_messages{$msg->reason}
        || $msg->inClass('no_session');

    # Get the DSL, only now that we know it's needed
    my $dsl = _get_dsl();

    if (!$dsl)
    {   report {to => 'default'}, NOTICE =>
            "Unable to write message $msg to the session. "
          . "Have you loaded Dancer2::Plugin::LogReport to all your separate Dancer apps?";
        return;
    }

    my $app = $dsl->app;

    # Check that we can write to the session before continuing. We can't
    # check $app->session as that can be true regardless. Instead, we check
    # for request(), which is used to access the cookies of a session.
    return unless $app->request;

    my $r = $msg->reason;
    if(my $newm = $hide_real_message->{$r})
    {   $msg    = __$newm;
        $msg->reason($r);
    }

    my $session = $app->session;
    my $msgs    = $session->read($messages_variable);
    push @$msgs, $msg;
    $session->write($messages_variable => $msgs);

    return $dsl;
}

#------

sub _forward_home($)
{   my $dsl = _message_add(shift) || _get_dsl();
    my $page = $_settings->{forward_url} || '/';

    # Don't forward if it's a GET request to the error page, as it will
    # cause a recursive loop. In this case, do nothing, and let dancer
    # handle it.
    my $req = $dsl->app->request or return;
    return if $req->uri eq $page && $req->is_get;

    $dsl->redirect($page);
}

sub _error_handler($$$$)
{   my ($disp, $options, $reason, $message) = @_;

    my $default_handler = sub {

        # Check whether this fatal message has been caught, in which case we
        # don't want to redirect
        return _message_add($message)
            if exists $options->{is_fatal} && !$options->{is_fatal};

        _forward_home($message);
    };

    my $user_fatal_handler = sub {
        my $return;
        foreach my $ufh (@user_fatal_handlers)
        {   last if $return = $ufh->(_get_dsl, $message, $reason);
        }
        $default_handler->($message) if !$return;
    };

    my $fatal_handler = @user_fatal_handlers
      ? $user_fatal_handler
      : $default_handler;

    $message->reason($reason);

    my %handler =
      ( # Default do nothing for the moment (TRACE|ASSERT|INFO)
        default => sub { _message_add $message }

        # A user-created error condition that is not recoverable.
        # This could have already been caught by the process
        # subroutine, in which case we should continue running
        # of the program. In all other cases, we should bail
        # out.
      , ERROR   => $fatal_handler

        # 'FAULT', 'ALERT', 'FAILURE', 'PANIC'
        # All these are fatal errors.
      , FAULT   => $fatal_handler
      , ALERT   => $fatal_handler
      , FAILURE => $fatal_handler
      , PANIC   => $fatal_handler
      );

    my $call = $handler{$reason} || $handler{default};
    $call->();
}

sub _report($@) {
    my ($reason, $dsl) = (shift, shift);

    my $msg = (blessed($_[0]) && $_[0]->isa('Log::Report::Message'))
       ? $_[0] : Dancer2::Core::Role::Logger::_serialize(@_);

    if ($reason eq 'SUCCESS')
    {
        $msg = __$msg unless blessed $msg;
        $msg = $msg->clone(_class => 'success');
        $reason = 'NOTICE';
    }
    report uc($reason) => $msg;
}

register trace   => sub { _report(TRACE => @_) };
register assert  => sub { _report(ASSERT => @_) };
register notice  => sub { _report(NOTICE => @_) };
register mistake => sub { _report(MISTAKE => @_) };
register panic   => sub { _report(PANIC => @_) };
register alert   => sub { _report(ALERT => @_) };
register fault   => sub { _report(FAULT => @_) };
register failure => sub { _report(FAILURE => @_) };

register success => sub { _report(SUCCESS => @_) };

register_plugin for_versions => ['2'];

#----------


1;

