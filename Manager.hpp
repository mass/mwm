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
  Window client;
  Window root;
};

struct Monitor
{
  std::string name;
  int x, y;
  int w, h;

  bool contains(int ox, int oy) const;
};

struct ScreenInfo
{
  int num;
  std::vector<Monitor> monitors;
};

class Manager
{
  public:

    Manager(const std::string& display,
            const std::vector<int>& screens);
    ~Manager();

    bool init();
    void run();

    void onReq_Map(const XMapRequestEvent& e);
    void onReq_Configure(const XConfigureRequestEvent& e);
    void onNot_Unmap(const XUnmapEvent& e);
    void onNot_Motion(const XButtonEvent& e);
    void onKeyPress(const XKeyEvent& e);
    void onBtnPress(const XButtonEvent& e);

    void addClient(Window w);
    void delClient(Client& c);

    Window getRoot(Window w);

  private:
    const std::string& _argDisp;
    const std::vector<int>& _argScreens;

    Display* _disp;
    int _numScreens;
    std::map<Window /*client*/, Client> _clients;
    std::map<Window /*root*/, ScreenInfo> _screens;
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

static inline std::pair<int,int> getCenter(int x, int y, int w, int h)
{
  int cx = x + (w/2);
  int cy = y + (h/2);
  return std::make_pair(cx,cy);
}

