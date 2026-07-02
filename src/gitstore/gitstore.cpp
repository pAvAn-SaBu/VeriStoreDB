#include "gitstore/gitstore.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <algorithm>

namespace vsdb {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GitStore::GitStore(const std::filesystem::path& objects_dir)
    : objects_dir_(objects_dir),
      head_file_(objects_dir.parent_path() / ".vsdb_head"),
      refs_dir_(objects_dir.parent_path() / "refs") {
    std::filesystem::create_directories(objects_dir_);
    std::filesystem::create_directories(refs_dir_);
}

// ---------------------------------------------------------------------------
// Hashing & blob storage
// ---------------------------------------------------------------------------

std::string GitStore::generate_hash(const std::string& content) {
    // Simple hash (not cryptographic — for demo purposes)
    std::hash<std::string> hasher;
    size_t hash_value = hasher(content);
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash_value;
    return ss.str();
}

std::string GitStore::hash_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return generate_hash(buffer.str());
}

std::string GitStore::store_file(const std::filesystem::path& file_path) {
    std::ifstream src(file_path, std::ios::binary);
    if (!src.is_open()) return "";

    std::stringstream buffer;
    buffer << src.rdbuf();
    std::string content = buffer.str();

    std::string hash = generate_hash(content);
    std::filesystem::path obj_path = objects_dir_ / hash;

    if (std::filesystem::exists(obj_path)) return hash; // already stored

    std::ofstream dst(obj_path, std::ios::binary);
    dst << content;
    return hash;
}

bool GitStore::restore_file(const std::string& hash, const std::filesystem::path& target_path) {
    std::filesystem::path obj_path = objects_dir_ / hash;
    if (!std::filesystem::exists(obj_path)) {
        std::cerr << "Error: Object " << hash << " not found\n";
        return false;
    }
    std::ifstream src(obj_path, std::ios::binary);
    std::ofstream dst(target_path, std::ios::binary);
    dst << src.rdbuf();
    return true;
}

// ---------------------------------------------------------------------------
// Commit serialization
// ---------------------------------------------------------------------------

std::string Commit::serialize() const {
    std::stringstream ss;
    ss << "hash="      << hash        << "\n";
    ss << "message="   << message     << "\n";
    ss << "timestamp=" << timestamp   << "\n";
    ss << "parent="    << parent_hash << "\n";
    ss << "files="     << file_hashes.size() << "\n";
    for (const auto& fh : file_hashes) ss << fh << "\n";
    return ss.str();
}

Commit Commit::deserialize(const std::string& data) {
    Commit commit;
    std::istringstream ss(data);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            commit.file_hashes.push_back(line);
            continue;
        }

        std::string key   = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        if      (key == "hash")      commit.hash        = value;
        else if (key == "message")   commit.message     = value;
        else if (key == "timestamp") commit.timestamp   = value;
        else if (key == "parent")    commit.parent_hash = value;
        // "files" line is just a count hint — actual hashes follow as plain lines
    }
    return commit;
}

bool GitStore::save_commit(const Commit& commit) {
    std::ofstream file(objects_dir_ / commit.hash);
    if (!file.is_open()) return false;
    file << commit.serialize();
    return true;
}

