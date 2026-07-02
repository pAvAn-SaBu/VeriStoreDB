#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <memory>
#include "gitstore/gitstore.h"

namespace vsdb {

enum class DataType {
    INT,
    FLOAT,
    TEXT,
    BOOL
};

struct Column {
    std::string name;
    DataType type;
    bool primary_key = false;
};

struct TableSchema {
    std::string table_name;
    std::vector<Column> columns;
    
    bool save_to_file(const std::filesystem::path& path) const;
    static TableSchema load_from_file(const std::filesystem::path& path);
};

struct Record {
    std::vector<std::string> values;
};

class Table {
public:
    Table(const std::string& name, const TableSchema& schema);
    
    bool insert(const Record& record);
    std::vector<Record> select_all() const;
    
    const TableSchema& get_schema() const { return schema_; }
    std::string get_name() const { return name_; }
    
    bool save_to_disk(const std::filesystem::path& data_dir);
    static std::unique_ptr<Table> load_from_disk(
        const std::filesystem::path& data_dir, 
        const std::string& table_name
    );
    
private:
    std::string name_;
    TableSchema schema_;
    std::vector<Record> records_;
    
    std::filesystem::path get_schema_path(const std::filesystem::path& data_dir) const;
    std::filesystem::path get_data_path(const std::filesystem::path& data_dir) const;
};

class Database {
public:
    Database();
    
    bool initialize();
    bool is_initialized() const;
    std::filesystem::path get_db_path() const;
    
    // Table management
    bool create_table(const std::string& name, const std::vector<Column>& columns);
    bool table_exists(const std::string& name) const;
    std::shared_ptr<Table> get_table(const std::string& name);
    
    // Data operations
    bool insert_into(const std::string& table_name, const Record& record);
    std::vector<Record> select_from(const std::string& table_name);
    
    // Time-travel query: read table as it was at a specific commit (read-only, no state change)
    std::pair<TableSchema, std::vector<Record>> select_at(
        const std::string& table_name, const std::string& commit_hash);
    
    // Version control operations
    std::string commit(const std::string& message);
    std::vector<Commit> get_log();
    bool checkout(const std::string& commit_hash);
    std::string safe_restore(const std::string& commit_hash);

    // Branch operations
    bool create_branch(const std::string& name);
    bool switch_branch(const std::string& name);
    std::vector<std::pair<std::string, bool>> list_branches();
    std::string get_current_branch();
    
private:
    std::filesystem::path db_root_;
    std::unordered_map<std::string, std::shared_ptr<Table>> tables_;
    std::unique_ptr<GitStore> git_store_;
    
    bool create_directory_structure();
    bool create_config_file();
    bool load_tables();
};

} // namespace vsdb