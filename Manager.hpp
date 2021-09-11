#pragma once

#include "DisplayDataChannel.hpp"
#include "Geometry.hpp"

#include <X11/Xlib.h>

#include <map>
#include <vector>

struct MonitorCfg
{
  std::string name;
  int screen;
  std::string connector;
  uint8_t visibleInput;
  DDCDisplayId id;
};

struct Monitor
{
  const MonitorCfg& cfg;
  Rect r;
  Window root;
  Point absOrigin;
  std::optional<bool> visible;

  Window gridDraw;
  unsigned gridX;
  unsigned gridY;

  inline bool isVisible() const { return visible.has_value() && visible.value(); }
  void setVisible(std::optional<bool> visible);
};

struct Root
{
  int screen;
  Point absOrigin;
};

struct Client
{
  Window client;
  Window root;
  Rect preMax;
  bool ign;
  Point absOrigin;
};

struct Drag
{
  Window w = 0;

  int xR, yR;
  int x, y;
  int width, height;

  int btn;
  DIR dirVert;
  DIR dirHorz;
};

class Manager
{
  public:

    Manager(const std::string& display,
            const std::map<int,Point>& screens,
            const std::string& screenshotDir,
            const std::map<std::string,MonitorCfg>& monitorCfg);
    ~Manager();

    bool init();
    void run();

  private:

    // X server events
    void onReq_Map(const XMapRequestEvent& e);
    void onNot_Unmap(const XUnmapEvent& e);
    void onReq_Configure(const XConfigureRequestEvent& e);
    void onNot_Motion(const XButtonEvent& e);
    void handleFocusChange(const XFocusChangeEvent& e, bool in);
    void onKeyPress(const XKeyEvent& e);
    void onBtnPress(const XButtonEvent& e);
    void onClientMessage(const XClientMessageEvent& e);

    // Keypress handlers
    void onKeyWinExplorer(const XKeyEvent& e);
    void onKeyTerminal(const XKeyEvent& e);
    void onKeyMoveMonitor(const XKeyEvent& e);
    void onKeyMoveFocus(const XKeyEvent& e);
    void onKeyMaximize(const XKeyEvent& e);
    void onKeyUnmaximize(const XKeyEvent& e);
    void onKeyClose(const XKeyEvent& e);
    void onKeyLauncher(const XKeyEvent& e);
    void onKeyScreenshot(const XKeyEvent& e);
    void onKeyMonitorInput(const XKeyEvent& e);
    void onKeyGrid(const XKeyEvent& e);
    void onKeyGridActive(const XKeyEvent& e);
    void onKeySnapGrid(const XKeyEvent& e);
    void onKeyMoveGridLoc(const XKeyEvent& e);
    void onKeyMoveGridSize(const XKeyEvent& e);

    // Misc
    void pollDdc(int64_t now);
    void addClient(Window w, bool checkIgn);
    void switchFocus(Window w);
    void snapGrid(Window w, Rect r);
    void drawGrid(Monitor* mon, bool active);
    Window getNextWindowInDir(DIR dir, Window w);

    const std::string& _argDisp;
    const std::map<int,Point>& _argScreens;
    const std::string& _argScreenshotDir;
    const std::map<std::string,MonitorCfg>& _argMonitorCfg;

    Display* _disp = nullptr;
    std::map<Window, Client> _clients;
    std::map<Window, Root> _roots;
    std::vector<Monitor> _monitors;

    DisplayDataChannel _ddc;
    int64_t _lastDdcPoll = 0;

    Drag _drag = {};
    uint64_t _lastConfigureSerial = 0;
    uint64_t _lastMapSerial = 0;
    bool _gridActive = false;
    Window _lastFocus = 0;
};