std::optional<Commit> GitStore::load_commit(const std::string& hash) const {
    std::filesystem::path commit_path = objects_dir_ / hash;
    if (!std::filesystem::exists(commit_path)) return std::nullopt;

    std::ifstream file(commit_path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Commit::deserialize(buffer.str());
}

// ---------------------------------------------------------------------------
// HEAD & branch ref helpers
// ---------------------------------------------------------------------------

std::string GitStore::read_head_raw() const {
    if (!std::filesystem::exists(head_file_)) return "";
    std::ifstream file(head_file_);
    std::string content;
    std::getline(file, content);
    return content;
}

bool GitStore::update_head(const std::string& commit_hash) {
    std::ofstream file(head_file_);
    if (!file.is_open()) return false;
    file << commit_hash;
    return true;
}

bool GitStore::update_head_ref(const std::string& branch_name) {
    std::ofstream file(head_file_);
    if (!file.is_open()) return false;
    file << "ref: " << branch_name;
    return true;
}

std::optional<std::string> GitStore::read_branch_ref(const std::string& branch_name) const {
    std::filesystem::path ref_path = refs_dir_ / branch_name;
    if (!std::filesystem::exists(ref_path)) return std::nullopt;

    std::ifstream file(ref_path);
    std::string hash;
    std::getline(file, hash);
    return hash.empty() ? std::nullopt : std::make_optional(hash);
}

bool GitStore::update_branch_ref(const std::string& branch_name, const std::string& commit_hash) {
    std::ofstream file(refs_dir_ / branch_name);
    if (!file.is_open()) return false;
    file << commit_hash;
    return true;
}

// ---------------------------------------------------------------------------
// Public API — HEAD resolution
// ---------------------------------------------------------------------------

std::optional<std::string> GitStore::get_head() const {
    std::string raw = read_head_raw();
    if (raw.empty()) return std::nullopt;

    if (raw.size() >= 5 && raw.substr(0, 5) == "ref: ") {
        // On a branch — resolve to the branch's commit hash
        return read_branch_ref(raw.substr(5));
    }

    // Detached HEAD — raw value is the hash
    return std::make_optional(raw);
}

std::string GitStore::get_current_branch() const {
    std::string raw = read_head_raw();
    if (raw.size() >= 5 && raw.substr(0, 5) == "ref: ") {
        return raw.substr(5);
    }
    return ""; // detached HEAD
}

// ---------------------------------------------------------------------------
// Public API — Init, Commit, Log, Checkout
// ---------------------------------------------------------------------------

bool GitStore::init_refs(const std::string& default_branch) {
    std::filesystem::create_directories(refs_dir_);
    // Create the default branch ref file (empty = no commits yet)
    std::ofstream branch_file(refs_dir_ / default_branch);
    // Write "ref: <default_branch>" to HEAD
    return update_head_ref(default_branch);
}

std::string GitStore::commit(const std::string& message, const std::filesystem::path& data_dir) {
    Commit new_commit;
    new_commit.message = message;

    // Timestamp
    auto now    = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ts;
    ts << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    new_commit.timestamp = ts.str();

    // Parent = current HEAD
    auto head = get_head();
    new_commit.parent_hash = head.value_or("");

    // Snapshot all data files into the object store
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_regular_file()) {
            std::string hash = store_file(entry.path());
            if (!hash.empty()) {
                new_commit.file_hashes.push_back(
                    entry.path().filename().string() + ":" + hash);
            }
        }
    }

    // Compute commit hash
    std::string commit_content = new_commit.message + new_commit.timestamp + new_commit.parent_hash;
    for (const auto& fh : new_commit.file_hashes) commit_content += fh;
    new_commit.hash = generate_hash(commit_content);

    if (!save_commit(new_commit)) return "";

    // Advance pointer: branch ref if on a branch, HEAD directly if detached
    std::string raw = read_head_raw();
    if (raw.size() >= 5 && raw.substr(0, 5) == "ref: ") {
        update_branch_ref(raw.substr(5), new_commit.hash);
        // HEAD file ("ref: <branch>") stays unchanged
    } else {
        update_head(new_commit.hash);
    }

    return new_commit.hash;
}

std::vector<Commit> GitStore::get_log() const {
    std::vector<Commit> log;

    auto head = get_head();
    if (!head) return log;

    std::string current_hash = *head;
    while (!current_hash.empty()) {
        auto commit = load_commit(current_hash);
        if (!commit) break;
        log.push_back(*commit);
        current_hash = commit->parent_hash;
    }
    return log;
}

bool GitStore::checkout(const std::string& commit_hash, const std::filesystem::path& data_dir) {
    auto commit = load_commit(commit_hash);
    if (!commit) {
        std::cerr << "Error: Commit " << commit_hash << " not found\n";
        return false;
    }

    // Clear data directory
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_regular_file()) std::filesystem::remove(entry.path());
    }

    // Restore files
    for (const auto& file_hash : commit->file_hashes) {
        size_t colon_pos = file_hash.find(':');
        if (colon_pos == std::string::npos) continue;
        std::string filename = file_hash.substr(0, colon_pos);
        std::string hash     = file_hash.substr(colon_pos + 1);
        if (!restore_file(hash, data_dir / filename)) {
            std::cerr << "Warning: Failed to restore " << filename << "\n";
        }
    }

    // Detached HEAD — write raw hash
    update_head(commit_hash);
    return true;
}

