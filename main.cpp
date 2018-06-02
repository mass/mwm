#include <mass/Log.hpp>

extern "C" {
#include <X11/Xlib.h>
}

int handle_configure_request(Display* display, XEvent* event) {
  LOG(INFO) << "Start ConfigureRequest";
  XConfigureRequestEvent e = *((XConfigureRequestEvent*) event);

  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;

  XConfigureWindow(display, e.window, e.value_mask, &changes);
  LOG(INFO) << "End ConfigureRequest";

  return 0;
}

int handle_map_request(Display* display, XEvent* event) {
  LOG(INFO) << "Start MapRequest";
  XMapRequestEvent e = *((XMapRequestEvent*) event);

  XMapWindow(display, e.window);
  LOG(INFO) << "End MapRequest";

  return 0;
}

int handle_xerror(Display* display, XErrorEvent* e) {
  LOG(ERROR) << "Error!";
  return 1;
}

int main(int argc, char* argv[])
{
  Display* display = XOpenDisplay(":100");
  if (display == NULL) {
    LOG(ERROR) << "No display!";
    return 1;
  }

  Window root = DefaultRootWindow(display);

  XSetErrorHandler(&handle_xerror);
  XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(display, 0);

  // Main event loop
  for (;;) {
    XEvent e;
    XNextEvent(display, &e);

    switch (e.type) {
      case ConfigureRequest:
        handle_configure_request(display, &e);
        break;
      case MapRequest:
        handle_map_request(display, &e);
        break;
      default:
        LOG(WARN) << "XEvent not yet handled";
        break;
    }
  }

  XCloseDisplay(display);
  return 0;
}
