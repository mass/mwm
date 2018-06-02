#pragma once

#include <X11/Xlib.h>

#include <map>
#include <vector>

struct Drag
{
  Window w;

  int xR, yR;
  int x, y;
  int width, height;

  int btn;
};

struct Client
{
  Window frame;
  Window client;
  Window root;
};

class Manager
{
  public:

    Manager();
    ~Manager();

    bool init();
    void run();

    void onReq_Map(const XMapRequestEvent& e);
    void onReq_Configure(const XConfigureRequestEvent& e);
    void onNot_Unmap(const XUnmapEvent& e);
    void onNot_Motion(const XButtonEvent& e);

    void frame(Window w);
    void unframe(Client& c);
    Window getRoot(Window w);
    void onKeyPress(const XKeyEvent& e);
    void onBtnPress(const XButtonEvent& e);

  private:

    Display* _disp;
    int _numScreens;
    std::map<Window /*client*/, Client> _clients;
    Drag _drag;
};

static std::string ToString(const XEvent& e) {
  static const char* const X_EVENT_TYPE_NAMES[] = {
      "",
      "",
      "KeyPress",
      "KeyRelease",
      "ButtonPress",
      "ButtonRelease",
      "MotionNotify",
      "EnterNotify",
      "LeaveNotify",
      "FocusIn",
      "FocusOut",
      "KeymapNotify",
      "Expose",
      "GraphicsExpose",
      "NoExpose",
      "VisibilityNotify",
      "CreateNotify",
      "DestroyNotify",
      "UnmapNotify",
      "MapNotify",
      "MapRequest",
      "ReparentNotify",
      "ConfigureNotify",
      "ConfigureRequest",
      "GravityNotify",
      "ResizeRequest",
      "CirculateNotify",
      "CirculateRequest",
      "PropertyNotify",
      "SelectionClear",
      "SelectionRequest",
      "SelectionNotify",
      "ColormapNotify",
      "ClientMessage",
      "MappingNotify",
      "GeneralEvent",
  };

  if (e.type < 2 || e.type >= LASTEvent) {
    return "Unknown";
  }

  return X_EVENT_TYPE_NAMES[e.type];
}
