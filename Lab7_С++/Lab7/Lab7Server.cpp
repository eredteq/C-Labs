#include <windows.h>
#include <stdio.h>
#include <string>

// Макроси ідентифікаторів
#define BTN_START 1
#define BTN_STOP 2
#define BTN_DISCONNECT 3
#define ID_INPUT 4
#define WM_PIPE_LOG (WM_APP + 2)

// Глобальні змінні для елементів інтерфейсу
HWND hwndLog, hwndInput, hwndBtnStart, hwndBtnStop, hwndBtnDisconnect, hwndLabel;
WNDPROC OriginalEditProc;

// Ім'я іменованого каналу
const wchar_t* PipeName = L"\\\\.\\pipe\\LOCAL\\lab7_pipe";

// Структура клієнтського слота каналу
struct PipeSlot {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE hThread = NULL;
    HANDLE hStopEvent = NULL;
    bool is_connected = false;
    bool is_active = false;
    int slotIndex = 0;
    HWND hWnd = NULL;
};

PipeSlot slots[3];
bool server_running = false;
bool awaiting_disconnect_id = false;

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK InputEditProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);
void PostLogAsync(HWND hWnd, const wchar_t* text);
void StartServer(HWND hWnd);
void StopServer();
void DisconnectClient(int id);
void ProcessCommand(const wchar_t* cmd, HWND hWnd);
DWORD WINAPI PipeWorkerThread(LPVOID lpParam);

// Створення вікна сервера
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSW wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"ServerClass";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"ServerClass", L"Сервер", WS_OVERLAPPEDWINDOW,
        100, 100, 550, 450, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Функція для додавання тексту у поле логів
void AppendLog(const wchar_t* text) {
    int len = GetWindowTextLengthW(hwndLog);
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

// Безпечне передавання логів із робочих потоків у вікно
void PostLogAsync(HWND hWnd, const wchar_t* text) {
    size_t len = wcslen(text) + 1;
    wchar_t* copy = new wchar_t[len];
    wcscpy_s(copy, len, text);
    PostMessageW(hWnd, WM_PIPE_LOG, 0, (LPARAM)copy);
}

// Робочий потік для обслуговування клієнтського слота іменованого каналу
DWORD WINAPI PipeWorkerThread(LPVOID lpParam) {
    PipeSlot* slot = (PipeSlot*)lpParam;

    while (slot->is_active && server_running) {
        // Створення екземпляра іменованого каналу
        slot->hPipe = CreateNamedPipeW(
            PipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            3,
            2048, 2048, 5000, NULL);

        if (slot->hPipe == INVALID_HANDLE_VALUE) {
            break;
        }

        OVERLAPPED ovConnect = { 0 };
        ovConnect.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

        BOOL fConnected = ConnectNamedPipe(slot->hPipe, &ovConnect);
        DWORD err = GetLastError();

        if (!fConnected) {
            if (err == ERROR_IO_PENDING) {
                HANDLE waitHandles[2] = { ovConnect.hEvent, slot->hStopEvent };
                DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (waitRes != WAIT_OBJECT_0 || !slot->is_active || !server_running) {
                    CancelIo(slot->hPipe);
                    CloseHandle(ovConnect.hEvent);
                    CloseHandle(slot->hPipe);
                    slot->hPipe = INVALID_HANDLE_VALUE;
                    continue;
                }
            }
            else if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(ovConnect.hEvent);
                CloseHandle(slot->hPipe);
                slot->hPipe = INVALID_HANDLE_VALUE;
                continue;
            }
        }
        CloseHandle(ovConnect.hEvent);

        if (!slot->is_active || !server_running) {
            CloseHandle(slot->hPipe);
            slot->hPipe = INVALID_HANDLE_VALUE;
            break;
        }

        slot->is_connected = true;
        wchar_t msgConn[128];
        swprintf_s(msgConn, 128, L"Клієнта підключено. Надано ID: %d\r\n", slot->slotIndex + 1);
        PostLogAsync(slot->hWnd, msgConn);

        // Цикл читання даних із каналу
        while (slot->is_connected && slot->is_active && server_running) {
            wchar_t buffer[1024] = { 0 };
            DWORD cbRead = 0;
            OVERLAPPED ovRead = { 0 };
            ovRead.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

            BOOL fSuccess = ReadFile(slot->hPipe, buffer, sizeof(buffer) - sizeof(wchar_t), &cbRead, &ovRead);
            if (!fSuccess) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    HANDLE waitHandles[2] = { ovRead.hEvent, slot->hStopEvent };
                    DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                    if (waitRes == WAIT_OBJECT_0) {
                        if (!GetOverlappedResult(slot->hPipe, &ovRead, &cbRead, FALSE) || cbRead == 0) {
                            CloseHandle(ovRead.hEvent);
                            break;
                        }
                    }
                    else {
                        CancelIo(slot->hPipe);
                        CloseHandle(ovRead.hEvent);
                        break;
                    }
                }
                else {
                    CloseHandle(ovRead.hEvent);
                    break;
                }
            }
            CloseHandle(ovRead.hEvent);

            if (cbRead > 0) {
                buffer[cbRead / sizeof(wchar_t)] = L'\0';
                wchar_t logMsg[1200];
                swprintf_s(logMsg, 1200, L"\r\n Дані від Клієнта %d:\r\n%s\r\n", slot->slotIndex + 1, buffer);
                PostLogAsync(slot->hWnd, logMsg);
            }
        }

        bool wasManualDisconnect = (WaitForSingleObject(slot->hStopEvent, 0) == WAIT_OBJECT_0);

        if (slot->hPipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(slot->hPipe);
            CloseHandle(slot->hPipe);
            slot->hPipe = INVALID_HANDLE_VALUE;
        }

        if (slot->is_connected && !wasManualDisconnect && server_running) {
            wchar_t msgDisc[128];
            swprintf_s(msgDisc, 128, L"Клієнт %d від'єднався. Слот каналу звільнено\r\n", slot->slotIndex + 1);
            PostLogAsync(slot->hWnd, msgDisc);
        }

        slot->is_connected = false;
        ResetEvent(slot->hStopEvent);
    }
    return 0;
}

