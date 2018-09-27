ifneq ($(strip $(V)), 1)
	Q ?= @
endif

#---------------------------------------------------------------------------------
# romfs folder (should be set by app's makefile)
#---------------------------------------------------------------------------------
ROMFS			?=	romfs

#---------------------------------------------------------------------------------
# romfs target (must be added to obj files that'll be linked in app's makefile)
#---------------------------------------------------------------------------------
ROMFS_TARGET	:=	final.romfs.o

#---------------------------------------------------------------------------------
# You probably don't need anything from there
#---------------------------------------------------------------------------------
ROMFS_LDFLAGS	:=	-lromfs-wiiu -lzip-romfs -lzlib125
LDFLAGS			+=	$(ROMFS_LDFLAGS)

%.romfs.o: $(ROMFS)
	@echo ROMFS $(notdir $@)
	$(Q)zip -r romfs.zip ./$(ROMFS)/*
	$(Q)$(OBJCOPY) --input binary --output elf32-powerpc --binary-architecture powerpc:common romfs.zip $@
	@rm -f romfs.zip
