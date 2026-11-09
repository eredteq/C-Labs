#include <windows.h>
#include <stdio.h>

#define BTN_CONNECT 1
#define BTN_SEND 2

// Оголошення глобальних змінних інтерфейсу
HWND hwndLog, hwndBtnConnect, hwndBtnSend;

// Глобальні змінні каналу
HANDLE hPipe = INVALID_HANDLE_VALUE;
const wchar_t* PipeName = L"\\\\.\\pipe\\LOCAL\\lab7_pipe";

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSW wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"Client2Class";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"Client2Class", L"Клієнт 2", WS_OVERLAPPEDWINDOW,
        660, 420, 400, 300, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Функція додавання тексту у поле логів
void AppendLog(const wchar_t* text) {
    int len = GetWindowTextLengthW(hwndLog);
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Створення елементів інтерфейсу
        hwndBtnConnect = CreateWindowW(L"BUTTON", L"Підключитися", WS_CHILD | WS_VISIBLE, 10, 10, 120, 30, hWnd, (HMENU)BTN_CONNECT, NULL, NULL);
        hwndBtnSend = CreateWindowW(L"BUTTON", L"Відправити дані", WS_CHILD | WS_VISIBLE, 140, 10, 150, 30, hWnd, (HMENU)BTN_SEND, NULL, NULL);
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
        // Обробка кнопки підключення/відключення
        if (LOWORD(wParam) == BTN_CONNECT) {
            if (hPipe == INVALID_HANDLE_VALUE) {
                hPipe = CreateFileW(PipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hPipe != INVALID_HANDLE_VALUE) {
                    SetWindowTextW(hwndBtnConnect, L"Від'єднатися");
                    AppendLog(L"З'єднання з сервером було встановлено\r\n");
                }
                else {
                    AppendLog(L"Помилка підключення. Сервер є недоступним\r\n");
                }
            }
            else {
                CloseHandle(hPipe);
                hPipe = INVALID_HANDLE_VALUE;
                SetWindowTextW(hwndBtnConnect, L"Підключитися");
                AppendLog(L"Клієнт ініціалізував відключення від сервера\r\n");
            }
        }
        // Обробка кнопки відправки даних
        else if (LOWORD(wParam) == BTN_SEND) {
            if (hPipe == INVALID_HANDLE_VALUE) {
                AppendLog(L"Error: відсутнє з'єднання з сервером.\r\n");
                return 0;
            }

            // Збір системних метрик (Варіант 2)
            int taskbarWidth = GetSystemMetrics(SM_CXFULLSCREEN);
            int monitorsCount = GetSystemMetrics(SM_CMONITORS);
            HDC hdc = GetDC(hWnd);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hWnd, hdc);

            // Формування рядка
            wchar_t szBuf[256];
            swprintf_s(szBuf, 256, L"Ширина панелі завдань (SM_CXFULLSCREEN): %d\r\nКількість моніторів (SM_CMONITORS): %d\r\nDPI Горизонтально (LOGPIXELSX): %d",
                taskbarWidth, monitorsCount, dpiX);

            // Відправка даних через канал
            DWORD cbWritten = 0;
            if (WriteFile(hPipe, szBuf, (DWORD)(wcslen(szBuf) * sizeof(wchar_t)), &cbWritten, NULL)) {
                FlushFileBuffers(hPipe);
                AppendLog(L"Дані надіслано:\r\n");
                AppendLog(szBuf);
                AppendLog(L"\r\n\r\n");
            }
            else {
                AppendLog(L"З'єднання розірвано зі сторони серверу\r\n");
                CloseHandle(hPipe);
                hPipe = INVALID_HANDLE_VALUE;
                SetWindowTextW(hwndBtnConnect, L"Підключитися");
            }
        }
        break;
    case WM_DESTROY:
        if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}