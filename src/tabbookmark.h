#ifndef TABBOOKMARK_H_
#define TABBOOKMARK_H_

#include "iaccessible.h"

// 也可以在utils.h文件添加，但如果在utils.h里添加就多修改一个文件。
// https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/app/chrome_command_ids.h

// 打开历史记录页面
#define IDC_SHOW_HISTORY 40010
// 打开设置页面
#define IDC_OPTIONS 40015
// 打开扩展程序页面
#define IDC_MANAGE_EXTENSIONS 40022
// 显示网站二维码
#define IDC_QRCODE_GENERATOR 35021
// 重新打开先前关闭的标签页
#define IDC_RESTORE_TAB 34028

#define IDC_RELOAD_CLEARING_CACHE 33009
#define IDC_COPY_URL 34060
#define IDC_FOCUS_THIS_TAB 35017
#define IDC_ALL_WINDOWS_FRONT 34048



HHOOK mouse_hook = nullptr;

#define KEY_PRESSED 0x8000
bool IsPressed(int key) {
  return key && (::GetKeyState(key) & KEY_PRESSED) != 0;
}



// 恢复窗口焦点
void RestoreFocus(POINT pt) {
  std::thread([pt]() {
    // 等待命令执行
    Sleep(50);

    // 通过鼠标位置找到窗口并聚焦
    HWND window_at_point = WindowFromPoint(pt);
    if (window_at_point) {
      // 发送点击事件给窗口
      SendMessage(window_at_point, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
      SendMessage(window_at_point, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
    }
  }).detach();
}


// 执行命令并确保窗口保持焦点
void ExecuteCommandAndKeepFocus(DWORD command, HWND hwnd, POINT pt) {
  // 执行命令
  ExecuteCommand(command, hwnd);
  
  // 恢复焦点
  RestoreFocus(pt);
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
  if (wParam != WM_MBUTTONUP) {
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
    ExecuteCommand(IDC_NEW_TAB, hwnd);
    ExecuteCommand(IDC_SELECT_PREVIOUS_TAB , hwnd);
    ExecuteCommand(IDC_CLOSE_TAB, hwnd);
    
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

// 处理 右键点击按钮 的事件
int HandleRightClickButton(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
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
  bool is_on_search_tab_button = IsOnSearchTabButton(top_container_view, pt);
  bool is_on_bookmark_button = IsOnBookmarkButton(top_container_view, pt);
  bool is_on_view_site_info_button = IsOnViewSiteInfoButton(top_container_view, pt);
  bool is_on_extensions_button = IsOnExtensionsButton(top_container_view, pt);
  bool is_on_chromium_button = IsOnChromiumButton(top_container_view, pt);


  // 判断是否点击在 新建标签 按钮上
  if (is_on_new_tab_button) {
    // 在原版chrome上，在该按钮上点击中键可以新建标签页并执行粘贴并转到/搜索的功能。
    SendKey(VK_MBUTTON);
    return 1;
    // 判断是否点击在 搜索标签页 按钮上
  } else if (is_on_search_tab_button) {
    
    ExecuteCommandAndKeepFocus(IDC_SHOW_HISTORY, hwnd, pt);

    /*     
    打开页面后马上进行其他动作会无反应，具体现象：例如打开历史记录页面后，鼠标马上移动到左侧的标签页进行点击，这时发现不起作用，必须主动点击一次后，再进行第二次点击，才会切换到左侧的标签页。
    紧接着发送左键或中键或右键可以解决此问题，因为在原版chrome上，在该按钮上点击中键或右键是无动作的，所以可以用来解决此问题。
    也欢迎大家提出其他解决方法。
    */
    //SendKey(VK_MBUTTON);

    return 1;
  } else if (is_on_bookmark_button) {
    ExecuteCommandAndKeepFocus(IDC_QRCODE_GENERATOR, hwnd, pt);
    //SendKey(VK_MBUTTON);
    return 1;
  } else if (is_on_view_site_info_button) {
    ExecuteCommandAndKeepFocus(IDC_QRCODE_GENERATOR, hwnd, pt);
    //SendKey(VK_MBUTTON);
    return 1;
  } else if (is_on_extensions_button) {
    ExecuteCommandAndKeepFocus(IDC_MANAGE_EXTENSIONS, hwnd, pt);
    //SendKey(VK_MBUTTON);
    return 1;
  } else if (is_on_chromium_button) {
    ExecuteCommandAndKeepFocus(IDC_OPTIONS, hwnd, pt);
    //SendKey(VK_MBUTTON);
    return 1;
  }

  return 0;
}

/* 
// 处理 右键点击 书签上的按钮 的事件
int HandleRightClickOnBookmarkHistory(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_RBUTTONUP) {
    return 0;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = HandleFindBar(hwnd, pt);
  if (!top_container_view) {
    return 0;
  }

  bool is_on_bookmark_history = IsOnBookmarkHistory(hwnd, pt);

  if (is_on_bookmark_history) {
    //ExecuteCommand(IDC_SHOW_HISTORY, hwnd);
    ExecuteCommandAndKeepFocus(IDC_SHOW_HISTORY, hwnd);
    return 1;
  }

  return 0;
}

 */

// 处理 右键点击书签栏上的里history按钮 的事件
int HandleRightClickOnBookmarkHistory(WPARAM wParam, PMOUSEHOOKSTRUCT pmouse) {
  if (wParam != WM_RBUTTONUP) {
    return 0;
  }

  POINT pt = pmouse->pt;
  HWND hwnd = WindowFromPoint(pt);
  NodePtr top_container_view = HandleFindBar(hwnd, pt);
  if (!top_container_view) {
    return 0;
  }

  bool is_on_bookmark_history = IsOnBookmarkHistory(top_container_view, pt);

  if (is_on_bookmark_history) {

    ExecuteCommand(IDC_SHOW_HISTORY, hwnd);
    ExecuteCommand(IDC_ALL_WINDOWS_FRONT, hwnd);
    
    //RestoreFocus(pt);
    
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

    // 添加对 HandleRightClickButton 函数的调用
    if (HandleRightClickButton(wParam, pmouse) != 0) {
      return 1;
    }

    // 添加对 HandleRightClickOnBookmarkHistory 函数的调用
    if (HandleRightClickOnBookmarkHistory(wParam, pmouse) != 0) {
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
  ExecuteCommand(IDC_SELECT_PREVIOUS_TAB , hwnd);
  ExecuteCommand(IDC_CLOSE_TAB, hwnd);
  
  //ExecuteCommand(IDC_NEW_TAB, hwnd);
  //ExecuteCommand(IDC_WINDOW_CLOSE_OTHER_TABS, hwnd);
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
