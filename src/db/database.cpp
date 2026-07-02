#include "db/database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace vsdb {

// TableSchema Implementation
bool TableSchema::save_to_file(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << table_name << "\n";
    file << columns.size() << "\n";
    
    for (const auto& col : columns) {
        file << col.name << ","
             << static_cast<int>(col.type) << ","
             << col.primary_key << "\n";
    }
    
    return true;
}

TableSchema TableSchema::load_from_file(const std::filesystem::path& path) {
    TableSchema schema;
    std::ifstream file(path);
    
    std::getline(file, schema.table_name);
    
    size_t num_columns;
    file >> num_columns;
    file.ignore();
    
    for (size_t i = 0; i < num_columns; ++i) {
        std::string line;
        std::getline(file, line);
        std::stringstream ss(line);
        
        Column col;
        std::string type_str, pk_str;
        
        std::getline(ss, col.name, ',');
        std::getline(ss, type_str, ',');
        std::getline(ss, pk_str, ',');
        
        col.type = static_cast<DataType>(std::stoi(type_str));
        col.primary_key = (pk_str == "1");
        
        schema.columns.push_back(col);
    }
    
    return schema;
}

// Table Implementation
Table::Table(const std::string& name, const TableSchema& schema)
    : name_(name), schema_(schema) {
}

bool Table::insert(const Record& record) {
    if (record.values.size() != schema_.columns.size()) {
        std::cerr << "Error: Column count mismatch\n";
        return false;
    }
    records_.push_back(record);
    return true;
}

std::vector<Record> Table::select_all() const {
    return records_;
}

std::filesystem::path Table::get_schema_path(const std::filesystem::path& data_dir) const {
    return data_dir / (name_ + ".schema");
}

std::filesystem::path Table::get_data_path(const std::filesystem::path& data_dir) const {
    return data_dir / (name_ + ".data");
}

bool Table::save_to_disk(const std::filesystem::path& data_dir) {
    // Save schema
    if (!schema_.save_to_file(get_schema_path(data_dir))) {
        return false;
    }
    
    // Save data
    std::ofstream data_file(get_data_path(data_dir));
    if (!data_file.is_open()) return false;
    
    data_file << records_.size() << "\n";
    
    for (const auto& record : records_) {
        for (size_t i = 0; i < record.values.size(); ++i) {
            data_file << record.values[i];
            if (i < record.values.size() - 1) {
                data_file << ",";
            }
        }
        data_file << "\n";
    }
    
    return true;
}

std::unique_ptr<Table> Table::load_from_disk(
    const std::filesystem::path& data_dir,
    const std::string& table_name
) {
    std::filesystem::path schema_path = data_dir / (table_name + ".schema");
    std::filesystem::path data_path = data_dir / (table_name + ".data");
    
    if (!std::filesystem::exists(schema_path)) {
        return nullptr;
    }
    
    TableSchema schema = TableSchema::load_from_file(schema_path);
    auto table = std::make_unique<Table>(table_name, schema);
    
    if (std::filesystem::exists(data_path)) {
        std::ifstream data_file(data_path);
        size_t num_records;
        data_file >> num_records;
        data_file.ignore();
        
        for (size_t i = 0; i < num_records; ++i) {
            std::string line;
            std::getline(data_file, line);
            std::stringstream ss(line);
            
            Record record;
            std::string value;
            while (std::getline(ss, value, ',')) {
                record.values.push_back(value);
            }
            
            table->insert(record);
        }
    }
    
    return table;
}

// Database Implementation
Database::Database() 
    : db_root_(std::filesystem::current_path()) {
    if (is_initialized()) {
        git_store_ = std::make_unique<GitStore>(db_root_ / "objects");
        load_tables();
    }
}

bool Database::is_initialized() const {
    return std::filesystem::exists(db_root_ / ".vsdb");
}

std::filesystem::path Database::get_db_path() const {
    return db_root_;
}

