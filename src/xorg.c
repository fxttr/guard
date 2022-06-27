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

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#include "inc/xorg.h"

XLock **
xorg_lockdown(Display *dpy)
{
	if (dpy == NULL)
		return NULL;

	size_t screens = ScreenCount(dpy);
	XLock **locks = malloc(sizeof(XLock *) * screens);
	unsigned int numlocks = 0;

	if (locks == NULL)
		return NULL;

	for (size_t i = 0; i < screens; ++i) {
		XLock *lock = xorg_lockdown_screen(dpy, i);

		if (lock != NULL) {
			locks[i] = lock;
			++numlocks;
		}
	}

	if (numlocks == 0) {
		free(locks);
		XCloseDisplay(dpy);
		return NULL;
	}

	errno = 0;

	return locks;
}

XLock *
xorg_lockdown_screen(Display *dpy, int screen)
{
	// Should not happen.
	if (dpy == NULL)
		return NULL;

	XLock *lock = malloc(sizeof(XLock));
	XSetWindowAttributes wa;
	Atom atom = XA_WM_NAME;
	XTextProperty prop = { "guard", atom, 8, 5 };

	if (lock == NULL)
		return NULL;

	lock->screen = screen;
	lock->root = RootWindow(dpy, screen);

	wa.override_redirect = True;
	wa.background_pixel = BlackPixel(dpy, screen);

	lock->win = XCreateWindow(dpy, lock->root, 0, 0,
	    DisplayWidth(dpy, lock->screen), DisplayHeight(dpy, lock->screen),
	    0, DefaultDepth(dpy, lock->screen), CopyFromParent,
	    DefaultVisual(dpy, lock->screen), CWOverrideRedirect | CWBackPixel,
	    &wa);

	XSetWMName(dpy, lock->win, &prop);

	XClassHint *hint = XAllocClassHint();
	if (hint != NULL) {
		hint->res_name = "guard";
		hint->res_class = "guard";
		XSetClassHint(dpy, lock->win, hint);
		XFree(hint);
	}

	XMapRaised(dpy, lock->win);

	for (size_t i = 1000; i > 0; --i) {
		int field = ButtonPressMask | ButtonReleaseMask |
		    PointerMotionMask;

		int grab_pointer = XGrabPointer(dpy, lock->root, False, field,
		    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

		int grab_keyboard = XGrabKeyboard(dpy, lock->root, True,
		    GrabModeAsync, GrabModeAsync, CurrentTime);

		if (grab_pointer == GrabSuccess && grab_keyboard == GrabSuccess)
			break;

		usleep(1000);
	}

	XSelectInput(dpy, lock->root, SubstructureNotifyMask);

	return lock;
}

void
xorg_unlock_screen(Display *dpy, XLock *lock)
{
	if (dpy == NULL || lock == NULL)
		return;

	XUngrabPointer(dpy, CurrentTime);

	XDestroyWindow(dpy, lock->win);

	free(lock);
}