// Функція запуску сервера
void StartServer(HWND hWnd) {
    if (server_running) {
        AppendLog(L"Помилка: Сервер вже працює.\r\n");
        return;
    }

    server_running = true;
    for (int i = 0; i < 3; i++) {
        slots[i].slotIndex = i;
        slots[i].hWnd = hWnd;
        slots[i].is_active = true;
        slots[i].is_connected = false;
        slots[i].hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        slots[i].hThread = CreateThread(NULL, 0, PipeWorkerThread, &slots[i], 0, NULL);
    }

    AppendLog(L"Сервер запущено. Очікування підключень до каналу...\r\n");
}

// Функція зупинки сервера
void StopServer() {
    if (!server_running) {
        AppendLog(L"Error: Сервер не запущено\r\n");
        return;
    }

    server_running = false;
    for (int i = 0; i < 3; i++) {
        slots[i].is_active = false;
        if (slots[i].hStopEvent) SetEvent(slots[i].hStopEvent);
    }

    for (int i = 0; i < 3; i++) {
        if (slots[i].hThread) {
            WaitForSingleObject(slots[i].hThread, 1500);
            CloseHandle(slots[i].hThread);
            slots[i].hThread = NULL;
        }
        if (slots[i].hStopEvent) {
            CloseHandle(slots[i].hStopEvent);
            slots[i].hStopEvent = NULL;
        }
        slots[i].is_connected = false;
    }

    AppendLog(L"Сервер зупинено. Всіх клієнтів відключено\r\n");
}

// Функція примусового відключення конкретного клієнта
void DisconnectClient(int id) {
    if (id < 1 || id > 3) {
        AppendLog(L"Error: Недійсний номер клієнта\r\n");
        return;
    }

    int index = id - 1;
    if (!slots[index].is_connected) {
        wchar_t msg[100];
        swprintf_s(msg, 100, L"Error: Слот %d порожній\r\n", id);
        AppendLog(msg);
        return;
    }

    SetEvent(slots[index].hStopEvent);

    wchar_t msg[100];
    swprintf_s(msg, 100, L"Клієнта %d відключено зі сторони сервера\r\n", id);
    AppendLog(msg);
}

