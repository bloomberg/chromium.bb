package WWW::Mechanize;

=head1 NAME

WWW::Mechanize - Handy web browsing in a Perl object

=head1 VERSION

Version 1.70

=cut

our $VERSION = '1.72';

=head1 SYNOPSIS

C<WWW::Mechanize>, or Mech for short, is a Perl module for stateful
programmatic web browsing, used for automating interaction with
websites.

Features include:

=over 4

=item * All HTTP methods

=item * High-level hyperlink and HTML form support, without having to parse HTML yourself

=item * SSL support

=item * Automatic cookies

=item * Custom HTTP headers

=item * Automatic handling of redirections

=item * Proxies

=item * HTTP authentication

=back

Mech supports performing a sequence of page fetches including
following links and submitting forms. Each fetched page is parsed
and its links and forms are extracted. A link or a form can be
selected, form fields can be filled and the next page can be fetched.
Mech also stores a history of the URLs you've visited, which can
be queried and revisited.

    use WWW::Mechanize;
    my $mech = WWW::Mechanize->new();

    $mech->get( $url );

    $mech->follow_link( n => 3 );
    $mech->follow_link( text_regex => qr/download this/i );
    $mech->follow_link( url => 'http://host.com/index.html' );

    $mech->submit_form(
        form_number => 3,
        fields      => {
            username    => 'mungo',
            password    => 'lost-and-alone',
        }
    );

    $mech->submit_form(
        form_name => 'search',
        fields    => { query  => 'pot of gold', },
        button    => 'Search Now'
    );


Mech is well suited for use in testing web applications.  If you use
one of the Test::*, like L<Test::HTML::Lint> modules, you can check the
fetched content and use that as input to a test call.

    use Test::More;
    like( $mech->content(), qr/$expected/, "Got expected content" );

Each page fetch stores its URL in a history stack which you can
traverse.

    $mech->back();

If you want finer control over your page fetching, you can use
these methods. C<follow_link> and C<submit_form> are just high
level wrappers around them.

    $mech->find_link( n => $number );
    $mech->form_number( $number );
    $mech->form_name( $name );
    $mech->field( $name, $value );
    $mech->set_fields( %field_values );
    $mech->set_visible( @criteria );
    $mech->click( $button );

L<WWW::Mechanize> is a proper subclass of L<LWP::UserAgent> and
you can also use any of L<LWP::UserAgent>'s methods.

    $mech->add_header($name => $value);

Please note that Mech does NOT support JavaScript, you need additional software
for that. Please check L<WWW::Mechanize::FAQ/"JavaScript"> for more.

=head1 IMPORTANT LINKS

=over 4

=item * L<http://code.google.com/p/www-mechanize/issues/list>

The queue for bugs & enhancements in WWW::Mechanize and
Test::WWW::Mechanize.  Please note that the queue at L<http://rt.cpan.org>
is no longer maintained.

=item * L<http://search.cpan.org/dist/WWW-Mechanize/>

The CPAN documentation page for Mechanize.

=item * L<http://search.cpan.org/dist/WWW-Mechanize/lib/WWW/Mechanize/FAQ.pod>

Frequently asked questions.  Make sure you read here FIRST.

=back

=cut

use strict;
use warnings;

use HTTP::Request 1.30;
use LWP::UserAgent 5.827;
use HTML::Form 1.00;
use HTML::TokeParser;

use base 'LWP::UserAgent';

our $HAS_ZLIB;
BEGIN {
    $HAS_ZLIB = eval 'use Compress::Zlib (); 1;';
}

=head1 CONSTRUCTOR AND STARTUP

=head2 new()

Creates and returns a new WWW::Mechanize object, hereafter referred to as
the "agent".

    my $mech = WWW::Mechanize->new()

The constructor for WWW::Mechanize overrides two of the parms to the
LWP::UserAgent constructor:

    agent => 'WWW-Mechanize/#.##'
    cookie_jar => {}    # an empty, memory-only HTTP::Cookies object

You can override these overrides by passing parms to the constructor,
as in:

    my $mech = WWW::Mechanize->new( agent => 'wonderbot 1.01' );

If you want none of the overhead of a cookie jar, or don't want your
bot accepting cookies, you have to explicitly disallow it, like so:

    my $mech = WWW::Mechanize->new( cookie_jar => undef );

Here are the parms that WWW::Mechanize recognizes.  These do not include
parms that L<LWP::UserAgent> recognizes.

=over 4

=item * C<< autocheck => [0|1] >>

Checks each request made to see if it was successful.  This saves
you the trouble of manually checking yourself.  Any errors found
are errors, not warnings.

The default value is ON, unless it's being subclassed, in which
case it is OFF.  This means that standalone L<WWW::Mechanize>instances
have autocheck turned on, which is protective for the vast majority
of Mech users who don't bother checking the return value of get()
and post() and can't figure why their code fails. However, if
L<WWW::Mechanize> is subclassed, such as for L<Test::WWW::Mechanize>
or L<Test::WWW::Mechanize::Catalyst>, this may not be an appropriate
default, so it's off.

=item * C<< noproxy => [0|1] >>

Turn off the automatic call to the L<LWP::UserAgent> C<env_proxy> function.

This needs to be explicitly turned off if you're using L<Crypt::SSLeay> to
access a https site via a proxy server.  Note: you still need to set your
HTTPS_PROXY environment variable as appropriate.

=item * C<< onwarn => \&func >>

Reference to a C<warn>-compatible function, such as C<< L<Carp>::carp >>,
that is called when a warning needs to be shown.

If this is set to C<undef>, no warnings will ever be shown.  However,
it's probably better to use the C<quiet> method to control that behavior.

If this value is not passed, Mech uses C<Carp::carp> if L<Carp> is
installed, or C<CORE::warn> if not.

=item * C<< onerror => \&func >>

Reference to a C<die>-compatible function, such as C<< L<Carp>::croak >>,
that is called when there's a fatal error.

If this is set to C<undef>, no errors will ever be shown.

If this value is not passed, Mech uses C<Carp::croak> if L<Carp> is
installed, or C<CORE::die> if not.

=item * C<< quiet => [0|1] >>

Don't complain on warnings.  Setting C<< quiet => 1 >> is the same as
calling C<< $mech->quiet(1) >>.  Default is off.

=item * C<< stack_depth => $value >>

Sets the depth of the page stack that keeps track of all the
downloaded pages. Default is effectively infinite stack size.  If
the stack is eating up your memory, then set this to a smaller
number, say 5 or 10.  Setting this to zero means Mech will keep no
history.

=back

To support forms, WWW::Mechanize's constructor pushes POST
on to the agent's C<requests_redirectable> list (see also
L<LWP::UserAgent>.)

=cut

sub new {
    my $class = shift;

    my %parent_parms = (
        agent       => "WWW-Mechanize/$VERSION",
        cookie_jar  => {},
    );

    my %mech_parms = (
        autocheck   => ($class eq 'WWW::Mechanize' ? 1 : 0),
        onwarn      => \&WWW::Mechanize::_warn,
        onerror     => \&WWW::Mechanize::_die,
        quiet       => 0,
        stack_depth => 8675309,     # Arbitrarily humongous stack
        headers     => {},
        noproxy     => 0,
    );

    my %passed_parms = @_;

    # Keep the mech-specific parms before creating the object.
    while ( my($key,$value) = each %passed_parms ) {
        if ( exists $mech_parms{$key} ) {
            $mech_parms{$key} = $value;
        }
        else {
            $parent_parms{$key} = $value;
        }
    }

    my $self = $class->SUPER::new( %parent_parms );
    bless $self, $class;

    # Use the mech parms now that we have a mech object.
    for my $parm ( keys %mech_parms ) {
        $self->{$parm} = $mech_parms{$parm};
    }
    $self->{page_stack} = [];
    $self->env_proxy() unless $mech_parms{noproxy};

    # libwww-perl 5.800 (and before, I assume) has a problem where
    # $ua->{proxy} can be undef and clone() doesn't handle it.
    $self->{proxy} = {} unless defined $self->{proxy};
    push( @{$self->requests_redirectable}, 'POST' );

    $self->_reset_page();

    return $self;
}

=head2 $mech->agent_alias( $alias )

Sets the user agent string to the expanded version from a table of actual user strings.
I<$alias> can be one of the following:

=over 4

=item * Windows IE 6

=item * Windows Mozilla

=item * Mac Safari

=item * Mac Mozilla

=item * Linux Mozilla

=item * Linux Konqueror

=back

then it will be replaced with a more interesting one.  For instance,

    $mech->agent_alias( 'Windows IE 6' );

sets your User-Agent to

    Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)

The list of valid aliases can be returned from C<known_agent_aliases()>.  The current list is:

=over

=item * Windows IE 6

=item * Windows Mozilla

=item * Mac Safari

=item * Mac Mozilla

=item * Linux Mozilla

=item * Linux Konqueror

=back

=cut

my %known_agents = (
    'Windows IE 6'      => 'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)',
    'Windows Mozilla'   => 'Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.4b) Gecko/20030516 Mozilla Firebird/0.6',
    'Mac Safari'        => 'Mozilla/5.0 (Macintosh; U; PPC Mac OS X; en-us) AppleWebKit/85 (KHTML, like Gecko) Safari/85',
    'Mac Mozilla'       => 'Mozilla/5.0 (Macintosh; U; PPC Mac OS X Mach-O; en-US; rv:1.4a) Gecko/20030401',
    'Linux Mozilla'     => 'Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.4) Gecko/20030624',
    'Linux Konqueror'   => 'Mozilla/5.0 (compatible; Konqueror/3; Linux)',
);

