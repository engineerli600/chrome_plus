#ifndef TABBOOKMARK_H_
#define TABBOOKMARK_H_

#include "iaccessible.h"

// https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/app/chrome_command_ids.h

// 打开历史记录页面
#define IDC_SHOW_HISTORY 40010
// 打开设置页面
#define IDC_OPTIONS 40015
// 打开扩展程序页面
#define IDC_MANAGE_EXTENSIONS 40022
// 显示网站二维码
#define IDC_QRCODE_GENERATOR 35021



HHOOK mouse_hook = nullptr;

#define KEY_PRESSED 0x8000
bool IsPressed(int key) {
  return key && (::GetKeyState(key) & KEY_PRESSED) != 0;
}

// Compared with `IsOnlyOneTab`, this function additionally implements tick
// fault tolerance to prevent users from directly closing the window when
// they click too fast.
bool IsNeedKeep(NodePtr top_container_view) {
  if (!IsKeepLastTab()) {
    return false;
  }

  auto tab_count = GetTabCount(top_container_view);
  bool keep_tab = (tab_count == 1);

  static auto last_closing_tab_tick = GetTickCount64();
  auto tick = GetTickCount64() - last_closing_tab_tick;
  last_closing_tab_tick = GetTickCount64();

  if (tick > 50 && tick <= 250 && tab_count == 2) {
    keep_tab = true;
  }

  return keep_tab;
}

// If the top_container_view is not found at the first time, try to close the
// find-in-page bar and find the top_container_view again.
NodePtr HandleFindBar(HWND hwnd, POINT pt) {
  // If the mouse is clicked directly on the find-in-page bar, follow Chrome's
  // original logic. Otherwise, clicking the button on the find-in-page bar may
  // directly close the find-in-page bar.
  if (IsOnDialog(hwnd, pt)) {
    return nullptr;
  }
  NodePtr top_container_view = GetTopContainerView(hwnd);
  if (!top_container_view) {
    ExecuteCommand(IDC_CLOSE_FIND_OR_STOP, hwnd);
    top_container_view = GetTopContainerView(hwnd);
    if (!top_container_view) {
      return nullptr;
    }
  }
  return top_container_view;
}

class IniConfig {
 public:
  IniConfig()
      : is_double_click_close(IsDoubleClickClose()),
        is_right_click_close(IsRightClickClose()),
        is_wheel_tab(IsWheelTab()),
        is_wheel_tab_when_press_right_button(IsWheelTabWhenPressRightButton()),
        is_bookmark_new_tab(IsBookmarkNewTab()),
        is_open_url_new_tab(IsOpenUrlNewTabFun()) {}

  bool is_double_click_close;
  bool is_right_click_close;
  bool is_wheel_tab;
  bool is_wheel_tab_when_press_right_button;
  std::string is_bookmark_new_tab;
  std::string is_open_url_new_tab;
};

IniConfig config;

// HandleMouseWheel 函数
// 处理鼠标滚轮事件
// Use the mouse wheel to switch tabs
bool HandleMouseWheel(WPARAM wParam, LPARAM lParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_MOUSEWHEEL ||
      (!config.is_wheel_tab && !config.is_wheel_tab_when_press_right_button)) {
    return false;
  }

  HWND hwnd = GetFocus();
  NodePtr top_container_view = GetTopContainerView(hwnd);

  PMOUSEHOOKSTRUCTEX pwheel = (PMOUSEHOOKSTRUCTEX)lParam;
  int zDelta = GET_WHEEL_DELTA_WPARAM(pwheel->mouseData);

  // If the mouse wheel is used to switch tabs when the mouse is on the tab bar.
  if (config.is_wheel_tab && IsOnTheTabBar(top_container_view, pmouse->pt)) {
    hwnd = GetTopWnd(hwnd);
    if (zDelta > 0) {
      ExecuteCommand(IDC_SELECT_PREVIOUS_TAB, hwnd);
    } else {
      ExecuteCommand(IDC_SELECT_NEXT_TAB, hwnd);
    }
    return true;
  }

  // If it is used to switch tabs when the right button is held.
  if (config.is_wheel_tab_when_press_right_button && IsPressed(VK_RBUTTON)) {
    hwnd = GetTopWnd(hwnd);
    if (zDelta > 0) {
      ExecuteCommand(IDC_SELECT_PREVIOUS_TAB, hwnd);
    } else {
      ExecuteCommand(IDC_SELECT_NEXT_TAB, hwnd);
    }
    return true;
  }

  return false;
}

// HandleDoubleClick 函数
// 处理双击事件
// Double-click to close tab.
int HandleDoubleClick(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_LBUTTONDBLCLK || !config.is_double_click_close) {
    return 0;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = HandleFindBar(hwnd, pt);
  if (!top_container_view) {
    return 0;
  }

  bool is_on_one_tab = IsOnOneTab(top_container_view, pt);
  bool is_on_close_button = IsOnCloseButton(top_container_view, pt);
  bool is_only_one_tab = IsOnlyOneTab(top_container_view);
  if (!is_on_one_tab || is_on_close_button) {
    return 0;
  }
  if (is_only_one_tab) {
    ExecuteCommand(IDC_NEW_TAB, hwnd);
    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB , hwnd);
    ExecuteCommand(IDC_CLOSE_TAB, hwnd);
  } else {
    ExecuteCommand(IDC_CLOSE_TAB, hwnd);
  }
  return 1;
}

