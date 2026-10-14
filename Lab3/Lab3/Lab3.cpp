#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>

// --- Глобальні ресурси для Варіанта 8 ---
std::vector<int> sharedBuffer;
const int N = 5; // Кількість чисел для середнього значення
double currentAverage = 0.0;
int currentNumber = 0;

// --- Об'єкти синхронізації ---
HANDLE hMutex;              // Для захисту спільного буфера
HANDLE hConsoleMutex;       // Для захисту виводу в консоль (щоб текст не перемішувався)
HANDLE hEventDataReady;     // Автоматична подія: сигнал, що дані згенеровано
HANDLE hEventCalcReady;     // Автоматична подія: сигнал, що середнє обчислено
HANDLE hShutdownEvent;      // Ручна подія: сигнал усім потокам на коректне завершення

// Список дескрипторів потоків для коректного завершення
std::vector<HANDLE> hAllThreads;
HANDLE hThreadTrackerMutex;

// --- Журналювання ---
// Функція виводить ID потоку під час старту, доступу до ресурсу та завершення
void Log(const std::string& action) {
    DWORD threadId = GetCurrentThreadId();
    WaitForSingleObject(hConsoleMutex, INFINITE);
    std::cout << "[Потік " << threadId << "] " << action << "\n";
    ReleaseMutex(hConsoleMutex);
}

// Потік-генератор 
DWORD WINAPI GeneratorThread(LPVOID lpParam) {
    Log("Старт роботи (Генератор)");

    // Поки не отримано сигнал на завершення
    while (WaitForSingleObject(hShutdownEvent, 0) == WAIT_TIMEOUT) {
        Sleep(1500 + rand() % 1000); // Імітація тривалої роботи

        WaitForSingleObject(hMutex, INFINITE);
        Log("Отримав доступ до ресурсу (Запис згенерованого числа)");

        currentNumber = rand() % 100;
        sharedBuffer.push_back(currentNumber);

        ReleaseMutex(hMutex);

        // Сигналізуємо обчислювачу про нові дані
        SetEvent(hEventDataReady);
    }

    Log("Завершення роботи (Генератор)");
    ExitThread(0); // Пункт 1: Завершення через ExitThread()
}

// Потік-обчислювач
DWORD WINAPI CalculatorThread(LPVOID lpParam) {
    Log("Старт роботи (Обчислювач)");

    HANDLE waitHandles[2] = { hShutdownEvent, hEventDataReady };

    while (true) {
        // Очікуємо або подію завершення, або надходження нових даних
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            break; // Отримано сигнал від hShutdownEvent
        }

        WaitForSingleObject(hMutex, INFINITE);
        Log("Отримав доступ до ресурсу (Обчислення середнього)");

        int count = min((int)sharedBuffer.size(), N);
        if (count > 0) {
            double sum = 0;
            for (size_t i = sharedBuffer.size() - count; i < sharedBuffer.size(); ++i) {
                sum += sharedBuffer[i];
            }
            currentAverage = sum / count;
        }

        ReleaseMutex(hMutex);

        // Сигналізуємо відображенню
        SetEvent(hEventCalcReady);
    }

    Log("Завершення роботи (Обчислювач)");
    ExitThread(0);
}

// Потік-відображення
DWORD WINAPI DisplayThread(LPVOID lpParam) {
    Log("Старт роботи (Відображення)");

    HANDLE waitHandles[2] = { hShutdownEvent, hEventCalcReady };

    while (true) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        WaitForSingleObject(hMutex, INFINITE);
        Log("Отримав доступ до ресурсу (Виведення результату)");

        WaitForSingleObject(hConsoleMutex, INFINITE);
        std::cout << "   ---> Поточне число: " << currentNumber
            << " | Середнє останніх " << N << ": " << currentAverage << " <--- \n";
        ReleaseMutex(hConsoleMutex);

        ReleaseMutex(hMutex);
    }

    Log("Завершення роботи (Відображення)");
    ExitThread(0);
}


const long long ARRAY_SIZE = 50000000;
std::vector<int> dataArray;
long long globalBenchSum = 0;
HANDLE hBenchMutex;

struct BenchData {
    long long startIdx;
    long long endIdx;
};

// Експериментальний потік підрахунку
DWORD WINAPI BenchmarkSumThread(LPVOID lpParam) {
    Log("Старт роботи (Експериментальний потік)");
    BenchData* td = (BenchData*)lpParam;
    long long localSum = 0;

    for (long long i = td->startIdx; i < td->endIdx; ++i) {
        localSum += dataArray[i];
    }

    WaitForSingleObject(hBenchMutex, INFINITE);
    Log("Отримав доступ до ресурсу (Додавання до глобальної суми)");
    globalBenchSum += localSum;
    ReleaseMutex(hBenchMutex);

    Log("Завершення роботи (Експериментальний потік)");
    ExitThread(0);
}

