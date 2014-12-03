-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION nicola" to load this file. \quit

CREATE OR REPLACE FUNCTION nicola(jsonb, text[])
  RETURNS jsonb
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;
