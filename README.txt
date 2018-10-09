---------------------------------------------------------------------------------
  libromfs-wiiu
---------------------------------------------------------------------------------

**** Informations ****
This library provides a romfs partition implementation for the Nintendo Wii U.
It's a special filesystem where files needed by applications can be stored in a
read-only manner and embedded into the executable (removing the need for additional
files on the sdcard).
It provides provides low latency and access times since files, once the app has loaded,
are stored in memory instead of being loaded from the sdcard.
This library is made for use with WUT


**** API ****
<romfs-wiiu.h> is the main header

  int32_t romfsInit(void)    
    Mounts app's romfs. Returns 0 if succesfull or a negative
    value if there was an error

  int32_t romfsExit(void)
    Unmounts app's romfs. Returns 0 if succesfull or a negative
    value if there was an error

romfs can be accessed trough normal file functions,
at the mountpoint "romfs:/"


**** Usage ****
[Usage may vary over time]
 - Create a romfs folder in the source tree
 - Edit your Makefile and add:
   1. A ROMFS variable, specifying the relative path to the romfs folder
   2. An "include $(WUT_ROOT)/share/romfs-wiiu.mk" (make sure
      it's included *after* exporting the ROMFS variable or it won't work)
   3. $(ROMFS_TARGET) to target's dependencies and linked objects
   4. $(ROMFS_LDFLAGS) to app's LDFLAGS
 - Add calls to romfsInit() and romfsExit() in your application's code


**** Installation ****
The library can be installed by running "make install" in this repo's root.

By default the library will be installed to $WUT_ROOT. The installation path
can be customized with the INSTALLDIR parameter.

Make sure you exported the $DEVKITPRO and $WUT_ROOT envrivrioment variables and 
that $WUT_ROOT is writable.


**** Implementation ****
The library is currently implemented by having a tar filesystem built from a romfs folder
at compile time and then stored as a variable in .data with a fixed name (this is not an optimal
solution, and may change in the future). No compression is currently used.
