---------------------------------------------------------------------------------
  libromfs-wiiu
---------------------------------------------------------------------------------

* Informations
This library provides a romfs partition implementation for the Nintendo Wii U.
It's a special filesystem where files needed by applications can be stored in a
read-only manner and embedded into the executable (removing the need for additional
files on the sdcard).
It provides provides low latency and access times since files, once the app has loaded,
are stored in memory instead of being loaded from the sdcard.


* Usage
Usage may vary over time, as the current implementation isn't optimal. Currently:
 - Create a romfs folder in the source tree
 - Edit your Makefile and add:
   1. A ROMFS variable, specifying the relative path to the romfs folder
   2. An "include $(DEVKITPRO)/portlibs/ppc/share/romfs-wiiu.mk" (make sure
      it's included *after* exporting the ROMFS variable or it won't work)
   3. $(ROMFS_TARGET) to target dependencies and linked objects


* Installing
Run "make install" in this repository's root.
Make sure you exported the $DEVKITPRO envrivrioment variable and that $DEVKITPRO/portlibs is writable.


* Implementation
The library is currently implemented by having a tar filesystem compressed from a romfs folder
at compile time and then stored as a variable in .data with a fixed name (this is not an optimal
solution, and may change in the future).
