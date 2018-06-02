#include "Manager.hpp"

#include <mass/Log.hpp>

// Static error handler for XLib
static int XError(Display* display, XErrorEvent* e) {
  char buf[1024];
  XGetErrorText(display, e->error_code, buf, sizeof(buf));

  LOG(ERROR) << "X Error"
             << " display=" << DisplayString(display)
             << " what=(" << buf << ")";
  return 0;
}

Manager::Manager() {}

Manager::~Manager()
{
  if (_disp != nullptr) XCloseDisplay(_disp);
}

bool Manager::init()
{
  _disp = XOpenDisplay(":100");
  if (_disp == nullptr) {
    LOG(ERROR) << "failed to open X display=:100";
    return false;
  }

  _numScreens = ScreenCount(_disp);
  LOG(INFO) << "_disp=" << DisplayString(_disp) << " screens=" << _numScreens;


  for (int i = 0; i < _numScreens; ++i) {
    XSelectInput(_disp, RootWindow(_disp, i), SubstructureRedirectMask | SubstructureNotifyMask);
  }

  XSetErrorHandler(&XError);
  //XSync(_disp, 0);

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

      //TODO
      case ButtonPress:
        LOG(INFO) << "BTN PRS";
        break;
      case ButtonRelease:
        LOG(INFO) << "BTN RLSE";
        break;
      case KeyPress:
        LOG(INFO) << "KEY PRS";
        break;
      case KeyRelease:
        LOG(INFO) << "KEY RLSE";
        break;

      default:
        LOG(ERROR) << "XEvent not yet handled type=" << e.type;
        break;
    }
  }
}

void Manager::onReq_Map(const XMapRequestEvent& e)
{
  LOG(INFO) << "request=Map window=" << e.window;

  frame(e.window);
  XMapWindow(_disp, e.window);
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

  auto it = _frames.find(e.window);
  if (it != end(_frames)) {
    LOG(INFO) << "configuring frame=" << it->second << " for window=" << e.window;
    XConfigureWindow(_disp, it->second, e.value_mask, &changes);
  }
}

void Manager::onNot_Unmap(const XUnmapEvent& e)
{
  LOG(INFO) << "notify=Unmap window=" << e.window;

  if (_frames.find(e.window) != end(_frames)) {
    unframe(e.window);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// Wraps a custom frame Window around the Window w.
void Manager::frame(Window w)
{
  XWindowAttributes attrs;
  XGetWindowAttributes(_disp, w, &attrs);

  const Window frame =
    XCreateSimpleWindow(_disp, getRoot(w), attrs.x, attrs.y, attrs.width, attrs.height,
                        5, 0xFF0000, 0x0000FF);
  XSelectInput(_disp, frame, SubstructureRedirectMask | SubstructureNotifyMask);
  XAddToSaveSet(_disp, w);
  XReparentWindow(_disp, w, frame, 0, 0);
  XMapWindow(_disp, frame);
  _frames[w] = frame;
  LOG(INFO) << "framed window=" << w << " frame=" << frame;
}

void Manager::unframe(Window w)
{
  const Window frame = _frames[w];
  XUnmapWindow(_disp, frame);
  XReparentWindow(_disp, w, getRoot(w), 0, 0);
  XRemoveFromSaveSet(_disp, w);
  _frames.erase(w);
  LOG(INFO) << "unframed window=" << w << " frame=" << frame;
}

Window Manager::getRoot(Window w)
{
  Window root, parent;
  Window* children;
  uint32_t num;

  XQueryTree(_disp, w, &root, &parent, &children, &num);

  return root;
}
