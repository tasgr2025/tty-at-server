#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>

enum class AtCommandType {
    Basic,      /** AT, ATZ, ATE0 */
    Execute,    /** AT+CMD  */
    Read,       /** AT+CMD?  */
    Test,       /** AT+CMD=? */
    Write,      /** AT+CMD=... */
    Unknown
};


struct AtCommand {
    std::string raw;      // исходная строка без пробелов в начале/конце
    std::string prefix;   // "", "+", "&", "%", "*", "#"
    std::string name;     // имя команды без "AT" и prefix
    AtCommandType type = AtCommandType::Unknown;
    std::vector<std::string> params;
};


void log_command_parse (FILE* fd, const AtCommand& cmd);
std::string ptrim(const std::string& s);
std::vector<std::string> splitParams(const std::string& s);
bool startsWithAt(const std::string& s);
bool parseAtCommand(const std::string& input, AtCommand& out);
const char* typeToString(AtCommandType type);

#endif /* PARSER_H */
