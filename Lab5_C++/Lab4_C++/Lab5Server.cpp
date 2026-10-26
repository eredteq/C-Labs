#include <windows.h>
#include <stdio.h>
#include <string>

// Макроси ідентифікаторів
#define BTN_START 1
#define BTN_STOP 2
#define BTN_CLOSE_MAILSLOT 3
#define ID_INPUT 4
#define IDT_TIMER 1001

// Глобальні змінні інтерфейсу
HWND hwndLog, hwndInput, hwndBtnStart, hwndBtnStop, hwndBtnClose, hwndLabel;

// Глобальні змінні для поштової скриньки
HANDLE hMailslot = INVALID_HANDLE_VALUE;
const wchar_t* MailslotName = L"\\\\.\\mailslot\\lab_mailslot";

// Прапорець для відстеження стану читання (таймера)
bool is_reading = false;

// Змінна для збереження оригінальної процедури поля вводу
WNDPROC OriginalEditProc;

// Оголошення функцій
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK InputEditProc(HWND, UINT, WPARAM, LPARAM);
void AppendLog(const wchar_t* text);
void StartServer(HWND hWnd);
void PauseReading(HWND hWnd);
void ResumeReading(HWND hWnd);
void CloseMailslot(HWND hWnd);
void ProcessCommand(const wchar_t* cmd, HWND hWnd);
void ReadMailslot();

// Головна функція
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

// Додавання тексту у поле логів
void AppendLog(const wchar_t* text) {
    int len = GetWindowTextLengthW(hwndLog);
    SendMessageW(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndLog, EM_REPLACESEL, 0, (LPARAM)text);
}

// Створення скриньки та запуск таймера
void StartServer(HWND hWnd) {
    // Перевірка наявності відкритої скриньки
    if (hMailslot != INVALID_HANDLE_VALUE) {
        AppendLog(L"Error: Поштову скриньку вже створено.\r\n");
        return;
    }

    // Створення поштової скриньки
    hMailslot = CreateMailslotW(MailslotName, 0, MAILSLOT_WAIT_FOREVER, NULL);

    if (hMailslot == INVALID_HANDLE_VALUE) {
        AppendLog(L"Error: Не вдалося створити поштову скриньку.\r\n");
        return;
    }

    // Запуск таймера з інтервалом 1000 мс
    SetTimer(hWnd, IDT_TIMER, 1000, NULL);
    is_reading = true;

    // Встановлення відповідного тексту на кнопці
    SetWindowTextW(hwndBtnStop, L"Зупинити читання");

    AppendLog(L"Поштову скриньку відкрито. Запущено перевірку повідомлень з інтервалом в 1 с.\r\n");
}

// Призупинення читання
void PauseReading(HWND hWnd) {
    // Перевірка стану скриньки та таймера
    if (hMailslot == INVALID_HANDLE_VALUE) {
        AppendLog(L"Error: Поштову скриньку ще не відкрито.\r\n");
        return;
    }
    if (!is_reading) {
        AppendLog(L"Error: Читання вже призупинено.\r\n");
        return;
    }

    // Зупинка таймера
    KillTimer(hWnd, IDT_TIMER);
    is_reading = false;

    // Зміна тексту кнопки
    SetWindowTextW(hwndBtnStop, L"Почати читання");
    AppendLog(L"Сервер призупинив читання повідомлень.\r\n");
}

// Відновлення читання (запуск таймера)
void ResumeReading(HWND hWnd) {
    // Перевірка стану скриньки та таймера
    if (hMailslot == INVALID_HANDLE_VALUE) {
        AppendLog(L"Error: Поштову скриньку ще не відкрито.\r\n");
        return;
    }
    if (is_reading) {
        AppendLog(L"Error: Читання вже активно.\r\n");
        return;
    }

    // Запуск таймера
    SetTimer(hWnd, IDT_TIMER, 1000, NULL);
    is_reading = true;

    // Зміна тексту кнопки
    SetWindowTextW(hwndBtnStop, L"Зупинити читання");
    AppendLog(L"Сервер відновив читання повідомлень.\r\n");
}

// Закриття скриньки
void CloseMailslot(HWND hWnd) {
    // Перевірка стану скриньки
    if (hMailslot == INVALID_HANDLE_VALUE) {
        AppendLog(L"Error: Поштова скринька вже закрита або не була створена.\r\n");
        return;
    }

    // Зупинка таймера та закриття файлового дескриптора
    KillTimer(hWnd, IDT_TIMER);
    CloseHandle(hMailslot);

    // Скидання глобальних станів
    hMailslot = INVALID_HANDLE_VALUE;
    is_reading = false;
    SetWindowTextW(hwndBtnStop, L"Зупинити читання");

    AppendLog(L"Поштову скриньку закрито.\r\n");
}

// Читання повідомлень зі скриньки
void ReadMailslot() {
    // Перевірка стану скриньки
    if (hMailslot == INVALID_HANDLE_VALUE) return;

    DWORD cbMessage, cMessage, cbRead;

    // Отримання інформації про стан скриньки
    BOOL fResult = GetMailslotInfo(hMailslot, NULL, &cbMessage, &cMessage, NULL);
    if (!fResult || cMessage == 0) return;

    // Цикл читання всіх повідомлень у черзі
    while (cMessage != 0 && cbMessage != MAILSLOT_NO_MESSAGE) {
        wchar_t buffer[2048] = { 0 };

        // Визначення кількості байтів для читання
        DWORD bytesToRead = (cbMessage < sizeof(buffer)) ? cbMessage : (sizeof(buffer) - sizeof(wchar_t));

        // Читання повідомлення
        if (ReadFile(hMailslot, buffer, bytesToRead, &cbRead, NULL)) {
            buffer[cbRead / sizeof(wchar_t)] = L'\0';

            // Формування та виведення тексту
            wchar_t logText[2048];
            swprintf_s(logText, 2048, L"\r\n-----------------------\n\r\nНовий клієнтський лист:\r\n%s\r\n", buffer);
            AppendLog(logText);
        }

        // Перевірка наступного повідомлення
        GetMailslotInfo(hMailslot, NULL, &cbMessage, &cMessage, NULL);
    }
}

