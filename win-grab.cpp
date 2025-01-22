#include <Windows.h>
#include <thread>
#include <iostream>

DWORD threadId{};
bool win_key_pressed = false;
bool left_mouse_pressed = false;
bool middle_mouse_pressed = false;
HWND active_window;
POINT start_mouse_position = { 0 };
RECT start_window_position = { 0 };

void SetStartMouseState(MSLLHOOKSTRUCT *info)
{
    active_window = 0;

    start_mouse_position.x = info->pt.x;
    start_mouse_position.y = info->pt.y;
}

void SetStartWindowState()
{
    active_window = GetForegroundWindow();
    RECT window_rect{ 0 };
    GetWindowRect(active_window, &window_rect);

    start_window_position.left = window_rect.left;
    start_window_position.top = window_rect.top;
    start_window_position.right = window_rect.right;
    start_window_position.bottom = window_rect.bottom;
}

void MoveWindow(RECT new_window_position)
{
    MoveWindow
    (
        active_window,
        new_window_position.left,
        new_window_position.top,
        new_window_position.right - new_window_position.left,
        new_window_position.bottom - new_window_position.top,
        true
    );
}

POINT GetMouseDelta(POINT mouse_position)
{
    return POINT
    {
        .x = mouse_position.x - start_mouse_position.x,
        .y = mouse_position.y - start_mouse_position.y,
    };
}

LRESULT CALLBACK ProcessKeyboard(int code, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT* info = (KBDLLHOOKSTRUCT*)lParam;

    if (info->vkCode == VK_LWIN || info->vkCode == VK_RWIN)
    {
        if (wParam == WM_KEYDOWN)
            win_key_pressed = true;
        if (wParam == WM_KEYUP)
            win_key_pressed = false;
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}

LRESULT CALLBACK ProcessMouse(int code, WPARAM wParam, LPARAM lParam)
{
    MSLLHOOKSTRUCT* info = (MSLLHOOKSTRUCT*)lParam;

    if (!win_key_pressed)
    {
        left_mouse_pressed = false;
        middle_mouse_pressed = false;
        goto exit;
    }

    if (wParam == WM_LBUTTONDOWN && !middle_mouse_pressed)
    {
        SetStartMouseState(info);

        left_mouse_pressed = true;
    }
    else if (wParam == WM_LBUTTONUP)
    {
        left_mouse_pressed = false;
    }
    else if (wParam == WM_MBUTTONDOWN && !left_mouse_pressed)
    {
        SetStartMouseState(info);

        middle_mouse_pressed = true;
    }
    else if (wParam == WM_MBUTTONUP)
    {
        middle_mouse_pressed = false;
    }
    else if (wParam == WM_MOUSEMOVE && left_mouse_pressed && win_key_pressed)
    {
        if (active_window == 0)
        {
            SetStartWindowState();
            goto exit;
        }

        POINT moved = GetMouseDelta(info->pt);

        RECT new_window_position =
        {
            .left = start_window_position.left + moved.x,
            .top = start_window_position.top + moved.y,
            .right = start_window_position.right + moved.x,
            .bottom = start_window_position.bottom + moved.y,
        };

        MoveWindow(new_window_position);
    }
    else if (wParam == WM_MOUSEMOVE && middle_mouse_pressed && win_key_pressed)
    {
        if (active_window == 0)
        {
            SetStartWindowState();
            goto exit;
        }

        POINT moved = GetMouseDelta(info->pt);

        RECT new_window_position =
        {
            .left = start_window_position.left,
            .top = start_window_position.top,
            .right = start_window_position.right + moved.x,
            .bottom = start_window_position.bottom + moved.y,
        };

        MoveWindow(new_window_position);
    }

    exit:
    return CallNextHookEx(NULL, code, wParam, lParam);
}

bool CtrlHandler(int signal)
{
    if (signal == CTRL_C_EVENT)
        PostThreadMessage(threadId, WM_CLOSE, 0, 0);

    return true;
}

void ThreadFunction(HANDLE event_handle)
{
    threadId = GetCurrentThreadId();
    SetEvent(event_handle);

    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

    HHOOK keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, ProcessKeyboard, nullptr, 0);
    HHOOK mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, ProcessMouse, nullptr, 0);

    MSG msg = { 0 };
    BOOL result;

    while (true)
    {
        result = GetMessage(&msg, nullptr, 0, 0);

        if (result <= 0 || msg.message == WM_CLOSE)
            break;
    }

    UnhookWindowsHookEx(keyboard_hook);
    UnhookWindowsHookEx(mouse_hook);
}

int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    HANDLE handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    std::thread message_thread(ThreadFunction, handle);

    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);

    message_thread.join();
}
