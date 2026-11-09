#include <windows.h>
#include <stdio.h>

#define BTN_CONNECT 1
#define BTN_SEND 2

// Глобальні змінні інтерфейсу
HWND hwndLog, hwndBtnConnect, hwndBtnSend;

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
    wcex.lpszClassName = L"Client1Class";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"Client1Class", L"Клієнт 1", WS_OVERLAPPEDWINDOW,
        660, 100, 400, 300, nullptr, nullptr, hInstance, nullptr);

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
        hwndBtnSend = CreateWindowW(L"BUTTON", L"Відправити дані", WS_CHILD | WS_VISIBLE, 160, 10, 150, 30, hWnd, (HMENU)BTN_SEND, NULL, NULL);
        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 50, 350, 180, hWnd, NULL, NULL, NULL);

        // Встановлення шрифту
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) MoveWindow(hwndLog, 10, 50, width - 20, height - 60, TRUE);
        break;
    }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 350;
        lpMMI->ptMinTrackSize.y = 250;
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

            // Збір системних метрик
            int cleanBoot = GetSystemMetrics(SM_CLEANBOOT);
            int antivirusState = GetSystemMetrics(SM_CXMIN);

            HDC hdc = GetDC(hWnd);
            int horzRes = GetDeviceCaps(hdc, HORZRES);
            ReleaseDC(hWnd, hdc);

            // Формування рядка з підписом Клієнта 1
            wchar_t szBuf[512];
            swprintf_s(szBuf, 512, L"Клієнт 1:\r\nЧас системи (SM_CLEANBOOT) = %d\r\nСтан антивірусу (SM_CXMIN) = %d\r\nШирина екрану GetDeviceCaps(hdc, HORZRES) = %d",
                cleanBoot, antivirusState, horzRes);

            // Запис у проекцію файлу з синхронізацією
            if (hMutex) WaitForSingleObject(hMutex, INFINITE);
            CopyMemory((PVOID)pBuf, szBuf, (wcslen(szBuf) + 1) * sizeof(wchar_t));
            if (hMutex) ReleaseMutex(hMutex);

            // Сигнал серверу про оновлення даних
            if (hEventDataReady) SetEvent(hEventDataReady);

            AppendLog(L"Дані записано у проекцію:\r\n");
            AppendLog(szBuf);
            AppendLog(L"\r\n\r\n");
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