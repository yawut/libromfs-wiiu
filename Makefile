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

#---------------------------------------------------------------------------------
# Build rules
#---------------------------------------------------------------------------------
.PHONY: clean install
$(TARGET): $(OFILES)
install: $(TARGET)
	mkdir -p $(PORTLIBS)/lib
	mkdir -p $(PORTLIBS)/share
	mkdir -p $(PORTLIBS)/include
	cp -f $(TARGET) $(PORTLIBS)/lib/
	cp -f -a $(SHARE)/. $(PORTLIBS)/share/
	cp -f -a $(INCLUDE)/. $(PORTLIBS)/include/
clean:
	rm -rf $(OFILES) $(CFILES:.c=.d) $(TARGET)

#---------------------------------------------------------------------------------
# wut and portlibs
#---------------------------------------------------------------------------------
include $(WUT_ROOT)/share/wut.mk
PORTLIBS	:=	$(DEVKITPRO)/portlibs/ppc
LDFLAGS		+=	-L$(PORTLIBS)/lib
CFLAGS		+=	-I$(PORTLIBS)/include