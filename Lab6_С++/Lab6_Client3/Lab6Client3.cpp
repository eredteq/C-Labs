#include <windows.h>
#include <stdio.h>

#define BTN_CONNECT 1
#define BTN_SEND 2

// Глобальні змінні інтерфейсу
HWND hwndLog, hwndInput, hwndBtnConnect, hwndBtnSend, hwndLabel;

// Глобальні змінні для FileMapping та синхронізації
HANDLE hMapFile = NULL;
LPVOID pBuf = NULL;
HANDLE hEventDataReady = NULL;
HANDLE hMutex = NULL;

const wchar_t* MapName = L"Local\\Lab6FileMapping";
const wchar_t* EventName = L"Local\\Lab6EventDataReady";
const wchar_t* MutexName = L"Local\\Lab6Mutex";
const DWORD MAP_SIZE = 2048;

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSW wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"Client3Class";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"Client3Class", L"Клієнт 3", WS_OVERLAPPEDWINDOW,
        1070, 100, 400, 350, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Додавання тексту у лог
void AppendLog(const wchar_t* text) {
    int len = GetWindowTextLengthW(hwndLog);
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Створення елементів інтерфейсу
        hwndBtnConnect = CreateWindowW(L"BUTTON", L"Відкрити проекцію", WS_CHILD | WS_VISIBLE, 10, 10, 140, 30, hWnd, (HMENU)BTN_CONNECT, NULL, NULL);
        hwndLabel = CreateWindowW(L"STATIC", L"Введення тексту:", WS_CHILD | WS_VISIBLE, 10, 50, 120, 20, hWnd, NULL, NULL, NULL);
        hwndInput = CreateWindowW(L"EDIT", L"Hello, world!", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 70, 240, 25, hWnd, NULL, NULL, NULL);
        hwndBtnSend = CreateWindowW(L"BUTTON", L"Відправити", WS_CHILD | WS_VISIBLE, 260, 70, 100, 25, hWnd, (HMENU)BTN_SEND, NULL, NULL);
        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 110, 350, 180, hWnd, NULL, NULL, NULL);

        // Встановлення шрифту
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInput, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) {
            MoveWindow(hwndInput, 10, 70, width - 130, 25, TRUE);
            MoveWindow(hwndBtnSend, width - 110, 70, 100, 25, TRUE);
            MoveWindow(hwndLog, 10, 110, width - 20, height - 120, TRUE);
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 350;
        lpMMI->ptMinTrackSize.y = 300;
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == BTN_CONNECT) {
            if (hMapFile == NULL) {
                // Відкриття об'єкта відображення файлу
                hMapFile = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, MapName);
                if (hMapFile != NULL) {
                    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MAP_SIZE);
                    hMutex = OpenMutexW(SYNCHRONIZE, FALSE, MutexName);
                    hEventDataReady = OpenEventW(EVENT_MODIFY_STATE, FALSE, EventName);

                    if (pBuf != NULL) {
                        SetWindowTextW(hwndBtnConnect, L"Закрити проекцію");
                        AppendLog(L"Підключення до проекції файлу успішне\r\n");
                    }
                    else {
                        CloseHandle(hMapFile);
                        hMapFile = NULL;
                        AppendLog(L"Error: Подання проектованого файлу неможливе.\r\n");
                    }
                }
                else {
                    AppendLog(L"Error: Об'єкт FileMapping сервера не знайдено.\r\n");
                }
            }
            else {
                // Закриття проекції та дескрипторів
                if (pBuf) { UnmapViewOfFile(pBuf); pBuf = NULL; }
                if (hMapFile) { CloseHandle(hMapFile); hMapFile = NULL; }
                if (hMutex) { CloseHandle(hMutex); hMutex = NULL; }
                if (hEventDataReady) { CloseHandle(hEventDataReady); hEventDataReady = NULL; }

                SetWindowTextW(hwndBtnConnect, L"Відкрити проекцію");
                AppendLog(L"Зв'язок із проекцією файлу розірвано.\r\n");
            }
        }
        else if (LOWORD(wParam) == BTN_SEND) {
            if (hMapFile == NULL || pBuf == NULL) {
                AppendLog(L"Error: Проекція файлу не відкрита.\r\n");
                return 0;
            }

            // Зчитування тексту з поля вводу
            wchar_t userText[256];
            GetWindowTextW(hwndInput, userText, 256);

            wchar_t szBuf[512];
            swprintf_s(szBuf, 512, L"Клієнт 3: %s", userText);

            // Запис у проекцію файлу з синхронізацією
            if (hMutex) WaitForSingleObject(hMutex, INFINITE);
            CopyMemory((PVOID)pBuf, szBuf, (wcslen(szBuf) + 1) * sizeof(wchar_t));
            if (hMutex) ReleaseMutex(hMutex);

            // Сигнал серверу про оновлення даних
            if (hEventDataReady) SetEvent(hEventDataReady);

            AppendLog(L"Ви: ");
            AppendLog(userText);
            AppendLog(L"\r\n");

            SetWindowTextW(hwndInput, L"");
        }
        break;
    case WM_DESTROY:
        if (pBuf) UnmapViewOfFile(pBuf);
        if (hMapFile) CloseHandle(hMapFile);
        if (hMutex) CloseHandle(hMutex);
        if (hEventDataReady) CloseHandle(hEventDataReady);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}