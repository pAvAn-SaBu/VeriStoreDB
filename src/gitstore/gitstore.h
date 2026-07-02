#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <optional>

namespace vsdb {

struct Commit {
    std::string hash;
    std::string message;
    std::string timestamp;
    std::string parent_hash;
    std::vector<std::string> file_hashes; // Hashes of all data files
    
    std::string serialize() const;
    static Commit deserialize(const std::string& data);
};

class GitStore {
public:
    GitStore(const std::filesystem::path& objects_dir);

    // Initialize refs directory and default branch (called once on vsdb init)
    bool init_refs(const std::string& default_branch = "main");

    // Create a new commit with current database state
    std::string commit(const std::string& message, const std::filesystem::path& data_dir);
    
    // Get commit history from HEAD backwards
    std::vector<Commit> get_log() const;
    
    // Restore database to a specific commit (destructive — rewrites HEAD as raw hash)
    bool checkout(const std::string& commit_hash, const std::filesystem::path& data_dir);

    // Non-destructive restore: restores target commit's files, then creates a
    // new commit on top of current HEAD so the full history chain is preserved.
    std::string safe_restore(const std::string& target_hash,
                             const std::filesystem::path& data_dir);
    
    // Get current HEAD commit hash (resolves branch ref if on a branch)
    std::optional<std::string> get_head() const;

    // Read a file's content from a specific commit (in-memory, no disk writes)
    std::string read_file_at_commit(const std::string& commit_hash, const std::string& filename);

    // --- Branch operations ---

    // Create a new branch pointing to current HEAD
    bool create_branch(const std::string& name);

    // Switch to a branch: restores its latest commit's files and updates HEAD
    bool switch_branch(const std::string& name, const std::filesystem::path& data_dir);

    // List all branches as (name, is_current) pairs, sorted alphabetically
    std::vector<std::pair<std::string, bool>> list_branches() const;

    // Return current branch name, or "" if in detached HEAD mode
    std::string get_current_branch() const;

private:
    std::filesystem::path objects_dir_;
    std::filesystem::path head_file_;
    std::filesystem::path refs_dir_;   // stores one file per branch

    // File blob operations
    std::string hash_file(const std::filesystem::path& file_path);
    std::string store_file(const std::filesystem::path& file_path);
    bool restore_file(const std::string& hash, const std::filesystem::path& target_path);

    // Commit object operations
    bool save_commit(const Commit& commit);
    std::optional<Commit> load_commit(const std::string& hash) const;

    // HEAD / branch ref helpers
    std::string read_head_raw() const;                               // raw .vsdb_head content
    bool update_head(const std::string& commit_hash);                // write raw hash (detached HEAD)
    bool update_head_ref(const std::string& branch_name);           // write "ref: <branch>"
    std::optional<std::string> read_branch_ref(const std::string& branch_name) const;
    bool update_branch_ref(const std::string& branch_name, const std::string& commit_hash);

    // Generate simple hash (not cryptographic)
    std::string generate_hash(const std::string& content);
};

} // namespace vsdb