-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION jsonb_extra" to load this file. \quit

CREATE OR REPLACE FUNCTION jsonb_extract(jsonb, text[])
  RETURNS jsonb[]
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION jsonb_extract_text(jsonb, text[])
  RETURNS text[]
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION jsonb_as_text(jsonb)
  RETURNS text
	AS 'MODULE_PATHNAME'
	LANGUAGE C STRICT IMMUTABLE;