// Функція запуску експерименту
void RunBenchmark(int threadCount) {
    globalBenchSum = 0;
    std::vector<HANDLE> benchThreads(threadCount);
    std::vector<BenchData> tData(threadCount);
    long long chunkSize = ARRAY_SIZE / threadCount;

    WaitForSingleObject(hConsoleMutex, INFINITE);
    std::cout << "\n[ЕКСПЕРИМЕНТ] Запуск для " << threadCount << " потоків...\n";
    ReleaseMutex(hConsoleMutex);

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < threadCount; ++i) {
        tData[i].startIdx = i * chunkSize;
        tData[i].endIdx = (i == threadCount - 1) ? ARRAY_SIZE : (i + 1) * chunkSize;
        benchThreads[i] = CreateThread(NULL, 0, BenchmarkSumThread, &tData[i], 0, NULL);
    }

    // Очікування завершення тестових потоків
    WaitForMultipleObjects(threadCount, benchThreads.data(), TRUE, INFINITE);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    WaitForSingleObject(hConsoleMutex, INFINITE);
    std::cout << "[ЕКСПЕРИМЕНТ] Результат: Час = " << duration << " мс, Сума = " << globalBenchSum << "\n\n";
    ReleaseMutex(hConsoleMutex);

    for (int i = 0; i < threadCount; ++i) CloseHandle(benchThreads[i]);
}

int main() {
    // Ініціалізація даних для експерименту
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    dataArray.assign(ARRAY_SIZE, 1);

    // Створення засобів синхронізації (М'ютекси та події)
    hMutex = CreateMutex(NULL, FALSE, NULL);
    hConsoleMutex = CreateMutex(NULL, FALSE, NULL);
    hThreadTrackerMutex = CreateMutex(NULL, FALSE, NULL);
    hBenchMutex = CreateMutex(NULL, FALSE, NULL);

    // Автоматичні події для міжпотокової координації Варіанта 8
    hEventDataReady = CreateEvent(NULL, FALSE, FALSE, NULL);
    hEventCalcReady = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Ручна подія для завершення всіх потоків одразу
    hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Запускаємо один обов'язковий потік відображення
    hAllThreads.push_back(CreateThread(NULL, 0, DisplayThread, NULL, 0, NULL));

    int choice = -1;
    while (choice != 0) {
        std::cout << "\n=== ГОЛОВНЕ МЕНЮ (Пункт 1: Динамічне керування) ===\n";
        std::cout << "1. Додати потік-генератор\n";
        std::cout << "2. Додати потік-обчислювач\n";
        std::cout << "3. Провести експеримент (Пункт 5: 2, 4, 8, 16 потоків)\n";
        std::cout << "0. Завершити роботу програми\n";
        std::cout << "Вибір: ";
        std::cin >> choice;

        if (choice == 1) {
            HANDLE hThread = CreateThread(NULL, 0, GeneratorThread, NULL, 0, NULL);
            WaitForSingleObject(hThreadTrackerMutex, INFINITE);
            hAllThreads.push_back(hThread);
            ReleaseMutex(hThreadTrackerMutex);
            std::cout << "-> Генератор додано.\n";
        }
        else if (choice == 2) {
            HANDLE hThread = CreateThread(NULL, 0, CalculatorThread, NULL, 0, NULL);
            WaitForSingleObject(hThreadTrackerMutex, INFINITE);
            hAllThreads.push_back(hThread);
            ReleaseMutex(hThreadTrackerMutex);
            std::cout << "-> Обчислювач додано.\n";
        }
        else if (choice == 3) {
            RunBenchmark(2);
            RunBenchmark(4);
            RunBenchmark(8);
            RunBenchmark(16);
        }
    }

    std::cout << "\nІніціалізація завершення роботи...\n";

    // Коректне завершення (Головний потік сигналізує та очікує)
    SetEvent(hShutdownEvent);
    WaitForMultipleObjects(hAllThreads.size(), hAllThreads.data(), TRUE, INFINITE);

    // Очищення дескрипторів
    for (HANDLE h : hAllThreads) CloseHandle(h);
    CloseHandle(hMutex);
    CloseHandle(hConsoleMutex);
    CloseHandle(hThreadTrackerMutex);
    CloseHandle(hBenchMutex);
    CloseHandle(hEventDataReady);
    CloseHandle(hEventCalcReady);
    CloseHandle(hShutdownEvent);

    std::cout << "Всі ресурси вивільнено. Програма успішно завершена.\n";
    return 0;
}