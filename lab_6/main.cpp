// Программа "Война потоков"
// Управление пушкой и врагами, отображение на консоли
// Цель: набрать 30 очков за 30 секунд, избегая промахов
#include <windows.h>
#include <process.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>

#define BULLET '*' // Символ пули
#define BARREL '|' // Символ пушки
#define STACK_SIZE (64*1024) // Размер стека для потоков

// Глобальные переменные
HANDLE screenlock; // Мьютекс для блокировки доступа к консоли
HANDLE bulletsem; // Семафор для ограничения количества пуль
HANDLE startevt; // Событие для отложенного старта игры
HANDLE conin, conout; // Дескрипторы консоли
HANDLE mainthread; // Дескриптор главного потока
CRITICAL_SECTION gameover; // Критическая секция для завершения игры

// Переменные для размеров консольного окна
int columns = 0;
int rows = 0;

// Счетчики попаданий и промахов
LONG hit = 0;
LONG miss = 0;
LONG delayfactor = 7; // Задержка для движения врагов

// Символы для анимации врагов
char badchar[] = "-\\|/";

// Генерация случайного числа в диапазоне [n0, n1)
int random(int n0, int n1)
{
  if (n0 == 0 && n1 == 1)
    return rand() % 2;

  return rand() % (n1 - n0) + n0;
}

// Очистка консоли
void cls()
{
  COORD org = { 0, 0 };
  DWORD res;
  FillConsoleOutputCharacter(conout, ' ', columns * rows, org, &res);
}

// Вывод символа в заданные координаты
void writeat(int x, int y, char c)
{
  WaitForSingleObject(screenlock, INFINITE);
  COORD pos = { x, y };
  DWORD res;
  WriteConsoleOutputCharacterA(conout, &c, 1, pos, &res);
  ReleaseMutex(screenlock);
}

// Получение клавиши с учетом повторений
int getakey(int& ct)
{
  INPUT_RECORD input;
  DWORD res;
  while (true) {
    ReadConsoleInput(conin, &input, 1, &res);

    if (input.EventType != KEY_EVENT)
      continue;

    if (!input.Event.KeyEvent.bKeyDown)
      continue;

    ct = input.Event.KeyEvent.wRepeatCount;
    return input.Event.KeyEvent.wVirtualKeyCode;
  }
}

// Обработчик Ctrl+C
BOOL WINAPI ctrl(DWORD type)
{
  ExitProcess(0);
  return TRUE;
}

// Получение символа в заданных координатах
int getat(int x, int y)
{
  char c;
  DWORD res;
  COORD org = { x, y };

  WaitForSingleObject(screenlock, INFINITE);
  ReadConsoleOutputCharacterA(conout, &c, 1, org, &res);
  ReleaseMutex(screenlock);
  return c;
}

// Обновление счета и проверка завершения игры
void score(void)
{
  TCHAR s[128];
  wsprintf(s, _T("Thread War! Попаданий: %3d, Промахов: %3d"), hit, miss);
  SetConsoleTitle(s);

  if (miss >= 1) {
    EnterCriticalSection(&gameover);
    SuspendThread(mainthread);
    MessageBox(NULL, _T("Game Over!"), _T("Thread War"), MB_OK | MB_SETFOREGROUND);
    ExitProcess(0);
  }

  if ((hit + miss) % 20 == 0)
    InterlockedDecrement(&delayfactor);
}

DWORD WINAPI badguy(LPVOID _y)
{
  int y = (long)_y;          // Получаем координату y
  int dir;
  int dly;
  int x;
  BOOL hitme = FALSE;

  // Определяем начальную позицию, влево или вправо
  x = y % 2 ? 0 : columns;

  // Определяем направление движения
  dir = x ? -1 : 1;

  // Пока не достигнут край экрана или не попали по нам
  while (((dir == 1 && x != columns) ||
    (dir == -1 && x != 0)) && !hitme) {

    // Проверяем, есть ли пуля в данной позиции
    if (getat(x, y) == BULLET)
      hitme = TRUE;

    // Отображаем символ "плохого парня"
    writeat(x, y, badchar[x % 4]);

    // Проверяем, есть ли пуля в данной позиции
    if (getat(x, y) == BULLET)
      hitme = TRUE;

    // Задержка перед следующим шагом
    if (delayfactor < 3)
      dly = 3;
    else
      dly = delayfactor + 3;

    for (int i = 0; i < dly; i++) {
      Sleep(40);
      if (getat(x, y) == BULLET) {
        hitme = TRUE;
        break;
      }
    }

    // Стираем символ "плохого парня"
    writeat(x, y, ' ');

    // Проверяем, есть ли пуля в данной позиции
    if (getat(x, y) == BULLET)
      hitme = TRUE;

    x += dir;
  }

  if (hitme)
  {
    // Звук попадания!
    MessageBeep(-1);
    InterlockedIncrement(&hit);
  }
  else {
    // Промах!
    InterlockedIncrement(&miss);
  }

  score();

  return 0;
}


