## libromfs-wiiu

### Informations  
This library implements an embedded filesystem mountpoint for usage with devkitPPC aimed at the Nintendo Wii U. The implementations works by statically linking the romfs directory compressed with ustar to the executable.

### Usage
Include library's header: `#include <romfs-wiiu.h>`
Call `romfsInit()` at the start of you app and `romfsExit()` before exiting

To generate the romfs, define a ROMFS variable in you makefile containing the path of your romfs folder:

    ROMFS := romfs_example_folder
Then, include romfs's makefile and add romfs target to your linking targets along with the ld flags (chage the example according to your makefile):

    include $(PORTLIBS_PATH)/wiiu/share/romfs-wiiu.mk
    CFLAGS		+=	$(ROMFS_CFLAGS)
    CXXFLAGS	+=	$(ROMFS_CFLAGS)
    LDFLAGS		+=	$(ROMFS_LDFLAGS)
    OFILES		+=	$(ROMFS_TARGET)

### Installing
A prebuild version is available at the wiiu-fling pacman repository.
Please reffer to [these](https://gitlab.com/QuarkTheAwesome/wiiu-fling) instructions to set up wiiu-fling. 

To manually install the library:

    $ git clone https://github.com/yawut/libromfs-wiiu.git
    $ cd libromfs-wiiu
    $ make
    $ sudo make install



