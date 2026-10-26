#include <windows.h>
#include <stdio.h>

#define BTN_CONNECT 1
#define BTN_SEND 2

// Глобальні змінні інтерфейсу
HWND hwndLog, hwndBtnConnect, hwndBtnSend;

// Глобальні змінні скриньки
HANDLE hMailslot = INVALID_HANDLE_VALUE;
const wchar_t* MailslotName = L"\\\\.\\mailslot\\lab_mailslot";

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
        600, 450, 400, 300, nullptr, nullptr, hInstance, nullptr);

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
        hwndBtnConnect = CreateWindowW(L"BUTTON", L"Відкрити скриньку", WS_CHILD | WS_VISIBLE, 10, 10, 140, 30, hWnd, (HMENU)BTN_CONNECT, NULL, NULL);
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
            // Перевірка чи скринька вже відкрита
            if (hMailslot == INVALID_HANDLE_VALUE) {
                // Відкриття файлу скриньки для запису
                hMailslot = CreateFileW(MailslotName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hMailslot != INVALID_HANDLE_VALUE) {
                    SetWindowTextW(hwndBtnConnect, L"Закрити скриньку");
                    AppendLog(L"Підключення до скриньки успішне\r\n");
                }
                else {
                    AppendLog(L"Error: Скринька сервера не знайдена.\r\n");
                }
            }
            else {
                // Закриття файлового дескриптора
                CloseHandle(hMailslot);
                hMailslot = INVALID_HANDLE_VALUE;
                SetWindowTextW(hwndBtnConnect, L"Відкрити скриньку");
                AppendLog(L"Зв'язок зі скринькою розірвано.\r\n");
            }
        }
        else if (LOWORD(wParam) == BTN_SEND) {
            if (hMailslot == INVALID_HANDLE_VALUE) {
                AppendLog(L"Error: Скринька не відкрита.\r\n");
                return 0;
            }

            // Збір системних метрик (Варіант 1)
            int iconX = GetSystemMetrics(SM_CXICON);
            int iconY = GetSystemMetrics(SM_CYICON);
            int scrollX = GetSystemMetrics(SM_CXVSCROLL);

            HDC hdc = GetDC(hWnd);
            int colorDepth = GetDeviceCaps(hdc, BITSPIXEL);
            ReleaseDC(hWnd, hdc);

            // Формування рядка з підписом Клієнта 2
            wchar_t szBuf[512];
            swprintf_s(szBuf, 512, L"Клієнт 2: Розмір іконки (SM_CXICON та SM_CYICON) = %dx%d, Ширина смуги прокрутки (SM_CXVSCROLL) = %d, Глибина кольору (hds, BITSPIXEL) = %d",
                iconX, iconY, scrollX, colorDepth);

            DWORD cbWritten;
            // Відправка байтів тексту у скриньку
            if (WriteFile(hMailslot, szBuf, wcslen(szBuf) * sizeof(wchar_t), &cbWritten, NULL)) {
                AppendLog(L"Лист надіслано:\r\n");
                AppendLog(szBuf);
                AppendLog(L"\r\n\r\n");
            }
            else {
                AppendLog(L"Помилка надсилання листа.\r\n");
            }
        }
        break;

    case WM_DESTROY:
        if (hMailslot != INVALID_HANDLE_VALUE) CloseHandle(hMailslot);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}