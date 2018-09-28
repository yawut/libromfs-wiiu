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
ROMFS_TARGET	:=	app.romfs.o

#---------------------------------------------------------------------------------
# You probably don't need anything from there
#---------------------------------------------------------------------------------
ROMFS_LDFLAGS	:=	-lromfs-wiiu
LDFLAGS			+=	$(ROMFS_LDFLAGS)

%.romfs.o: $(ROMFS)
	@echo ROMFS $(notdir $@)
	$(Q)tar -H ustar -cvf romfs.tar -C $(ROMFS) .
	$(Q)$(OBJCOPY) --input binary --output elf32-powerpc --binary-architecture powerpc:common romfs.tar $@
	@rm -f romfs.tar
