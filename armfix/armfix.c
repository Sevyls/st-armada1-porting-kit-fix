/*
 * armfix: helper injected into Star Trek Armada (via Strlkup.dll proxy).
 *
 * Problem on macOS/Wine: the GOG DirectX wrapper draws the whole menu into the
 * game window, while Armada's (invisible) shell dialogs sit on top to receive
 * mouse input. The Wine Mac driver keeps a window fully transparent until it
 * has drawn something; macOS then lets clicks fall through it to the
 * (disabled) game window -> beep, no input.
 *
 * Fix: for every caption-less popup dialog owned by the game window, make it
 * layered with a low alpha (armfix.txt, default 13/255 - below ~13 macOS
 * treats the window as click-through) and paint it once, so the Mac driver
 * considers it drawn/opaque and macOS delivers clicks to it.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#ifndef LWA_ALPHA
#define LWA_ALPHA 0x2
#endif

#define MAX_TRACK 64
static HWND tracked[MAX_TRACK];
static BYTE g_alpha = 13;

static int ntracked;

static int is_tracked(HWND h)
{
    for (int i = 0; i < ntracked; i++) if (tracked[i] == h) return 1;
    return 0;
}

static void untrack_invisible(void)
{
    int j = 0;
    for (int i = 0; i < ntracked; i++)
        if (IsWindow(tracked[i]) && IsWindowVisible(tracked[i])) tracked[j++] = tracked[i];
    ntracked = j;
}

static void fix_dialog(HWND h)
{
    LONG ex = GetWindowLongA(h, GWL_EXSTYLE);
    RECT rc;
    HDC dc;

    if (g_alpha < 255 && !(ex & WS_EX_LAYERED))
        SetWindowLongA(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    if (g_alpha < 255) SetLayeredWindowAttributes(h, 0, g_alpha, LWA_ALPHA);

    /* paint the dialog once so the Mac driver treats it as drawn (opaque);
       at low opacity it is practically invisible but receives clicks. */
    GetClientRect(h, &rc);
    if ((dc = GetDC(h)))
    {
        HBRUSH b = (HBRUSH)GetStockObject(BLACK_BRUSH);
        FillRect(dc, &rc, b);
        ReleaseDC(h, dc);
    }
    GdiFlush();
}

static BOOL CALLBACK enum_proc(HWND h, LPARAM lp)
{
    DWORD pid;
    char cls[64];
    HWND owner;
    LONG style;

    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    if (!IsWindowVisible(h)) return TRUE;
    if (!GetClassNameA(h, cls, sizeof(cls)) || lstrcmpA(cls, "#32770")) return TRUE;
    style = GetWindowLongA(h, GWL_STYLE);
    if (!(style & WS_POPUP) || (style & WS_CHILD)) return TRUE;
    if ((style & WS_CAPTION) == WS_CAPTION) return TRUE;   /* real message/config dialogs */
    /* the dialog must belong to the game: its owner chain (shell dialogs
       open nested sub-dialogs owned by other dialogs) must reach the
       "Armada" main window */
    owner = GetWindow(h, GW_OWNER);
    for (int depth = 0; owner && depth < 8; depth++)
    {
        if (GetClassNameA(owner, cls, sizeof(cls)) && !lstrcmpA(cls, "Armada")) break;
        owner = GetWindow(owner, GW_OWNER);
    }
    if (!owner) return TRUE;
    if (is_tracked(h)) return TRUE;

    fix_dialog(h);
    if (ntracked < MAX_TRACK) tracked[ntracked++] = h;
    return TRUE;
}

static void read_config(void)
{
    char path[MAX_PATH], *s;
    char buf[32] = {0};
    HANDLE f;
    DWORD got = 0;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    if ((s = strrchr(path, '\\'))) s[1] = 0;
    lstrcatA(path, "armfix.txt");
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE)
    {
        if (ReadFile(f, buf, sizeof(buf) - 1, &got, NULL) && got)
        {
            int v = atoi(buf);
            if (v >= 0 && v <= 255) g_alpha = (BYTE)v;
        }
        CloseHandle(f);
    }
}

static void apply_alpha_all(void)
{
    for (int i = 0; i < ntracked; i++)
    {
        HWND h = tracked[i];
        LONG ex = GetWindowLongA(h, GWL_EXSTYLE);
        if (!(ex & WS_EX_LAYERED)) SetWindowLongA(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        SetLayeredWindowAttributes(h, 0, g_alpha, LWA_ALPHA);
    }
}

static DWORD WINAPI worker(LPVOID p)
{
    read_config();
    for (;;)
    {
        BYTE old = g_alpha;
        read_config();
        if (old != g_alpha) apply_alpha_all();
        untrack_invisible();
        EnumWindows(enum_proc, 0);
        Sleep(250);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(inst);
        CloseHandle(CreateThread(NULL, 0, worker, NULL, 0, NULL));
    }
    return TRUE;
}
