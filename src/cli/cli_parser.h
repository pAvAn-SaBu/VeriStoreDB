#pragma once

#include <string>
#include <vector>

namespace vsdb {

enum class Command {
    NONE,
    INIT,
    CREATE_TABLE,
    INSERT,
    SELECT,
    COMMIT,
    LOG,
    CHECKOUT,
    RESTORE,
    BRANCH
};

struct ParsedCommand {
    Command cmd = Command::NONE;
    std::string table_name;
    std::vector<std::string> columns;
    std::vector<std::string> values;
    std::string commit_message;
    std::string commit_hash;
    std::string at_hash;          // for time-travel queries (--at <commit_hash>)
    std::string branch_name;      // for branch commands
    bool        branch_create = false; // true when -b flag is set
};

class CLIParser {
public:
    ParsedCommand parse(int argc, char** argv);
};

} // namespace vsdb