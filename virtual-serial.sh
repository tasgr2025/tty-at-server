#!/usr/bin/env bash

# Настройки
SOCAT_CMD="socat"
PIDFILE="/tmp/virtual_serial.pid"
LINK0="/tmp/ttyV0"
LINK1="/tmp/ttyV1"

# Проверка наличия socat
if ! command -v $SOCAT_CMD &> /dev/null; then
    echo "Ошибка: утилита $SOCAT_CMD не найдена. Установите её, например:"
    echo "  sudo apt install socat   # Debian/Ubuntu"
    echo "  sudo yum install socat   # CentOS/RHEL"
    exit 1
fi

# Функция остановки
stop() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Останавливаем процесс socat (PID $PID)..."
            kill "$PID"
            rm -f "$PIDFILE"
            # Удаляем симлинки (опционально)
            rm -f "$LINK0" "$LINK1"
            echo "Виртуальные порты удалены."
        else
            echo "Процесс не найден. Возможно, уже остановлен."
            rm -f "$PIDFILE"
        fi
    else
        echo "PID-файл не найден. Процесс не запущен."
    fi
}

# Функция запуска
start() {
    # Проверяем, не запущен ли уже
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Процесс уже запущен (PID $PID). Используйте stop для остановки."
            exit 1
        else
            rm -f "$PIDFILE"
        fi
    fi

    echo "Запускаем виртуальные последовательные порты..."
    # Запускаем socat в фоне, создаём симлинки с удобными именами
    $SOCAT_CMD -d -d pty,raw,echo=0,link="$LINK0" pty,raw,echo=0,link="$LINK1" &
    SOCAT_PID=$!
    echo $SOCAT_PID > "$PIDFILE"

    # Даём время на создание устройств
    sleep 1

    # Проверяем, создались ли симлинки
    if [ -L "$LINK0" ] && [ -L "$LINK1" ]; then
        echo "Виртуальные порты созданы:"
        echo "  $LINK0"
        echo "  $LINK1"
        echo "Данные, записанные в один порт, появятся в другом."
        echo "Для остановки выполните: $0 stop"
    else
        echo "Не удалось создать симлинки. Проверьте права на запись в /tmp."
        kill "$SOCAT_PID" 2>/dev/null
        rm -f "$PIDFILE"
        exit 1
    fi
}

# Обработка аргументов командной строки
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        sleep 1
        start
        ;;
    *)
        echo "Использование: $0 {start|stop|restart}"
        exit 1
esac

exit 0