sub agent_alias {
    my $self = shift;
    my $alias = shift;

    if ( defined $known_agents{$alias} ) {
        return $self->agent( $known_agents{$alias} );
    }
    else {
        $self->warn( qq{Unknown agent alias "$alias"} );
        return $self->agent();
    }
}

=head2 known_agent_aliases()

Returns a list of all the agent aliases that Mech knows about.

=cut

sub known_agent_aliases {
    return sort keys %known_agents;
}

=head1 PAGE-FETCHING METHODS

=head2 $mech->get( $uri )

Given a URL/URI, fetches it.  Returns an L<HTTP::Response> object.
I<$uri> can be a well-formed URL string, a L<URI> object, or a
L<WWW::Mechanize::Link> object.

The results are stored internally in the agent object, but you don't
know that.  Just use the accessors listed below.  Poking at the
internals is deprecated and subject to change in the future.

C<get()> is a well-behaved overloaded version of the method in
L<LWP::UserAgent>.  This lets you do things like

    $mech->get( $uri, ':content_file' => $tempfile );

and you can rest assured that the parms will get filtered down
appropriately.

B<NOTE:> Because C<:content_file> causes the page contents to be
stored in a file instead of the response object, some Mech functions
that expect it to be there won't work as expected. Use with caution.

=cut

sub get {
    my $self = shift;
    my $uri = shift;

    $uri = $uri->url if ref($uri) eq 'WWW::Mechanize::Link';

    $uri = $self->base
            ? URI->new_abs( $uri, $self->base )
            : URI->new( $uri );

    # It appears we are returning a super-class method,
    # but it in turn calls the request() method here in Mechanize
    return $self->SUPER::get( $uri->as_string, @_ );
}

=head2 $mech->put( $uri, content => $content )

PUTs I<$content> to $uri.  Returns an L<HTTP::Response> object.
I<$uri> can be a well-formed URI string, a L<URI> object, or a
L<WWW::Mechanize::Link> object.

=cut

sub put {
    my $self = shift;
    my $uri = shift;

    $uri = $uri->url if ref($uri) eq 'WWW::Mechanize::Link';

    $uri = $self->base
            ? URI->new_abs( $uri, $self->base )
            : URI->new( $uri );

    # It appears we are returning a super-class method,
    # but it in turn calls the request() method here in Mechanize
    return $self->_SUPER_put( $uri->as_string, @_ );
}


# Added until LWP::UserAgent has it.
sub _SUPER_put {
    require HTTP::Request::Common;
    my($self, @parameters) = @_;
    my @suff = $self->_process_colonic_headers(\@parameters,1);
    return $self->request( HTTP::Request::Common::PUT( @parameters ), @suff );
}

=head2 $mech->reload()

Acts like the reload button in a browser: repeats the current
request. The history (as per the L</back> method) is not altered.

Returns the L<HTTP::Response> object from the reload, or C<undef>
if there's no current request.

=cut

sub reload {
    my $self = shift;

    return unless my $req = $self->{req};

    return $self->_update_page( $req, $self->_make_request( $req, @_ ) );
}

=head2 $mech->back()

The equivalent of hitting the "back" button in a browser.  Returns to
the previous page.  Won't go back past the first page. (Really, what
would it do if it could?)

Returns true if it could go back, or false if not.

=cut

sub back {
    my $self = shift;

    my $stack = $self->{page_stack};
    return unless $stack && @{$stack};

    my $popped = pop @{$self->{page_stack}};
    my $req    = $popped->{req};
    my $res    = $popped->{res};

    $self->_update_page( $req, $res );

    return 1;
}

=head1 STATUS METHODS

=head2 $mech->success()

Returns a boolean telling whether the last request was successful.
If there hasn't been an operation yet, returns false.

This is a convenience function that wraps C<< $mech->res->is_success >>.

=cut

sub success {
    my $self = shift;

    return $self->res && $self->res->is_success;
}


=head2 $mech->uri()

Returns the current URI as a L<URI> object. This object stringifies
to the URI itself.

=head2 $mech->response() / $mech->res()

Return the current response as an L<HTTP::Response> object.

Synonym for C<< $mech->response() >>

=head2 $mech->status()

Returns the HTTP status code of the response.  This is a 3-digit
number like 200 for OK, 404 for not found, and so on.

=head2 $mech->ct() / $mech->content_type()

Returns the content type of the response.

=head2 $mech->base()

Returns the base URI for the current response

=head2 $mech->forms()

When called in a list context, returns a list of the forms found in
the last fetched page. In a scalar context, returns a reference to
an array with those forms. The forms returned are all L<HTML::Form>
objects.

=head2 $mech->current_form()

Returns the current form as an L<HTML::Form> object.

=head2 $mech->links()

When called in a list context, returns a list of the links found in the
last fetched page.  In a scalar context it returns a reference to an array
with those links.  Each link is a L<WWW::Mechanize::Link> object.

=head2 $mech->is_html()

Returns true/false on whether our content is HTML, according to the
HTTP headers.

=cut

sub uri {
    my $self = shift;
    return $self->response->request->uri;
}

sub res {           my $self = shift; return $self->{res}; }
sub response {      my $self = shift; return $self->{res}; }
sub status {        my $self = shift; return $self->{status}; }
sub ct {            my $self = shift; return $self->{ct}; }
sub content_type {  my $self = shift; return $self->{ct}; }
sub base {          my $self = shift; return $self->{base}; }
sub is_html {
    my $self = shift;
    return defined $self->ct &&
        ($self->ct eq 'text/html' || $self->ct eq 'application/xhtml+xml');
}

=head2 $mech->title()

Returns the contents of the C<< <TITLE> >> tag, as parsed by
L<HTML::HeadParser>.  Returns undef if the content is not HTML.

=cut

sub title {
    my $self = shift;

    return unless $self->is_html;

    if ( not defined $self->{title} ) {
        require HTML::HeadParser;
        my $p = HTML::HeadParser->new;
        $p->parse($self->content);
        $self->{title} = $p->header('Title');
    }
    return $self->{title};
}

=head1 CONTENT-HANDLING METHODS

=head2 $mech->content(...)

Returns the content that the mech uses internally for the last page
fetched. Ordinarily this is the same as $mech->response()->content(),
but this may differ for HTML documents if L</update_html> is
overloaded (in which case the value passed to the base-class
implementation of same will be returned), and/or extra named arguments
are passed to I<content()>:

=over 2

=item I<< $mech->content( format => 'text' ) >>

Returns a text-only version of the page, with all HTML markup
stripped. This feature requires I<HTML::TreeBuilder> to be installed,
or a fatal error will be thrown.

=item I<< $mech->content( base_href => [$base_href|undef] ) >>

Returns the HTML document, modified to contain a
C<< <base href="$base_href"> >> mark-up in the header.
I<$base_href> is C<< $mech->base() >> if not specified. This is
handy to pass the HTML to e.g. L<HTML::Display>.

=back

