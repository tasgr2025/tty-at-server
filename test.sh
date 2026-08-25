#!/usr/bin/env bash

# Этот скрипт выполняет тестирование at-сервера.

# Порт к которому подключен at-клиент
TTY="/tmp/ttyV1"

# Cловарь запросов и ответов для at-сервера
DICT="dict.txt"

# Контрольный словарь запросов и ответов
TEST_DICT="test-dict.txt"

# Порт к которому подключен at-сервер
TTY_SERVER="/tmp/ttyV0"

# Исполняемый файл at-сервера
AT_SERVER="./atserver"

# Команда запуска at-сервера
AT_SERVER_START="./virtual-serial.sh start"

cleanup() {
    if kill -0 ${AT_SERVER_PID} 2>/dev/null; then
        echo "Завершение ${AT_SERVER}"
        kill ${AT_SERVER_PID} > /dev/null
        sleep 1
        # Если не завершился, SIGKILL
        if kill -0 ${AT_SERVER_PID} 2>/dev/null; then
            kill -9 ${AT_SERVER_PID}
        fi
    fi
}
trap cleanup EXIT

if [ ! -f "${AT_SERVER}" ]; then
    echo "Ошибка: проверяемый исполняемый файл ${AT_SERVER} не найден."
    exit 1
fi

./virtual-serial.sh start

${AT_SERVER} ${TTY_SERVER} ${DICT} &
AT_SERVER_PID=$!

# Проверка существования порта
if [ ! -e "${TTY}" ]; then
    echo "Ошибка: порт ${TTY} не существует."
    exit 1
fi

# Проверка файла словаря
if [ ! -f "$DICT" ]; then
    echo "Ошибка: файл словаря ${DICT} не найден."
    exit 1
fi

# Проверка файла контрольного словаря
if [ ! -f "$DICT" ]; then
    echo "Ошибка: файл контрольного словаря ${TEST_DICT} не найден."
    exit 1
fi

# Отправляет команды и получает ответ
send_at() {
    local cmd="$1"
    local response=""
    response=$(./send-at-cmd.sh "${TTY}" "${cmd}" 2>/dev/null)
    # удалить \r и лишние пробелы в конце
    echo "$response" | sed -e 's/\r//g' -e 's/[[:space:]]*$//'
}

# Подсчёт результатов
total=0
passed=0
failed=0

echo "Тестирование AT-команд на порту ${TTY}"
echo "Рабочий словарь: ${DICT}"
echo "Контрольный словарь: ${TEST_DICT}"
echo

# Читать файл построчно, игнорируя пустые строки и комментарии #
while IFS= read -r line || [ -n "$line" ]; do
    # Убрать пробелы в начале/конце
    line=$(echo "$line" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
    # Пропустить пустые и комментарии
    [ -z "$line" ] && continue
    [[ "$line" =~ ^# ]] && continue

    # Разделить по последнему '='
    if [[ "$line" == *"="* ]]; then
        cmd="${line%=*}"      # всё до последнего '='
        expected="${line##*=}" # всё после последнего '='
    else
        cmd="$line"
        expected=""
    fi

    # Убрать пробелы по краям
    cmd=$(echo "$cmd" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
    expected=$(echo "$expected" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')

    if [ -z "$cmd" ]; then
        echo "Пропускается некорректная строка: $line"
        continue
    fi

    total=$((total + 1))
    echo -n "Тест $total: $cmd -> "

    # Отправить команду
    response=$(send_at "$cmd")
    # Сравнить с ожидаемым
    if [ "$response" = "$expected" ]; then
        echo "УСПЕШНО (ответ: $response)"
        passed=$((passed + 1))
    else
        echo "ПРОВАЛЕНО"
        echo "   Ожидалось: $expected"
        echo "   Получено:  $response"
        failed=$((failed + 1))
    fi
done < "$TEST_DICT"


echo
echo "Результаты"
echo "Всего: $total, Пройдено: $passed, Провалено: $failed"
RC=0
if [ $failed -eq 0 ]; then
    echo "ВСЕ ТЕСТЫ ПРОЙДЕНЫ"
else
    RC=1
    echo "ОДИН ИЛИ НЕСКОЛЬКО ТЕСТОВ ПРОВАЛЕНО"
fi

cleanup
./virtual-serial.sh stop
exit $RC
