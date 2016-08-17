#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ASSERT(X) ((X)?(void)0:__debugbreak(),(void)0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char MUTEX_NAME[] = "NotifyClock-E12FB551-CEB7-41CD-9826-4EE66F2C8167";
static const char CLASS_NAME[] = "NotifyClock-CD0284C5-0226-4C94-AB51-45575117D13F";

static const UINT NID_ID = 0;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint8_t bitmaps[10][8] = {
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static HWND g_hWnd;
static HICON g_hCurrentIcon16x16;
static HICON g_hCurrentIcon32x32;
static HBITMAP g_maskBitmap16x16;
static HBITMAP g_maskBitmap32x32;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void dprintf(const char *fmt, ...)
{
	char tmp[1000];
	va_list v;

	va_start(v, fmt);
	_vsnprintf(tmp, sizeof tmp, fmt, v);
	va_end(v);

	OutputDebugString(tmp);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t Get1x(uint8_t x)
{
	return x;
}

static void Digit(uint8_t *p, int c)
{
	ASSERT(c >= 0 && c < 10);

	for (int i = 0; i < 8; ++i)
	{
		uint8_t b = Get1x(bitmaps[c][i]);

		p[i * 2] = b;
	}
}

static uint8_t Get2x(uint8_t x)
{
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

static void Digit2x(uint8_t *p, int c)
{
	ASSERT(c >= 0 && c < 10);

	for (int i = 0; i < 8; ++i)
	{
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

static void CreateTimeIcons(HICON *hIcon16x16, HICON *hIcon32x32, int top, int bot)
{
	{
		uint8_t xor[16][2];

		memset(xor, 0, sizeof xor);

		Digit(&xor[0][0], top / 10 % 10);
		Digit(&xor[0][1], top % 10);
		Digit(&xor[8][0], bot / 10 % 10);
		Digit(&xor[8][1], bot % 10);

		ICONINFO ii = { TRUE,0,0,g_maskBitmap16x16,CreateBitmap(16,16,1,1,xor), };
		*hIcon16x16 = CreateIconIndirect(&ii);
	}

	{
		uint8_t xor[32][4];

		memset(xor, 0, sizeof xor);

		Digit2x(&xor[0][0], top / 10 % 10);
		Digit2x(&xor[0][2], top % 10);
		Digit2x(&xor[16][0], bot / 10 % 10);
		Digit2x(&xor[16][2], bot % 10);

		ICONINFO ii = { TRUE,0,0,g_maskBitmap32x32,CreateBitmap(32,32,1,1,xor), };
		*hIcon32x32 = CreateIconIndirect(&ii);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoNotify(DWORD dwMessage, HICON hIcon)
{
	NOTIFYICONDATA nid = { sizeof nid };

	nid.hWnd = g_hWnd;
	nid.hIcon = hIcon;
	nid.uID = NID_ID;
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	nid.uCallbackMessage = WM_USER;

	snprintf(nid.szTip, sizeof nid.szTip, "blah");

	BOOL good = Shell_NotifyIcon(dwMessage, &nid);
	ASSERT(good);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void UpdateIcon(DWORD dwMessage)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	HICON hNewIcon16x16, hNewIcon32x32;
	CreateTimeIcons(&hNewIcon16x16, &hNewIcon32x32, time.wMinute, time.wSecond);

	DoNotify(dwMessage, hNewIcon32x32);
	SendMessage(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hNewIcon32x32);
	SendMessage(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hNewIcon16x16);

	if (g_hCurrentIcon16x16)
		DestroyIcon(g_hCurrentIcon16x16);

	if (g_hCurrentIcon32x32)
		DestroyIcon(g_hCurrentIcon32x32);

	g_hCurrentIcon16x16 = hNewIcon16x16;
	g_hCurrentIcon32x32 = hNewIcon32x32;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void CreateStuff(void)
{
	{
		uint8_t zero[32 * 32 / 8] = { 0, };

		g_maskBitmap16x16 = CreateBitmap(16, 16, 1, 1, zero);
		g_maskBitmap32x32 = CreateBitmap(32, 32, 1, 1, zero);
	}

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

	g_hWnd = CreateWindow(CLASS_NAME, "NotifyClock", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, -1, CW_USEDEFAULT, -1, NULL, NULL, GetModuleHandle(0), NULL);
	ShowWindow(g_hWnd, SW_SHOW);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static VOID CALLBACK UpdateIconTimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	UpdateIcon(NIM_MODIFY);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void MainLoop(void)
{
	for (;;)
	{
		int r;
		MSG msg;

		r = GetMessage(&msg, NULL, 0, 0);
		if (r == 0 || r == -1)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DoNotify(NIM_DELETE, NULL);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	(void)hInstance, (void)hPrevInstance, (void)lpCmdLine, (void)nCmdShow;

	if (OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME))
		return 0;

	dprintf("Icon dims: %d x %d\n", GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));

	HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

	CreateStuff();

	UpdateIcon(NIM_ADD);

	SetTimer(g_hWnd, 1, 1000, &UpdateIconTimerProc);

	MainLoop();

	ReleaseMutex(hMutex);
	hMutex = NULL;

	return 0;
}