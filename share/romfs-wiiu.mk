ifneq ($(strip $(V)), 1)
	Q ?= @
endif

ifneq ($(strip $(ROMFS)),)

TOPDIR		?=	.

ROMFS_LIBS	:=	-lromfs-wiiu
ROMFS_CFLAGS	:=	-I$(DEVKITPRO)/portlibs/wiiu/include
ROMFS_TARGET	:=	app.romfs.o

%.romfs.o: $(TOPDIR)/$(ROMFS)
	@echo ROMFS $(notdir $@)
	$(Q)tar --format ustar -cvf romfs.tar -C $(TOPDIR)/$(ROMFS) .
	$(Q)$(OBJCOPY) --input-target binary --output-target elf32-powerpc --binary-architecture powerpc:common romfs.tar $@
	@rm -f romfs.tar

else

ROMFS_LIBS	:=
ROMFS_CFLAGS	:=
ROMFS_TARGET	:=

endif
