/**
 * Copyright (c) 2022, Florian Büstgens
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Florian Büstgens ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Florian Büstgens BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/_types.h>
#include <sys/procctl.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "X11/Xlib.h"
#include "inc/xorg.h"

typedef struct xorg_thread_data {
	XLock **locks;
	Display *dpy;
} xorg_thread_data;

static const char *get_passwd(void);

static const char *
get_passwd(void)
{
	errno = 0;
	struct passwd *pw = getpwuid(getuid());

	if (!pw) {
		err(1, "guard: Could not retrieve password");
	}

	endpwent();

	if (geteuid() == 0) {
		if (!(geteuid() != pw->pw_gid && setgid(pw->pw_gid) < 0)) {
			if (setuid(pw->pw_uid) < 0)
				err(1, "Cannot drop privileges");
		}
	}

	return pw->pw_passwd;
}

void *
lock_xorg(void *args)
{
	xorg_thread_data *data = (xorg_thread_data *)args;

	XLock **locks = xorg_lockdown(data->dpy);

	data->locks = locks;

	pthread_exit(NULL);
}

int
main(void)
{
	// setup
	int pid = getpid();
	int pid_flags = PPROT_SET | PPROT_INHERIT;

	// disable OOM killer
	if (procctl(P_PID, pid, PROC_SPROTECT, &pid_flags) == -1)
		err(1, "Could not disable OOM killer");

	// enter capsicum
	if (caph_limit_stdio() < 0 && caph_enter() < 0)
		err(1, "Could enter capsicum");

	pthread_t xorg;
	xorg_thread_data xorg_thread_data;

	Display *dpy = XOpenDisplay(0);
	xorg_thread_data.dpy = dpy;

	pthread_create(&xorg, NULL, lock_xorg, (void *)&xorg_thread_data);
	pthread_join(xorg, NULL);

	usleep(3000);

	for (size_t i = 0; xorg_thread_data.locks[i] != NULL; ++i) {
		xorg_unlock_screen(dpy, xorg_thread_data.locks[i]);
	}

	return 0;
}
