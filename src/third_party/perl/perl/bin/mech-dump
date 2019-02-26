#!/usr/bin/perl -w

=head1 NAME

mech-dump - Dumps information about a web page

=cut

use warnings;
use strict;
use WWW::Mechanize ();
use Getopt::Long;
use Pod::Usage;

use HTTP::Cookies;
my @actions;
my $absolute;

my $user;
my $pass;
my $agent;
my $agent_alias;
my $cookie_filename;

GetOptions(
    'user=s'        => \$user,
    'password=s'    => \$pass,
    headers         => sub { push( @actions, \&dump_headers ) },
    forms           => sub { push( @actions, \&dump_forms ) },
    links           => sub { push( @actions, \&dump_links ) },
    images          => sub { push( @actions, \&dump_images ) },
    all             => sub { push( @actions, \&dump_headers, \&dump_forms, \&dump_links, \&dump_images ) },
    text            => sub { push( @actions, \&dump_text ) },
    absolute        => \$absolute,
    'agent=s'       => \$agent,
    'agent-alias=s' => \$agent_alias,
    'cookie-file=s' => \$cookie_filename,
    help            => sub { pod2usage(1); },
) or pod2usage(2);

=head1 SYNOPSIS

mech-dump [options] [file|url]

Options:

    --headers              Dump HTTP response headers
    --forms                Dump table of forms (default action)
    --links                Dump table of links
    --images               Dump table of images
    --all                  Dump all four of the above, in that order

    --text                 Dumps the textual part of the web page

    --user=user            Set the username
    --password=pass        Set the password
    --cookie-file=filename Set the filename to use for persistent cookies

    --agent=agent          Specify the UserAgent to pass
    --agent-alias=alias
                           Specify the alias for the UserAgent to pass.
                           Pick one of:
                               * Windows IE 6
                               * Windows Mozilla
                               * Mac Safari
                               * Mac Mozilla
                               * Linux Mozilla
                               * Linux Konqueror

    --absolute             Show URLs as absolute, even if relative in the page
    --help                 Show this message

The order of the options specified is relevant.  Repeated options
get repeated dumps.

=cut

my $uri = shift or die "Must specify a URL or file to check.  See --help for details.\n";
if ( -e $uri ) {
    require URI::file;
    $uri = URI::file->new_abs( $uri )->as_string;
}

@actions = (\&dump_forms) unless @actions;

my $mech = WWW::Mechanize->new();
if ( defined $agent ) {
    $mech->agent( $agent );
}
elsif ( defined $agent_alias ) {
    $mech->agent_alias( $agent_alias );
}
if ( defined $cookie_filename ) {
    my $cookies = HTTP::Cookies->new( file => $cookie_filename, autosave => 1, ignore_discard => 1 );
    $cookies->load() ;
    $mech->cookie_jar($cookies);
}
else {
    $mech->cookie_jar(undef) ;
}

$mech->env_proxy();
my $response = $mech->get( $uri );
if (!$response->is_success and defined ($response->www_authenticate)) {
    if (!defined $user or !defined $pass) {
        die("Page requires username and password, but none specified.\n");
    }
    $mech->credentials($user,$pass);
    $response = $mech->get( $uri );
    $response->is_success or die "Can't fetch $uri with username and password\n", $response->status_line, "\n";
}
$mech->is_html or die qq{$uri returns type "}, $mech->ct, qq{", not "text/html"\n};

while ( my $action = shift @actions ) {
    $action->( $mech );
    print "\n" if @actions;
}


sub dump_headers {
    my $mech = shift;
    $mech->dump_headers( undef );
    return;
}

sub dump_forms {
    my $mech = shift;
    $mech->dump_forms( undef, $absolute );
    return;
}

sub dump_links {
    my $mech = shift;
    $mech->dump_links( undef, $absolute );
    return;
}

sub dump_images {
    my $mech = shift;
    $mech->dump_images( undef, $absolute );
    return;
}

sub dump_text {
    my $mech = shift;
    $mech->dump_text();
    return;
}

=head1 SEE ALSO

L<WWW::Mechanize>
