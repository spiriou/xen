XEN_ROOT = $(CURDIR)/../..
include $(XEN_ROOT)/tools/Rules.mk

# Caution: some tools/libs/*/Makefile generate header files directly in
# tools/include, and they do the [un]install actions for those, too.
# In case other headers need to be built cwin tools/include this should be
# taken into account, i.e. there should be no rules added here for generating
# any tools/include/*.h files.

# Relative to $(XEN_ROOT)/xen/xsm/flask
FLASK_H_DEPEND := policy/initial_sids

.PHONY: all all-y build xen-dir
all build: all-y xen-foreign xen-dir xen-xsm/.dir
all-y:

.PHONY: xen-foreign
xen-foreign:
	$(MAKE) -C xen-foreign

xen-dir:
	mkdir -p xen/libelf acpi
	find xen/ acpi/ -type l -exec rm '{}' +
	ln -s $(XEN_ROOT)/xen/include/public/COPYING xen/
	ln -s $(XEN_ROOT)/xen/include/public/*.h xen/
	ln -s $(XEN_ROOT)/xen/include/public/*/ xen/
	ln -s ../xen-sys/$(XEN_OS) xen/sys
	ln -s $(addprefix $(XEN_ROOT)/xen/include/xen/,libelf.h elfstructs.h) xen/libelf/
	ln -s ../xen-foreign xen/foreign
	ln -s $(XEN_ROOT)/xen/include/acpi/platform acpi/
	ln -s $(XEN_ROOT)/xen/include/acpi/ac*.h acpi/
ifeq ($(CONFIG_X86),y)
	ln -s $(XEN_ROOT)/xen/arch/x86/include/asm xen/
	mkdir -p xen/lib/x86
	ln -s $(filter-out %autogen.h,$(wildcard $(XEN_ROOT)/xen/include/xen/lib/x86/*.h)) xen/lib/x86/
	ln -s $(XEN_ROOT)/xen/include/xen/lib/x86/Makefile xen/lib/x86/
endif

all-$(CONFIG_X86): xen-dir
	$(MAKE) -C xen/lib/x86 all XEN_ROOT=$(XEN_ROOT) PYTHON=$(PYTHON)

# Not xen/xsm as that clashes with link to
# $(XEN_ROOT)/xen/include/public/xsm above.
xen-xsm/.dir: $(XEN_ROOT)/xen/xsm/flask/policy/mkflask.sh \
	      $(patsubst %,$(XEN_ROOT)/xen/xsm/flask/%,$(FLASK_H_DEPEND))
	mkdir -p xen-xsm/flask
	cd $(XEN_ROOT)/xen/xsm/flask/ && \
		$(SHELL) policy/mkflask.sh $(AWK) $(CURDIR)/xen-xsm/flask $(FLASK_H_DEPEND)
	touch $@

.PHONY: install
install: all
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/arch-x86
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/arch-x86/hvm
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/arch-arm
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/arch-arm/hvm
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/foreign
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/hvm
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/io
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/sys
	$(INSTALL_DIR) $(DESTDIR)$(includedir)/xen/xsm
	$(INSTALL_DATA) xen/COPYING $(DESTDIR)$(includedir)/xen
	$(INSTALL_DATA) xen/*.h $(DESTDIR)$(includedir)/xen
	$(INSTALL_DATA) xen/arch-x86/*.h $(DESTDIR)$(includedir)/xen/arch-x86
	$(INSTALL_DATA) xen/arch-x86/hvm/*.h $(DESTDIR)$(includedir)/xen/arch-x86/hvm
	$(INSTALL_DATA) xen/arch-arm/*.h $(DESTDIR)$(includedir)/xen/arch-arm
	$(INSTALL_DATA) xen/arch-arm/hvm/*.h $(DESTDIR)$(includedir)/xen/arch-arm/hvm
	$(INSTALL_DATA) xen/foreign/*.h $(DESTDIR)$(includedir)/xen/foreign
	$(INSTALL_DATA) xen/hvm/*.h $(DESTDIR)$(includedir)/xen/hvm
	$(INSTALL_DATA) xen/io/*.h $(DESTDIR)$(includedir)/xen/io
	$(INSTALL_DATA) xen/sys/*.h $(DESTDIR)$(includedir)/xen/sys
	$(INSTALL_DATA) xen/xsm/*.h $(DESTDIR)$(includedir)/xen/xsm

.PHONY: uninstall
uninstall:
	echo "[FIXME] uninstall headers"
	rm -rf $(DESTDIR)$(includedir)/xen

.PHONY: clean
clean:
	rm -rf xen xen-xsm acpi
	$(MAKE) -C xen-foreign clean

.PHONY: dist
dist: install

.PHONY: distclean
distclean: clean
