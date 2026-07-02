#include "cli/cli_parser.h"
#include "db/database.h"
#include <iostream>
#include <sstream>

std::vector<vsdb::Column> parse_columns(const std::vector<std::string>& col_defs) {
    std::vector<vsdb::Column> columns;
    
    for (const auto& def : col_defs) {
        std::stringstream ss(def);
        std::string name, type_str;
        
        std::getline(ss, name, ':');
        std::getline(ss, type_str);
        
        vsdb::Column col;
        col.name = name;
        
        if (type_str == "int") {
            col.type = vsdb::DataType::INT;
        } else if (type_str == "float") {
            col.type = vsdb::DataType::FLOAT;
        } else if (type_str == "text") {
            col.type = vsdb::DataType::TEXT;
        } else if (type_str == "bool") {
            col.type = vsdb::DataType::BOOL;
        } else {
            std::cerr << "Unknown type: " << type_str << "\n";
            col.type = vsdb::DataType::TEXT;
        }
        
        columns.push_back(col);
    }
    
    return columns;
}

void print_table(const vsdb::TableSchema& schema, const std::vector<vsdb::Record>& records) {
    // Print header
    for (const auto& col : schema.columns) {
        std::cout << col.name << "\t";
    }
    std::cout << "\n";
    
    // Print separator
    for (size_t i = 0; i < schema.columns.size(); ++i) {
        std::cout << "--------\t";
    }
    std::cout << "\n";
    
    // Print rows
    for (const auto& record : records) {
        for (const auto& value : record.values) {
            std::cout << value << "\t";
        }
        std::cout << "\n";
    }
}

int main(int argc, char** argv) {
    vsdb::CLIParser parser;
    vsdb::ParsedCommand cmd = parser.parse(argc, argv);
    
    vsdb::Database db;
    
    switch (cmd.cmd) {
        case vsdb::Command::INIT:
            if (db.initialize()) {
                return 0;
            } else {
                return 1;
            }
            
        case vsdb::Command::CREATE_TABLE: {
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            
            auto columns = parse_columns(cmd.columns);
            if (db.create_table(cmd.table_name, columns)) {
                return 0;
            }
            return 1;
        }
            
        case vsdb::Command::INSERT: {
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            
            if (cmd.values.empty()) {
                std::cerr << "Error: No values provided\n";
                return 1;
            }
            
            vsdb::Record record;
            record.values = cmd.values;
            
            if (db.insert_into(cmd.table_name, record)) {
                std::cout << "Inserted 1 row into '" << cmd.table_name << "'\n";
                return 0;
            }
            return 1;
        }
            
        case vsdb::Command::SELECT: {
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            
            // Time-travel query: --at <commit_hash>
            if (!cmd.at_hash.empty()) {
                std::cout << "\n[Time-travel] Viewing table '" << cmd.table_name
                          << "' at commit " << cmd.at_hash << "\n\n";

                auto [schema, records] = db.select_at(cmd.table_name, cmd.at_hash);

                if (schema.columns.empty()) {
                    return 1;
                }

                print_table(schema, records);
                std::cout << "\n" << records.size()
                          << " rows returned (historical snapshot — current database unchanged)\n";
                return 0;
            }

            // Normal select
            auto table = db.get_table(cmd.table_name);
            if (!table) {
                std::cerr << "Error: Table '" << cmd.table_name << "' not found\n";
                return 1;
            }
            
            auto records = db.select_from(cmd.table_name);
            print_table(table->get_schema(), records);
            std::cout << "\n" << records.size() << " rows returned\n";
            return 0;
        }
            
        case vsdb::Command::COMMIT:
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            if (db.commit(cmd.commit_message).empty()) {
                return 1;
            }
            return 0;
            
        case vsdb::Command::LOG: {
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            
            auto commits = db.get_log();
            
            if (commits.empty()) {
                std::cout << "No commits yet\n";
                return 0;
            }
            
            for (size_t i = 0; i < commits.size(); ++i) {
                const auto& commit = commits[i];
                if (i == 0) {
                    std::string branch = db.get_current_branch();
                    if (!branch.empty()) {
                        std::cout << "Commit: " << commit.hash
                                  << "  \033[33m(HEAD -> " << branch << ")\033[0m\n";
                    } else {
                        std::cout << "Commit: " << commit.hash
                                  << "  \033[33m(HEAD)\033[0m\n";
                    }
                } else {
                    std::cout << "Commit: " << commit.hash << "\n";
                }
                std::cout << "Date:   " << commit.timestamp << "\n";
                std::cout << "        " << commit.message << "\n\n";
            }
            return 0;
        }
            
        case vsdb::Command::CHECKOUT:
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            if (db.checkout(cmd.commit_hash)) {
                return 0;
            }
            return 1;

        case vsdb::Command::RESTORE:
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }
            if (!db.safe_restore(cmd.commit_hash).empty()) {
                return 0;
            }
            return 1;

        case vsdb::Command::BRANCH: {
            if (!db.is_initialized()) {
                std::cerr << "Error: Database not initialized. Run 'vsdb init' first.\n";
                return 1;
            }

            // vsdb branch -b <name>  →  create
            if (cmd.branch_create) {
                if (cmd.branch_name.empty()) {
                    std::cerr << "Error: Branch name required with -b\n";
                    return 1;
                }
                return db.create_branch(cmd.branch_name) ? 0 : 1;
            }

            // vsdb branch <name>  →  switch
            if (!cmd.branch_name.empty()) {
                return db.switch_branch(cmd.branch_name) ? 0 : 1;
            }

            // vsdb branch  →  list all branches
            auto branches = db.list_branches();
            if (branches.empty()) {
                std::cout << "No branches found.\n";
                return 0;
            }
            for (const auto& [name, is_current] : branches) {
                if (is_current) {
                    std::cout << "\033[32m* " << name << "\033[0m\n"; // green
                } else {
                    std::cout << "  " << name << "\n";
                }
            }
            return 0;
        }

        case vsdb::Command::NONE:
        default:
            std::cerr << "No valid command specified.\n";
            return 1;
    }
    
    return 0;
}