// Функція обробки текстових команд
void ProcessCommand(const wchar_t* cmd, HWND hWnd) {
    AppendLog(L">> ");
    AppendLog(cmd);
    AppendLog(L"\r\n");

    if (awaiting_disconnect_id) {
        int id = _wtoi(cmd);
        if (id > 0) {
            DisconnectClient(id);
        }
        else {
            AppendLog(L"Error: Введено некоректне ID клієнта. Необхідно ввести дійсне ID\r\n");
        }
        awaiting_disconnect_id = false;
        return;
    }

    if (wcscmp(cmd, L"server startup") == 0 || wcscmp(cmd, L"server st") == 0) {
        StartServer(hWnd);
    }
    else if (wcscmp(cmd, L"server shutdown") == 0 || wcscmp(cmd, L"server sd") == 0) {
        StopServer();
    }
    else if (wcscmp(cmd, L"help") == 0) {
        AppendLog(L" Доступні команди:\r\n");
        AppendLog(L"server startup - запуск сервера. Скорочено: server st\r\n");
        AppendLog(L"server shutdown - вимкнення сервера. Скорочено: server sd\r\n");
        AppendLog(L"client remove [ID] - прибрати клієнта за його ID. Скорочено: client rm [ID]\r\n");
        AppendLog(L"help - відображення довідки\r\n");
    }
    else if (wcsncmp(cmd, L"client remove ", 14) == 0) {
        int id = 0;
        swscanf_s(cmd, L"client remove %d", &id);
        DisconnectClient(id);
    }
    else if (wcsncmp(cmd, L"client rm ", 10) == 0) {
        int id = 0;
        swscanf_s(cmd, L"client rm %d", &id);
        DisconnectClient(id);
    }
    else {
        AppendLog(L"Невідома команда. Використайте 'help' для відображення довідки\r\n");
    }
}

// Віконна процедура для поля вводу
LRESULT CALLBACK InputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageW(GetParent(hWnd), WM_APP + 1, 0, 0);
        return 0;
    }
    if (msg == WM_CHAR && wParam == VK_RETURN) {
        return 0;
    }
    return CallWindowProcW(OriginalEditProc, hWnd, msg, wParam, lParam);
}

// Обробка повідомлень вікна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        hwndBtnStart = CreateWindowW(L"BUTTON", L"Запустити", WS_CHILD | WS_VISIBLE, 10, 10, 100, 30, hWnd, (HMENU)BTN_START, NULL, NULL);
        hwndBtnStop = CreateWindowW(L"BUTTON", L"Зупинити", WS_CHILD | WS_VISIBLE, 120, 10, 100, 30, hWnd, (HMENU)BTN_STOP, NULL, NULL);
        hwndBtnDisconnect = CreateWindowW(L"BUTTON", L"Від'єднати клієнта", WS_CHILD | WS_VISIBLE, 230, 10, 150, 30, hWnd, (HMENU)BTN_DISCONNECT, NULL, NULL);

        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 50, 510, 300, hWnd, NULL, NULL, NULL);

        hwndLabel = CreateWindowW(L"STATIC", L">>", WS_CHILD | WS_VISIBLE, 10, 360, 20, 25, hWnd, NULL, NULL, NULL);
        hwndInput = CreateWindowW(L"EDIT", L"help", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            35, 360, 485, 25, hWnd, (HMENU)ID_INPUT, NULL, NULL);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnStart, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnDisconnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        OriginalEditProc = (WNDPROC)SetWindowLongPtrW(hwndInput, GWLP_WNDPROC, (LONG_PTR)InputEditProc);

        AppendLog(L"Ініціалізація системи. Введіть 'help' для отримання списку команд\r\n");
        break;
    }
    case WM_PIPE_LOG: {
        wchar_t* text = (wchar_t*)lParam;
        if (text) {
            AppendLog(text);
            delete[] text;
        }
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) {
            MoveWindow(hwndLog, 10, 50, width - 20, height - 100, TRUE);
            MoveWindow(hwndLabel, 10, height - 35, 20, 25, TRUE);
            MoveWindow(hwndInput, 35, height - 35, width - 45, 25, TRUE);
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 450;
        lpMMI->ptMinTrackSize.y = 350;
        break;
    }
    case WM_APP + 1: {
        wchar_t cmdBuf[256];
        GetWindowTextW(hwndInput, cmdBuf, 256);
        SetWindowTextW(hwndInput, L"");

        if (wcslen(cmdBuf) > 0) {
            ProcessCommand(cmdBuf, hWnd);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == BTN_START) {
            StartServer(hWnd);
        }
        else if (LOWORD(wParam) == BTN_STOP) {
            StopServer();
        }
        else if (LOWORD(wParam) == BTN_DISCONNECT) {
            AppendLog(L"Активні клієнти:\r\n");
            bool hasClients = false;
            for (int i = 0; i < 3; i++) {
                if (slots[i].is_connected) {
                    wchar_t msg[50];
                    swprintf_s(msg, 50, L"Клієнт ID: %d\r\n", i + 1);
                    AppendLog(msg);
                    hasClients = true;
                }
            }
            if (!hasClients) {
                AppendLog(L"Відсутні підключені клієнти\r\n");
            }
            else {
                AppendLog(L"Необхідно ввести номер клієнта для відключення у поле команд та підтвердити:\r\n");
                awaiting_disconnect_id = true;
                SetFocus(hwndInput);
            }
        }
        break;
    case WM_DESTROY:
        if (server_running) StopServer();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}