bool Database::initialize() {
    if (is_initialized()) {
        std::cerr << "Error: Database already initialized in this directory.\n";
        return false;
    }
    
    if (!create_directory_structure()) {
        std::cerr << "Error: Failed to create directory structure.\n";
        return false;
    }
    
    if (!create_config_file()) {
        std::cerr << "Error: Failed to create configuration file.\n";
        return false;
    }
    
    std::cout << "Database initialized successfully in " << db_root_ << "\n";
    std::cout << "Created directories:\n";
    std::cout << "  - data/      (for table data storage)\n";
    std::cout << "  - objects/   (for version control objects)\n";
    std::cout << "  - refs/      (for branch references)\n";

    git_store_ = std::make_unique<GitStore>(db_root_ / "objects");
    git_store_->init_refs("main");
    std::cout << "Initialized on branch 'main'\n";

    return true;
}

bool Database::create_directory_structure() {
    try {
        std::filesystem::create_directories(db_root_ / "data");
        std::filesystem::create_directories(db_root_ / "objects");
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << "\n";
        return false;
    }
}

bool Database::create_config_file() {
    try {
        std::ofstream config_file(db_root_ / ".vsdb");
        
        if (!config_file.is_open()) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        
        config_file << "version=1.0\n";
        config_file << "initialized=" << ss.str() << "\n";
        config_file << "format=vsdb\n";
        
        config_file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating config file: " << e.what() << "\n";
        return false;
    }
}

bool Database::load_tables() {
    std::filesystem::path data_dir = db_root_ / "data";
    
    if (!std::filesystem::exists(data_dir)) {
        return true;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.path().extension() == ".schema") {
            std::string table_name = entry.path().stem().string();
            auto table = Table::load_from_disk(data_dir, table_name);
            
            if (table) {
                tables_[table_name] = std::move(table);
            }
        }
    }
    
    return true;
}

bool Database::create_table(const std::string& name, const std::vector<Column>& columns) {
    if (!is_initialized()) {
        std::cerr << "Error: Database not initialized\n";
        return false;
    }
    
    if (table_exists(name)) {
        std::cerr << "Error: Table '" << name << "' already exists\n";
        return false;
    }
    
    TableSchema schema;
    schema.table_name = name;
    schema.columns = columns;
    
    auto table = std::make_shared<Table>(name, schema);
    
    if (!table->save_to_disk(db_root_ / "data")) {
        std::cerr << "Error: Failed to save table to disk\n";
        return false;
    }
    
    tables_[name] = table;
    
    std::cout << "Table '" << name << "' created successfully\n";
    return true;
}

bool Database::table_exists(const std::string& name) const {
    return tables_.find(name) != tables_.end();
}

std::shared_ptr<Table> Database::get_table(const std::string& name) {
    if (!table_exists(name)) {
        // Try to load from disk
        auto table = Table::load_from_disk(db_root_ / "data", name);
        if (table) {
            tables_[name] = std::move(table);
        }
    }
    
    auto it = tables_.find(name);
    return (it != tables_.end()) ? it->second : nullptr;
}

bool Database::insert_into(const std::string& table_name, const Record& record) {
    auto table = get_table(table_name);
    if (!table) {
        std::cerr << "Error: Table '" << table_name << "' does not exist\n";
        return false;
    }
    
    if (!table->insert(record)) {
        return false;
    }
    
    if (!table->save_to_disk(db_root_ / "data")) {
        std::cerr << "Error: Failed to save table to disk\n";
        return false;
    }
    
    return true;
}

std::vector<Record> Database::select_from(const std::string& table_name) {
    auto table = get_table(table_name);
    if (!table) {
        std::cerr << "Error: Table '" << table_name << "' does not exist\n";
        return {};
    }
    
    return table->select_all();
}