// HandleRightClick 函数
// 处理右键点击事件，用于关闭标签页（按住 Shift 键时显示原始菜单）
// Right-click to close tab (Hold Shift to show the original menu).
int HandleRightClick(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_RBUTTONUP || IsPressed(VK_SHIFT) ||
      !config.is_right_click_close) {
    return 0;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = HandleFindBar(hwnd, pt);
  if (!top_container_view) {
    return 0;
  }

  bool is_on_one_tab = IsOnOneTab(top_container_view, pt);
  bool keep_tab = IsNeedKeep(top_container_view);

  if (is_on_one_tab) {
    if (keep_tab) {
    ExecuteCommand(IDC_NEW_TAB, hwnd);
    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB , hwnd);
    ExecuteCommand(IDC_CLOSE_TAB, hwnd);
    
    // ExecuteCommand(IDC_NEW_TAB, hwnd);
    // ExecuteCommand(IDC_WINDOW_CLOSE_OTHER_TABS, hwnd);
    } else {
      // Attempt new SendKey function which includes a `dwExtraInfo`
      // value (MAGIC_CODE).
      SendKey(VK_MBUTTON);
    }
    return 1;
  }
  return 0;
}

// HandleMiddleClick 函数
// 处理中键点击事件
// Preserve the last tab when the middle button is clicked on the tab.
int HandleMiddleClick(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_MBUTTONUP || IsPressed(VK_CONTROL) ) {
    return 0;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = HandleFindBar(hwnd, pt);
  if (!top_container_view) {
    return 0;
  }

  bool is_on_one_tab = IsOnOneTab(top_container_view, pt);
  bool keep_tab = IsNeedKeep(top_container_view);

  if (is_on_one_tab && keep_tab) {
    //SendKey(VK_CONTROL, 'H');
    ExecuteCommand(IDC_COPY_URL, hwnd);
    SendKey(VK_CONTROL, VK_SHIFT, 'B');
    //ExecuteCommand(IDC_NEW_TAB, hwnd);
    //ExecuteCommand(IDC_SELECT_PREVIOUS_TAB , hwnd);
    //ExecuteCommand(IDC_CLOSE_TAB, hwnd);
    
    // ExecuteCommand(IDC_NEW_TAB, hwnd);
    // ExecuteCommand(IDC_WINDOW_CLOSE_OTHER_TABS, hwnd);
    return 1;
  }

  return 0;
}

// HandleLeftClick 函数
// 处理鼠标左键点击事件，如果当前标签是最后一个标签，且需要保留最后一个标签页，并且鼠标在关闭按钮上，当鼠标左键时，不关闭标签页
int HandleLeftClick(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_LBUTTONUP) {  // 如果不是左键松开事件，则返回 0
    return 0;
  }

  POINT pt = pmouse->pt;  // 获取鼠标点击位置
  HWND hwnd = WindowFromPoint(pt);  // 获取点击位置的窗口句柄
  NodePtr top_container_view = HandleFindBar(hwnd, pt);  // 获取 top_container_view
  if (!top_container_view) {
    return 0;  // 如果未找到 top_container_view，则返回 0
  }

  bool is_on_one_tab = IsOnOneTab(top_container_view, pt);
  bool is_on_close_button = IsOnCloseButton(top_container_view, pt); 
  bool keep_tab = IsNeedKeep(top_container_view);  // 检查是否需要保留标签页

  if (is_on_one_tab && is_on_close_button && keep_tab) {
    ExecuteCommand(IDC_NEW_TAB, hwnd);
    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB , hwnd);
    ExecuteCommand(IDC_CLOSE_TAB, hwnd);
    
    // ExecuteCommand(IDC_NEW_TAB, hwnd);
    // ExecuteCommand(IDC_WINDOW_CLOSE_OTHER_TABS, hwnd);
    return 1;  // 返回 1，表示处理了事件且不关闭标签页
  }

  return 0;  // 返回 0，表示未处理事件
}

// 处理右键点击新建标签按钮的事件
int HandleRightClickOnNewTabButton(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_RBUTTONUP) {
    return 0;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = HandleFindBar(hwnd, pt);
  if (!top_container_view) {
    return 0;
  }

  bool is_on_new_tab_button = IsOnNewTabButton(top_container_view, pt);

  // 判断是否点击在新建标签按钮上
  if (is_on_new_tab_button) {
    // 模拟中键执行粘贴并访问good
    //SendKey(VK_MBUTTON);

    // for test
    ExecuteCommand(IDC_SHOW_HISTORY, hwnd);
    // 执行命令后，再次进行动作（比如切换到其他标签）会有1秒的卡顿，这时再点一下左键或右键或中键，可以解决此问题。
    Sleep(100);
    SendKey(VK_LBUTTON);


    //SendKey(VK_CONTROL, 'H');
    return 1;
  }

  return 0;
}

