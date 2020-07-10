package Alien::Build::Plugin::Fetch::LWP;

use strict;
use warnings;
use Alien::Build::Plugin;
use Carp ();

# ABSTRACT: Plugin for fetching files using LWP
our $VERSION = '1.74'; # VERSION


has '+url' => '';


has ssl => 0;

sub init
{
  my($self, $meta) = @_;

  $meta->add_requires('share' => 'LWP::UserAgent' => 0 );
  
  $meta->prop->{start_url} ||= $self->url;
  $self->url($meta->prop->{start_url});
  $self->url || Carp::croak('url is a required property');

  if($self->url =~ /^https:/ || $self->ssl)
  {
    $meta->add_requires('share' => 'LWP::Protocol::https' => 0 );
  }

  $meta->register_hook( fetch => sub {
    my(undef, $url) = @_;
    $url ||= $self->url;

    my $ua = LWP::UserAgent->new;
    $ua->env_proxy;
    my $res = $ua->get($url);

    die "error fetching $url: @{[ $res->status_line ]}"
      unless $res->is_success;

    my($type, $charset) = $res->content_type_charset;
    my $base            = $res->base;
    my $filename        = $res->filename;

    if($type eq 'text/html')
    {
      return {
        type    => 'html',
        charset => $charset,
        base    => "$base",
        content => $res->decoded_content || $res->content,
      };
    }
    elsif($type eq 'text/ftp-dir-listing')
    {
      return {
        type => 'dir_listing',
        base => "$base",
        content => $res->decoded_content || $res->content,
      };
    }
    else
    {
      return {
        type     => 'file',
        filename => $filename || 'downloadedfile',
        content  => $res->content,
      };
    }
    
  });

  $self;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Alien::Build::Plugin::Fetch::LWP - Plugin for fetching files using LWP

=head1 VERSION

version 1.74

=head1 SYNOPSIS

 use alienfile;
 share {
   start_url 'http://ftp.gnu.org/gnu/make';
   plugin 'Fetch::LWP';
 };

=head1 DESCRIPTION

Note: in most case you will want to use L<Alien::Build::Plugin::Download::Negotiate>
instead.  It picks the appropriate fetch plugin based on your platform and environment.
In some cases you may need to use this plugin directly instead.

This fetch plugin fetches files and directory listings via the C<http> C<https>, C<ftp>,
C<file> protocol using L<LWP>.  If the URL specified uses the C<https> scheme, then
the required SSL modules will automatically be injected as requirements.  If your
initial URL is not C<https>, but you know that it will be needed on a subsequent
request you can use the ssl property below.

=head1 PROPERTIES

=head2 url

The initial URL to fetch.  This may be a directory listing (in HTML) or the final file.

=head2 ssl

If set to true, then the SSL modules required to make an C<https> connection will be
added as prerequisites.

=head1 SEE ALSO

L<Alien::Build::Plugin::Download::Negotiate>, L<Alien::Build>, L<alienfile>, L<Alien::Build::MM>, L<Alien>

=head1 AUTHOR

Author: Graham Ollis E<lt>plicease@cpan.orgE<gt>

Contributors:

Diab Jerius (DJERIUS)

Roy Storey (KIWIROY)

Ilya Pavlov

David Mertens (run4flat)

Mark Nunberg (mordy, mnunberg)

Christian Walde (Mithaldu)

Brian Wightman (MidLifeXis)

Zaki Mughal (zmughal)

mohawk (mohawk2, ETJ)

Vikas N Kumar (vikasnkumar)

Flavio Poletti (polettix)

Salvador Fandiño (salva)

Gianni Ceccarelli (dakkar)

Pavel Shaydo (zwon, trinitum)

Kang-min Liu (劉康民, gugod)

Nicholas Shipp (nshp)

Juan Julián Merelo Guervós (JJ)

Joel Berger (JBERGER)

Petr Pisar (ppisar)

Lance Wicks (LANCEW)

Ahmad Fatoum (a3f, ATHREEF)

José Joaquín Atria (JJATRIA)

Duke Leto (LETO)

Shoichi Kaji (SKAJI)

Shawn Laffan (SLAFFAN)

Paul Evans (leonerd, PEVANS)

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011-2019 by Graham Ollis.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
