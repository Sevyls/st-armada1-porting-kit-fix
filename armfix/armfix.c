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


/* ---- keep the game "active" when the user switches away (Cmd+Tab) -------
 * On deactivation the game and Wine's wined3d minimize the fullscreen window
 * and release/restore Direct3D resources. Under Wine on macOS the window
 * can't be restored afterwards, and doing it during a loading screen crashes
 * the game. We subclass the game window and hide the deactivation (and the
 * matching re-activation) from everything behind us. */
static BOOL g_hid_deactivate;

/* While the game is in the background its fullscreen windows would still
   cover the screen (and catch clicks). Make them fully transparent - macOS
   then shows the other apps and passes clicks through - and restore them
   when the game becomes active again. Nothing is minimized or hidden for
   Windows, so the game and Direct3D don't notice anything. */
#define MAX_FADED 16
static HWND g_faded[MAX_FADED];
static BYTE g_faded_alpha[MAX_FADED];
static int g_nfaded;

static BOOL CALLBACK fade_out_proc(HWND h, LPARAM lp)
{
    char cls[64];
    LONG ex;
    if (g_nfaded >= MAX_FADED || !IsWindowVisible(h)) return TRUE;
    if (!GetClassNameA(h, cls, sizeof(cls))) return TRUE;
    if (lstrcmpA(cls, "Armada") && !is_tracked(h)) return TRUE;
    ex = GetWindowLongA(h, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) SetWindowLongA(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    SetLayeredWindowAttributes(h, 0, 0, LWA_ALPHA);
    g_faded[g_nfaded] = h;
    g_faded_alpha[g_nfaded] = lstrcmpA(cls, "Armada") ? g_alpha : 255;
    g_nfaded++;
    return TRUE;
}

static void fade_out(void)
{
    g_nfaded = 0;
    EnumThreadWindows(GetCurrentThreadId(), fade_out_proc, 0);
}

static void fade_in(void)
{
    for (int i = 0; i < g_nfaded; i++)
        if (IsWindow(g_faded[i]))
            SetLayeredWindowAttributes(g_faded[i], 0, g_faded_alpha[i], LWA_ALPHA);
    g_nfaded = 0;
}

/* Returns TRUE if the message must be hidden from the procs behind us. */
static BOOL swallow(UINT msg, WPARAM wp)
{
    switch (msg)
    {
    case WM_ACTIVATEAPP:
        if (!wp) { g_hid_deactivate = TRUE; fade_out(); return TRUE; }
        if (g_hid_deactivate) { g_hid_deactivate = FALSE; fade_in(); return TRUE; }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) return TRUE;
        break;
    }
    return FALSE;
}

/* Several wrapper slots, each with its own "previous" window procedure, so we
   can put ourselves on top again if wined3d or the GOG wrapper subclass the
   window after us, without ever creating a loop in the chain. */
#define NSLOTS 8
static WNDPROC g_prev[NSLOTS];
#define SLOT(n) \
static LRESULT CALLBACK keep_active_proc##n(HWND h, UINT m, WPARAM w, LPARAM l) \
{ \
    if (swallow(m, w)) return 0; \
    return CallWindowProcA(g_prev[n], h, m, w, l); \
}
SLOT(0) SLOT(1) SLOT(2) SLOT(3) SLOT(4) SLOT(5) SLOT(6) SLOT(7)
static WNDPROC g_slots[NSLOTS] = {
    keep_active_proc0, keep_active_proc1, keep_active_proc2, keep_active_proc3,
    keep_active_proc4, keep_active_proc5, keep_active_proc6, keep_active_proc7 };
static int g_used;
static HWND g_game;

static void keep_game_active(void)
{
    HWND h = FindWindowA("Armada", NULL);
    DWORD pid;
    WNDPROC cur;
    int i;

    if (!h) return;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) return;
    if (h != g_game) { g_game = h; g_used = 0; }   /* new game window */
    cur = (WNDPROC)GetWindowLongA(h, GWL_WNDPROC);
    for (i = 0; i < g_used; i++)
        if (cur == g_slots[i]) return;             /* we are on top */
    if (g_used >= NSLOTS) return;
    g_prev[g_used] = cur;
    SetWindowLongA(h, GWL_WNDPROC, (LONG)g_slots[g_used]);
    g_used++;
}

static DWORD WINAPI worker(LPVOID p)
{
    unsigned tick = 0;
    read_config();
    for (;;)
    {
        if (++tick % 20 == 0)   /* re-read armfix.txt every ~5 s only */
        {
            BYTE old = g_alpha;
            read_config();
            if (old != g_alpha) apply_alpha_all();
        }
        keep_game_active();
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
