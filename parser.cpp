#include "parser.h"


using std::string;
using std::vector;
using std::fprintf;


void log_command_parse (FILE* fd, const AtCommand& cmd) {
    fprintf(fd, "OK   : %s", cmd.raw.c_str());
    fprintf(fd, " | prefix=%s", cmd.prefix.c_str());
    fprintf(fd, " | name=%s", cmd.name.c_str());
    fprintf(fd, " | type=%s", typeToString(cmd.type));
    fprintf(fd, " | params=");

    if (cmd.params.empty())
    {
        fprintf(fd, "[]");
    }
    else
    {
        fprintf(fd, "[");
        for (size_t i = 0; i < cmd.params.size(); ++i)
        {
            if (i)
            {
                fprintf(fd, ", ");
            }
            fprintf(fd, "%s", cmd.params[i].c_str());
        }
        fprintf(fd, "]");
    }
    fprintf(fd, "\n");
}


string ptrim(const std::string& s) {
    size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
        ++first;
    }

    size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }

    return s.substr(first, last - first);
}


vector<std::string> splitParams(const std::string& s) {
    std::vector<std::string> result;
    if (s.empty()) {
        return result;
    }

    std::string current;
    bool inQuotes = false;
    char quoteChar = '\0';
    bool escaped = false;

    for (char c : s) {
        if (escaped) {
            current.push_back(c);
            escaped = false;
            continue;
        }

        if (c == '\\' && inQuotes) {
            escaped = true;
            continue;
        }

        if (c == '"' || c == '\'') {
            if (inQuotes && c == quoteChar) {
                inQuotes = false;
            } else if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            } else {
                current.push_back(c);
            }
            continue;
        }

        if (c == ',' && !inQuotes) {
            result.push_back(ptrim(current));
            current.clear();
            continue;
        }

        current.push_back(c);
    }

    result.push_back(ptrim(current));
    return result;
}


bool startsWithAt(const std::string& s) {
    return s.size() >= 2 &&
           (s[0] == 'A' || s[0] == 'a') &&
           (s[1] == 'T' || s[1] == 't');
}


bool parseAtCommand(const std::string& input, AtCommand& out) {
    std::string line = ptrim(input);
    if (!startsWithAt(line)) {
        return false;
    }

    AtCommand cmd;
    cmd.raw = line;

    std::string body = line.substr(2); // после "AT"

    if (body.empty()) {
        cmd.prefix.clear();
        cmd.name.clear();
        cmd.type = AtCommandType::Basic;
        out = cmd;
        return true;
    }

    char c0 = body[0];
    if (c0 == '+' || c0 == '&' || c0 == '%' || c0 == '*' || c0 == '#') {
        cmd.prefix = std::string(1, c0);
        body = body.substr(1);
    } else {
        cmd.prefix.clear();
    }

    if (body.empty()) {
        return false;
    }

    size_t eq = body.find('=');
    size_t q = body.find('?');

    // Нет ни '=', ни '?' — execute или basic
    if (eq == std::string::npos && q == std::string::npos) {
        if (cmd.prefix.empty()) {
            // Базовая команда вида ATZ или ATE0
            size_t i = 0;
            while (i < body.size() && std::isalpha(static_cast<unsigned char>(body[i]))) {
                ++i;
            }

            cmd.name = ptrim(body.substr(0, i));
            if (cmd.name.empty()) {
                return false;
            }

            if (i < body.size()) {
                cmd.params.push_back(body.substr(i));
            }
            cmd.type = AtCommandType::Basic;
        } else {
            // Расширенная команда без параметров: AT+CMD
            cmd.name = ptrim(body);
            if (cmd.name.empty()) {
                return false;
            }
            cmd.type = AtCommandType::Execute;
        }

        out = cmd;
        return true;
    }

    // Определяем, где заканчивается имя команды
    size_t nameEnd;
    if (eq == std::string::npos) {
        nameEnd = q;
    } else if (q == std::string::npos) {
        nameEnd = eq;
    } else {
        nameEnd = std::min(eq, q);
    }

    cmd.name = ptrim(body.substr(0, nameEnd));
    if (cmd.name.empty()) {
        return false;
    }

    if (eq != std::string::npos && eq == nameEnd) {
        if (q != std::string::npos && q == eq + 1) {
            // AT+CMD=?
            cmd.type = AtCommandType::Test;
        } else if (q != std::string::npos && q != eq + 1) {
            // Некорректный формат
            cmd.type = AtCommandType::Unknown;
        } else {
            // AT+CMD=...
            cmd.type = AtCommandType::Write;
            cmd.params = splitParams(body.substr(eq + 1));
        }
    } else if (q != std::string::npos && q == nameEnd) {
        if (eq != std::string::npos) {
            // Некорректный формат вида AT+CMD?=...
            cmd.type = AtCommandType::Unknown;
        } else {
            // AT+CMD?
            cmd.type = AtCommandType::Read;
        }
    } else {
        cmd.type = AtCommandType::Unknown;
    }

    out = cmd;
    return true;
}


const char* typeToString(AtCommandType type) {
    switch (type) {
        case AtCommandType::Basic:   return "Basic";
        case AtCommandType::Execute: return "Execute";
        case AtCommandType::Read:    return "Read";
        case AtCommandType::Test:    return "Test";
        case AtCommandType::Write:   return "Write";
        default:                     return "Unknown";
    }
}
