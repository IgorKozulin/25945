/* Лабораторная работа №17. Строчный редактор.
 *
 * Задание: выключить эхо и каноническую обработку терминала (вместе с ней
 * выключается и обработка символа забоя) и сделать свой редактор строки:
 * каждый введённый символ сразу появляется на экране, а служебные символы
 * обрабатываются вручную. Поэтому весь ввод читается по одному байту
 * через read(2) прямо с терминала (STDIN_FILENO) в неканоническом режиме.
 *
 * Поддерживается:
 * - Backspace/DEL (ERASE) - удаление последнего символа
 * - Ctrl-U (KILL) - удаление всей текущей строки
 * - Ctrl-W (WERASE) - удаление последнего слова и пробелов за ним
 * - Ctrl-D (EOF) - выход, если курсор в начале строки
 * - ограничение 40 символов на строку с переносом слов
 * - игнорирование ESC-последовательностей (стрелки, F-клавиши)
 * - звуковой сигнал (CTRL-G) для всех прочих непечатаемых символов
 */

#include <stdio.h>      /* printf, putchar, fflush                */
#include <termios.h>    /* struct termios, tcgetattr, tcsetattr,
                           ICANON, ECHO, VERASE, VKILL, TCSANOW   */
#include <unistd.h>     /* read, STDIN_FILENO                     */
#include <ctype.h>      /* isprint                                */
#include <string.h>     /* memcpy                                 */