// Функция для создания "плохих парней"
DWORD WINAPI badguys(LPVOID)
{
  // Ждем 15 секунд перед началом
  WaitForSingleObject(startevt, 15000);

  // Создаем "плохих парней"
  while (true) {
    if (random(0, 100) < (hit + miss) / 25 + 20)
      CreateThread(NULL, STACK_SIZE, badguy,
        (void*)(random(1, 10)), 0, NULL);
    Sleep(1000);            // Пауза между созданием
  }
}

// Функция для обработки пуль
DWORD WINAPI bullet(LPVOID _xy_)
{
  int x = LOWORD(_xy_);
  int y = HIWORD(_xy_);

  if (getat(x, y) == '*')
    return 0;               // Пуля попала в землю

  // Проверяем доступность пули
  if (WaitForSingleObject(bulletsem, 0) == WAIT_TIMEOUT)
    return 0;

  while (--y >= 0) {
    writeat(x, y, BULLET);       // Отображаем пулю
    Sleep(100);
    writeat(x, y, ' '); // Стираем пулю
  }

  // Освобождаем семафор
  ReleaseSemaphore(bulletsem, 1, NULL);

  return 0;
}

// Главная функция
int main()
{
  // Инициализация консольных дескрипторов ввода и вывода
  conin = GetStdHandle(STD_INPUT_HANDLE);
  conout = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleCtrlHandler(ctrl, TRUE);
  SetConsoleMode(conin, ENABLE_WINDOW_INPUT);

  // Настройка курсора
  CONSOLE_CURSOR_INFO cinfo;
  cinfo.bVisible = false;
  SetConsoleCursorInfo(conout, &cinfo);

  // Создание дескрипторов для потоков
  DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
    &mainthread, 0, FALSE, DUPLICATE_SAME_ACCESS);

  // Создание события и мьютекса
  startevt = CreateEvent(NULL, TRUE, FALSE, NULL);
  screenlock = CreateMutex(NULL, FALSE, NULL);
  InitializeCriticalSection(&gameover);
  bulletsem = CreateSemaphore(NULL, 3, 3, NULL);

  // Получение информации о размере экрана
  CONSOLE_SCREEN_BUFFER_INFO info;
  GetConsoleScreenBufferInfo(conout, &info);

  // Вывод счета
  score();

  // Инициализация случайных чисел
  srand((unsigned)time(NULL));
  cls();  // Очистка экрана

  // Инициализация координат игрока
  columns = info.srWindow.Right - info.srWindow.Left + 1;
  rows = info.srWindow.Bottom - info.srWindow.Top + 1;
  int y = rows - 1;
  int x = columns / 2;

  // Создание потока для врагов
  CreateThread(NULL, STACK_SIZE, badguys, NULL, 0, NULL);

  // Главный цикл игры
  while (true) {
    int c, ct;

    writeat(x, y, BARREL);  // Отображение бочки
    c = getakey(ct);        // Получение ввода пользователя

    switch (c) {
    case VK_SPACE:
      CreateThread(NULL, 64 * 1024, bullet, (void*)MAKELONG(x, y), 0, NULL);
      Sleep(100);  // Задержка после выстрела
      break;

    case VK_LEFT:  // Движение влево
      SetEvent(startevt);  // Запуск врагов
      writeat(x, y, ' ');  // Стереть текущую позицию
      x = (x > ct) ? (x - ct) : 0;
      break;

    case VK_RIGHT:  // Движение вправо
      SetEvent(startevt);
      writeat(x, y, ' ');
      x = (x + ct < columns - 1) ? (x + ct) : (columns - 1);
      break;
    }
  }
}
