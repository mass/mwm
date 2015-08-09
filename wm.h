/**
 * wm.h
 */
#ifndef WM_H
#define WM_H

#include <unistd.h>
#include <X11/Xlib.h>

#include "log.h"

int main(int argc, char** argv);

int handle_configure_request(Display* display, XEvent* event);
int handle_map_request(Display* display, XEvent* event);

int handle_xerror(Display* display, XErrorEvent* e);

#endif
