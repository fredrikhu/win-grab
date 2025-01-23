#include <Windows.h>
#include <thread>

typedef struct KEYBOARDEVENT {
    KBDLLHOOKSTRUCT keyboard_struct;
    int keyboard_code;
    bool has_stored_keyboard_event;
    bool ignore_next_input;
    bool used_shortcut;
} KEYBOARDEVENT;

DWORD threadId = { 0 };
bool win_key_pressed = false;
bool left_mouse_pressed = false;
bool middle_mouse_pressed = false;
HWND active_window;
POINT start_mouse_position = { 0 };
RECT start_window_position = { 0 };
KEYBOARDEVENT stored_keyboard_event = { 0 };

void SetStartMouseState(MSLLHOOKSTRUCT *)
{
    active_window = 0;
    GetPhysicalCursorPos(&start_mouse_position);
}

void SetStartWindowState()
{
    active_window = WindowFromPhysicalPoint(start_mouse_position);
    active_window = GetAncestor(active_window, GA_ROOT);

    GetWindowRect(active_window, &start_window_position);
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

POINT GetMouseDelta(POINT)
{
    POINT mouse_position = { 0 };
    GetPhysicalCursorPos(&mouse_position);

    return POINT
    {
        .x = mouse_position.x - start_mouse_position.x,
        .y = mouse_position.y - start_mouse_position.y,
    };
}

void SimulateKeyboardEvent()
{
    INPUT input =
    {
        .type = INPUT_KEYBOARD,
        .ki =
        {
            .wVk = (WORD)stored_keyboard_event.keyboard_struct.vkCode,
            .wScan = 0,
            .dwFlags = 0,
            .time = 0,
            .dwExtraInfo = 0,
        }
    };

    stored_keyboard_event.has_stored_keyboard_event = false;
    stored_keyboard_event.ignore_next_input = true;

    SendInput(1, &input, sizeof(INPUT));
}

LRESULT CALLBACK ProcessKeyboard(int code, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT* info = (KBDLLHOOKSTRUCT*)lParam;

    if (info->vkCode == VK_LWIN || info->vkCode == VK_RWIN)
    {
        if (wParam == WM_KEYDOWN && !stored_keyboard_event.ignore_next_input)
        {
            if (win_key_pressed)
                return -1;

            win_key_pressed = true;
            stored_keyboard_event =
            {
                .keyboard_struct = *info,
                .keyboard_code = code,
                .has_stored_keyboard_event = true,
            };

            return -1;
        }
        else if (wParam == WM_KEYDOWN)
        {
            stored_keyboard_event.ignore_next_input = false;
        }
        else if (wParam == WM_KEYUP)
        {
            win_key_pressed = false;
            if (stored_keyboard_event.has_stored_keyboard_event)
            {
                SimulateKeyboardEvent();
            }
            else if (stored_keyboard_event.used_shortcut)
            {
                stored_keyboard_event.used_shortcut = false;
                return CallNextHookEx(NULL, code, wParam, lParam);
            }
            else
            {
                return -1;
            }
        }
    }
    else if (stored_keyboard_event.has_stored_keyboard_event)
    {
        SimulateKeyboardEvent();
        stored_keyboard_event.used_shortcut = true;
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

        CallNextHookEx(NULL, code, wParam, lParam);
        return -1;
    }
    else if (wParam == WM_LBUTTONUP)
    {
        left_mouse_pressed = false;
    }
    else if (wParam == WM_MBUTTONDOWN && !left_mouse_pressed)
    {
        SetStartMouseState(info);
        middle_mouse_pressed = true;

        CallNextHookEx(NULL, code, wParam, lParam);
        return -1;
    }
    else if (wParam == WM_MBUTTONUP && middle_mouse_pressed)
    {
        middle_mouse_pressed = false;

        CallNextHookEx(NULL, code, wParam, lParam);
        return -1;
    }
    else if (wParam == WM_MOUSEMOVE && left_mouse_pressed && win_key_pressed)
    {
        stored_keyboard_event.has_stored_keyboard_event = false;

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
        stored_keyboard_event.has_stored_keyboard_event = false;

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

void ThreadFunction(HANDLE event_handle)
{
    threadId = GetCurrentThreadId();
    SetEvent(event_handle);

    SetWindowsHookEx(WH_KEYBOARD_LL, ProcessKeyboard, nullptr, 0);
    SetWindowsHookEx(WH_MOUSE_LL, ProcessMouse, nullptr, 0);

    MSG msg = { 0 };

    while (true)
    {
        BOOL result = GetMessage(&msg, nullptr, 0, 0);

        if (result <= 0 || msg.message == WM_CLOSE)
            break;
    }
}

int main()
{
    HANDLE handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    std::thread message_thread(ThreadFunction, handle);

    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);

    message_thread.join();
}

int __stdcall WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    return main();
}