std::string GitStore::safe_restore(const std::string& target_hash,
                                   const std::filesystem::path& data_dir) {
    auto target_commit = load_commit(target_hash);
    if (!target_commit) {
        std::cerr << "Error: Commit " << target_hash << " not found\n";
        return "";
    }

    // Restore target commit's files to disk
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_regular_file()) std::filesystem::remove(entry.path());
    }
    for (const auto& file_hash : target_commit->file_hashes) {
        size_t colon_pos = file_hash.find(':');
        if (colon_pos == std::string::npos) continue;
        std::string filename = file_hash.substr(0, colon_pos);
        std::string hash     = file_hash.substr(colon_pos + 1);
        if (!restore_file(hash, data_dir / filename)) {
            std::cerr << "Warning: Failed to restore " << filename << "\n";
        }
    }

    // Create a new commit on top of current HEAD (preserves history)
    return commit("Restored to " + target_hash, data_dir);
}

std::string GitStore::read_file_at_commit(const std::string& commit_hash,
                                          const std::string& filename) {
    auto commit = load_commit(commit_hash);
    if (!commit) {
        std::cerr << "Error: Commit " << commit_hash << " not found\n";
        return "";
    }

    std::string object_hash;
    for (const auto& fh : commit->file_hashes) {
        size_t colon_pos = fh.find(':');
        if (colon_pos == std::string::npos) continue;
        if (fh.substr(0, colon_pos) == filename) {
            object_hash = fh.substr(colon_pos + 1);
            break;
        }
    }
    if (object_hash.empty()) return "";

    std::filesystem::path obj_path = objects_dir_ / object_hash;
    if (!std::filesystem::exists(obj_path)) {
        std::cerr << "Error: Object " << object_hash << " not found in store\n";
        return "";
    }
    std::ifstream obj_file(obj_path, std::ios::binary);
    std::stringstream buffer;
    buffer << obj_file.rdbuf();
    return buffer.str();
}

// ---------------------------------------------------------------------------
// Public API — Branches
// ---------------------------------------------------------------------------

bool GitStore::create_branch(const std::string& name) {
    if (std::filesystem::exists(refs_dir_ / name)) {
        std::cerr << "Error: Branch '" << name << "' already exists\n";
        return false;
    }

    auto head = get_head();
    std::ofstream file(refs_dir_ / name);
    if (!file.is_open()) return false;
    if (head) file << *head;

    std::cout << "Created branch '" << name << "'";
    if (head) std::cout << " at commit " << *head;
    std::cout << "\n";
    return true;
}

bool GitStore::switch_branch(const std::string& name,
                             const std::filesystem::path& data_dir) {
    if (!std::filesystem::exists(refs_dir_ / name)) {
        std::cerr << "Error: Branch '" << name << "' does not exist. "
                  << "Use 'vsdb branch -b " << name << "' to create it.\n";
        return false;
    }

    auto branch_hash = read_branch_ref(name);

    // Clear data directory to prevent leaking files from the previous branch
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_regular_file()) std::filesystem::remove(entry.path());
    }

    if (branch_hash) {
        // Restore the branch's latest commit
        auto commit = load_commit(*branch_hash);
        if (commit) {
            for (const auto& file_hash : commit->file_hashes) {
                size_t colon_pos = file_hash.find(':');
                if (colon_pos == std::string::npos) continue;
                std::string filename = file_hash.substr(0, colon_pos);
                std::string hash     = file_hash.substr(colon_pos + 1);
                if (!restore_file(hash, data_dir / filename)) {
                    std::cerr << "Warning: Failed to restore " << filename << "\n";
                }
            }
        }
    }

    update_head_ref(name);

    std::cout << "Switched to branch '" << name << "'";
    if (branch_hash) std::cout << " (at commit " << *branch_hash << ")";
    std::cout << "\n";
    return true;
}

std::vector<std::pair<std::string, bool>> GitStore::list_branches() const {
    std::vector<std::pair<std::string, bool>> branches;
    std::string current = get_current_branch();

    if (!std::filesystem::exists(refs_dir_)) return branches;

    for (const auto& entry : std::filesystem::directory_iterator(refs_dir_)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            branches.push_back({name, name == current});
        }
    }

    std::sort(branches.begin(), branches.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return branches;
}

} // namespace vsdb