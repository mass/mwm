#pragma once

#include <u/log.hpp>

#include <X11/cursorfont.h>
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>

#include <cassert>
#include <cstring>

constexpr static inline const char* XEventToString(const XEvent& e);
constexpr static inline const char* XOpcodeToString(const unsigned char opcode);
static inline int XError(Display* display, XErrorEvent* e);
static inline Rect GetWinRect(Display* disp, Window w);
static inline Window GetWinRoot(Display* disp, Window w);
static inline void DumpXRR(Display* disp, Window root);

/// Implementation /////////////////////////////////////////////////////////////

static inline int XError(Display* display, XErrorEvent* e) {
  char buf[4096];
  ::memset(buf, 0, sizeof(buf));
  XGetErrorText(display, e->error_code, buf, sizeof(buf));
  LOG(ERROR) << "X ERROR"
             << " display=" << DisplayString(display)
             << " majorOpcode=" << XOpcodeToString(e->request_code)
             << " minorOpcode=" << (int) e->minor_code
             << " what=(" << buf << ")";
  return 0;
}

static inline Rect GetWinRect(Display* disp, Window w)
{
  XWindowAttributes attr;
  if (XGetWindowAttributes(disp, w, &attr) == 0)
    return Rect();
  return Rect(attr.x, attr.y, attr.width, attr.height);
}

static inline Window GetWinRoot(Display* disp, Window w)
{
  Window root, parent;
  Window* children = nullptr;
  uint32_t num;
  const auto ret = XQueryTree(disp, w, &root, &parent, &children, &num);
  XFree(children);
  return (ret != 0) ? root : 0;
}

constexpr static inline const char* XEventToString(const XEvent& e)
{
  constexpr const char* const X_EVENT_TYPE_NAMES[] = {
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

constexpr static inline const char* XOpcodeToString(const unsigned char opcode)
{
  constexpr const char* const X_REQ_OPCODE_NAMES[] = {
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

static inline void DumpXRR(Display* disp, Window root)
{
  auto* res = XRRGetScreenResources(disp, root);
  assert(res != nullptr);

  LOG(INFO) << "screen resources"
            << " root=" << root
            << " n_crtc=" << res->ncrtc
            << " n_output=" << res->noutput
            << " n_mode=" << res->nmode;

  for (int i = 0; i < res->ncrtc; ++i) {
    auto* crtc = XRRGetCrtcInfo(disp, res, res->crtcs[i]);
    assert(crtc != nullptr);
    LOG(INFO) << "crtc"
              << " i=" << i
              << " xid=" << res->crtcs[i]
              << " x=" << crtc->x
              << " y=" << crtc->y
              << " w=" << crtc->width
              << " h=" << crtc->height
              << " rot=" << crtc->rotation
              << " rots=" << crtc->rotations
              << " n_output=" << crtc->noutput
              << " n_possible=" << crtc->npossible;

    for (int j = 0; j < crtc->noutput; ++j) {
      auto* output = XRRGetOutputInfo(disp, res, crtc->outputs[j]);
      assert(output != nullptr);
      LOG(INFO) << "  output"
                << " j=" << j
                << " xid=" << crtc->outputs[j]
                << " name=(" << std::string_view(output->name, output->nameLen) << ")"
                << " mm_w=" << output->mm_width
                << " mm_h=" << output->mm_height
                << " conn=" << output->connection
                << " subpixelOrder=" << output->subpixel_order
                << " n_crtc=" << output->ncrtc
                << " n_clones=" << output->nclone
                << " n_modes=" << output->nmode
                << " n_preferred=" << output->npreferred;
      XRRFreeOutputInfo(output);
    }
    XRRFreeCrtcInfo(crtc);
  }
  XRRFreeScreenResources(res);
}
