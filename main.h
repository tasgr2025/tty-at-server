#ifndef MAIN_H
#define MAIN_H

#include <string>
#include <vector>
#include <utility>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <functional>

#include "parser.h"

/** Обработчик сигналов */
void signal_handler(int sig);

/** Обрезает пробелы в начале и конце строки */
std::string trim(const std::string& str);

/** Рекурсивный матчер: соответствует ли текст шаблону. Проверка
 * соответствия строки шаблону с поддержкой '.', '*' и классов символов [a,b,c] */
bool matchPattern(const std::string& pattern, const std::string& str);

/** Загружает словарь из файла */
std::vector<std::pair<std::string, std::string>> loadDictionary(const std::string& filename);

/** Настраивает последовательный порт в raw-режим */
bool configureTTY(int fd);

#endif /* MAIN_H */
