#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#define ID_TRAY_ICON 1001
#define ID_TRAY_MENU_EXIT 1002
#define ID_TRAY_MENU_FOCUS 1003
#define ID_TRAY_MENU_UNFOCUS 1004

HINSTANCE hInst;
NOTIFYICONDATA nid;
HWND hCurrentFocusedWindow = NULL;
HWINEVENTHOOK hEventHook;
int focusTransparency = 255, unfocusTransparency = 128;
HWND hwndFocusSlider = NULL, hwndUnfocusSlider = NULL;

// Function to set window transparency
void SetWindowTransparency(HWND hwnd, int transparency) {
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)transparency, LWA_ALPHA);
}

// Callback function to monitor window focus changes
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
    if (event == EVENT_SYSTEM_FOREGROUND) {
        if (hCurrentFocusedWindow && hCurrentFocusedWindow != hwnd) {
            // Apply unfocused transparency to the previous focused window
            SetWindowTransparency(hCurrentFocusedWindow, unfocusTransparency);
        }
        // Apply focused transparency to the new focused window
        hCurrentFocusedWindow = hwnd;
        SetWindowTransparency(hwnd, focusTransparency);
    }
}

// Function to create a slider window
HWND CreateSliderWindow(HINSTANCE hInstance, int& transparencyValue, const char* title) {
    HWND hwndSliderWindow = CreateWindowEx(
        0, WC_DIALOG, title, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 250, 100, NULL, NULL, hInstance, NULL
    );

    HWND hwndSlider = CreateWindowEx(
        0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
        10, 10, 200, 30, hwndSliderWindow, NULL, hInstance, NULL
    );

    SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessage(hwndSlider, TBM_SETPOS, TRUE, transparencyValue);
    SetWindowLongPtr(hwndSliderWindow, GWLP_USERDATA, (LONG_PTR)&transparencyValue);

    return hwndSliderWindow;
}

// Slider window procedure
LRESULT CALLBACK SliderProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int* transparencyValue = (int*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    HWND hwndSlider = FindWindowEx(hwnd, NULL, TRACKBAR_CLASS, NULL);

    switch (msg) {
    case WM_HSCROLL:
        if ((HWND)lParam == hwndSlider) {
            *transparencyValue = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
            if (hCurrentFocusedWindow) {
                SetWindowTransparency(hCurrentFocusedWindow, *transparencyValue);
            }
        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Function to display the slider window
void ShowSlider(HWND& hwndSlider, int& transparencyValue, const char* title, HINSTANCE hInstance) {
    if (!hwndSlider) {
        hwndSlider = CreateSliderWindow(hInstance, transparencyValue, title);
        SetWindowLongPtr(hwndSlider, GWLP_WNDPROC, (LONG_PTR)SliderProc);
    }
    ShowWindow(hwndSlider, SW_SHOW);
}

// Message handler for the main window
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Initialize common controls
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES };
        InitCommonControlsEx(&icex);

        // Create a system tray icon
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = ID_TRAY_ICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_APP;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        strcpy_s(nid.szTip, "Transparency Adjuster");
        Shell_NotifyIcon(NIM_ADD, &nid);

        // Set up event hook for focus change
        hEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
        break;
    }
    case WM_APP: {
        if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_MENU_FOCUS, "Adjust Focus Transparency");
            AppendMenu(hMenu, MF_STRING, ID_TRAY_MENU_UNFOCUS, "Adjust Unfocus Transparency");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_MENU_EXIT, "Exit");

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_TRAY_MENU_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        case ID_TRAY_MENU_FOCUS:
            ShowSlider(hwndFocusSlider, focusTransparency, "Focus Transparency", hInst);
            break;
        case ID_TRAY_MENU_UNFOCUS:
            ShowSlider(hwndUnfocusSlider, unfocusTransparency, "Unfocus Transparency", hInst);
            break;
        }
        break;
    }
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        UnhookWinEvent(hEventHook);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// WinMain - Entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TransparencyAdjusterClass";

    RegisterClass(&wc);

    HWND hwnd = CreateWindow("TransparencyAdjusterClass", "Transparency Adjuster", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
