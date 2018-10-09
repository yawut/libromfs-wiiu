BASEDIR	:= $(dir $(firstword $(MAKEFILE_LIST)))
VPATH	:= $(BASEDIR)

#---------------------------------------------------------------------------------
# Build options
#---------------------------------------------------------------------------------
TARGET		:=	libromfs-wiiu.a
INCLUDE		:=	./include
SHARE		:=	./share
SOURCES		:=	source
CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES		:=	$(CFILES:.c=.o)
INSTALLDIR	?=	$(WUT_ROOT)
CFLAGS		+=	-O3

#---------------------------------------------------------------------------------
# Build rules
#---------------------------------------------------------------------------------
.PHONY: clean install
$(TARGET): $(OFILES)
install: $(TARGET)
	mkdir -p $(INSTALLDIR)/lib
	mkdir -p $(INSTALLDIR)/share
	mkdir -p $(INSTALLDIR)/include
	cp -f $(TARGET) $(INSTALLDIR)/lib/
	cp -f -a $(SHARE)/. $(INSTALLDIR)/share/
	cp -f -a $(INCLUDE)/. $(INSTALLDIR)/include/
clean:
	rm -rf $(OFILES) $(CFILES:.c=.d) $(TARGET)

#---------------------------------------------------------------------------------
# wut
#---------------------------------------------------------------------------------
include $(WUT_ROOT)/share/wut.mk