std::pair<TableSchema, std::vector<Record>> Database::select_at(
    const std::string& table_name, const std::string& commit_hash) {

    if (!git_store_) {
        std::cerr << "Error: Git store not initialized\n";
        return {};
    }

    // --- Read schema from the object store (in-memory) ---
    std::string schema_content = git_store_->read_file_at_commit(
        commit_hash, table_name + ".schema");

    if (schema_content.empty()) {
        std::cerr << "Error: Table '" << table_name
                  << "' not found in commit " << commit_hash << "\n";
        return {};
    }

    // Parse schema from string, mirroring TableSchema::load_from_file
    TableSchema schema;
    std::istringstream schema_ss(schema_content);
    std::getline(schema_ss, schema.table_name);

    size_t num_columns = 0;
    schema_ss >> num_columns;
    schema_ss.ignore();

    for (size_t i = 0; i < num_columns; ++i) {
        std::string line;
        std::getline(schema_ss, line);
        std::stringstream col_ss(line);

        Column col;
        std::string type_str, pk_str;
        std::getline(col_ss, col.name, ',');
        std::getline(col_ss, type_str, ',');
        std::getline(col_ss, pk_str, ',');

        col.type = static_cast<DataType>(std::stoi(type_str));
        col.primary_key = (pk_str == "1");
        schema.columns.push_back(col);
    }

    // --- Read data from the object store (in-memory) ---
    std::string data_content = git_store_->read_file_at_commit(
        commit_hash, table_name + ".data");

    std::vector<Record> records;
    if (!data_content.empty()) {
        std::istringstream data_ss(data_content);
        size_t num_records = 0;
        data_ss >> num_records;
        data_ss.ignore();

        for (size_t i = 0; i < num_records; ++i) {
            std::string line;
            std::getline(data_ss, line);
            std::stringstream row_ss(line);

            Record record;
            std::string value;
            while (std::getline(row_ss, value, ',')) {
                record.values.push_back(value);
            }
            records.push_back(record);
        }
    }

    return {schema, records};
}

std::string Database::commit(const std::string& message) {
    if (!git_store_) {
        std::cerr << "Error: Git store not initialized\n";
        return "";
    }
    
    std::string commit_hash = git_store_->commit(message, db_root_ / "data");
    
    if (!commit_hash.empty()) {
        std::cout << "Committed successfully\n";
        std::cout << "Commit hash: " << commit_hash << "\n";
    }
    
    return commit_hash;
}

std::vector<Commit> Database::get_log() {
    if (!git_store_) {
        return {};
    }
    
    return git_store_->get_log();
}

bool Database::checkout(const std::string& commit_hash) {
    if (!git_store_) {
        std::cerr << "Error: Git store not initialized\n";
        return false;
    }
    
    if (git_store_->checkout(commit_hash, db_root_ / "data")) {
        // Reload tables from disk
        tables_.clear();
        load_tables();
        
        std::cout << "Checked out commit " << commit_hash << "\n";
        return true;
    }
    
    return false;
}

std::string Database::safe_restore(const std::string& commit_hash) {
    if (!git_store_) {
        std::cerr << "Error: Git store not initialized\n";
        return "";
    }

    auto old_head = git_store_->get_head();

    std::string new_hash = git_store_->safe_restore(commit_hash, db_root_ / "data");

    if (!new_hash.empty()) {
        tables_.clear();
        load_tables();

        std::cout << "\nRestoring database to state at commit " << commit_hash << "...\n";
        std::cout << "Created restore commit: " << new_hash << "\n";
        if (old_head) {
            std::cout << "  Parent: " << *old_head << " (your previous HEAD — still intact)\n";
        }
        std::cout << "  Message: \"Restored to " << commit_hash << "\"\n";
        std::cout << "\nRun 'vsdb log' to see full history.\n";
    }

    return new_hash;
}

bool Database::create_branch(const std::string& name) {
    if (!git_store_) return false;
    return git_store_->create_branch(name);
}

bool Database::switch_branch(const std::string& name) {
    if (!git_store_) return false;
    if (git_store_->switch_branch(name, db_root_ / "data")) {
        tables_.clear();
        load_tables();
        return true;
    }
    return false;
}

std::vector<std::pair<std::string, bool>> Database::list_branches() {
    if (!git_store_) return {};
    return git_store_->list_branches();
}

std::string Database::get_current_branch() {
    if (!git_store_) return "";
    return git_store_->get_current_branch();
}

} // namespace vsdb