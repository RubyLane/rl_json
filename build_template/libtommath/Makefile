src = $(abspath ../../deps/libtommath)
VPATH = $(src)
include $(src)/makefile

# Have to override the install target to fetch the headers from $(src)
custom_install: $(LIBNAME) .install_common
	install -m 644 $(LIBNAME) $(DESTDIR)$(LIBPATH)
	install -m 644 $(src)/$(HEADERS_PUB) $(DESTDIR)$(INCPATH)