Passing arguments to C<content()> if the current document is not
HTML has no effect now (i.e. the return value is the same as
C<< $self->response()->content() >>. This may change in the future,
but will likely be backwards-compatible when it does.

=cut

sub content {
    my $self = shift;
    my $content = $self->{content};

    if ( $self->is_html ) {
        my %parms = @_;

        if ( exists $parms{base_href} ) {
            my $base_href = (delete $parms{base_href}) || $self->base;
            $content=~s/<head>/<head>\n<base href="$base_href">/i;
        }

        if ( my $format = delete $parms{format} ) {
            if ( $format eq 'text' ) {
                $content = $self->text;
            }
            else {
                $self->die( qq{Unknown "format" parameter "$format"} );
            }
        }

        $self->_check_unhandled_parms( %parms );
    }

    return $content;
}

=head2 $mech->text()

Returns the text of the current HTML content.  If the content isn't
HTML, $mech will die.

The text is extracted by parsing the content, and then the extracted
text is cached, so don't worry about performance of calling this
repeatedly.

=cut

sub text {
    my $self = shift;

    if ( not defined $self->{text} ) {
        require HTML::TreeBuilder;
        my $tree = HTML::TreeBuilder->new();
        $tree->parse( $self->content );
        $tree->eof();
        $tree->elementify(); # just for safety
        $self->{text} = $tree->as_text();
        $tree->delete;
    }

    return $self->{text};
}

sub _check_unhandled_parms {
    my $self  = shift;
    my %parms = @_;

    for my $cmd ( sort keys %parms ) {
        $self->die( qq{Unknown named argument "$cmd"} );
    }
}

=head1 LINK METHODS

=head2 $mech->links()

Lists all the links on the current page.  Each link is a
WWW::Mechanize::Link object. In list context, returns a list of all
links.  In scalar context, returns an array reference of all links.

=cut

sub links {
    my $self = shift;

    $self->_extract_links() unless $self->{links};

    return @{$self->{links}} if wantarray;
    return $self->{links};
}

=head2 $mech->follow_link(...)

Follows a specified link on the page.  You specify the match to be
found using the same parms that C<L<find_link()>> uses.

Here some examples:

=over 4

=item * 3rd link called "download"

    $mech->follow_link( text => 'download', n => 3 );

=item * first link where the URL has "download" in it, regardless of case:

    $mech->follow_link( url_regex => qr/download/i );

or

    $mech->follow_link( url_regex => qr/(?i:download)/ );

=item * 3rd link on the page

    $mech->follow_link( n => 3 );

=back

Returns the result of the GET method (an HTTP::Response object) if
a link was found. If the page has no links, or the specified link
couldn't be found, returns undef.

=cut

sub follow_link {
    my $self = shift;
    my %parms = ( n=>1, @_ );

    if ( $parms{n} eq 'all' ) {
        delete $parms{n};
        $self->warn( q{follow_link(n=>"all") is not valid} );
    }

    my $link = $self->find_link(%parms);
    if ( $link ) {
        return $self->get( $link->url );
    }

    if ( $self->{autocheck} ) {
        $self->die( 'Link not found' );
    }

    return;
}

=head2 $mech->find_link( ... )

Finds a link in the currently fetched page. It returns a
L<WWW::Mechanize::Link> object which describes the link.  (You'll
probably be most interested in the C<url()> property.)  If it fails
to find a link it returns undef.

You can take the URL part and pass it to the C<get()> method.  If
that's your plan, you might as well use the C<follow_link()> method
directly, since it does the C<get()> for you automatically.

Note that C<< <FRAME SRC="..."> >> tags are parsed out of the the HTML
and treated as links so this method works with them.

You can select which link to find by passing in one or more of these
key/value pairs:

=over 4

=item * C<< text => 'string', >> and C<< text_regex => qr/regex/, >>

C<text> matches the text of the link against I<string>, which must be an
exact match.  To select a link with text that is exactly "download", use

    $mech->find_link( text => 'download' );

C<text_regex> matches the text of the link against I<regex>.  To select a
link with text that has "download" anywhere in it, regardless of case, use

    $mech->find_link( text_regex => qr/download/i );

Note that the text extracted from the page's links are trimmed.  For
example, C<< <a> foo </a> >> is stored as 'foo', and searching for
leading or trailing spaces will fail.

=item * C<< url => 'string', >> and C<< url_regex => qr/regex/, >>

Matches the URL of the link against I<string> or I<regex>, as appropriate.
The URL may be a relative URL, like F<foo/bar.html>, depending on how
it's coded on the page.

=item * C<< url_abs => string >> and C<< url_abs_regex => regex >>

Matches the absolute URL of the link against I<string> or I<regex>,
as appropriate.  The URL will be an absolute URL, even if it's relative
in the page.

=item * C<< name => string >> and C<< name_regex => regex >>

Matches the name of the link against I<string> or I<regex>, as appropriate.

=item * C<< id => string >> and C<< id_regex => regex >>

Matches the attribute 'id' of the link against I<string> or
I<regex>, as appropriate.

=item * C<< class => string >> and C<< class_regex => regex >>

Matches the attribute 'class' of the link against I<string> or
I<regex>, as appropriate.

=item * C<< tag => string >> and C<< tag_regex => regex >>

Matches the tag that the link came from against I<string> or I<regex>,
as appropriate.  The C<tag_regex> is probably most useful to check for
more than one tag, as in:

    $mech->find_link( tag_regex => qr/^(a|frame)$/ );

The tags and attributes looked at are defined below, at
L<< $mech->find_link() : link format >>.

=back

If C<n> is not specified, it defaults to 1.  Therefore, if you don't
specify any parms, this method defaults to finding the first link on the
page.

Note that you can specify multiple text or URL parameters, which
will be ANDed together.  For example, to find the first link with
text of "News" and with "cnn.com" in the URL, use:

    $mech->find_link( text => 'News', url_regex => qr/cnn\.com/ );

The return value is a reference to an array containing a
L<WWW::Mechanize::Link> object for every link in C<< $self->content >>.

The links come from the following:

=over 4

=item C<< <a href=...> >>

=item C<< <area href=...> >>

=item C<< <frame src=...> >>

=item C<< <iframe src=...> >>

=item C<< <link href=...> >>

=item C<< <meta content=...> >>

=back

=cut

sub find_link {
    my $self = shift;
    my %parms = ( n=>1, @_ );

    my $wantall = ( $parms{n} eq 'all' );

    $self->_clean_keys( \%parms, qr/^(n|(text|url|url_abs|name|tag|id|class)(_regex)?)$/ );

    my @links = $self->links or return;

    my $nmatches = 0;
    my @matches;
    for my $link ( @links ) {
        if ( _match_any_link_parms($link,\%parms) ) {
            if ( $wantall ) {
                push( @matches, $link );
            }
            else {
                ++$nmatches;
                return $link if $nmatches >= $parms{n};
            }
        }
    } # for @links

    if ( $wantall ) {
        return @matches if wantarray;
        return \@matches;
    }

    return;
} # find_link

# Used by find_links to check for matches
# The logic is such that ALL parm criteria that are given must match
sub _match_any_link_parms {
    my $link = shift;
    my $p = shift;

    # No conditions, anything matches
    return 1 unless keys %$p;

    return if defined $p->{url}           && !($link->url eq $p->{url} );
    return if defined $p->{url_regex}     && !($link->url =~ $p->{url_regex} );
    return if defined $p->{url_abs}       && !($link->url_abs eq $p->{url_abs} );
    return if defined $p->{url_abs_regex} && !($link->url_abs =~ $p->{url_abs_regex} );
    return if defined $p->{text}          && !(defined($link->text) && $link->text eq $p->{text} );
    return if defined $p->{text_regex}    && !(defined($link->text) && $link->text =~ $p->{text_regex} );
    return if defined $p->{name}          && !(defined($link->name) && $link->name eq $p->{name} );
    return if defined $p->{name_regex}    && !(defined($link->name) && $link->name =~ $p->{name_regex} );
    return if defined $p->{tag}           && !($link->tag && $link->tag eq $p->{tag} );
    return if defined $p->{tag_regex}     && !($link->tag && $link->tag =~ $p->{tag_regex} );

    return if defined $p->{id}            && !($link->attrs->{id} && $link->attrs->{id} eq $p->{id} );
    return if defined $p->{id_regex}      && !($link->attrs->{id} && $link->attrs->{id} =~ $p->{id_regex} );
    return if defined $p->{class}         && !($link->attrs->{class} && $link->attrs->{class} eq $p->{class} );
    return if defined $p->{class_regex}   && !($link->attrs->{class} && $link->attrs->{class} =~ $p->{class_regex} );

    # Success: everything that was defined passed.
    return 1;

}

# Cleans the %parms parameter for the find_link and find_image methods.
sub _clean_keys {
    my $self = shift;
    my $parms = shift;
    my $rx_keyname = shift;

    for my $key ( keys %$parms ) {
        my $val = $parms->{$key};
        if ( $key !~ qr/$rx_keyname/ ) {
            $self->warn( qq{Unknown link-finding parameter "$key"} );
            delete $parms->{$key};
            next;
        }

        my $key_regex = ( $key =~ /_regex$/ );
        my $val_regex = ( ref($val) eq 'Regexp' );

        if ( $key_regex ) {
            if ( !$val_regex ) {
                $self->warn( qq{$val passed as $key is not a regex} );
                delete $parms->{$key};
                next;
            }
        }
        else {
            if ( $val_regex ) {
                $self->warn( qq{$val passed as '$key' is a regex} );
                delete $parms->{$key};
                next;
            }
            if ( $val =~ /^\s|\s$/ ) {
                $self->warn( qq{'$val' is space-padded and cannot succeed} );
                delete $parms->{$key};
                next;
            }
        }
    } # for keys %parms

    return;
} # _clean_keys()


=head2 $mech->find_all_links( ... )

Returns all the links on the current page that match the criteria.  The
method for specifying link criteria is the same as in C<L</find_link()>>.
Each of the links returned is a L<WWW::Mechanize::Link> object.

In list context, C<find_all_links()> returns a list of the links.
Otherwise, it returns a reference to the list of links.

C<find_all_links()> with no parameters returns all links in the
page.

=cut

sub find_all_links {
    my $self = shift;
    return $self->find_link( @_, n=>'all' );
}

=head2 $mech->find_all_inputs( ... criteria ... )

find_all_inputs() returns an array of all the input controls in the
current form whose properties match all of the regexes passed in.
The controls returned are all descended from HTML::Form::Input.

If no criteria are passed, all inputs will be returned.

If there is no current page, there is no form on the current
page, or there are no submit controls in the current form
then the return will be an empty array.

You may use a regex or a literal string:

    # get all textarea controls whose names begin with "customer"
    my @customer_text_inputs = $mech->find_all_inputs(
        type       => 'textarea',
        name_regex => qr/^customer/,
    );

    # get all text or textarea controls called "customer"
    my @customer_text_inputs = $mech->find_all_inputs(
        type_regex => qr/^(text|textarea)$/,
        name       => 'customer',
    );

=cut

sub find_all_inputs {
    my $self = shift;
    my %criteria = @_;

    my $form = $self->current_form() or return;

    my @found;
    foreach my $input ( $form->inputs ) { # check every pattern for a match on the current hash
        my $matched = 1;
        foreach my $criterion ( sort keys %criteria ) { # Sort so we're deterministic
            my $field = $criterion;
            my $is_regex = ( $field =~ s/(?:_regex)$// );
            my $what = $input->{$field};
            $matched = defined($what) && (
                $is_regex
                    ? ( $what =~ $criteria{$criterion} )
                    : ( $what eq $criteria{$criterion} )
                );
            last if !$matched;
        }
        push @found, $input if $matched;
    }
    return @found;
}

=head2 $mech->find_all_submits( ... criteria ... )

C<find_all_submits()> does the same thing as C<find_all_inputs()>
except that it only returns controls that are submit controls,
ignoring other types of input controls like text and checkboxes.

=cut

sub find_all_submits {
    my $self = shift;

    return $self->find_all_inputs( @_, type_regex => qr/^(submit|image)$/ );
}


=head1 IMAGE METHODS

=head2 $mech->images

Lists all the images on the current page.  Each image is a
WWW::Mechanize::Image object. In list context, returns a list of all
images.  In scalar context, returns an array reference of all images.

=cut

sub images {
    my $self = shift;

    $self->_extract_images() unless $self->{images};

    return @{$self->{images}} if wantarray;
    return $self->{images};
}

=head2 $mech->find_image()

Finds an image in the current page. It returns a
L<WWW::Mechanize::Image> object which describes the image.  If it fails
to find an image it returns undef.

You can select which image to find by passing in one or more of these
key/value pairs:

=over 4

=item * C<< alt => 'string' >> and C<< alt_regex => qr/regex/, >>

C<alt> matches the ALT attribute of the image against I<string>, which must be an
exact match. To select a image with an ALT tag that is exactly "download", use

    $mech->find_image( alt => 'download' );

C<alt_regex> matches the ALT attribute of the image  against a regular
expression.  To select an image with an ALT attribute that has "download"
anywhere in it, regardless of case, use

    $mech->find_image( alt_regex => qr/download/i );

=item * C<< url => 'string', >> and C<< url_regex => qr/regex/, >>

Matches the URL of the image against I<string> or I<regex>, as appropriate.
The URL may be a relative URL, like F<foo/bar.html>, depending on how
it's coded on the page.

=item * C<< url_abs => string >> and C<< url_abs_regex => regex >>

Matches the absolute URL of the image against I<string> or I<regex>,
as appropriate.  The URL will be an absolute URL, even if it's relative
in the page.

=item * C<< tag => string >> and C<< tag_regex => regex >>

Matches the tag that the image came from against I<string> or I<regex>,
as appropriate.  The C<tag_regex> is probably most useful to check for
more than one tag, as in:

    $mech->find_image( tag_regex => qr/^(img|input)$/ );

The tags supported are C<< <img> >> and C<< <input> >>.

=back

If C<n> is not specified, it defaults to 1.  Therefore, if you don't
specify any parms, this method defaults to finding the first image on the
page.

Note that you can specify multiple ALT or URL parameters, which
will be ANDed together.  For example, to find the first image with
ALT text of "News" and with "cnn.com" in the URL, use:

    $mech->find_image( image => 'News', url_regex => qr/cnn\.com/ );

The return value is a reference to an array containing a
L<WWW::Mechanize::Image> object for every image in C<< $self->content >>.

=cut

sub find_image {
    my $self = shift;
    my %parms = ( n=>1, @_ );

    my $wantall = ( $parms{n} eq 'all' );

    $self->_clean_keys( \%parms, qr/^(n|(alt|url|url_abs|tag)(_regex)?)$/ );

    my @images = $self->images or return;

    my $nmatches = 0;
    my @matches;
    for my $image ( @images ) {
        if ( _match_any_image_parms($image,\%parms) ) {
            if ( $wantall ) {
                push( @matches, $image );
            }
            else {
                ++$nmatches;
                return $image if $nmatches >= $parms{n};
            }
        }
    } # for @images

    if ( $wantall ) {
        return @matches if wantarray;
        return \@matches;
    }

    return;
}

# Used by find_images to check for matches
# The logic is such that ALL parm criteria that are given must match
sub _match_any_image_parms {
    my $image = shift;
    my $p = shift;

    # No conditions, anything matches
    return 1 unless keys %$p;

    return if defined $p->{url}           && !($image->url eq $p->{url} );
    return if defined $p->{url_regex}     && !($image->url =~ $p->{url_regex} );
    return if defined $p->{url_abs}       && !($image->url_abs eq $p->{url_abs} );
    return if defined $p->{url_abs_regex} && !($image->url_abs =~ $p->{url_abs_regex} );
    return if defined $p->{alt}           && !(defined($image->alt) && $image->alt eq $p->{alt} );
    return if defined $p->{alt_regex}     && !(defined($image->alt) && $image->alt =~ $p->{alt_regex} );
    return if defined $p->{tag}           && !($image->tag && $image->tag eq $p->{tag} );
    return if defined $p->{tag_regex}     && !($image->tag && $image->tag =~ $p->{tag_regex} );

    # Success: everything that was defined passed.
    return 1;
}


=head2 $mech->find_all_images( ... )

Returns all the images on the current page that match the criteria.  The
method for specifying image criteria is the same as in C<L</find_image()>>.
Each of the images returned is a L<WWW::Mechanize::Image> object.

In list context, C<find_all_images()> returns a list of the images.
Otherwise, it returns a reference to the list of images.

C<find_all_images()> with no parameters returns all images in the page.

=cut

sub find_all_images {
    my $self = shift;
    return $self->find_image( @_, n=>'all' );
}

=head1 FORM METHODS

These methods let you work with the forms on a page.  The idea is
to choose a form that you'll later work with using the field methods
below.

=head2 $mech->forms

Lists all the forms on the current page.  Each form is an L<HTML::Form>
object.  In list context, returns a list of all forms.  In scalar
context, returns an array reference of all forms.

=cut

sub forms {
    my $self = shift;

    $self->_extract_forms() unless $self->{forms};

    return @{$self->{forms}} if wantarray;
    return $self->{forms};
}

sub current_form {
    my $self = shift;

    if ( !$self->{current_form} ) {
        $self->form_number(1);
    }

    return $self->{current_form};
}

=head2 $mech->form_number($number)

Selects the I<number>th form on the page as the target for subsequent
calls to C<L</field()>> and C<L</click()>>.  Also returns the form that was
selected.

If it is found, the form is returned as an L<HTML::Form> object and set internally
for later use with Mech's form methods such as C<L</field()>> and C<L</click()>>.

Emits a warning and returns undef if no form is found.

The first form is number 1, not zero.

=cut

sub form_number {
    my ($self, $form) = @_;
    # XXX Should we die if no $form is defined? Same question for form_name()

    my $forms = $self->forms;
    if ( $forms->[$form-1] ) {
        $self->{current_form} = $forms->[$form-1];
        return $self->{current_form};
    }

    return;
}

=head2 $mech->form_name( $name )

Selects a form by name.  If there is more than one form on the page
with that name, then the first one is used, and a warning is
generated.

If it is found, the form is returned as an L<HTML::Form> object and
set internally for later use with Mech's form methods such as
C<L</field()>> and C<L</click()>>.

Returns undef if no form is found.

=cut

sub form_name {
    my ($self, $form) = @_;

    my $temp;
    my @matches = grep {defined($temp = $_->attr('name')) and ($temp eq $form) } $self->forms;

    my $nmatches = @matches;
    if ( $nmatches > 0 ) {
        if ( $nmatches > 1 ) {
            $self->warn( "There are $nmatches forms named $form.  The first one was used." )
        }
        return $self->{current_form} = $matches[0];
    }

    return;
}

=head2 $mech->form_id( $name )

Selects a form by ID.  If there is more than one form on the page
with that ID, then the first one is used, and a warning is generated.

If it is found, the form is returned as an L<HTML::Form> object and
set internally for later use with Mech's form methods such as
C<L</field()>> and C<L</click()>>.

Returns undef if no form is found.

=cut

sub form_id {
    my ($self, $formid) = @_;

    my $temp;
    my @matches = grep { defined($temp = $_->attr('id')) and ($temp eq $formid) } $self->forms;
    if ( @matches ) {
        $self->warn( 'There are ', scalar @matches, " forms with ID $formid.  The first one was used." )
            if @matches > 1;
        return $self->{current_form} = $matches[0];
    }
    else {
        $self->warn( qq{ There is no form with ID "$formid"} );
        return undef;
    }
}


=head2 $mech->form_with_fields( @fields )

Selects a form by passing in a list of field names it must contain.  If there
is more than one form on the page with that matches, then the first one is used,
and a warning is generated.

If it is found, the form is returned as an L<HTML::Form> object and set internally
for later used with Mech's form methods such as C<L</field()>> and C<L</click()>>.

Returns undef if no form is found.

Note that this functionality requires libwww-perl 5.69 or higher.

=cut

sub form_with_fields {
    my ($self, @fields) = @_;
    die 'no fields provided' unless scalar @fields;

    my @matches;
    FORMS: for my $form (@{ $self->forms }) {
        my @fields_in_form = $form->param();
        for my $field (@fields) {
            next FORMS unless grep { $_ eq $field } @fields_in_form;
        }
        push @matches, $form;
    }

    my $nmatches = @matches;
    if ( $nmatches > 0 ) {
        if ( $nmatches > 1 ) {
            $self->warn( "There are $nmatches forms with the named fields.  The first one was used." )
        }
        return $self->{current_form} = $matches[0];
    }
    else {
        $self->warn( qq{There is no form with the requested fields} );
        return undef;
    }
}

=head1 FIELD METHODS

These methods allow you to set the values of fields in a given form.

=head2 $mech->field( $name, $value, $number )

=head2 $mech->field( $name, \@values, $number )

Given the name of a field, set its value to the value specified.
This applies to the current form (as set by the L</form_name()> or
L</form_number()> method or defaulting to the first form on the
page).

The optional I<$number> parameter is used to distinguish between two fields
with the same name.  The fields are numbered from 1.

=cut

sub field {
    my ($self, $name, $value, $number) = @_;
    $number ||= 1;

    my $form = $self->current_form();
    if ($number > 1) {
        $form->find_input($name, undef, $number)->value($value);
    }
    else {
        if ( ref($value) eq 'ARRAY' ) {
            $form->param($name, $value);
        }
        else {
            $form->value($name => $value);
        }
    }
}

=head2 $mech->select($name, $value)

=head2 $mech->select($name, \@values)

Given the name of a C<select> field, set its value to the value
specified.  If the field is not C<< <select multiple> >> and the
C<$value> is an array, only the B<first> value will be set.  [Note:
the documentation previously claimed that only the last value would
be set, but this was incorrect.]  Passing C<$value> as a hash with
an C<n> key selects an item by number (e.g.
C<< {n => 3} >> or C<< {n => [2,4]} >>).
The numbering starts at 1.  This applies to the current form.

If you have a field with C<< <select multiple> >> and you pass a single
C<$value>, then C<$value> will be added to the list of fields selected,
without clearing the others.  However, if you pass an array reference,
then all previously selected values will be cleared.

Returns true on successfully setting the value. On failure, returns
false and calls C<< $self>warn() >> with an error message.

=cut

sub select {
    my ($self, $name, $value) = @_;

    my $form = $self->current_form();

    my $input = $form->find_input($name);
    if (!$input) {
        $self->warn( qq{Input "$name" not found} );
        return;
    }

    if ($input->type ne 'option') {
        $self->warn( qq{Input "$name" is not type "select"} );
        return;
    }

    # For $mech->select($name, {n => 3}) or $mech->select($name, {n => [2,4]}),
    # transform the 'n' number(s) into value(s) and put it in $value.
    if (ref($value) eq 'HASH') {
        for (keys %$value) {
            $self->warn(qq{Unknown select value parameter "$_"})
              unless $_ eq 'n';
        }

        if (defined($value->{n})) {
            my @inputs = $form->find_input($name, 'option');
            my @values = ();
            # distinguish between multiple and non-multiple selects
            # (see INPUTS section of `perldoc HTML::Form`)
            if (@inputs == 1) {
                @values = $inputs[0]->possible_values();
            }
            else {
                foreach my $input (@inputs) {
                    my @possible = $input->possible_values();
                    push @values, pop @possible;
                }
            }

            my $n = $value->{n};
            if (ref($n) eq 'ARRAY') {
                $value = [];
                for (@$n) {
                    unless (/^\d+$/) {
                        $self->warn(qq{"n" value "$_" is not a positive integer});
                        return;
                    }
                    push @$value, $values[$_ - 1];  # might be undef
                }
            }
            elsif (!ref($n) && $n =~ /^\d+$/) {
                $value = $values[$n - 1];           # might be undef
            }
            else {
                $self->warn('"n" value is not a positive integer or an array ref');
                return;
            }
        }
        else {
            $self->warn('Hash value is invalid');
            return;
        }
    } # hashref

    if (ref($value) eq 'ARRAY') {
        $form->param($name, $value);
        return 1;
    }

    $form->value($name => $value);
    return 1;
}

=head2 $mech->set_fields( $name => $value ... )

This method sets multiple fields of the current form. It takes a list
of field name and value pairs. If there is more than one field with
the same name, the first one found is set. If you want to select which
of the duplicate field to set, use a value which is an anonymous array
which has the field value and its number as the 2 elements.

        # set the second foo field
        $mech->set_fields( $name => [ 'foo', 2 ] );

The fields are numbered from 1.

This applies to the current form.

=cut

sub set_fields {
    my $self = shift;
    my %fields = @_;

    my $form = $self->current_form or $self->die( 'No form defined' );

    while ( my ( $field, $value ) = each %fields ) {
        if ( ref $value eq 'ARRAY' ) {
            $form->find_input( $field, undef,
                         $value->[1])->value($value->[0] );
        }
        else {
            $form->value($field => $value);
        }
    } # while
} # set_fields()

=head2 $mech->set_visible( @criteria )

This method sets fields of the current form without having to know
their names.  So if you have a login screen that wants a username and
password, you do not have to fetch the form and inspect the source (or
use the F<mech-dump> utility, installed with WWW::Mechanize) to see
what the field names are; you can just say

    $mech->set_visible( $username, $password );

and the first and second fields will be set accordingly.  The method
is called set_I<visible> because it acts only on visible fields;
hidden form inputs are not considered.  The order of the fields is
the order in which they appear in the HTML source which is nearly
always the order anyone viewing the page would think they are in,
but some creative work with tables could change that; caveat user.

Each element in C<@criteria> is either a field value or a field
specifier.  A field value is a scalar.  A field specifier allows
you to specify the I<type> of input field you want to set and is
denoted with an arrayref containing two elements.  So you could
specify the first radio button with

    $mech->set_visible( [ radio => 'KCRW' ] );

Field values and specifiers can be intermixed, hence

    $mech->set_visible( 'fred', 'secret', [ option => 'Checking' ] );

would set the first two fields to "fred" and "secret", and the I<next>
C<OPTION> menu field to "Checking".

The possible field specifier types are: "text", "password", "hidden",
"textarea", "file", "image", "submit", "radio", "checkbox" and "option".

C<set_visible> returns the number of values set.

=cut

sub set_visible {
    my $self = shift;

    my $form = $self->current_form;
    my @inputs = $form->inputs;

    my $num_set = 0;
    for my $value ( @_ ) {
        # Handle type/value pairs an arrayref
        if ( ref $value eq 'ARRAY' ) {
            my ( $type, $value ) = @$value;
            while ( my $input = shift @inputs ) {
                next if $input->type eq 'hidden';
                if ( $input->type eq $type ) {
                    $input->value( $value );
                    $num_set++;
                    last;
                }
            } # while
        }
        # by default, it's a value
        else {
            while ( my $input = shift @inputs ) {
                next if $input->type eq 'hidden';
                $input->value( $value );
                $num_set++;
                last;
            } # while
        }
    } # for

    return $num_set;
} # set_visible()

=head2 $mech->tick( $name, $value [, $set] )

"Ticks" the first checkbox that has both the name and value associated
with it on the current form.  Dies if there is no named check box for
that value.  Passing in a false value as the third optional argument
will cause the checkbox to be unticked.

=cut

sub tick {
    my $self = shift;
    my $name = shift;
    my $value = shift;
    my $set = @_ ? shift : 1;  # default to 1 if not passed

    # loop though all the inputs
    my $index = 0;
    while ( my $input = $self->current_form->find_input( $name, 'checkbox', $index ) ) {
        # Can't guarantee that the first element will be undef and the second
        # element will be the right name
        foreach my $val ($input->possible_values()) {
            next unless defined $val;
            if ($val eq $value) {
                $input->value($set ? $value : undef);
                return;
            }
        }

        # move onto the next input
        $index++;
    } # while

    # got self far?  Didn't find anything
    $self->warn( qq{No checkbox "$name" for value "$value" in form} );
} # tick()

=head2 $mech->untick($name, $value)

Causes the checkbox to be unticked.  Shorthand for
C<tick($name,$value,undef)>

=cut

sub untick {
    shift->tick(shift,shift,undef);
}

=head2 $mech->value( $name [, $number] )

Given the name of a field, return its value. This applies to the current
form.

The optional I<$number> parameter is used to distinguish between two fields
with the same name.  The fields are numbered from 1.

If the field is of type file (file upload field), the value is always
cleared to prevent remote sites from downloading your local files.
To upload a file, specify its file name explicitly.

=cut

sub value {
    my $self = shift;
    my $name = shift;
    my $number = shift || 1;

    my $form = $self->current_form;
    if ( $number > 1 ) {
        return $form->find_input( $name, undef, $number )->value();
    }
    else {
        return $form->value( $name );
    }
} # value

=head2 $mech->click( $button [, $x, $y] )

Has the effect of clicking a button on the current form.  The first
argument is the name of the button to be clicked.  The second and
third arguments (optional) allow you to specify the (x,y) coordinates
of the click.

If there is only one button on the form, C<< $mech->click() >> with
no arguments simply clicks that one button.

Returns an L<HTTP::Response> object.

=cut

sub click {
    my ($self, $button, $x, $y) = @_;
    for ($x, $y) { $_ = 1 unless defined; }
    my $request = $self->current_form->click($button, $x, $y);
    return $self->request( $request );
}

=head2 $mech->click_button( ... )

Has the effect of clicking a button on the current form by specifying
its name, value, or index.  Its arguments are a list of key/value
pairs.  Only one of name, number, input or value must be specified in
the keys.

=over 4

=item * C<< name => name >>

Clicks the button named I<name> in the current form.

=item * C<< number => n >>

Clicks the I<n>th button in the current form. Numbering starts at 1.

=item * C<< value => value >>

Clicks the button with the value I<value> in the current form.

=item * C<< input => $inputobject >>

Clicks on the button referenced by $inputobject, an instance of
L<HTML::Form::SubmitInput> obtained e.g. from

    $mech->current_form()->find_input( undef, 'submit' )

$inputobject must belong to the current form.

=item * C<< x => x >>

=item * C<< y => y >>

These arguments (optional) allow you to specify the (x,y) coordinates
of the click.

=back

=cut

sub click_button {
    my $self = shift;
    my %args = @_;

    for ( keys %args ) {
        if ( !/^(number|name|value|input|x|y)$/ ) {
            $self->warn( qq{Unknown click_button parameter "$_"} );
        }
    }

    for ($args{x}, $args{y}) {
        $_ = 1 unless defined;
    }

    my $form = $self->current_form or $self->die( 'click_button: No form has been selected' );

    my $request;
    if ( $args{name} ) {
        $request = $form->click( $args{name}, $args{x}, $args{y} );
    }
    elsif ( $args{number} ) {
        my $input = $form->find_input( undef, 'submit', $args{number} );
        $request = $input->click( $form, $args{x}, $args{y} );
    }
    elsif ( $args{input} ) {
        $request = $args{input}->click( $form, $args{x}, $args{y} );
    }
    elsif ( $args{value} ) {
        my $i = 1;
        while ( my $input = $form->find_input(undef, 'submit', $i) ) {
            if ( $args{value} && ($args{value} eq $input->value) ) {
                $request = $input->click( $form, $args{x}, $args{y} );
                last;
            }
            $i++;
        } # while
    } # $args{value}

    return $self->request( $request );
}

=head2 $mech->submit()

Submits the page, without specifying a button to click.  Actually,
no button is clicked at all.

Returns an L<HTTP::Response> object.

This used to be a synonym for C<< $mech->click( 'submit' ) >>, but is no
longer so.

=cut

sub submit {
    my $self = shift;

    my $request = $self->current_form->make_request;
    return $self->request( $request );
}

=head2 $mech->submit_form( ... )

This method lets you select a form from the previously fetched page,
fill in its fields, and submit it. It combines the form_number/form_name,
set_fields and click methods into one higher level call. Its arguments
are a list of key/value pairs, all of which are optional.

=over 4

=item * C<< fields => \%fields >>

Specifies the fields to be filled in the current form.

=item * C<< with_fields => \%fields >>

Probably all you need for the common case. It combines a smart form selector
and data setting in one operation. It selects the first form that contains all
fields mentioned in C<\%fields>.  This is nice because you don't need to know
the name or number of the form to do this.

(calls C<L</form_with_fields()>> and C<L</set_fields()>>).

If you choose this, the form_number, form_name, form_id and fields options will be ignored.

=item * C<< form_number => n >>

Selects the I<n>th form (calls C<L</form_number()>>).  If this parm is not
specified, the currently-selected form is used.

=item * C<< form_name => name >>

Selects the form named I<name> (calls C<L</form_name()>>)

=item * C<< form_id => ID >>

Selects the form with ID I<ID> (calls C<L</form_id()>>)

=item * C<< button => button >>

Clicks on button I<button> (calls C<L</click()>>)

=item * C<< x => x, y => y >>

Sets the x or y values for C<L</click()>>

=back

If no form is selected, the first form found is used.

If I<button> is not passed, then the C<L</submit()>> method is used instead.

If you want to submit a file and get its content from a scalar rather
than a file in the filesystem, you can use:

    $mech->submit_form(with_fields => { logfile => [ [ undef, 'whatever', Content => $content ], 1 ] } );

Returns an L<HTTP::Response> object.

=cut

sub submit_form {
    my( $self, %args ) = @_;

    for ( keys %args ) {
        if ( !/^(form_(number|name|fields|id)|(with_)?fields|button|x|y)$/ ) {
            # XXX Why not die here?
            $self->warn( qq{Unknown submit_form parameter "$_"} );
        }
    }

    my $fields;
    for (qw/with_fields fields/) {
        if ($args{$_}) {
            if ( ref $args{$_} eq 'HASH' ) {
                $fields = $args{$_};
            }
            else {
                die "$_ arg to submit_form must be a hashref";
            }
            last;
        }
    }

    if ( $args{with_fields} ) {
        $fields || die q{must submit some 'fields' with with_fields};
        $self->form_with_fields(keys %{$fields}) or die "There is no form with the requested fields";
    }
    elsif ( my $form_number = $args{form_number} ) {
        $self->form_number( $form_number ) or die "There is no form numbered $form_number";
    }
    elsif ( my $form_name = $args{form_name} ) {
        $self->form_name( $form_name ) or die qq{There is no form named "$form_name"};
    }
    elsif ( my $form_id = $args{form_id} ) {
        $self->form_id( $form_id ) or die qq{There is no form with ID "$form_id"};
    }
    else {
        # No form selector was used.
        # Maybe a form was set separately, or we'll default to the first form.
    }

    $self->set_fields( %{$fields} ) if $fields;

    my $response;
    if ( $args{button} ) {
        $response = $self->click( $args{button}, $args{x} || 0, $args{y} || 0 );
    }
    else {
        $response = $self->submit();
    }

    return $response;
}

=head1 MISCELLANEOUS METHODS

=head2 $mech->add_header( name => $value [, name => $value... ] )

Sets HTTP headers for the agent to add or remove from the HTTP request.

    $mech->add_header( Encoding => 'text/klingon' );

If a I<value> is C<undef>, then that header will be removed from any
future requests.  For example, to never send a Referer header:

    $mech->add_header( Referer => undef );

If you want to delete a header, use C<delete_header>.

Returns the number of name/value pairs added.

B<NOTE>: This method was very different in WWW::Mechanize before 1.00.
Back then, the headers were stored in a package hash, not as a member of
the object instance.  Calling C<add_header()> would modify the headers
for every WWW::Mechanize object, even after your object no longer existed.

=cut

sub add_header {
    my $self = shift;
    my $npairs = 0;

    while ( @_ ) {
        my $key = shift;
        my $value = shift;
        ++$npairs;

        $self->{headers}{$key} = $value;
    }

    return $npairs;
}

=head2 $mech->delete_header( name [, name ... ] )

Removes HTTP headers from the agent's list of special headers.  For
instance, you might need to do something like:

    # Don't send a Referer for this URL
    $mech->add_header( Referer => undef );

    # Get the URL
    $mech->get( $url );

    # Back to the default behavior
    $mech->delete_header( 'Referer' );

=cut

sub delete_header {
    my $self = shift;

    while ( @_ ) {
        my $key = shift;

        delete $self->{headers}{$key};
    }

    return;
}


=head2 $mech->quiet(true/false)

Allows you to suppress warnings to the screen.

    $mech->quiet(0); # turns on warnings (the default)
    $mech->quiet(1); # turns off warnings
    $mech->quiet();  # returns the current quietness status

=cut

sub quiet {
    my $self = shift;

    $self->{quiet} = $_[0] if @_;

    return $self->{quiet};
}

=head2 $mech->stack_depth( $max_depth )

Get or set the page stack depth. Use this if you're doing a lot of page
scraping and running out of memory.

A value of 0 means "no history at all."  By default, the max stack depth
is humongously large, effectively keeping all history.

=cut

sub stack_depth {
    my $self = shift;
    $self->{stack_depth} = shift if @_;
    return $self->{stack_depth};
}

=head2 $mech->save_content( $filename )

Dumps the contents of C<< $mech->content >> into I<$filename>.
I<$filename> will be overwritten.  Dies if there are any errors.

If the content type does not begin with "text/", then the content
is saved in binary mode.

=cut

sub save_content {
    my $self = shift;
    my $filename = shift;

    open( my $fh, '>', $filename ) or $self->die( "Unable to create $filename: $!" );
    binmode $fh unless $self->content_type =~ m{^text/};
    print {$fh} $self->content or $self->die( "Unable to write to $filename: $!" );
    close $fh or $self->die( "Unable to close $filename: $!" );

    return;
}


=head2 $mech->dump_headers( [$fh] )

Prints a dump of the HTTP response headers for the most recent
response.  If I<$fh> is not specified or is undef, it dumps to
STDOUT.

Unlike the rest of the dump_* methods, you cannot specify a filehandle
to print to.

=cut

sub dump_headers {
    my $self = shift;
    my $fh   = shift || \*STDOUT;

    print {$fh} $self->response->headers_as_string;

    return;
}


=head2 $mech->dump_links( [[$fh], $absolute] )

Prints a dump of the links on the current page to I<$fh>.  If I<$fh>
is not specified or is undef, it dumps to STDOUT.

If I<$absolute> is true, links displayed are absolute, not relative.

=cut

sub dump_links {
    my $self = shift;
    my $fh = shift || \*STDOUT;
    my $absolute = shift;

    for my $link ( $self->links ) {
        my $url = $absolute ? $link->url_abs : $link->url;
        $url = '' if not defined $url;
        print {$fh} $url, "\n";
    }
    return;
}

=head2 $mech->dump_images( [[$fh], $absolute] )

Prints a dump of the images on the current page to I<$fh>.  If I<$fh>
is not specified or is undef, it dumps to STDOUT.

If I<$absolute> is true, links displayed are absolute, not relative.

=cut

sub dump_images {
    my $self = shift;
    my $fh = shift || \*STDOUT;
    my $absolute = shift;

    for my $image ( $self->images ) {
        my $url = $absolute ? $image->url_abs : $image->url;
        $url = '' if not defined $url;
        print {$fh} $url, "\n";
    }
    return;
}

=head2 $mech->dump_forms( [$fh] )

Prints a dump of the forms on the current page to I<$fh>.  If I<$fh>
is not specified or is undef, it dumps to STDOUT.

=cut

sub dump_forms {
    my $self = shift;
    my $fh = shift || \*STDOUT;

    for my $form ( $self->forms ) {
        print {$fh} $form->dump, "\n";
    }
    return;
}

=head2 $mech->dump_text( [$fh] )

Prints a dump of the text on the current page to I<$fh>.  If I<$fh>
is not specified or is undef, it dumps to STDOUT.

=cut

sub dump_text {
    my $self = shift;
    my $fh = shift || \*STDOUT;
    my $absolute = shift;

    print {$fh} $self->text, "\n";

    return;
}


=head1 OVERRIDDEN LWP::UserAgent METHODS

=head2 $mech->clone()

Clone the mech object.  The clone will be using the same cookie jar
as the original mech.

=cut

sub clone {
    my $self  = shift;
    my $clone = $self->SUPER::clone();

    $clone->cookie_jar( $self->cookie_jar );

    return $clone;
}


=head2 $mech->redirect_ok()

An overloaded version of C<redirect_ok()> in L<LWP::UserAgent>.
This method is used to determine whether a redirection in the request
should be followed.

Note that WWW::Mechanize's constructor pushes POST on to the agent's
C<requests_redirectable> list.

=cut

sub redirect_ok {
    my $self = shift;
    my $prospective_request = shift;
    my $response = shift;

    my $ok = $self->SUPER::redirect_ok( $prospective_request, $response );
    if ( $ok ) {
        $self->{redirected_uri} = $prospective_request->uri;
    }

    return $ok;
}


=head2 $mech->request( $request [, $arg [, $size]])

Overloaded version of C<request()> in L<LWP::UserAgent>.  Performs
the actual request.  Normally, if you're using WWW::Mechanize, it's
because you don't want to deal with this level of stuff anyway.

Note that C<$request> will be modified.

Returns an L<HTTP::Response> object.

=cut

sub request {
    my $self = shift;
    my $request = shift;

    $request = $self->_modify_request( $request );

    if ( $request->method eq 'GET' || $request->method eq 'POST' ) {
        $self->_push_page_stack();
    }

    return $self->_update_page($request, $self->_make_request( $request, @_ ));
}

=head2 $mech->update_html( $html )

Allows you to replace the HTML that the mech has found.  Updates the
forms and links parse-trees that the mech uses internally.

Say you have a page that you know has malformed output, and you want to
update it so the links come out correctly:

    my $html = $mech->content;
    $html =~ s[</option>.{0,3}</td>][</option></select></td>]isg;
    $mech->update_html( $html );

This method is also used internally by the mech itself to update its
own HTML content when loading a page. This means that if you would
like to I<systematically> perform the above HTML substitution, you
would overload I<update_html> in a subclass thusly:

   package MyMech;
   use base 'WWW::Mechanize';

   sub update_html {
       my ($self, $html) = @_;
       $html =~ s[</option>.{0,3}</td>][</option></select></td>]isg;
       $self->WWW::Mechanize::update_html( $html );
   }

If you do this, then the mech will use the tidied-up HTML instead of
the original both when parsing for its own needs, and for returning to
you through L</content>.

Overloading this method is also the recommended way of implementing
extra validation steps (e.g. link checkers) for every HTML page
received.  L</warn> and L</die> would then come in handy to signal
validation errors.

=cut

sub update_html {
    my $self = shift;
    my $html = shift;

    $self->_reset_page;
    $self->{ct} = 'text/html';
    $self->{content} = $html;

    return;
}

=head2 $mech->credentials( $username, $password )

Provide credentials to be used for HTTP Basic authentication for
all sites and realms until further notice.

The four argument form described in L<LWP::UserAgent> is still
supported.

=cut

sub credentials {
    my $self = shift;

    # The lastest LWP::UserAgent also supports 2 arguments,
    # in which case the first is host:port
    if (@_ == 4 || (@_ == 2 && $_[0] =~ /:\d+$/)) {
        return $self->SUPER::credentials(@_);
    }

    @_ == 2
        or $self->die( 'Invalid # of args for overridden credentials()' );

    return @$self{qw( __username __password )} = @_;
}

=head2 $mech->get_basic_credentials( $realm, $uri, $isproxy )

Returns the credentials for the realm and URI.

=cut

sub get_basic_credentials {
    my $self = shift;
    my @cred = grep { defined } @$self{qw( __username __password )};
    return @cred if @cred == 2;
    return $self->SUPER::get_basic_credentials(@_);
}

=head2 $mech->clear_credentials()

Remove any credentials set up with C<credentials()>.

=cut

sub clear_credentials {
    my $self = shift;
    delete @$self{qw( __username __password )};
}

=head1 INHERITED UNCHANGED LWP::UserAgent METHODS

As a sublass of L<LWP::UserAgent>, WWW::Mechanize inherits all of
L<LWP::UserAgent>'s methods.  Many of which are overridden or
extended. The following methods are inherited unchanged. View the
L<LWP::UserAgent> documentation for their implementation descriptions.

This is not meant to be an inclusive list.  LWP::UA may have added
others.

=head2 $mech->head()

Inherited from L<LWP::UserAgent>.

=head2 $mech->post()

Inherited from L<LWP::UserAgent>.

=head2 $mech->mirror()

Inherited from L<LWP::UserAgent>.

=head2 $mech->simple_request()

Inherited from L<LWP::UserAgent>.

=head2 $mech->is_protocol_supported()

Inherited from L<LWP::UserAgent>.

=head2 $mech->prepare_request()

Inherited from L<LWP::UserAgent>.

=head2 $mech->progress()

Inherited from L<LWP::UserAgent>.

=head1 INTERNAL-ONLY METHODS

These methods are only used internally.  You probably don't need to
know about them.

=head2 $mech->_update_page($request, $response)

Updates all internal variables in $mech as if $request was just
performed, and returns $response. The page stack is B<not> altered by
this method, it is up to caller (e.g. L</request>) to do that.

=cut

sub _update_page {
    my ($self, $request, $res) = @_;

    $self->{req} = $request;
    $self->{redirected_uri} = $request->uri->as_string;

    $self->{res} = $res;

    $self->{status}  = $res->code;
    $self->{base}    = $res->base;
    $self->{ct}      = $res->content_type || '';

    if ( $res->is_success ) {
        $self->{uri} = $self->{redirected_uri};
        $self->{last_uri} = $self->{uri};
    }

    if ( $res->is_error ) {
        if ( $self->{autocheck} ) {
            $self->die( 'Error ', $request->method, 'ing ', $request->uri, ': ', $res->message );
        }
    }

    $self->_reset_page;

    # Try to decode the content. Undef will be returned if there's nothing to decompress.
    # See docs in HTTP::Message for details. Do we need to expose the options there?
    my $content = $res->decoded_content();
    $content = $res->content if (not defined $content);

    $content .= _taintedness();

    if ($self->is_html) {
        $self->update_html($content);
    }
    else {
        $self->{content} = $content;
    }

    return $res;
} # _update_page

our $_taintbrush;

# This is lifted wholesale from Test::Taint
sub _taintedness {
    return $_taintbrush if defined $_taintbrush;

    # Somehow we need to get some taintedness into our $_taintbrush.
    # Let's try the easy way first. Either of these should be
    # tainted, unless somebody has untainted them, so this
    # will almost always work on the first try.
    # (Unless, of course, taint checking has been turned off!)
    $_taintbrush = substr("$0$^X", 0, 0);
    return $_taintbrush if _is_tainted( $_taintbrush );

    # Let's try again. Maybe somebody cleaned those.
    $_taintbrush = substr(join('', grep { defined } @ARGV, %ENV), 0, 0);
    return $_taintbrush if _is_tainted( $_taintbrush );

    # If those don't work, go try to open some file from some unsafe
    # source and get data from them.  That data is tainted.
    # (Yes, even reading from /dev/null works!)
    for my $filename ( qw(/dev/null / . ..), values %INC, $0, $^X ) {
        if ( open my $fh, '<', $filename ) {
            my $data;
            if ( defined sysread $fh, $data, 1 ) {
                $_taintbrush = substr( $data, 0, 0 );
                last if _is_tainted( $_taintbrush );
            }
        }
    }

    # Sanity check
    die "Our taintbrush should have zero length!" if length $_taintbrush;

    return $_taintbrush;
}

sub _is_tainted {
    no warnings qw(void uninitialized);

    return !eval { join('', shift), kill 0; 1 };
} # _is_tainted


=head2 $mech->_modify_request( $req )

Modifies a L<HTTP::Request> before the request is sent out,
for both GET and POST requests.

We add a C<Referer> header, as well as header to note that we can accept gzip
encoded content, if L<Compress::Zlib> is installed.

=cut

sub _modify_request {
    my $self = shift;
    my $req = shift;

    # add correct Accept-Encoding header to restore compliance with
    # http://www.freesoft.org/CIE/RFC/2068/158.htm
    # http://use.perl.org/~rhesa/journal/25952
    if (not $req->header( 'Accept-Encoding' ) ) {
        # "identity" means "please! unencoded content only!"
        $req->header( 'Accept-Encoding', $HAS_ZLIB ? 'gzip' : 'identity' );
    }

    my $last = $self->{last_uri};
    if ( $last ) {
        $last = $last->as_string if ref($last);
        $req->header( Referer => $last );
    }
    while ( my($key,$value) = each %{$self->{headers}} ) {
        if ( defined $value ) {
            $req->header( $key => $value );
        }
        else {
            $req->remove_header( $key );
        }
    }

    return $req;
}


=head2 $mech->_make_request()

Convenience method to make it easier for subclasses like
L<WWW::Mechanize::Cached> to intercept the request.

=cut

sub _make_request {
    my $self = shift;
    return $self->SUPER::request(@_);
}

=head2 $mech->_reset_page()

Resets the internal fields that track page parsed stuff.

=cut

sub _reset_page {
    my $self = shift;

    $self->{links}        = undef;
    $self->{images}       = undef;
    $self->{forms}        = undef;
    $self->{current_form} = undef;
    $self->{title}        = undef;
    $self->{text}         = undef;

    return;
}

=head2 $mech->_extract_links()

Extracts links from the content of a webpage, and populates the C<{links}>
property with L<WWW::Mechanize::Link> objects.

=cut

my %link_tags = (
    a      => 'href',
    area   => 'href',
    frame  => 'src',
    iframe => 'src',
    link   => 'href',
    meta   => 'content',
);

sub _extract_links {
    my $self = shift;


    $self->{links} = [];
    if ( defined $self->{content} ) {
        my $parser = HTML::TokeParser->new(\$self->{content});
        while ( my $token = $parser->get_tag( keys %link_tags ) ) {
            my $link = $self->_link_from_token( $token, $parser );
            push( @{$self->{links}}, $link ) if $link;
        } # while
    }

    return;
}


my %image_tags = (
    img   => 'src',
    input => 'src',
);

sub _extract_images {
    my $self = shift;

    $self->{images} = [];

    if ( defined $self->{content} ) {
        my $parser = HTML::TokeParser->new(\$self->{content});
        while ( my $token = $parser->get_tag( keys %image_tags ) ) {
            my $image = $self->_image_from_token( $token, $parser );
            push( @{$self->{images}}, $image ) if $image;
        } # while
    }

    return;
}

sub _image_from_token {
    my $self = shift;
    my $token = shift;
    my $parser = shift;

    my $tag = $token->[0];
    my $attrs = $token->[1];

    if ( $tag eq 'input' ) {
        my $type = $attrs->{type} or return;
        return unless $type eq 'image';
    }

    require WWW::Mechanize::Image;
    return
        WWW::Mechanize::Image->new({
            tag     => $tag,
            base    => $self->base,
            url     => $attrs->{src},
            name    => $attrs->{name},
            height  => $attrs->{height},
            width   => $attrs->{width},
            alt     => $attrs->{alt},
        });
}

sub _link_from_token {
    my $self = shift;
    my $token = shift;
    my $parser = shift;

    my $tag = $token->[0];
    my $attrs = $token->[1];
    my $url = $attrs->{$link_tags{$tag}};

    my $text;
    my $name;
    if ( $tag eq 'a' ) {
        $text = $parser->get_trimmed_text("/$tag");
        $text = '' unless defined $text;

        my $onClick = $attrs->{onclick};
        if ( $onClick && ($onClick =~ /^window\.open\(\s*'([^']+)'/) ) {
            $url = $1;
        }
    } # a

    # Of the tags we extract from, only 'AREA' has an alt tag
    # The rest should have a 'name' attribute.
    # ... but we don't do anything with that bit of wisdom now.

    $name = $attrs->{name};

    if ( $tag eq 'meta' ) {
        my $equiv = $attrs->{'http-equiv'};
        my $content = $attrs->{'content'};
        return unless $equiv && (lc $equiv eq 'refresh') && defined $content;

        if ( $content =~ /^\d+\s*;\s*url\s*=\s*(\S+)/i ) {
            $url = $1;
            $url =~ s/^"(.+)"$/$1/ or $url =~ s/^'(.+)'$/$1/;
        }
        else {
            undef $url;
        }
    } # meta

    return unless defined $url;   # probably just a name link or <AREA NOHREF...>

    require WWW::Mechanize::Link;
    return
        WWW::Mechanize::Link->new({
            url  => $url,
            text => $text,
            name => $name,
            tag  => $tag,
            base => $self->base,
            attrs => $attrs,
        });
} # _link_from_token


sub _extract_forms {
    my $self = shift;

    my @forms = HTML::Form->parse( $self->content, $self->base );
    $self->{forms} = \@forms;
    for my $form ( @forms ) {
        for my $input ($form->inputs) {
             if ($input->type eq 'file') {
                 $input->value( undef );
             }
        }
    }

    return;
}

=head2 $mech->_push_page_stack()

The agent keeps a stack of visited pages, which it can pop when it needs
to go BACK and so on.

The current page needs to be pushed onto the stack before we get a new
page, and the stack needs to be popped when BACK occurs.

Neither of these take any arguments, they just operate on the $mech
object.

=cut

sub _push_page_stack {
    my $self = shift;

    my $req = $self->{req};
    my $res = $self->{res};

    return unless $req && $res && $self->stack_depth;

    # Don't push anything if it's a virgin object
    my $stack = $self->{page_stack} ||= [];
    if ( @{$stack} >= $self->stack_depth ) {
        shift @{$stack};
    }
    push( @{$stack}, { req => $req, res => $res } );

    return 1;
}

=head2 warn( @messages )

Centralized warning method, for diagnostics and non-fatal problems.
Defaults to calling C<CORE::warn>, but may be overridden by setting
C<onwarn> in the constructor.

=cut

sub warn {
    my $self = shift;

    return unless my $handler = $self->{onwarn};

    return if $self->quiet;

    return $handler->(@_);
}

=head2 die( @messages )

Centralized error method.  Defaults to calling C<CORE::die>, but
may be overridden by setting C<onerror> in the constructor.

=cut

sub die {
    my $self = shift;

    return unless my $handler = $self->{onerror};

    return $handler->(@_);
}


# NOT an object method!
sub _warn {
    require Carp;
    return &Carp::carp; ## no critic
}

# NOT an object method!
sub _die {
    require Carp;
    return &Carp::croak; ## no critic
}

1; # End of module

__END__

=head1 WWW::MECHANIZE'S GIT REPOSITORY

WWW::Mechanize is hosted at GitHub, though the bug tracker still
lives at Google Code.

Repository: https://github.com/bestpractical/www-mechanize/.  
Bugs: http://code.google.com/p/www-mechanize/issues

=head1 OTHER DOCUMENTATION

=head2 I<Spidering Hacks>, by Kevin Hemenway and Tara Calishain

I<Spidering Hacks> from O'Reilly
(L<http://www.oreilly.com/catalog/spiderhks/>) is a great book for anyone
wanting to know more about screen-scraping and spidering.

There are six hacks that use Mech or a Mech derivative:

=over 4

=item #21 WWW::Mechanize 101

=item #22 Scraping with WWW::Mechanize

=item #36 Downloading Images from Webshots

=item #44 Archiving Yahoo! Groups Messages with WWW::Yahoo::Groups

=item #64 Super Author Searching

=item #73 Scraping TV Listings

=back

The book was also positively reviewed on Slashdot:
L<http://books.slashdot.org/article.pl?sid=03/12/11/2126256>

=head1 ONLINE RESOURCES AND SUPPORT

=over 4

=item * WWW::Mechanize mailing list

The Mech mailing list is at
L<http://groups.google.com/group/www-mechanize-users> and is specific
to Mechanize, unlike the LWP mailing list below.  Although it is a
users list, all development discussion takes place here, too.

=item * LWP mailing list

The LWP mailing list is at
L<http://lists.perl.org/showlist.cgi?name=libwww>, and is more
user-oriented and well-populated than the WWW::Mechanize list.

=item * Perlmonks

L<http://perlmonks.org> is an excellent community of support, and
many questions about Mech have already been answered there.

=item * L<WWW::Mechanize::Examples>

A random array of examples submitted by users, included with the
Mechanize distribution.

=back

=head1 ARTICLES ABOUT WWW::MECHANIZE

=over 4

=item * L<http://www-128.ibm.com/developerworks/linux/library/wa-perlsecure.html>

IBM article "Secure Web site access with Perl"

=item * L<http://www.oreilly.com/catalog/googlehks2/chapter/hack84.pdf>

Leland Johnson's hack #84 in I<Google Hacks, 2nd Edition> is
an example of a production script that uses WWW::Mechanize and
HTML::TableContentParser. It takes in keywords and returns the estimated
price of these keywords on Google's AdWords program.

=item * L<http://www.perl.com/pub/a/2004/06/04/recorder.html>

Linda Julien writes about using HTTP::Recorder to create WWW::Mechanize
scripts.

=item * L<http://www.developer.com/lang/other/article.php/3454041>

Jason Gilmore's article on using WWW::Mechanize for scraping sales
information from Amazon and eBay.

=item * L<http://www.perl.com/pub/a/2003/01/22/mechanize.html>

Chris Ball's article about using WWW::Mechanize for scraping TV
listings.

=item * L<http://www.stonehenge.com/merlyn/LinuxMag/col47.html>

Randal Schwartz's article on scraping Yahoo News for images.  It's
already out of date: He manually walks the list of links hunting
for matches, which wouldn't have been necessary if the C<find_link()>
method existed at press time.

=item * L<http://www.perladvent.org/2002/16th/>

WWW::Mechanize on the Perl Advent Calendar, by Mark Fowler.

=item * L<http://www.linux-magazin.de/Artikel/ausgabe/2004/03/perl/perl.html>

Michael Schilli's article on Mech and L<WWW::Mechanize::Shell> for the
German magazine I<Linux Magazin>.

=back

=head2 Other modules that use Mechanize

Here are modules that use or subclass Mechanize.  Let me know of any others:

=over 4

=item * L<Finance::Bank::LloydsTSB>

=item * L<HTTP::Recorder>

Acts as a proxy for web interaction, and then generates WWW::Mechanize scripts.

=item * L<Win32::IE::Mechanize>

Just like Mech, but using Microsoft Internet Explorer to do the work.

=item * L<WWW::Bugzilla>

=item * L<WWW::CheckSite>

=item * L<WWW::Google::Groups>

=item * L<WWW::Hotmail>

=item * L<WWW::Mechanize::Cached>

=item * L<WWW::Mechanize::FormFiller>

=item * L<WWW::Mechanize::Shell>

=item * L<WWW::Mechanize::Sleepy>

=item * L<WWW::Mechanize::SpamCop>

=item * L<WWW::Mechanize::Timed>

=item * L<WWW::SourceForge>

=item * L<WWW::Yahoo::Groups>

=back

=head1 ACKNOWLEDGEMENTS

Thanks to the numerous people who have helped out on WWW::Mechanize in
one way or another, including
Kirrily Robert for the original C<WWW::Automate>,
Lyle Hopkins,
Damien Clark,
Ansgar Burchardt,
Gisle Aas,
Jeremy Ary,
Hilary Holz,
Rafael Kitover,
Norbert Buchmuller,
Dave Page,
David Sainty,
H.Merijn Brand,
Matt Lawrence,
Michael Schwern,
Adriano Ferreira,
Miyagawa,
Peteris Krumins,
Rafael Kitover,
David Steinbrunner,
Kevin Falcone,
Mike O'Regan,
Mark Stosberg,
Uri Guttman,
Peter Scott,
Phillipe Bruhat,
Ian Langworth,
John Beppu,
Gavin Estey,
Jim Brandt,
Ask Bjoern Hansen,
Greg Davies,
Ed Silva,
Mark-Jason Dominus,
Autrijus Tang,
Mark Fowler,
Stuart Children,
Max Maischein,
Meng Wong,
Prakash Kailasa,
Abigail,
Jan Pazdziora,
Dominique Quatravaux,
Scott Lanning,
Rob Casey,
Leland Johnson,
Joshua Gatcomb,
Julien Beasley,
Abe Timmerman,
Peter Stevens,
Pete Krawczyk,
Tad McClellan,
and the late great Iain Truskett.

=head1 COPYRIGHT

Copyright (c) 2005-2010 Andy Lester. All rights reserved. This program is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=cut
