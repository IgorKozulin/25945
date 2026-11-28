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
 * - Ctrl-T - режим правки строки: стрелки влево/вправо, Home/End, Delete;
 *            Enter - применить, ESC - отменить (правка в любом месте)
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
#include <string.h>     /* memcpy, memmove                        */
#include <sys/select.h> /* select - таймаут при чтении ESC        */
#include <sys/time.h>   /* struct timeval                         */

/* Перерисовать текущую строку: \r - в начало строки, ESC[K - стереть
   остаток строки, затем сама строка и возврат курсора на позицию cur */
static void redraw_line(const char *s, int len, int cur) {
    printf("\r\033[K");
    for (int i = 0; i < len; i++) putchar(s[i]);
    for (int i = len; i > cur; i--) putchar('\b');
    fflush(stdout);
}

/* Есть ли готовый байт на вводе в течение ms миллисекунд.
   Нужно, чтобы отличить одиночный ESC (отмена) от ESC-последовательности
   стрелки: после ESC ждём немного и, если байта нет, считаем это ESC. */
static int key_ready(int ms) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

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
    char ctrl_t_char = '\024';             // CTRL-T - режим правки строки

    // Приветственное сообщение
    printf("String editor started. Use Backspace or DEL for erase char (VERASE), "
           "Ctrl-U for kill line (VKILL), Ctrl-W for kill word, "
           "Ctrl-D at empty to exit.\n"
           "Ctrl-T - edit the line: arrows, Home/End, Delete; Enter apply, ESC cancel.\n"
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
            while (pos > line_start && buffer[pos - 1] != ' ') pos--;

            // Удаляем от pos до total_len
            /* Стираем на экране всё, что правее pos (слово и пробелы
               за ним), и обрезаем буфер */
            int erase_count = total_len - pos;
            for (int i = 0; i < erase_count; i++) {
                printf("\b \b");
            }
            total_len = pos;

        } else if (c == ctrl_t_char) {
            /* === Режим правки строки (Ctrl-T) ===
               Текущая строка копируется в отдельный буфер edit, курсор ecur
               можно двигать стрелками и править текст в любом месте.
               Enter - применить изменения, ESC (или Ctrl-D) - отменить. */
            char edit[64];
            int elen = line_len;
            int ecur = line_len;

            memcpy(edit, buffer + line_start, elen);
            redraw_line(edit, elen, ecur);   /* показать строку и курсор */

            int edit_done = 0;               /* 1 - применить, 2 - отменить */
            while (!edit_done) {
                char k;
                if (read(STDIN_FILENO, &k, 1) != 1) { edit_done = 2; break; }

                if (k == '\r' || k == '\n') {
                    edit_done = 1;           /* Enter - применить */

                } else if (k == '\004') {
                    edit_done = 2;           /* Ctrl-D - отмена */

                } else if (k == esc_char) {
                    /* ESC: либо последовательность стрелки/Home/End/Delete,
                       либо одиночный ESC - отмена */
                    if (!key_ready(250)) { edit_done = 2; break; }

                    char a;
                    if (read(STDIN_FILENO, &a, 1) != 1) { edit_done = 2; break; }
                    if (a == '[' || a == 'O') {
                        char b;
                        if (read(STDIN_FILENO, &b, 1) != 1) { edit_done = 2; break; }

                        if (b == 'D') {                  /* стрелка влево */
                            if (ecur > 0) ecur--;
                        } else if (b == 'C') {           /* стрелка вправо */
                            if (ecur < elen) ecur++;
                        } else if (b == 'H') {           /* Home */
                            ecur = 0;
                        } else if (b == 'F') {           /* End */
                            ecur = elen;
                        } else if (b == '1' || b == '4') { /* Home/End: ESC [ 1~ / 4~ */
                            char t;
                            if (read(STDIN_FILENO, &t, 1) == 1 && t == '~') {
                                ecur = (b == '1') ? 0 : elen;
                            }
                        } else if (b == '3') {           /* Delete: ESC [ 3 ~ */
                            char t;
                            if (read(STDIN_FILENO, &t, 1) == 1 && t == '~' && ecur < elen) {
                                memmove(edit + ecur, edit + ecur + 1, elen - ecur - 1);
                                elen--;
                            }
                        }
                        redraw_line(edit, elen, ecur);

                    } else {
                        /* ESC + неизвестный символ - игнорируем */
                        redraw_line(edit, elen, ecur);
                    }

                } else if (k == erase_char) {
                    /* Backspace: удаляем символ слева от курсора */
                    if (ecur > 0) {
                        memmove(edit + ecur - 1, edit + ecur, elen - ecur);
                        ecur--;
                        elen--;
                        redraw_line(edit, elen, ecur);
                    }

                } else if (k == werase_char) {
                    /* Ctrl-W: удаляем слово слева от курсора и пробелы за ним */
                    int p = ecur;
                    while (p > 0 && edit[p - 1] == ' ') p--;
                    while (p > 0 && edit[p - 1] != ' ') p--;
                    if (p < ecur) {
                        memmove(edit + p, edit + ecur, elen - ecur);
                        elen -= ecur - p;
                        ecur = p;
                        redraw_line(edit, elen, ecur);
                    }

                } else if (isprint(k)) {
                    /* Вставка символа в позицию курсора */
                    if (elen < 40) {
                        memmove(edit + ecur + 1, edit + ecur, elen - ecur);
                        edit[ecur] = k;
                        ecur++;
                        elen++;
                        redraw_line(edit, elen, ecur);
                    } else {
                        putchar(bell_char);      /* строка уже 40 символов */
                        fflush(stdout);
                    }

                } else {
                    putchar(bell_char);
                    fflush(stdout);
                }
            }

            if (edit_done == 1 &&
                total_len - line_len + elen < (int)sizeof(buffer)) {
                /* Применяем: заменяем текущую строку в buffer на edit */
                memmove(buffer + line_start + elen,
                        buffer + line_start + line_len,
                        total_len - line_start - line_len);
                memcpy(buffer + line_start, edit, elen);
                total_len = total_len - line_len + elen;
                redraw_line(buffer + line_start, elen, elen);
            } else {
                /* Отмена: возвращаем строку как была */
                redraw_line(buffer + line_start, line_len, line_len);
            }

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
            if (total_len + 4 >= (int)sizeof(buffer)) {
                putchar(bell_char);        // буфер заполнен - дальше некуда
            } else if (line_len < 40) {
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

                        /* Если слово заняло все 40 столбцов, новый символ
                           должен начать следующую строку */
                        if (word_len >= 40) {
                            buffer[total_len++] = '\n';
                            putchar('\n');
                        }

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