int main() {
    /* Сохраняем текущие настройки терминала, чтобы потом восстановить их:
       oldt - как было, newt - как нужно нам */
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    /* ICANON выключает канонический режим (ввод построчно, с обработкой
       ERASE/KILL самим драйвером терминала), ECHO - автоматический вывод
       введённых символов. Теперь всё это программа делает сама */
    newt.c_lflag &= ~(ICANON | ECHO);  // Выключаем канонический режим и эхо
    /* TCSANOW - применить новые настройки немедленно */
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    char buffer[4096] = {0};           // Буфер для всех строк
    int total_len = 0;                 // Общая длина буфера

    // Специальные символы
    /* Значения ERASE и KILL берём из настроек терминала: на разных системах
       это может быть Backspace (^H) или DEL (^?), поэтому жёстко их не задаём */
    char erase_char = oldt.c_cc[VERASE];   // Обычно Backspace или DEL
    char kill_char  = oldt.c_cc[VKILL];    // Ctrl-U
    char werase_char = '\027';             // Ctrl-W
    char eof_char   = '\004';              // Ctrl-D
    char bell_char  = '\007';              // CTRL-G
    char esc_char   = '\033';              // ESC

    // Приветственное сообщение
    printf("String editor started. Use Backspace or DEL for erase char (VERASE), "
           "Ctrl-U for kill line (VKILL), Ctrl-W for kill word, "
           "Ctrl-D at empty to exit.\n"
           "Non-printable keys beep.\n");
    fflush(stdout);   /* выталкиваем приглашение до начала чтения с клавиатуры */

    while (1) {
        char c;
        /* Читаем ровно один байт: в неканоническом режиме read возвращает
           управление сразу после нажатия клавиши, не дожидаясь Enter */
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        // === Определяем начало текущей строки ===
        /* Идём от конца буфера назад до ближайшего '\n' (или до начала
           буфера). line_start - индекс первого символа текущей строки,
           line_len - сколько в ней уже набрано */
        int line_start = total_len - 1;
        while (line_start >= 0 && buffer[line_start] != '\n') line_start--;
        line_start += 1;
        int line_len = total_len - line_start;

        // === Обработка специальных символов ===
        if (c == eof_char) {
            /* Ctrl-D завершает работу только в начале строки,
               иначе воспринимается как недопустимый символ */
            if (line_len == 0) break;      // Ctrl-D в начале строки - выход
            else putchar(bell_char);

        } else if (c == erase_char) {
            /* Стираем последний символ: убираем его из буфера и затираем
               на экране последовательностью «шаг назад, пробел, шаг назад» */
            if (line_len > 0) {
                total_len--;
                printf("\b \b");           // Стираем символ
            }

        } else if (c == kill_char) {
            /* Стираем всю текущую строку: столько раз «\b \b», сколько в ней
               символов, и откатываем total_len к её началу */
            for (int i = 0; i < line_len; i++) {
                printf("\b \b");
            }
            total_len = line_start;        // Удаляем всю строку

        } else if (c == werase_char) {

            int pos = total_len;

            // Пропускаем конечные пробелы
            /* pos - «рабочий» указатель, сдвигается назад по буферу;
               начинаем с конца и сначала пропускаем пробелы за словом */
            while (pos > line_start && buffer[pos - 1] == ' ') pos--;

            // Случай, когда только пробелы - удаляем всю строку
            if (pos == line_start) {
                
                for (int i = 0; i < line_len; i++) printf("\b \b");
                total_len = line_start;
                continue;
            }

            // Пропускаем слово
            /* Идём назад, пока не упрёмся в пробел или начало строки:
               так находим начало последнего слова */
            int word_end = pos;
            while (pos > line_start && buffer[pos - 1] != ' ') pos--;
            int word_start = pos;

            // Пропускаем пробелы перед словом
            while (pos > line_start && buffer[pos - 1] == ' ') pos--;

            // Удаляем от pos до total_len
            /* Стираем на экране всё, что правее pos (слово и пробелы
               за ним), и обрезаем буфер */
            int erase_count = total_len - pos;
            for (int i = 0; i < erase_count; i++) {
                printf("\b \b");
            }
            total_len = pos;

        } else if (c == esc_char) {
            // Игнорируем ESC-последовательности (F-клавиши, стрелки и т.д.)
            /* Нажатие стрелки/функциональной клавиши приходит не одним
               байтом, а последовательностью ESC ... . Чтобы её «хвост» не
               попал в текст как обычные символы, дочитываем её целиком:
               CSI - ESC '[' ... , SS3 - ESC 'O' ... ; конец CSI - байт
               в диапазоне '@'..'~' */
            putchar(bell_char);
            char next;
            if (read(STDIN_FILENO, &next, 1) == 1) {
                if (next == '[') {  // CSI: \e[...]
                    while (read(STDIN_FILENO, &next, 1) == 1) {
                        if (next >= '@' && next <= '~') break;
                    }
                } else if (next == 'O') {  // SS3: \eO...
                    read(STDIN_FILENO, &next, 1);
                }
            }

        } else if (isprint(c)) {

            // === Обработка печатаемых символов ===
            /* Пока в строке меньше 40 символов - просто дописываем символ
               в буфер и сразу показываем его на экране */
            if (line_len < 40) {
                buffer[total_len++] = c;
                putchar(c);
            } else {
                /* Строка заполнена до 40 символов. Пробел на этой позиции
                   некуда девать - только сигнал */
                if (c == ' ') {
                    putchar(bell_char);    // Пробел на 40-й позиции - сигнал
                } else {

                    /* Если последний символ строки - пробел, слово уже
                       закончилось и новый символ просто начинает новую строку */
                    int last_is_space = (total_len > line_start && buffer[total_len - 1] == ' ');
                    if (last_is_space) {
                        // Можно начать новую строку
                        /* Вставляем '\n' и сам символ, дублируя это на экране */
                        buffer[total_len++] = '\n';
                        putchar('\n');
                        buffer[total_len++] = c;
                        putchar(c);
                    } else {
                        // Переносим последнее слово на новую строку
                        /* Слово «пересекает» 40-й столбец: находим его начало,
                           временно копируем в отдельный массив, стираем с экрана,
                           ставим '\n' и печатаем слово заново на новой строке */
                        int word_end = total_len;
                        int word_start = total_len;
                        while (word_start > line_start && buffer[word_start - 1] != ' ') {
                            word_start--;
                        }
                        int word_len = word_end - word_start;
                        char word[50];
                        memcpy(word, buffer + word_start, word_len);

                        // Стираем слово
                        for (int i = 0; i < word_len; i++) printf("\b \b");
                        total_len = word_start;

                        // Добавляем перенос
                        buffer[total_len++] = '\n';
                        putchar('\n');

                        // Возвращаем слово
                        /* Возвращаем слово в буфер и на экран уже в новой строке */
                        memcpy(buffer + total_len, word, word_len);
                        total_len += word_len;
                        for (int i = 0; i < word_len; i++) putchar(word[i]);

                        // Добавляем новый символ
                        buffer[total_len++] = c;
                        putchar(c);
                    }
                }
            }

        } else {
            // Все остальные непечатаемые символы
            putchar(bell_char);
        }

        fflush(stdout);   /* сброс буфера вывода, чтобы реакция была мгновенной */
    }

    // Восстанавливаем терминал
    /* Возвращаем прежние настройки (канонический режим и эхо), иначе
       терминал останется «испорченным» после выхода из программы */
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return 0;
}
