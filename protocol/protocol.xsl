<?xml version="1.0" ?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" indent="yes" encoding="UTF-8"/>
<xsl:preserve-space elements="copyright"/>
<xsl:template match="/">
<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html;charset=utf-8" />
    <link href="wayland-protocol.css" rel="stylesheet" type="text/css" />
    <title>Wayland</title>
  </head>
  <body>
    <h1><img src="wayland.png" alt="Wayland logo" /></h1>
    <h1>Wayland Protocol Specification</h1>

    <!-- Copyright blurb -->
    <xsl:apply-templates select="protocol/copyright"/>

    <!-- TOC -->
    <xsl:apply-templates select="protocol" mode="toc" />

    <!-- Interface descriptions -->
    <xsl:apply-templates select="protocol/interface" mode="interface_description" />

  </body>
</html>
</xsl:template>

<!-- Copyright blurb -->
<xsl:template match="copyright">
  <div>
    <pre class="copyright">
      <xsl:value-of select="." disable-output-escaping="yes"/>
    </pre>
  </div>
</xsl:template>

<!-- TOC -->
<xsl:template match="protocol" mode="toc">
  <div class="toc">
    <h2>Table of Contents</h2>
    <ul>
      <xsl:apply-templates select="interface" mode="toc" />
    </ul>
  </div>
</xsl:template>

<!-- interface in TOC -->
<xsl:template match="interface" mode="toc">
  <li>
    <xsl:call-template name="link">
      <xsl:with-param name="which" select="'href'" />
    </xsl:call-template>

    <!-- request list -->
    <xsl:if test="request">
      <div>
	Requests:
	<ul>
	  <xsl:apply-templates select="request" mode="toc"/>
	</ul>
      </div>
    </xsl:if>

    <!-- event list -->
    <xsl:if test="event">
      <div>
	Events:
	<ul>
	  <xsl:apply-templates select="event" mode="toc"/>
	</ul>
      </div>
    </xsl:if>

    <!-- enum list -->
    <xsl:if test="enum">
      <div>
	Enums:
	<ul>
	  <xsl:apply-templates select="enum" mode="toc"/>
	</ul>
      </div>
    </xsl:if>
  </li>
</xsl:template>

<!--
  Template to create a <a> tag in the form
    #<interfacename>-<request|event>-<request/event name>
  the '#' prefix is added if $which is 'href'
  $which decides which attribute name (href or name) of <a> to set
-->
<xsl:template name="link" >
  <xsl:param name="which" />
  <a>
    <xsl:attribute name="{$which}">
      <xsl:if test="$which = 'href'">#</xsl:if>
      <xsl:value-of select="../@name"/>
      <xsl:text>-</xsl:text> <!-- xsl:text needed to avoid whitespace -->
      <xsl:value-of select="name()"/>
      <xsl:text>-</xsl:text> <!-- xsl:text needed to avoid whitespace -->
      <xsl:value-of select="@name"/></xsl:attribute>
      <!-- only display link text for href links -->
      <xsl:if test="$which = 'href'">
	<span class="mono"><xsl:value-of select="@name"/></span>
	<xsl:if test="description/@summary"> - <xsl:value-of select="description/@summary"/></xsl:if>
      </xsl:if>
  </a>
</xsl:template>

<!-- requests and events in TOC -->
<xsl:template match="request|event|enum" mode="toc">
  <li>
    <xsl:call-template name="link">
      <xsl:with-param name="which" select="'href'" />
    </xsl:call-template>
  </li>
</xsl:template>

<!-- Interface descriptions -->
<xsl:template match="protocol/interface" mode="interface_description">
  <div class="interface">
    <xsl:call-template name="link">
      <xsl:with-param name="which" select="'name'" />
    </xsl:call-template>
    <h1>
      <span class="mono"><xsl:value-of select="@name" /></span>
      <!-- only show summary if it exists -->
      <xsl:if test="description/@summary">
	- <xsl:value-of select="description/@summary" />
      </xsl:if>
    </h1>
    <p class="version">Version: <xsl:value-of select="@version" /></p>
    <p><xsl:value-of select="description"/></p>
    <xsl:if test="request">
      <div class="requests">
	<h2>Requests</h2>
	<!-- Request list -->
	<xsl:apply-templates select="request" mode="interface_description" />
      </div>
    </xsl:if>

    <xsl:if test="event">
      <div class="events">
	<h2>Events</h2>
	<!-- Event list -->
	<xsl:apply-templates select="event" mode="interface_description" />
      </div>
    </xsl:if>

    <xsl:if test="enum">
      <div class="enums">
	<h2>Enums</h2>
	<!-- enum list -->
	<xsl:apply-templates select="enum" mode="interface_description"/>
      </div>
    </xsl:if>
  </div>
</xsl:template>

<!-- table contents for request/event arguments or enum values -->
<xsl:template match="arg|entry">
  <tr>
    <td class="arg_name"><xsl:value-of select="@name"/></td>
    <xsl:if test="name() = 'arg'" >
      <td class="arg_type"><xsl:value-of select="@type"/></td>
    </xsl:if>
    <xsl:if test="name() = 'entry'" >
      <td class="arg_value"><xsl:value-of select="@value"/></td>
    </xsl:if>
    <td class="arg_desc"><xsl:value-of select="@summary"/></td>
  </tr>
</xsl:template>

<!-- Request/event list -->
<xsl:template match="request|event|enum" mode="interface_description">
  <div>
    <xsl:call-template name="link">
      <xsl:with-param name="which" select="'name'" />
    </xsl:call-template>
    <h3>
      <span class="mono"><xsl:value-of select="../@name"/>::<xsl:value-of select="@name" /></span>
      <xsl:if test="description/@summary">
	- <xsl:value-of select="description/@summary" />
      </xsl:if>
    </h3>
    <p><xsl:value-of select="description"/></p>
    <xsl:if test="arg">
      Arguments:
      <table>
	<xsl:apply-templates select="arg"/>
      </table>
    </xsl:if>
    <xsl:if test="entry">
      Values:
      <table>
	<xsl:apply-templates select="entry"/>
      </table>
    </xsl:if>
  </div>
</xsl:template>
</xsl:stylesheet>

<!-- vim: set expandtab shiftwidth=2: -->
