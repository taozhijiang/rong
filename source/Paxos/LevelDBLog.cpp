/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

#include <system/ConstructException.h>
#include <other/Log.h>
#include <string/StrUtil.h>
#include <string/Endian.h>

#include <Paxos/LevelDBLog.h>


namespace rong {

const char* const META_RESTART_COUNTER      = "META_RESTART_COUNTER";
const char* const META_HIGHEST_INSTANCE_ID  = "META_HIGHEST_INSTANCE_ID";

const char* const META_INSTANCE_ID          = "META_INSTANCE_ID";
const char* const META_ACCEPTED             = "META_ACCEPTED";
const char* const META_PROMISED_PROPOSAL_ID = "META_PROMISED_PROPOSAL_ID";
const char* const META_ACCEPTED_PROPOSAL_ID = "META_ACCEPTED_PROPOSAL_ID";
const char* const META_ACCEPTED_VALUE       = "META_ACCEPTED_VALUE";

const char* const META_APPLY_INSTANCE_ID    = "META_APPLY_INSTANCE_ID";

using roo::Endian;

LevelDBLog::LevelDBLog(const std::string& path) :
    start_index_(1),
    last_index_(0),
    log_mutex_(),
    log_meta_path_(path),
    log_meta_fp_() {

    leveldb::Options options;
    options.create_if_missing = true;
    // options.block_cache = leveldb::NewLRUCache(100 * 1048576);  // 100MB cache
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, log_meta_path_, &db);

    if (!status.ok()) {
        roo::log_err("Open levelDB %s failed.", log_meta_path_.c_str());
        throw roo::ConstructException("Open levelDB failed.");
    }

    log_meta_fp_.reset(db);

    // skip meta, and get start_index_, last_index_
    std::unique_ptr<leveldb::Iterator> it(log_meta_fp_->NewIterator(leveldb::ReadOptions()));

    it->SeekToFirst();
    if (it->Valid()) {
        std::string key = it->key().ToString();
        if (key.find("META_") == std::string::npos) {
            start_index_ = Endian::uint64_from_net(key);
            roo::log_debug("Try seek and found start_index with %lu.", start_index_);
        }
    }

    it->SeekToLast();
    while (it->Valid()) {

        std::string key = it->key().ToString();
        // 元数据，这边跳过
        if (key.find("META_") != std::string::npos) {
            it->Prev();
            continue;
        }

        last_index_ = Endian::uint64_from_net(key);
        roo::log_debug("Try seek and found last_index %lu.", last_index_);
        break;
    }

    if (last_index_ != 0 && last_index_ < start_index_) {
        std::string message = roo::va_format("Invalid start_index %lu and last_index %lu", start_index_, last_index_);
        throw roo::ConstructException(message.c_str());
    }

    roo::log_warning("final start_index %lu, last_index %lu", start_index_, last_index_);
}

LevelDBLog::~LevelDBLog() {
    log_meta_fp_.reset();
}

uint64_t LevelDBLog::append(uint64_t index, const EntryPtr& newEntry) {

    std::lock_guard<std::mutex> lock(log_mutex_);

    // GAP ...
    if (index < last_index_ + 1) {
        roo::log_info("fast return with index %lu, last_index %lu.", index, last_index_);
        return last_index_;
    }

    if (index > last_index_ + 1) {
        PANIC("LearnLog GAP found, index %lu and last_index+1 %lu.", index, last_index_ + 1);
    }

    // index == last_index_ + 1

    std::string buf;
    newEntry->SerializeToString(&buf);
    last_index_++;
    log_meta_fp_->Put(leveldb::WriteOptions(), Endian::uint64_to_net(last_index_), buf);

    return last_index_;
}

LevelDBLog::EntryPtr LevelDBLog::entry(uint64_t index) const {

    std::string val;
    leveldb::Status status = log_meta_fp_->Get(leveldb::ReadOptions(), Endian::uint64_to_net(index), &val);
    if (!status.ok()) {
        roo::log_err("Read entry at index %lu failed.", index);
        return EntryPtr();
    }

    EntryPtr entry = std::make_shared<Entry>();
    entry->ParseFromString(val);
    return entry;
}

