#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    // 1. Жестко устанавливаем часовой пояс Новосибирска
    // Это гарантирует, что время будет новосибирским даже на компьютере соседа
    setenv("TZ", "Asia/Novosibirsk", 1);
    tzset();

    // 2. Получаем текущее время (секунды с эпохи Unix)
    time_t now;
    time(&now);

    // 3. Конвертируем в локальное время с учетом установленного TZ
    struct tm *sp = localtime(&now);

    if (sp == NULL) {
        perror("Ошибка localtime");
        return 1;
    }

    // 4. Вывод времени строго до секунд (ДД/ММ/ГГГГ ЧЧ:ММ:СС Пояс)
    // %S добавлен специально по вашему требованию
    printf("%02d/%02d/%04d %02d:%02d:%02d %s\n",
           sp->tm_mday,          // День
           sp->tm_mon + 1,       // Месяц (0-11 -> +1)
           sp->tm_year + 1900,   // Год
           sp->tm_hour,          // Часы
           sp->tm_min,           // Минуты
           sp->tm_sec,           // Секунды
           tzname[sp->tm_isdst]  // Название пояса (NOVT / NOVST)
    );

    return 0;
}