// 处理点击书签的事件
// Open bookmarks in a new tab.
bool HandleBookmark(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_LBUTTONUP || IsPressed(VK_CONTROL) || IsPressed(VK_SHIFT) ||
      config.is_bookmark_new_tab == "disabled") {
    return false;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = GetTopContainerView(
      GetFocus());  // Must use `GetFocus()`, otherwise when opening bookmarks
                    // in a bookmark folder (and similar expanded menus),
                    // `top_container_view` cannot be obtained, making it
                    // impossible to correctly determine `is_on_new_tab`. See
                    // #98.

  bool is_on_bookmark = IsOnBookmark(hwnd, pt);
  bool is_on_new_tab = IsOnNewTab(top_container_view);

  if (is_on_bookmark && !is_on_new_tab) {
    if (config.is_bookmark_new_tab == "foreground") {
      SendKey(VK_MBUTTON, VK_SHIFT);
    } else if (config.is_bookmark_new_tab == "background") {
      SendKey(VK_MBUTTON);
    }
    return true;
  }

  return false;
}

// MouseProc 函数
// 鼠标事件钩子函数，用于处理鼠标事件
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode != HC_ACTION) {
    return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
  }

  do {
    if (wParam == WM_MOUSEMOVE || wParam == WM_NCMOUSEMOVE) {
      break;
    }
    PMOUSEHOOKSTRUCT pmouse = (PMOUSEHOOKSTRUCT)lParam;

    // Defining a `dwExtraInfo` value to prevent hook the message sent by
    // Chrome++ itself.
    if (pmouse->dwExtraInfo == MAGIC_CODE) {
      break;
    }

    if (HandleMouseWheel(wParam, lParam, pmouse)) {
      return 1;
    }

    if (HandleDoubleClick(wParam, pmouse) != 0) {
      // Do not return 1. Returning 1 could cause the keep_tab to fail
      // or trigger double-click operations consecutively when the user
      // double-clicks on the tab page rapidly and repeatedly.
    }

    if (HandleRightClick(wParam, pmouse) != 0) {
      return 1;
    }

    if (HandleMiddleClick(wParam, pmouse) != 0) {
      return 1;
    }

    if (HandleBookmark(wParam, pmouse)) {
      return 1;
    }

    // 添加对 HandleRightClickOnNewTabButton 函数的调用
    if (HandleRightClickOnNewTabButton(wParam, pmouse) != 0) {
      return 1;
    }

    // 添加对 HandleLeftClick 函数的调用
    if (HandleLeftClick(wParam, pmouse) != 0) {
      return 1;
    }

  } while (0);
  return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
}

int HandleKeepTab(WPARAM wParam) {
  if (!(wParam == 'W' && IsPressed(VK_CONTROL) && !IsPressed(VK_SHIFT)) &&
      !(wParam == VK_F4 && IsPressed(VK_CONTROL))) {
    return 0;
  }

  HWND hwnd = GetFocus();
  wchar_t name[256] = {0};
  GetClassName(hwnd, name, 255);
  if (wcsstr(name, L"Chrome_WidgetWin_") == nullptr) {
    return 0;
  }

  if (IsFullScreen(hwnd)) {
    // Have to exit full screen to find the tab.
    ExecuteCommand(IDC_FULLSCREEN, hwnd);
  }

  HWND tmp_hwnd = hwnd;
  hwnd = GetAncestor(tmp_hwnd, GA_ROOTOWNER);
  ExecuteCommand(IDC_CLOSE_FIND_OR_STOP, tmp_hwnd);

  NodePtr top_container_view = GetTopContainerView(hwnd);
  if (!IsNeedKeep(top_container_view)) {
    return 0;
  }

  ExecuteCommand(IDC_NEW_TAB, hwnd);
  ExecuteCommand(IDC_WINDOW_CLOSE_OTHER_TABS, hwnd);
  return 1;
}

int HandleOpenUrlNewTab(WPARAM wParam) {
  if (!(config.is_open_url_new_tab != "disabled" && wParam == VK_RETURN &&
        !IsPressed(VK_MENU))) {
    return 0;
  }

  NodePtr top_container_view = GetTopContainerView(GetForegroundWindow());
  if (IsOmniboxFocus(top_container_view) && !IsOnNewTab(top_container_view)) {
    if (config.is_open_url_new_tab == "foreground") {
      SendKey(VK_MENU, VK_RETURN);
    } else if (config.is_open_url_new_tab == "background") {
      SendKey(VK_SHIFT, VK_MENU, VK_RETURN);
    }
    return 1;
  }
  return 0;
}

HHOOK keyboard_hook = nullptr;
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION && !(lParam & 0x80000000))  // pressed
  {
    if (HandleKeepTab(wParam) != 0) {
      return 1;
    }

    if (HandleOpenUrlNewTab(wParam) != 0) {
      return 1;
    }
  }
  return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

void TabBookmark() {
  mouse_hook =
      SetWindowsHookEx(WH_MOUSE, MouseProc, hInstance, GetCurrentThreadId());
  keyboard_hook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, hInstance,
                                   GetCurrentThreadId());
}

#endif  // TABBOOKMARK_H_