bool LevelDBLog::entries(uint64_t start, std::vector<EntryPtr>& entries, uint64_t limit) const {

    std::lock_guard<std::mutex> lock(log_mutex_);

    if (start < start_index_)
        start = start_index_;

    // 在peer和leader日志一致的时候，就会是这种情况
    if (start > last_index_) {
        return true;
    }

    uint64_t count = 0;
    for (; start <= last_index_; ++start) {
        std::string val;
        leveldb::Status status = log_meta_fp_->Get(leveldb::ReadOptions(), Endian::uint64_to_net(start), &val);
        if (!status.ok()) {
            roo::log_err("Read entries at index %lu failed.", start);
            return false;
        }

        EntryPtr entry = std::make_shared<Entry>();
        entry->ParseFromString(val);
        entries.emplace_back(entry);

        if (limit != 0 && ++count > limit)
            break;
    }

    return true;
}

// Delete the log entries before the given index.
// After this call, the log will contain no entries indexed less than start_index
void LevelDBLog::truncate_prefix(uint64_t start_index) {

    std::lock_guard<std::mutex> lock(log_mutex_);

    if (start_index <= start_index_)
        return;

    leveldb::WriteBatch batch;
    for (; start_index_ < start_index; ++start_index_) {
        batch.Delete(Endian::uint64_to_net(start_index_));
    }

    leveldb::Status status = log_meta_fp_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        roo::log_err("TruncatePrefix log entries from index %lu failed.", start_index);
    }

    if (last_index_ < start_index_ - 1)
        last_index_ = start_index_ - 1;

    roo::log_info("TruncatePrefix at %lu finished, current start_index %lu, last_index %lu.",
                  start_index, start_index_, last_index_);
}

// Delete the log entries past the given index.
// After this call, the log will contain no entries indexed greater than last_index.
void LevelDBLog::truncate_suffix(uint64_t last_index) {

    std::lock_guard<std::mutex> lock(log_mutex_);

    if (last_index >= last_index_)
        return;

    leveldb::WriteBatch batch;
    for (; last_index_ > last_index; --last_index_) {
        batch.Delete(Endian::uint64_to_net(last_index_));
    }

    leveldb::Status status = log_meta_fp_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        roo::log_err("TruncateSuffix log entries from last index %lu failed.", last_index);
    }

    if (last_index_ < start_index_ - 1)
        last_index_ = start_index_ - 1;

    roo::log_info("Truncatesuffix at %lu finished, current start_index %lu, last_index %lu.",
                  last_index, start_index_, last_index_);
}



int LevelDBLog::meta_data(LogMeta* meta_data) const {

    std::string val;
    leveldb::Status status;

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_RESTART_COUNTER, &val);
    if (status.ok())
        meta_data->set_restart_counter(Endian::uint64_from_net(val));

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_HIGHEST_INSTANCE_ID, &val);
    if (status.ok())
        meta_data->set_highest_instance_id(Endian::uint64_from_net(val));

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_INSTANCE_ID, &val);
    if (status.ok())
        meta_data->set_instance_id(Endian::uint64_from_net(val));

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_ACCEPTED, &val);
    if (status.ok())
        meta_data->set_accepted(Endian::uint64_from_net(val) != 0);

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_PROMISED_PROPOSAL_ID, &val);
    if (status.ok())
        meta_data->set_accepted_proposal_id(Endian::uint64_from_net(val));

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_ACCEPTED_PROPOSAL_ID, &val);
    if (status.ok())
        meta_data->set_accepted_proposal_id(Endian::uint64_from_net(val));

    status = log_meta_fp_->Get(leveldb::ReadOptions(), META_ACCEPTED_VALUE, &val);
    if (status.ok())
        meta_data->set_accepted_value(val);

    return 0;
}


