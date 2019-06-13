/*-
 * Copyright (c) 2019 TAO Zhijiang<taozhijiang@gmail.com>
 *
 * Licensed under the BSD-3-Clause license, see LICENSE for full information.
 *
 */

#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <leveldb/comparator.h>

#include <other/Log.h>
#include <other/FilesystemUtil.h>
#include <message/ProtoBuf.h>

#include <Paxos/LevelDBStore.h>


namespace rong {


LevelDBStore::LevelDBStore(const std::string& db_path, const std::string& snapshot_path) :
    kv_path_(db_path),
    snapshot_path_(snapshot_path),
    kv_fp_() {

    leveldb::Options options;
    options.create_if_missing = true;
    // options.block_cache = leveldb::NewLRUCache(100 * 1048576);  // 100MB cache
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, kv_path_, &db);

    if (!status.ok()) {
        roo::log_err("Open levelDB %s failed.", kv_path_.c_str());
        throw roo::ConstructException("Open levelDB failed.");
    }

    kv_fp_.reset(db);

    roo::FilesystemUtil::mkdir(snapshot_path_);

    // 检查快照目录是否存在，不存在则创建之
    if (!roo::FilesystemUtil::accessable(snapshot_path_, R_OK | W_OK | X_OK) ||
        !roo::FilesystemUtil::is_directory(snapshot_path_)) {
        std::string msg = "Prepare snapshot directory failed: " + snapshot_path_;
        throw roo::ConstructException(msg.c_str());
    }
}

LevelDBStore::~LevelDBStore() {
    kv_fp_.reset();
}


int LevelDBStore::select_handle(const Client::StateMachineSelectOps::Request& request,
                                Client::StateMachineSelectOps::Response& response) const {

    int result = 0;

    do {
        if (request.has_read()) {

            std::string content;
            if (get(request.read().key(), content) == 0) {
                response.mutable_read()->set_content(content);
            } else {
                roo::log_err("Read key %s failed.", request.read().key().c_str());
                result = -1;
            }
            break;

        } else if (request.has_range()) {

            std::vector<std::string> range_store;
            std::string start_key   = request.range().start_key();
            std::string end_key     = request.range().end_key();
            uint64_t limit          = request.range().limit();
            result = range(start_key, end_key, limit, range_store);
            if (result != 0) {
                roo::log_err("Rang for key start %s, end %s, limit %lu failed.", start_key.c_str(), end_key.c_str(), limit);
                break;
            }

            *response.mutable_range()->mutable_contents()
                = { range_store.begin(), range_store.end() };
            break;

        } else if (request.has_search()) {

            std::vector<std::string> search_store;
            std::string search_key = request.search().search_key();
            uint64_t limit = request.search().limit();
            result = search(search_key, limit, search_store);
            if (result != 0) {
                roo::log_err("Search for key %s, limit %lu failed.", search_key.c_str(), limit);
                break;
            }

            *response.mutable_search()->mutable_contents()
                = { search_store.begin(), search_store.end() };
            break;

        }

        roo::log_err("Unknown operations for StateMachineSelectOps.");
        result = -1;

    } while (0);

    if (result == 0) {
        response.set_code(0);
        response.set_msg("OK");
    } else {
        response.set_code(result);
        response.set_msg("FAIL");
    }

    return result;
}


int LevelDBStore::update_handle(const Client::StateMachineUpdateOps::Request& request) const {


    if (request.has_write()) {

        roo::log_info("update_handle write for key %s, value %s.",
                      request.write().key().c_str(), request.write().content().c_str());

        if (set(request.write().key(), request.write().content()) == 0)
            return 0;

        roo::log_err("Write %s = %s failed.", request.write().key().c_str(), request.write().content().c_str());
        return -1;


    }

    if (request.has_remove()) {

        roo::log_info("update_handle remove for key %s.",
                      request.remove().key().c_str());
        if (del(request.remove().key()) == 0)
            return 0;

        roo::log_err("Delete key %s failed.", request.remove().key().c_str());
        return -1;

    }


    roo::log_err("Unknown operations for StateMachineUpdateOps.");
    return -1;
}


int LevelDBStore::get(const std::string& key, std::string& val) const {

    boost::shared_lock<boost::shared_mutex> rlock(kv_lock_);

    leveldb::Status status = kv_fp_->Get(leveldb::ReadOptions(), key, &val);
    if (status.ok()) return 0;

    return -1;
}


int LevelDBStore::set(const std::string& key, const std::string& val) const {

    boost::unique_lock<boost::shared_mutex> wlock(kv_lock_);

    leveldb::WriteOptions options;
    options.sync = true;

    leveldb::Status status = kv_fp_->Put(options, key, val);
    if (status.ok()) return 0;

    return -1;
}

int LevelDBStore::del(const std::string& key) const {

    boost::unique_lock<boost::shared_mutex> wlock(kv_lock_);

    leveldb::WriteOptions options;
    options.sync = true;

    leveldb::Status status = kv_fp_->Delete(leveldb::WriteOptions(), key);
    if (status.ok()) return 0;

    return -1;
}


