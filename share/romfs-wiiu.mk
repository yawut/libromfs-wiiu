ifneq ($(strip $(V)), 1)
	Q ?= @
endif

ROMFS_LDFLAGS	:=	-lromfs-wiiu
ROMFS_CFLAGS	:=	-I$(DEVKITPRO)/portlibs/wiiu/include

ifneq ($(strip $(ROMFS)),)

ROMFS_TARGET	:=	app.romfs.o

%.romfs.o: $(ROMFS)
	@echo ROMFS $(notdir $@)
	$(Q)tar -H ustar -cvf romfs.tar -C $(ROMFS) .
	$(Q)$(OBJCOPY) --input binary --output elf32-powerpc --binary-architecture powerpc:common romfs.tar $@
	@rm -f romfs.tar

else

ROMFS_TARGET	:=	

endif
