#pragma once

#include <X11/Xlib.h>
#include <X11/Xproto.h>

#include <limits.h>
#include <map>
#include <vector>

enum class DIR
{
  Up,
  Down,
  Left,
  Right,
  Last
};

struct Point
{
  int x;
  int y;

  Point() : Point(0, 0) {}
  Point(int x, int y) : x(x), y(y) {}
};

struct Rect
{
  Point o;
  int w, h;

  Rect() : Rect(0, 0, 0, 0) {}
  Rect(int x, int y, int w, int h)
    : o(x, y), w(w), h(h) {}
  Rect(Window w, Display* d)
  {
    XWindowAttributes attr;
    XGetWindowAttributes(d, w, &attr);
    o.x = attr.x; o.y = attr.y;
    w = attr.width;
    h = attr.height;
  }

  inline bool contains(const Point& a) const {
    return (a.x >= o.x && a.x <= o.x + w) &&
           (a.y >= o.y && a.y <= o.y + h);
  }

  inline Point getCenter()
  {
    Point c(o);
    c.x += (w/2);
    c.y += (h/2);
    return c;
  }
};

struct Drag
{
  Window w;

  int xR, yR;
  int x, y;
  int width, height;

  int btn;
  DIR dirVert;
  DIR dirHorz;
};

struct Client
{
  Window client;
  Window root;

  Rect preMax;

  bool ign;

  inline Rect getRect(Display* disp) {
    return {client, disp};
  }
};

struct Monitor
{
  std::string name;
  Rect r;

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
    void onClientMessage(const XClientMessageEvent& e);

    void switchFocus(Window w);
    void handleFocusChange(Window w, bool in);

    void addClient(Window w, bool checkIgn);
    void delClient(Client& c);

    Window getRoot(Window w);
    Window getNextWindowInDir(DIR dir, Window w);

    template<typename T>
    T closestRectFromPoint(const Point& p, std::vector<std::pair<Rect, T>>& rects);

  private:

    const std::string& _argDisp;
    const std::vector<int>& _argScreens;

    Display* _disp;
    int _numScreens;
    std::map<Window /*client*/, Client> _clients;
    std::map<Window /*root*/, ScreenInfo> _screens;
    Drag _drag;
};

static inline Point getCenter(int x, int y, int w, int h)
{
  Point p;
  p.x = x + (w/2);
  p.y = y + (h/2);
  return p;
}

static inline int getDist(Point a, Point b, DIR dir)
{
  int dist;
  switch (dir) {
    case DIR::Up:
      dist = a.y - b.y;
      break;
    case DIR::Down:
      dist = b.y - a.y;
      break;
    case DIR::Left:
      dist = a.x - b.x;
      break;
    case DIR::Right:
      dist = b.x - a.x;
      break;
    default:
      throw std::runtime_error("invalid DIR enum");
  };
  if (dist < 0) dist = INT_MAX;

  return dist;
}

static inline const char* ToString(const XEvent& e)
{
  static const char* const X_EVENT_TYPE_NAMES[] = {
      "Undefined",
      "Undefined",
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

  if (e.type >= LASTEvent)
    return "Undefined";
  return X_EVENT_TYPE_NAMES[e.type];
}

static inline const char* GetXOpcodeStr(const unsigned char opcode)
{
  static const char* const X_REQ_OPCODE_NAMES[] = {
    "Undefined",
    "X_CreateWindow",
    "X_ChangeWindowAttributes",
    "X_GetWindowAttributes",
    "X_DestroyWindow",
    "X_DestroySubwindows",
    "X_ChangeSaveSet",
    "X_ReparentWindow",
    "X_MapWindow",
    "X_MapSubwindows",
    "X_UnmapWindow",
    "X_UnmapSubwindows",
    "X_ConfigureWindow",
    "X_CirculateWindow",
    "X_GetGeometry",
    "X_QueryTree",
    "X_InternAtom",
    "X_GetAtomName",
    "X_ChangeProperty",
    "X_DeleteProperty",
    "X_GetProperty",
    "X_ListProperties",
    "X_SetSelectionOwner",
    "X_GetSelectionOwner",
    "X_ConvertSelection",
    "X_SendEvent",
    "X_GrabPointer",
    "X_UngrabPointer",
    "X_GrabButton",
    "X_UngrabButton",
    "X_ChangeActivePointerGrab",
    "X_GrabKeyboard",
    "X_UngrabKeyboard",
    "X_GrabKey",
    "X_UngrabKey",
    "X_AllowEvents",
    "X_GrabServer",
    "X_UngrabServer",
    "X_QueryPointer",
    "X_GetMotionEvents",
    "X_TranslateCoords",
    "X_WarpPointer",
    "X_SetInputFocus",
    "X_GetInputFocus",
    "X_QueryKeymap",
    "X_OpenFont",
    "X_CloseFont",
    "X_QueryFont",
    "X_QueryTextExtents",
    "X_ListFonts",
    "X_ListFontsWithInfo",
    "X_SetFontPath",
    "X_GetFontPath",
    "X_CreatePixmap",
    "X_FreePixmap",
    "X_CreateGC",
    "X_ChangeGC",
    "X_CopyGC",
    "X_SetDashes",
    "X_SetClipRectangles",
    "X_FreeGC",
    "X_ClearArea",
    "X_CopyArea",
    "X_CopyPlane",
    "X_PolyPoint",
    "X_PolyLine",
    "X_PolySegment",
    "X_PolyRectangle",
    "X_PolyArc",
    "X_FillPoly",
    "X_PolyFillRectangle",
    "X_PolyFillArc",
    "X_PutImage",
    "X_GetImage",
    "X_PolyText8",
    "X_PolyText16",
    "X_ImageText8",
    "X_ImageText16",
    "X_CreateColormap",
    "X_FreeColormap",
    "X_CopyColormapAndFree",
    "X_InstallColormap",
    "X_UninstallColormap",
    "X_ListInstalledColormaps",
    "X_AllocColor",
    "X_AllocNamedColor",
    "X_AllocColorCells",
    "X_AllocColorPlanes",
    "X_FreeColors",
    "X_StoreColors",
    "X_StoreNamedColor",
    "X_QueryColors",
    "X_LookupColor",
    "X_CreateCursor",
    "X_CreateGlyphCursor",
    "X_FreeCursor",
    "X_RecolorCursor",
    "X_QueryBestSize",
    "X_QueryExtension",
    "X_ListExtensions",
    "X_ChangeKeyboardMapping",
    "X_GetKeyboardMapping",
    "X_ChangeKeyboardControl",
    "X_GetKeyboardControl",
    "X_Bell",
    "X_ChangePointerControl",
    "X_GetPointerControl",
    "X_SetScreenSaver",
    "X_GetScreenSaver",
    "X_ChangeHosts",
    "X_ListHosts",
    "X_SetAccessControl",
    "X_SetCloseDownMode",
    "X_KillClient",
    "X_RotateProperties",
    "X_ForceScreenSaver",
    "X_SetPointerMapping",
    "X_GetPointerMapping",
    "X_SetModifierMapping",
    "X_GetModifierMapping",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "X_NoOperation",
  };

  constexpr unsigned char LAST_ELEM = (sizeof(X_REQ_OPCODE_NAMES) / sizeof(const char*)) - 1;
  static_assert(LAST_ELEM == X_NoOperation);

  if (opcode > LAST_ELEM)
    return "Undefined";
  return X_REQ_OPCODE_NAMES[opcode];
}
