/*
	Glidix GUI

	Copyright (c) 2014-2017, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <libgwm.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>

GWMWindow* topWindow;
GWMWindow* newGeneralTab(GWMWindow *notebook);
GWMWindow* newProcTab(GWMWindow *notebook);
GWMWindow* newPCITab(GWMWindow *notebook);

int main()
{
	if (gwmInit() != 0)
	{
		fprintf(stderr, "gwmInit() failed!\n");
		return 1;
	};
	
	const char *caption = "System Information";
	if (geteuid() == 0)
	{
		caption = "System Information (admin mode)";
	};
	
	GWMWindow *win = gwmNewTopLevelWindow();
	gwmSetWindowCaption(win, caption);
	
	topWindow = win;
	
	DDISurface *canvas = gwmGetWindowCanvas(win);
	DDISurface *icon = ddiLoadAndConvertPNG(&canvas->format, "/usr/share/images/sysinfo.png", NULL);
	if (icon != NULL)
	{
		gwmSetWindowIcon(win, icon);
		ddiDeleteSurface(icon);
	};
	
	GWMLayout *box = gwmCreateBoxLayout(GWM_BOX_VERTICAL);
	gwmSetWindowLayout(win, box);
	
	GWMWindow *notebook = gwmNewNotebook(win);
	gwmBoxLayoutAddWindow(box, notebook, 1, 2, GWM_BOX_FILL | GWM_BOX_ALL);
	
	GWMWindow *defTab = newGeneralTab(notebook);
	newProcTab(notebook);
	newPCITab(notebook);
	
	gwmActivateTab(defTab);
	gwmFit(win);
	gwmLayout(win, 640, 480);
	gwmSetWindowFlags(win, GWM_WINDOW_MKFOCUSED | GWM_WINDOW_RESIZEABLE);
	gwmMainLoop();
	gwmQuit();
	return 0;
};
