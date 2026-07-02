#include "cli/cli_parser.h"
#include <CLI/CLI.hpp>
#include <iostream>

namespace vsdb {

ParsedCommand CLIParser::parse(int argc, char** argv) {
    CLI::App app{"VSDB - Version-controlled Database System"};
    app.require_subcommand(1);
    
    ParsedCommand result;
    
    // INIT command
    auto* init_cmd = app.add_subcommand("init", "Initialize a new VSDB database");
    
    // CREATE TABLE command
    auto* create_cmd = app.add_subcommand("create", "Create a new table");
    std::string create_table;
    std::vector<std::string> create_cols;
    create_cmd->add_option("table", create_table, "Table name")->required();
    create_cmd->add_option("--columns,-c", create_cols, "Column definitions (name:type)")->required();
    
    // INSERT command
    auto* insert_cmd = app.add_subcommand("insert", "Insert data into a table");
    std::string insert_table;
    std::vector<std::string> insert_cols;
    std::vector<std::string> insert_vals;
    insert_cmd->add_option("table", insert_table, "Table name")->required();
    insert_cmd->add_option("--columns,-c", insert_cols, "Column names");
    insert_cmd->add_option("--values,-v", insert_vals, "Values to insert");
    
    // SELECT command
    auto* select_cmd = app.add_subcommand("select", "Select data from a table");
    std::string select_table;
    std::string select_at_hash;
    select_cmd->add_option("table", select_table, "Table name")->required();
    select_cmd->add_option("--at", select_at_hash, "Query table as it was at a specific commit hash (time-travel)");
    
    // COMMIT command
    auto* commit_cmd = app.add_subcommand("commit", "Commit current changes");
    std::string commit_msg;
    commit_cmd->add_option("-m,--message", commit_msg, "Commit message")->required();
    
    // LOG command
    auto* log_cmd = app.add_subcommand("log", "Show commit history");
    
    // CHECKOUT command
    auto* checkout_cmd = app.add_subcommand("checkout", "Hard-rewind to a specific commit (rewrites HEAD)");
    std::string checkout_hash;
    checkout_cmd->add_option("commit", checkout_hash, "Commit hash")->required();

    // RESTORE command
    auto* restore_cmd = app.add_subcommand("restore", "Safe restore: brings back an old commit as a new commit, keeping full history");
    std::string restore_hash;
    restore_cmd->add_option("commit", restore_hash, "Commit hash to restore")->required();

    // BRANCH command
    auto* branch_cmd = app.add_subcommand("branch", "Manage branches (list / create / switch)");
    std::string branch_name_opt;
    bool branch_create_flag = false;
    branch_cmd->add_option("name", branch_name_opt, "Branch name")->required(false);
    branch_cmd->add_flag("-b,--create", branch_create_flag, "Create a new branch at current HEAD");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        return result;
    }
    
    // Determine which command was called
    if (app.got_subcommand(init_cmd)) {
        result.cmd = Command::INIT;
    } else if (app.got_subcommand(create_cmd)) {
        result.cmd = Command::CREATE_TABLE;
        result.table_name = create_table;
        result.columns = create_cols;
    } else if (app.got_subcommand(insert_cmd)) {
        result.cmd = Command::INSERT;
        result.table_name = insert_table;
        result.columns = insert_cols;
        result.values = insert_vals;
    } else if (app.got_subcommand(select_cmd)) {
        result.cmd = Command::SELECT;
        result.table_name = select_table;
        result.at_hash = select_at_hash;
    } else if (app.got_subcommand(commit_cmd)) {
        result.cmd = Command::COMMIT;
        result.commit_message = commit_msg;
    } else if (app.got_subcommand(log_cmd)) {
        result.cmd = Command::LOG;
    } else if (app.got_subcommand(checkout_cmd)) {
        result.cmd = Command::CHECKOUT;
        result.commit_hash = checkout_hash;
    } else if (app.got_subcommand(restore_cmd)) {
        result.cmd = Command::RESTORE;
        result.commit_hash = restore_hash;
    } else if (app.got_subcommand(branch_cmd)) {
        result.cmd = Command::BRANCH;
        result.branch_name   = branch_name_opt;
        result.branch_create = branch_create_flag;
    }
    
    return result;
}

} // namespace vsdb