// Парсер команд
void ProcessCommand(const wchar_t* cmd, HWND hWnd) {
    AppendLog(L">> ");
    AppendLog(cmd);
    AppendLog(L"\r\n");

    // Обробка команд запуску скриньки
    if (wcscmp(cmd, L"server startup") == 0 || wcscmp(cmd, L"server st") == 0) {
        StartServer(hWnd);
    }
    // Обробка команд призупинення читання
    else if (wcscmp(cmd, L"read stop") == 0 || wcscmp(cmd, L"r stop") == 0) {
        PauseReading(hWnd);
    }
    // Обробка команд відновлення читання
    else if (wcscmp(cmd, L"read start") == 0 || wcscmp(cmd, L"r start") == 0) {
        ResumeReading(hWnd);
    }
    // Обробка команд закриття скриньки
    else if (wcscmp(cmd, L"mailslot close") == 0 || wcscmp(cmd, L"mailslot cl") == 0) {
        CloseMailslot(hWnd);
    }
    // Обробка довідки
    else if (wcscmp(cmd, L"help") == 0) {
        AppendLog(L"Доступні команди:\r\n");
        AppendLog(L"server startup - відкрити скриньку та запустити таймер. Скорочено: server st\r\n");
        AppendLog(L"read stop - призупинити читання скриньки. Скорочено: r stop\r\n");
        AppendLog(L"read start - відновити читання скриньки. Скорочено: r start\r\n");
        AppendLog(L"mailslot close - закрити скриньку. Скорочено: mailslot cl\r\n");
        AppendLog(L"help - показати цю довідку\r\n");
    }
    // Обробка невідомої команди
    else {
        AppendLog(L"Невідома команда. Введіть 'help' для довідки.\r\n");
    }
}

// Підміна віконної процедури для поля вводу
LRESULT CALLBACK InputEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Обробка натискання клавіш
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageW(GetParent(hWnd), WM_APP + 1, 0, 0);
        return 0;
    }
    if (msg == WM_CHAR && wParam == VK_RETURN) {
        return 0;
    }
    return CallWindowProc(OriginalEditProc, hWnd, msg, wParam, lParam);
}

// Головна віконна процедура
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Створення кнопок
        hwndBtnStart = CreateWindowW(L"BUTTON", L"Створити скриньку", WS_CHILD | WS_VISIBLE, 10, 10, 130, 30, hWnd, (HMENU)BTN_START, NULL, NULL);
        hwndBtnStop = CreateWindowW(L"BUTTON", L"Зупинити читання", WS_CHILD | WS_VISIBLE, 150, 10, 130, 30, hWnd, (HMENU)BTN_STOP, NULL, NULL);
        hwndBtnClose = CreateWindowW(L"BUTTON", L"Закрити скриньку", WS_CHILD | WS_VISIBLE, 290, 10, 130, 30, hWnd, (HMENU)BTN_CLOSE_MAILSLOT, NULL, NULL);

        // Створення полів логування та вводу
        hwndLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER | ES_READONLY,
            10, 50, 510, 300, hWnd, NULL, NULL, NULL);
        hwndLabel = CreateWindowW(L"STATIC", L">>", WS_CHILD | WS_VISIBLE, 10, 360, 20, 25, hWnd, NULL, NULL, NULL);
        hwndInput = CreateWindowW(L"EDIT", L"help", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            35, 360, 485, 25, hWnd, (HMENU)ID_INPUT, NULL, NULL);

        // Налаштування шрифту
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hwndBtnStart, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndBtnClose, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLog, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hwndInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Підміна віконної процедури
        OriginalEditProc = (WNDPROC)SetWindowLongPtr(hwndInput, GWLP_WNDPROC, (LONG_PTR)InputEditProc);

        AppendLog(L"Ініціалізація системи. Введіть 'help' для отримання списку команд\r\n");
        break;
    }

    case WM_TIMER: {
        // Виклик функції читання по спрацьовуванню таймера
        if (wParam == IDT_TIMER) {
            ReadMailslot();
        }
        break;
    }

    case WM_SIZE: {
        // Адаптивне масштабування елементів інтерфейсу
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
        // Обмеження мінімального розміру вікна
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 450;
        lpMMI->ptMinTrackSize.y = 350;
        break;
    }

    case WM_APP + 1: {
        // Отримання та обробка тексту з терміналу
        wchar_t cmdBuf[256];
        GetWindowTextW(hwndInput, cmdBuf, 256);
        SetWindowTextW(hwndInput, L"");

        if (wcslen(cmdBuf) > 0) {
            ProcessCommand(cmdBuf, hWnd);
        }
        break;
    }

    case WM_COMMAND:
        // Обробка натискання кнопок
        if (LOWORD(wParam) == BTN_START) {
            StartServer(hWnd);
        }
        else if (LOWORD(wParam) == BTN_STOP) {
            // Кнопка працює як перемикач
            if (is_reading) {
                PauseReading(hWnd);
            }
            else {
                ResumeReading(hWnd);
            }
        }
        else if (LOWORD(wParam) == BTN_CLOSE_MAILSLOT) {
            CloseMailslot(hWnd);
        }
        break;

    case WM_DESTROY:
        // Звільнення ресурсів перед виходом
        if (hMailslot != INVALID_HANDLE_VALUE) {
            CloseHandle(hMailslot);
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}