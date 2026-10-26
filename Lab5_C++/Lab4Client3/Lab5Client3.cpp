#include <windows.h>
#include <stdio.h>

#define BTN_CONNECT 1
#define BTN_SEND 2

// Глобальні змінні інтерфейсу
HWND hwndLog, hwndBtnConnect, hwndBtnSend;
HWND hwndInputW, hwndInputH, hwndLabelW, hwndLabelH;

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
    wcex.lpszClassName = L"Client3Class";
    RegisterClassW(&wcex);

    HWND hWnd = CreateWindowW(L"Client3Class", L"Клієнт 3", WS_OVERLAPPEDWINDOW,
        1050, 100, 450, 350, nullptr, nullptr, hInstance, nullptr);

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
        // Створення кнопок та полів математичних вводу
        hwndBtnConnect = CreateWindowW(L"BUTTON", L"Відкрити скриньку", WS_CHILD | WS_VISIBLE, 10, 10, 140, 30, hWnd, (HMENU)BTN_CONNECT, NULL, NULL);

        hwndLabelW = CreateWindowW(L"STATIC", L"Ширина (A):", WS_CHILD | WS_VISIBLE, 10, 50, 80, 20, hWnd, NULL, NULL, NULL);
        hwndInputW = CreateWindowW(L"EDIT", L"10", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 100, 50, 60, 25, hWnd, NULL, NULL, NULL);

        hwndLabelH = CreateWindowW(L"STATIC", L"Висота (B):", WS_CHILD | WS_VISIBLE, 180, 50, 80, 20, hWnd, NULL, NULL, NULL);
        hwndInputH = CreateWindowW(L"EDIT", L"5", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 270, 50, 60, 25, hWnd, NULL, NULL, NULL);

        hwndBtnSend = CreateWindowW(L"BUTTON", L"Обчислити та відправити", WS_CHILD | WS_VISIBLE, 10, 85, 200, 30, hWnd, (HMENU)BTN_SEND, NULL, NULL);

        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 125, 410, 175, hWnd, NULL, NULL, NULL);

        // Встановлення шрифту
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabelW, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInputW, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabelH, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInputH, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (hwndLog) {
            MoveWindow(hwndLog, 10, 125, width - 20, height - 135, TRUE);
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 450;
        lpMMI->ptMinTrackSize.y = 350;
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

            // Зчитування чисел з текстових полів
            wchar_t wStr[20], hStr[20];
            GetWindowTextW(hwndInputW, wStr, 20);
            GetWindowTextW(hwndInputH, hStr, 20);

            // Математичне обчислення площі прямокутника
            int widthValue = _wtoi(wStr);
            int heightValue = _wtoi(hStr);
            int area = widthValue * heightValue;

            // Формування рядка з підписом Клієнта 3
            wchar_t szBuf[512];
            swprintf_s(szBuf, 512, L"Клієнт 3: Площа прямокутника (%d x %d) = %d",
                widthValue, heightValue, area);

            DWORD cbWritten;
            // Відправка байтів тексту у скриньку
            if (WriteFile(hMailslot, szBuf, wcslen(szBuf) * sizeof(wchar_t), &cbWritten, NULL)) {
                AppendLog(L"Обчислено та надіслано:\r\n");
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