int LevelDBLog::set_meta_data(const LogMeta& meta) const {

    if (!meta.has_restart_counter() && !meta.has_highest_instance_id() &&
        !meta.has_instance_id() &&
        !meta.has_accepted() && !meta.has_promised_proposal_id() &&
        !meta.has_accepted_proposal_id() && !meta.has_accepted_value())
        return -1;

    leveldb::WriteBatch batch;
    if (meta.has_restart_counter())
        batch.Put(META_RESTART_COUNTER, Endian::uint64_to_net(meta.restart_counter()));
    if (meta.has_highest_instance_id())
        batch.Put(META_HIGHEST_INSTANCE_ID, Endian::uint64_to_net(meta.highest_instance_id()));
    if (meta.has_instance_id())
        batch.Put(META_INSTANCE_ID, Endian::uint64_to_net(meta.instance_id()));
    if (meta.has_accepted())
        batch.Put(META_ACCEPTED, Endian::uint64_to_net(meta.accepted() ? 1 : 0));
    if (meta.has_promised_proposal_id())
        batch.Put(META_PROMISED_PROPOSAL_ID, Endian::uint64_to_net(meta.promised_proposal_id()));
    if (meta.has_accepted_proposal_id())
        batch.Put(META_ACCEPTED_PROPOSAL_ID,  Endian::uint64_to_net(meta.accepted_proposal_id()));
    if (meta.has_accepted_value())
        batch.Put(META_ACCEPTED_VALUE, meta.accepted_value());

    leveldb::Status status = log_meta_fp_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        roo::log_err("Update Meta data failed.");
        roo::log_err("info: %s %lu, %s %lu, %s %lu, %s %lu, %s %lu, %s %lu, %s %s.",
                     META_RESTART_COUNTER, meta.restart_counter(),
                     META_HIGHEST_INSTANCE_ID, meta.highest_instance_id(),
                     META_INSTANCE_ID, meta.instance_id(), META_ACCEPTED, meta.accepted() ? 1UL : 0UL,
                     META_PROMISED_PROPOSAL_ID, meta.promised_proposal_id(),
                     META_ACCEPTED_PROPOSAL_ID, meta.accepted_proposal_id(),
                     META_ACCEPTED_VALUE, meta.accepted_value().c_str());
        return -1;
    }

    return 0;
}

uint64_t LevelDBLog::meta_apply_instance_id() const {

    std::string val;
    leveldb::Status status
        = log_meta_fp_->Get(leveldb::ReadOptions(), META_APPLY_INSTANCE_ID, &val);
    if (status.ok())
        return Endian::uint64_from_net(val);

    return 0;
}

int LevelDBLog::set_meta_apply_instance_id(uint64_t instance_id) const {

    leveldb::Status status =
        log_meta_fp_->Put(leveldb::WriteOptions(),
                          META_APPLY_INSTANCE_ID, Endian::uint64_to_net(instance_id));
    if (!status.ok()) {
        roo::log_err("Update Meta set %s = %lu failed.", META_APPLY_INSTANCE_ID, instance_id);
        return -1;
    }
    return 0;
}

uint64_t LevelDBLog::meta_highest_instance_id() const {

    std::string val;
    leveldb::Status status
        = log_meta_fp_->Get(leveldb::ReadOptions(), META_HIGHEST_INSTANCE_ID, &val);
    if (status.ok())
        return Endian::uint64_from_net(val);

    return 0;
}

int LevelDBLog::set_meta_highest_instance_id(uint64_t instance_id) const {

    leveldb::Status status =
        log_meta_fp_->Put(leveldb::WriteOptions(),
                          META_HIGHEST_INSTANCE_ID, Endian::uint64_to_net(instance_id));
    if (!status.ok()) {
        roo::log_err("Update Meta set %s = %lu failed.", META_HIGHEST_INSTANCE_ID, instance_id);
        return -1;
    }
    return 0;
}

} // namespace rong
