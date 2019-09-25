#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <romfs-wiiu.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	char line[128];
	
	// initialize procui/console
	WHBProcInit();
	WHBLogConsoleInit();
	
	// initialize romfs library
	int res = romfsInit();
	if (res) {
		WHBLogPrintf(">> Failed to init romfs: %d", res);
		goto end;
	}
	
	// open helloworld.txt
	FILE *fp = fopen("romfs:/helloworld.txt", "r");
	if (fp == NULL) {
		WHBLogPrint(">> Failed to open file");
		goto end;
	}
	
	WHBLogPrint(">> Content of romfs:/helloworld.txt:");
	
	// output content to console
	while (fgets(line, sizeof(line), fp) != NULL)
		WHBLogPrint(line);
	
	// cleanup
	fclose(fp);

end:
	WHBLogPrint(">> Done. Press Home to exit");

	// draw the contents of console to screen
	WHBLogConsoleDraw();
	
	// wait for the user to exit
	while(WHBProcIsRunning())
		OSSleepTicks(OSMillisecondsToTicks(100));

	// deinitialize romfs library
	romfsExit();

	// deinitialize procui/console
	WHBLogConsoleFree();
	WHBProcShutdown();

	return 0;
} 
