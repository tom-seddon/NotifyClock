// NotifyClock - copyright (C) 2016 by Tom Seddon
//
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.If not, see <http://www.gnu.org/licenses/>.

#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <Windowsx.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// pretty sure this warning is bogus, since VS2015 supports C99?
#pragma warning(disable : 4204) // nonstandard extension used: non-constant
                                // aggregate initializer

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ASSERT(X) ((X) ? (void)0 : __debugbreak(), (void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char MUTEX_NAME[] =
    "NotifyClock-E12FB551-CEB7-41CD-9826-4EE66F2C8167";
static const char CLASS_NAME[] =
    "NotifyClock-CD0284C5-0226-4C94-AB51-45575117D13F";

static const UINT NID_ID = 0;
static UINT WM_TaskbarCreated = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// clang-format off
static const uint8_t bitmaps[10][8]={
    { 0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00, },
    { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, },
    { 0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00, },
    { 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00, },
    { 0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00, },
    { 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00, },
    { 0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00, },
    { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00, },
    { 0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00, },
    { 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00, },
};
// clang-format on

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static WORD g_displayedHour = MAXWORD;
static WORD g_displayedMinute = MAXWORD;
static HWND g_hWnd;
static HBITMAP g_maskBitmap32x32;
static BOOL g_gotDateFormat;
static WCHAR *g_dateFormat;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void dprintf(const char *fmt, ...) {
  char tmp[1000];
  va_list v;

  va_start(v, fmt);
  _vsnprintf(tmp, sizeof tmp, fmt, v);
  va_end(v);

  OutputDebugString(tmp);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t Get2x(uint8_t x) {
  uint8_t r = 0;

  ASSERT(!(x & 0xf0));

  if (x & 8)
    r |= 128 | 64;

  if (x & 4)
    r |= 32 | 16;

  if (x & 2)
    r |= 8 | 4;

  if (x & 1)
    r |= 2 | 1;

  return r;
}

static void Digit2x(uint8_t *p, int c) {
  ASSERT(c >= 0 && c < 10);

  for (int i = 0; i < 8; ++i) {
    int y0 = i * 2 + 0;
    int y1 = i * 2 + 1;

    uint8_t b = bitmaps[c][i];

    uint8_t l = Get2x(b >> 4);
    uint8_t r = Get2x(b & 15);

    p[y0 * 4 + 0] = p[y1 * 4 + 0] = l;
    p[y0 * 4 + 1] = p[y1 * 4 + 1] = r;
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static BOOL CALLBACK FindDateFormat(LPWSTR lpDateFormatString, CALID CalendarID,
                                    LPARAM lParam) {
  (void)CalendarID, (void)lParam;
  // dprintf("Format: %S\n",lpDateFormatString);

  if (!g_dateFormat) {
    if (wcsstr(lpDateFormatString, L"dddd"))
      g_dateFormat = _wcsdup(lpDateFormatString);
  }

  return TRUE;
}

static void UpdateIcon(DWORD dwMessage, BOOL force) {
  SYSTEMTIME time;
  {
    GetLocalTime(&time);

    if (!force) {
      if (g_displayedHour == time.wHour && g_displayedMinute == time.wMinute)
        return;
    }
  }

  uint8_t bits[32][4];
  {
    memset(bits, 0, sizeof bits);

    Digit2x(&bits[0][0], time.wHour / 10 % 10);
    Digit2x(&bits[0][2], time.wHour % 10);
    Digit2x(&bits[16][0], time.wMinute / 10 % 10);
    Digit2x(&bits[16][2], time.wMinute % 10);
  }

  HICON hIcon;
  {
    ICONINFO iconInfo = {
        TRUE, 0, 0, g_maskBitmap32x32, CreateBitmap(32, 32, 1, 1, bits),
    };
    hIcon = CreateIconIndirect(&iconInfo);
    if (!hIcon)
      return;
  }

  if (!g_gotDateFormat) {
    EnumDateFormatsExEx(&FindDateFormat, LOCALE_NAME_USER_DEFAULT,
                        DATE_LONGDATE, 0);

    g_gotDateFormat = TRUE;
  }

  BOOL good;
  {
    NOTIFYICONDATAW nid = {
        sizeof nid,
        .hWnd = g_hWnd,
        .hIcon = hIcon,
        .uID = NID_ID,
        .uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE,
        .uCallbackMessage = WM_USER,
        .uVersion = NOTIFYICON_VERSION_4,
    };

    DWORD dateFlags;
    if (g_dateFormat)
      dateFlags = 0;
    else
      dateFlags = DATE_LONGDATE;

    int n = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, dateFlags, &time,
                            g_dateFormat, NULL, 0, NULL);

    WCHAR *dateStr = calloc(n, sizeof *dateStr);

    n = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, dateFlags, &time,
                        g_dateFormat, dateStr, n, NULL);
    ASSERT(n > 0);

    wcsncpy(nid.szTip, dateStr, ARRAYSIZE(nid.szTip) - 1);

    good = Shell_NotifyIconW(dwMessage, &nid);
  }

  DestroyIcon(hIcon);
  hIcon = NULL;

  if (!good)
    return;

  g_displayedHour = time.wHour;
  g_displayedMinute = time.wMinute;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoPopupMenu(int x, int y) {
  SetForegroundWindow(g_hWnd);

  HMENU hMenu;
  int idCopy, idExit;
  {
    hMenu = CreatePopupMenu();
    int id = 1;

    AppendMenu(hMenu, 0, idCopy = id++, "&Copy");

    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    AppendMenu(hMenu, 0, idExit = id++, "E&xit");
  }

  int id = TrackPopupMenuEx(hMenu, TPM_RETURNCMD, x, y, g_hWnd, 0);

  DestroyMenu(hMenu);
  hMenu = NULL;

  if (id == idExit)
    PostQuitMessage(0);
  else if (id == idCopy) {
    if (OpenClipboard(g_hWnd)) {
      EmptyClipboard();

      SYSTEMTIME time;
      GetLocalTime(&time);

      char text[1000];
      int len = snprintf(text, sizeof text, "%04u%02u%02u_%02u%02u%02u",
                         time.wYear, time.wMonth, time.wDay, time.wHour,
                         time.wMinute, time.wSecond);

      HANDLE hText = GlobalAlloc(GMEM_MOVEABLE, len + 1);

      memcpy(GlobalLock(hText), text, len + 1);

      GlobalUnlock(hText);

      SetClipboardData(CF_TEXT, hText);

      CloseClipboard();
    }
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam) {
  switch (uMsg) {
  case WM_USER: {
    if (lParam == WM_RBUTTONUP) {
      POINT mpt;
      GetCursorPos(&mpt);

      DoPopupMenu(mpt.x, mpt.y);
      return 0;
    }
  } break;

  case WM_CLOSE:
    PostQuitMessage(0);
    return 0;

  default:
    if (uMsg == WM_TaskbarCreated) {
      UpdateIcon(NIM_ADD, TRUE);
    }
    break;
  }

  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static VOID CALLBACK UpdateIconTimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent,
                                         DWORD dwTime) {
  (void)hWnd, (void)uMsg, (void)idEvent, (void)dwTime;

  UpdateIcon(NIM_MODIFY, FALSE);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CreateStuff(void) {
  {
    uint8_t zero[32 * 32 / 8] = {
        0,
    };

    g_maskBitmap32x32 = CreateBitmap(32, 32, 1, 1, zero);
  }

  {
    WNDCLASSEX w;

    w.cbClsExtra = 0;
    w.cbSize = sizeof w;
    w.cbWndExtra = 0;
    w.hbrBackground = GetStockObject(WHITE_BRUSH);
    w.hCursor = LoadCursor(0, IDC_ARROW);
    w.hIconSm = w.hIcon = LoadIcon(0, IDI_APPLICATION);
    w.hInstance = GetModuleHandle(NULL);
    w.lpfnWndProc = &WndProc;
    w.lpszClassName = CLASS_NAME;
    w.lpszMenuName = NULL;
    w.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassEx(&w);
  }

  g_hWnd = CreateWindow(CLASS_NAME, "NotifyClock", WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, -1, 0, 0, NULL, NULL, GetModuleHandle(0),
                        NULL);
  // ShowWindow(g_hWnd, SW_SHOW);

  UpdateIcon(NIM_ADD, TRUE);

  SetTimer(g_hWnd, 1, 1000, &UpdateIconTimerProc);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void MainLoop(void) {
  for (;;) {
    int r;
    MSG msg;

    r = GetMessage(&msg, NULL, 0, 0);
    if (r == 0 || r == -1)
      break;

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  Shell_NotifyIcon(NIM_DELETE,
                   &(NOTIFYICONDATA){.cbSize = sizeof(NOTIFYICONDATA),
                                     .hWnd = g_hWnd,
                                     .uID = NID_ID});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  (void)hInstance, (void)hPrevInstance, (void)lpCmdLine, (void)nCmdShow;

  WM_TaskbarCreated = RegisterWindowMessage("TaskbarCreated");

  if (OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME))
    return 0;

  HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

  CreateStuff();

  MainLoop();

  ReleaseMutex(hMutex);
  hMutex = NULL;

  return 0;
}
