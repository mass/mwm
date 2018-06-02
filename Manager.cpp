#include "Manager.hpp"

#include <mass/Log.hpp>

#include <X11/Xutil.h>

#include <algorithm>

// Static error handler for XLib
static int XError(Display* display, XErrorEvent* e) {
  char buf[1024];
  XGetErrorText(display, e->error_code, buf, sizeof(buf));

  LOG(ERROR) << "X Error"
             << " display=" << DisplayString(display)
             << " what=(" << buf << ")";
  return 0;
}

Manager::Manager()
{
  _drag.w = 0;
}

Manager::~Manager()
{
  if (_disp != nullptr) XCloseDisplay(_disp);
}

bool Manager::init()
{
  XSetErrorHandler(&XError);

  _disp = XOpenDisplay(":100");
  if (_disp == nullptr) {
    LOG(ERROR) << "failed to open X display=:100";
    return false;
  }

  _numScreens = ScreenCount(_disp);
  LOG(INFO) << "display=" << DisplayString(_disp) << " screens=" << _numScreens;

  for (int i = 0; i < _numScreens; ++i) {
    auto root = RootWindow(_disp, i);
    LOG(INFO) << "screen=" << DisplayString(_disp) << "." << i << " root=" << root;

    XSelectInput(_disp, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_Tab), Mod1Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabButton(_disp, 1, Mod1Mask, root, false,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(_disp, 3, Mod1Mask, root, false,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
  }

  return true;
}

void Manager::run()
{
  // Main event loop
  for (;;) {
    XEvent e;
    XNextEvent(_disp, &e);

    switch (e.type) {
      // Ignore these events
      case ReparentNotify:
      case MapNotify:
      case ConfigureNotify:
      case CreateNotify:
      case DestroyNotify:
      case KeyRelease:
      case ButtonRelease:
        break;

      case MapRequest:
        onReq_Map(e.xmaprequest);
        break;
      case ConfigureRequest:
        onReq_Configure(e.xconfigurerequest);
        break;

      case UnmapNotify:
        onNot_Unmap(e.xunmap);
        break;
      case MotionNotify:
        while (XCheckTypedWindowEvent(_disp, e.xmotion.window, MotionNotify, &e)); // Get latest
        onNot_Motion(e.xbutton);
        break;

      case KeyPress:
        onKeyPress(e.xkey);
        break;
      case ButtonPress:
        onBtnPress(e.xbutton);
        break;

      default:
        LOG(ERROR) << "XEvent not yet handled type=" << e.type
                   << " event=(" << ToString(e) << ")";
        break;
    }
  }
}

void Manager::onReq_Map(const XMapRequestEvent& e)
{
  LOG(INFO) << "request=Map window=" << e.window;
  addClient(e.window);
}

void Manager::onReq_Configure(const XConfigureRequestEvent& e)
{
  LOG(INFO) << "request=Configure window=" << e.window;

  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;

  XConfigureWindow(_disp, e.window, e.value_mask, &changes);

  auto it = _clients.find(e.window);
  if (it != end(_clients)) {
    LOG(INFO) << "configuring client=" << it->second.client << " for window=" << e.window;
    XConfigureWindow(_disp, it->second.client, e.value_mask, &changes);
  }
}

void Manager::onNot_Unmap(const XUnmapEvent& e)
{
  LOG(INFO) << "notify=Unmap window=" << e.window;

  auto it = _clients.find(e.window);
  if (it != end(_clients)) {
    delClient(it->second);
  }
}

void Manager::onNot_Motion(const XButtonEvent& e)
{
  if (_drag.w == 0) return;

  auto client = _drag.w;
  if (_clients.find(client) == end(_clients)) {
    LOG(ERROR) << "client not found for motion event client=" << client;
    return;
  }

  int xdiff = e.x_root - _drag.xR;
  int ydiff = e.y_root - _drag.yR;

  if (_drag.btn == 1) {
    XMoveWindow(_disp, client, _drag.x + xdiff, _drag.y + ydiff);
  } else if (_drag.btn == 3) {
    XResizeWindow(_disp, client,
                  std::max(1, _drag.width + xdiff), std::max(1, _drag.height + ydiff));
  }
}

void Manager::onKeyPress(const XKeyEvent& e)
{
  //LOG(INFO) << "keyPress window=" << e.window << " subwindow=" << e.subwindow
  //          << " keyCode=" << e.keycode;

  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_Tab)))
  {
    LOG(INFO) << "got ALT-TAB window=" << e.window << " subwindow=" << e.subwindow;
    if (e.subwindow != 0) XRaiseWindow(_disp, e.subwindow);
  }
}

void Manager::onBtnPress(const XButtonEvent& e)
{
  if (e.subwindow == 0) return;

  LOG(INFO) << "btnPress window=" << e.window << " subwindow=" << e.subwindow;

  _drag.btn = e.button;
  _drag.xR = e.x_root;
  _drag.yR = e.y_root;
  _drag.w = e.subwindow;

  XWindowAttributes attr;
  XGetWindowAttributes(_disp, e.subwindow, &attr);
  _drag.x = attr.x;
  _drag.y = attr.y;
  _drag.width = attr.width;
  _drag.height = attr.height;

  XRaiseWindow(_disp, e.subwindow);
  XSetInputFocus(_disp, e.subwindow, RevertToPointerRoot, CurrentTime);
}

////////////////////////////////////////////////////////////////////////////////
/// TODO
void Manager::addClient(Window w)
{
  auto it = _clients.find(w);
  if (it != end(_clients)) {
    LOG(ERROR) << "window=" << w << " is already framed!";
    return;
  }

  Client c;
  c.client = w;
  c.root = getRoot(w);
  _clients.insert({w, c});

  XMapWindow(_disp, w);

  LOG(INFO) << "added client=" << w;
}

void Manager::delClient(Client& c)
{
  _clients.erase(c.client);
  LOG(INFO) << "deleted client=" << c.client;
}

Window Manager::getRoot(Window w)
{
  Window root, parent;
  Window* children;
  uint32_t num;

  XQueryTree(_disp, w, &root, &parent, &children, &num);

  return root;
}
