#ifndef ERL_PERSISTENCE_HPP
#define ERL_PERSISTENCE_HPP

// Unit IV: binary persistence with atomic updates.
//
// File layout:  [FileHeader][record 0][record 1] ... [record n-1]
// saveAtomic() writes a complete new file next to the target (".tmp") and only then
// renames it over the target, so a crash can never leave a half written log behind.
// loadRecords() uses seekg/tellg to validate the file size before reading, and a
// checksum to detect corruption.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include "Exceptions.hpp"

namespace ReliefOps {
namespace Persistence {

const unsigned MAGIC = 0x45524C48u;   // "ERLH"

struct FileHeader {
    unsigned magic;
    unsigned recordSize;
    unsigned count;
    unsigned checksum;
};

inline unsigned checksumOf(const char *data, std::size_t n) {   // FNV-1a
    unsigned h = 2166136261u;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= static_cast<unsigned char>(data[i]);
        h *= 16777619u;
    }
    return h;
}

template <class Rec>
void saveAtomic(const std::string &path, const std::vector<Rec> &records) {
    static_assert(std::is_trivially_copyable<Rec>::value, "records must be plain data");
    const std::string tmp = path + ".tmp";
    const std::size_t bytes = sizeof(Rec) * records.size();

    {
        std::fstream out(tmp.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out)
            throw PersistenceException("cannot create " + tmp);

        FileHeader h;
        h.magic = MAGIC;
        h.recordSize = static_cast<unsigned>(sizeof(Rec));
        h.count = static_cast<unsigned>(records.size());
        h.checksum = checksumOf(reinterpret_cast<const char *>(records.data()), bytes);

        out.write(reinterpret_cast<const char *>(&h), sizeof(h));
        if (bytes > 0)
            out.write(reinterpret_cast<const char *>(records.data()), static_cast<std::streamsize>(bytes));
        out.flush();
        if (!out) {
            out.close();
            std::remove(tmp.c_str());
            throw PersistenceException("write failed for " + tmp);
        }
    }   // stream closed here, before the rename

    try {
        std::filesystem::rename(tmp, path);   // replaces the old file in one step
    } catch (const std::filesystem::filesystem_error &e) {
        std::remove(tmp.c_str());
        throw PersistenceException(std::string("rename failed: ") + e.what());
    }
}

template <class Rec>
std::vector<Rec> loadRecords(const std::string &path) {
    static_assert(std::is_trivially_copyable<Rec>::value, "records must be plain data");
    std::fstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in)
        throw PersistenceException("cannot open " + path);

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);

    FileHeader h;
    in.read(reinterpret_cast<char *>(&h), sizeof(h));
    if (!in || h.magic != MAGIC)
        throw PersistenceException(path + " is not an ERL-HUB file");
    if (h.recordSize != sizeof(Rec))
        throw PersistenceException(path + " holds a different record type");

    const std::streamoff expected = static_cast<std::streamoff>(sizeof(FileHeader)) +
                                    static_cast<std::streamoff>(h.count) * static_cast<std::streamoff>(sizeof(Rec));
    if (size != expected)
        throw PersistenceException(path + " is truncated or has extra bytes");

    std::vector<Rec> records(h.count);
    const std::size_t bytes = sizeof(Rec) * records.size();
    if (bytes > 0) {
        in.read(reinterpret_cast<char *>(records.data()), static_cast<std::streamsize>(bytes));
        if (!in)
            throw PersistenceException("short read from " + path);
    }
    if (checksumOf(reinterpret_cast<const char *>(records.data()), bytes) != h.checksum)
        throw PersistenceException("checksum mismatch in " + path);
    return records;
}

// Read the current log (if any), add one record, write everything back atomically.
template <class Rec>
void appendAtomic(const std::string &path, const Rec &record) {
    std::vector<Rec> all;
    if (std::filesystem::exists(path))
        all = loadRecords<Rec>(path);
    all.push_back(record);
    saveAtomic(path, all);
}

}  // namespace Persistence
}  // namespace ReliefOps

#endif