int LevelDBStore::range(const std::string& start, const std::string& end, uint64_t limit,
                        std::vector<std::string>& range_store) const {

    boost::shared_lock<boost::shared_mutex> rlock(kv_lock_);

    std::unique_ptr<leveldb::Iterator> it(kv_fp_->NewIterator(leveldb::ReadOptions()));
    uint64_t count = 0;
    leveldb::Options options;

    it->SeekToFirst();
    if (!start.empty())
        it->Seek(start);

    for (/* */; it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string key_str = key.ToString();

        // leveldb::Slice value = it->value();
        // std::string val_str = value.ToString();

        if (limit && ++count > limit) {
            break;
        }

        if (!end.empty() && options.comparator->Compare(key, end) > 0) {
            break;
        }

        range_store.push_back(key_str);
    }

    return 0;
}


int LevelDBStore::search(const std::string& search_key, uint64_t limit,
                         std::vector<std::string>& search_store) const {

    boost::shared_lock<boost::shared_mutex> rlock(kv_lock_);

    std::unique_ptr<leveldb::Iterator> it(kv_fp_->NewIterator(leveldb::ReadOptions()));
    uint64_t count = 0;
    leveldb::Options options;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        std::string key_str = key.ToString();

        // leveldb::Slice value = it->value();
        // std::string val_str = value.ToString();

        if (key_str.find(search_key) != std::string::npos) {
            search_store.push_back(key_str);
        }

        if (limit && ++count > limit) {
            break;
        }
    }

    return 0;
}

// 任何时候操作的临时文件
static const std::string snapshot_temp_file = "state_machine_snapshot.temp";

// 服务本地创建的快照文件
static const std::string snapshot_self_file = "state_machine_snapshot.self";


bool LevelDBStore::create_snapshot(uint64_t last_included_index, uint64_t last_included_term) {

    Snapshot::SnapshotContent snapshot{};
    snapshot.mutable_meta()->set_last_included_index(last_included_index);
    snapshot.mutable_meta()->set_last_included_term(last_included_term);

    boost::shared_lock<boost::shared_mutex> rlock(kv_lock_);

    std::unique_ptr<leveldb::Iterator> it(kv_fp_->NewIterator(leveldb::ReadOptions()));
    uint64_t count = 0;

    Snapshot::KeyValue* ptr;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {

        leveldb::Slice key = it->key();
        leveldb::Slice value = it->value();

        ptr = snapshot.add_data();
        ptr->set_key(key.ToString());
        ptr->set_value(value.ToString());

        ++count;
    }

    std::string content;
    if (!roo::ProtoBuf::marshalling_to_string(snapshot, &content)) {
        roo::log_err("marshalling_to_string of snapshot failed.");
        return false;
    }

    std::string full_tmp = snapshot_path_ + snapshot_temp_file;
    if (roo::FilesystemUtil::write_file(full_tmp, content) == 0) {
        roo::log_warning("snapshot %lu count item, dump to %s successfully!", count, full_tmp.c_str());

        // 正式文件
        std::string self_full = snapshot_path_ + snapshot_self_file;
        ::rename(full_tmp.c_str(), self_full.c_str());
        roo::log_warning("commit to official snapshot file: %s", self_full.c_str());
        return true;
    }

    roo::log_err("snapshot write filesystem failed: %s.", full_tmp.c_str());
    return false;
}


bool LevelDBStore::load_snapshot(std::string& content, uint64_t& last_included_index, uint64_t& last_included_term) {

    Snapshot::SnapshotContent snapshot;

    std::string self_full = snapshot_path_ + snapshot_self_file;
    if (roo::FilesystemUtil::read_file(self_full, content) != 0) {
        roo::log_err("Load snapshot content from %s failed.", self_full.c_str());
        return false;
    }

    if (!roo::ProtoBuf::unmarshalling_from_string(content, &snapshot)) {
        roo::log_err("unmarshalling snapshot failed, may content currputed.");
        return false;
    }

    last_included_index = snapshot.meta().last_included_index();
    last_included_term  = snapshot.meta().last_included_term();
    roo::log_warning("load_snapshot, last_included term %lu and index %lu.",
                     last_included_term, last_included_index);

    return true;
}


bool LevelDBStore::apply_snapshot(const Snapshot::SnapshotContent& snapshot) {

    boost::unique_lock<boost::shared_mutex> wlock(kv_lock_);

    // destroy levelDB completely first
    leveldb::DestroyDB(kv_path_, leveldb::Options());

#if 0
    kv_fp_.reset();

    roo::log_warning("unlink database %s.", kv_path_.c_str());
    ::rename(kv_path_.c_str(), (kv_path_+".old").c_str());

    leveldb::Options create_options;
    create_options.error_if_exists = true;
    create_options.create_if_missing = true;
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(create_options, kv_path_, &db);

    if (!status.ok()) {
        PANIC("Reset LevelDB error: %s", kv_path_.c_str());
    }
    kv_fp_.reset(db);

#endif

    roo::log_warning("Store will apply_snapshot, last_included index %lu and term %lu.",
                     snapshot.meta().last_included_index(), snapshot.meta().last_included_term());

    leveldb::WriteBatch batch;
    for (auto iter = snapshot.data().begin(); iter != snapshot.data().end(); ++iter) {
        batch.Put(iter->key(), iter->value());
    }

    leveldb::Status status = kv_fp_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
        roo::log_err("Restoring snapshot failed, give up...");
        return false;
    }

    // 回滚 kv_path_.old ??

    return true;
}



} // namespace rong
