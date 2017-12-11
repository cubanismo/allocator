lib_LTLIBRARIES = libnouveau-allocator.la

libnouveau_allocator_la_CFLAGS = -I$(top_srcdir)/include $(LIBDRM_NOUVEAU_CFLAGS)
libnouveau_allocator_la_LDFLAGS = $(LIBDRM_NOUVEAU_LIBS)

libnouveau_allocator_la_LDFLAGS += -avoid-version -module -shared
libnouveau_allocator_la_LDFLAGS += -export-symbols nouveau_allocator_driver.syms

libnouveau_allocator_la_SOURCES = nouveau_allocator_driver.c

NOUVEAU_ALLOCATOR_CONF_FILE=50_nouveau_allocator.json

dist_pkgdata_DATA = $(NOUVEAU_ALLOCATOR_CONF_FILE)

# Generate Nouveau's driver Json
JSON_GENERATOR=$(top_srcdir)/scripts/json-generator.pl

$(NOUVEAU_ALLOCATOR_CONF_FILE): $(JSON_GENERATOR)
	@echo -e "  JSON     $@"
	@$(PERL) $(JSON_GENERATOR) -driver=libnouveau-allocator.so > $(builddir)/$@

clean-local:
	rm -f $(NOUVEAU_ALLOCATOR_CONF_FILE)
