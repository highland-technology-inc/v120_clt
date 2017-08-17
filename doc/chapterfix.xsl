<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:db="http://docbook.org/ns/docbook"
>
<xsl:strip-space elements="*"/><xsl:preserve-space elements="programlisting"/>

<xsl:template match="node()|@*">
  <xsl:copy>
    <xsl:apply-templates select="node()|@*"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="db:refentryinfo/db:title">
  <title><xsl:value-of select="$shorttitle"/></title> 
</xsl:template>

<xsl:template match="db:refentryinfo/db:productname">
  <productname><xsl:value-of select="$title"/></productname> 
</xsl:template>

<xsl:template match="db:refmeta/db:manvolnum">
  <manvolnum><xsl:value-of select="$manvolnum" /></manvolnum>
</xsl:template>

<xsl:template match="db:refmeta/db:refmiscinfo"/>

</xsl:stylesheet>
