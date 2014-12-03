# contrib/nicola/Makefile

MODULE_big = nicola
OBJS = nicola.o

EXTENSION = nicola
DATA = nicola--1.0.sql

REGRESS = nicola

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/nicola
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
