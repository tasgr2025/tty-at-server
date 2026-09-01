#include "main.h"
#include <iostream>
#include <fstream>

using std::vector;
using std::string;
using std::pair;
using std::ifstream;
using std::endl;
using std::cout;

/** Максимальная длина входной команды (байт) */
#define COMMAND_MAX_LENGTH 256

volatile sig_atomic_t running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM)
        running = 0;
}


string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}


bool matchPattern(const std::string& pattern, const std::string& str) {
    size_t pLen = pattern.size();
    size_t sLen = str.size();

    // Мемоизация для предотвращения экспоненциального времени
    std::vector<std::vector<int>> memo(pLen + 1, std::vector<int>(sLen + 1, -1));
 
    // Вспомогательная функция для разбора класса символов, начиная с индекса i
    auto parseClass = [&](size_t i, size_t& classLen, std::string& chars) -> bool {
        size_t close = pattern.find(']', i + 1);
        if (close == std::string::npos) {
            return false; // нет закрывающей скобки
        }
        classLen = close - i + 1;
        chars.clear();
        for (size_t k = i + 1; k < close; ++k) {
            if (pattern[k] != ',') {
                chars.push_back(pattern[k]);
            }
        }
        return true;
    };

    // Рекурсивная функция сопоставления
    std::function<bool(size_t, size_t)> dfs = [&](size_t pi, size_t si) -> bool {
        if (memo[pi][si] != -1) {
            return memo[pi][si] == 1;
        }
        bool result = false;
        if (pi == pLen) {
            result = (si == sLen);
        } else if (pattern[pi] == '*') {
            // '*' может соответствовать пустой строке или одному и более символам
            result = dfs(pi + 1, si) || (si < sLen && dfs(pi, si + 1));
        } else if (pattern[pi] == '.') {
            if (si < sLen) {
                result = dfs(pi + 1, si + 1);
            }
        } else if (pattern[pi] == '[') {
            size_t classLen;
            std::string chars;
            if (parseClass(pi, classLen, chars)) {
                if (si < sLen && chars.find(str[si]) != std::string::npos) {
                    result = dfs(pi + classLen, si + 1);
                }
            }
        } else {
            // Обычный символ
            if (si < sLen && pattern[pi] == str[si]) {
                result = dfs(pi + 1, si + 1);
            }
        }
        memo[pi][si] = result ? 1 : 0;
        return result;
    };
    return dfs(0, 0);
}


vector<pair<string, string>> loadDictionary(const string& filename) {
    vector<pair<string, string>> dict;
    ifstream file(filename);
    if (!file.is_open()) {
        fprintf(stderr, "Невозможно открыть файл \"%s\" словаря ответов.\n", filename.c_str());
        return dict;
    }
    string line;
    while (getline(file, line)) {
        size_t pos = line.rfind('=');
        if (pos == string::npos) continue;
        string pattern = line.substr(0, pos);
        string response = line.substr(pos + 1);
        pattern = trim(pattern);
        response = trim(response);
        if (pattern.empty()) continue;
        dict.push_back({pattern, response});
    }
    return dict;
}


bool configureTTY(int fd) {
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        perror("tcgetattr");
        return false;
    }
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    options.c_oflag &= ~OPOST;
    // Минимальное время ожидания и количество символов для read
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("tcsetattr");
        return false;
    }
    return true;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <tty устройство> <файл словаря>\n", argv[0]);
        return 1;
    }
    
    const char* ttyPath = argv[1];
    const char* dictPath = argv[2];
    const char* log_path = "cmd-parser.log";
    
    // Загрузить словарь
    auto dict = loadDictionary(dictPath);
    if (dict.empty()) {
        fprintf(stderr, "Словарь \"%s\" пуст или имеет неверный формат.\n", dictPath);
        return 1;
    }
    
    // Открыть tty
    int fd = open(ttyPath, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        fprintf(stderr, "Не удаётся открыть \"%s\".\n", ttyPath);
        return 1;
    }
    
    if (!configureTTY(fd)) {
        close(fd);
        return 1;
    }

    FILE* fl = fopen(log_path, "a");
    if (!fl) {
        fprintf(stderr, "Не удаётся открыть \"%s\" для записи.\n", log_path);
        exit(1);
    }
    fprintf(fl, "Журнал работы at-сервера\n");
    
    // Установить обработчики сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    fprintf(stdout, "AT сервер слушает на \"%s\" ...\n", ttyPath);
    
    string command;
    char ch;
    
    while (running) {
        ssize_t n = read(fd, &ch, 1);
        if (n < 0) {
            if (errno == EINTR) continue; // сигнал прервал read
            perror("read");
            break;
        } else if (n == 0) {
            /* EOF, устройство закрыто */
            break;
        }
        
        // Окончание команды: \r или \n
        if (ch == '\r' || ch == '\n') {
            if (!command.empty()) {
                // Поиск совпадения в словаре
                bool matched = false;
                AtCommand command_parsed;
                bool parsed_ok = parseAtCommand(command, command_parsed);

                for (const auto& entry : dict) {
                    /* проверить на соответствие команды словарю */
                    if (matchPattern(entry.first, command)) {
                        const string& response = entry.second;

                        // Записать в лог до отправки ответа
                        if (parsed_ok) {
                            log_command_parse(fl, command_parsed);
                        } else {
                            fprintf(fl, "FAIL : %s\n", command.c_str());
                        }
                        fflush(fl);

                        // Отправить ответ + \rн
                        if (write(fd, response.c_str(), response.length()) < 0) {
                            perror("write response");
                        }
                        if (write(fd, "\r\n", 2) < 0) {
                            perror("write CRLF");
                        }
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    // Записать в лог до отправки ответа
                    if (parsed_ok) {
                        log_command_parse(fl, command_parsed);
                    } else {
                        fprintf(fl, "FAIL : %s\n", command.c_str());
                    }
                    if (write(fd, "ERROR\r\n", 7) < 0) {
                        perror("write ERROR");
                    }
                }
                command.clear();
            }
            /* Если был \r, а следующий символ \n - он будет прочитан отдельно,
            но command будет пуст, поэтому ничего не делаем */
        } else {
            if ((int)command.size() >= COMMAND_MAX_LENGTH) {
                command.clear();
                fprintf(stderr, "Слишком длинная команда, пропущена.\n");
                continue;
            }
            command.push_back(ch);
        }
    }
    fprintf(fl, "Файл журнала закрыт\n");
    fflush(fl);
    fclose(fl);
    close(fd);
    fprintf(stdout, "AT сервер остановлен\n");
    return 0;
}
