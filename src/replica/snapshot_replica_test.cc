//
// snapshot_replica_test.cc
// Copyright (C) 2017 4paradigm.com
// Author denglong
// Date 2017-08-16
//

#include "replica/log_replicator.h"
#include "replica/replicate_node.h"
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include "proto/tablet.pb.h"
#include "logging.h"
#include "thread_pool.h"
#include <brpc/server.h>
#include "storage/table.h"
#include "storage/segment.h"
#include "storage/ticket.h"
#include "timer.h"
#include "tablet/tablet_impl.h"
#include "client/tablet_client.h"
#include <gflags/gflags.h>
#include "base/file_util.h"

using ::baidu::common::ThreadPool;
using ::rtidb::storage::Table;
using ::rtidb::storage::Ticket;
using ::rtidb::storage::DataBlock;
using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;
using ::baidu::common::INFO;
using ::baidu::common::DEBUG;
using ::rtidb::tablet::TabletImpl;

DECLARE_string(db_root_path);
DECLARE_string(endpoint);

inline std::string GenRand() {
    return std::to_string(rand() % 10000000 + 1);
}

namespace rtidb {
namespace replica {

class MockClosure : public ::google::protobuf::Closure {

public:
    MockClosure() {}
    ~MockClosure() {}
    void Run() {}

};

class SnapshotReplicaTest : public ::testing::Test {

public:
    SnapshotReplicaTest() {}

    ~SnapshotReplicaTest() {}
};

TEST_F(SnapshotReplicaTest, AddReplicate) {
    ::rtidb::tablet::TabletImpl* tablet = new ::rtidb::tablet::TabletImpl();
    tablet->Init();
    brpc::Server server;
    if (server.AddService(tablet, brpc::SERVER_OWNS_SERVICE) != 0) {
       PDLOG(WARNING, "fail to register tablet rpc service");
       exit(1);
    }
    brpc::ServerOptions options;
    std::string leader_point = "127.0.0.1:18529";
    if (server.Start(leader_point.c_str(), &options) != 0) {
        PDLOG(WARNING, "fail to start server %s", leader_point.c_str());
        exit(1);
    }

    uint32_t tid = 2;
    uint32_t pid = 123;

    ::rtidb::client::TabletClient client(leader_point);
    client.Init();
    std::vector<std::string> endpoints;
    bool ret = client.CreateTable("table1", tid, pid, 100000, true, endpoints, 
                ::rtidb::api::TTLType::kAbsoluteTime, 16, 0, ::rtidb::api::CompressType::kNoCompress);
    ASSERT_TRUE(ret);

    std::string end_point = "127.0.0.1:18530";
    ret = client.AddReplica(tid, pid, end_point);
    ASSERT_TRUE(ret);
    sleep(1);

    ::rtidb::api::TableStatus table_status;
    if (client.GetTableStatus(tid, pid, table_status) < 0) {
        ASSERT_TRUE(0);
    }
    ASSERT_EQ(::rtidb::api::kTableNormal, table_status.state());

    ret = client.DelReplica(tid, pid, end_point);
    ASSERT_TRUE(ret);
}

TEST_F(SnapshotReplicaTest, LeaderAndFollower) {
    ::rtidb::tablet::TabletImpl* tablet = new ::rtidb::tablet::TabletImpl();
    tablet->Init();
    brpc::Server server;
    if (server.AddService(tablet, brpc::SERVER_OWNS_SERVICE) != 0) {
       PDLOG(WARNING, "fail to register tablet rpc service");
       exit(1);
    }
    brpc::ServerOptions options;
    std::string leader_point = "127.0.0.1:18529";
    if (server.Start(leader_point.c_str(), &options) != 0) {
        PDLOG(WARNING, "fail to start server %s", leader_point.c_str());
        exit(1);
    }
    //server.RunUntilAskedToQuit();

    uint32_t tid = 1;
    uint32_t pid = 123;

    ::rtidb::client::TabletClient client(leader_point);
    client.Init();
    std::vector<std::string> endpoints;
    bool ret = client.CreateTable("table1", tid, pid, 100000, true, endpoints,
                ::rtidb::api::TTLType::kAbsoluteTime, 16, 0, ::rtidb::api::CompressType::kNoCompress);
    ASSERT_TRUE(ret);
    uint64_t cur_time = ::baidu::common::timer::get_micros() / 1000;
    ret = client.Put(tid, pid, "testkey", cur_time, "value1");
    ASSERT_TRUE(ret);

    uint32_t count = 0;
    while (count < 10) {
        count++;
        char key[100];
        snprintf(key, 100, "test%u", count);
        client.Put(tid, pid, key, cur_time, key);
    }

    FLAGS_db_root_path = "/tmp/" + ::GenRand();
    FLAGS_endpoint = "127.0.0.1:18530";
    ::rtidb::tablet::TabletImpl* tablet1 = new ::rtidb::tablet::TabletImpl();
    tablet1->Init();
    brpc::Server server1;
    if (server1.AddService(tablet1, brpc::SERVER_OWNS_SERVICE) != 0) {
       PDLOG(WARNING, "fail to register tablet rpc service");
       exit(1);
    }
    std::string follower_point = "127.0.0.1:18530";
    if (server1.Start(follower_point.c_str(), &options) != 0) {
        PDLOG(WARNING, "fail to start server %s", follower_point.c_str());
        exit(1);
    }
    //server.RunUntilAskedToQuit();
    ::rtidb::client::TabletClient client1(follower_point);
    client1.Init();
    ret = client1.CreateTable("table1", tid, pid, 14400, false, endpoints,
                ::rtidb::api::TTLType::kAbsoluteTime, 16, 0, ::rtidb::api::CompressType::kNoCompress);
    ASSERT_TRUE(ret);
    client.AddReplica(tid, pid, follower_point);
    sleep(3);
	
	::rtidb::api::ScanRequest sr;
	MockClosure closure;
    sr.set_tid(tid);
    sr.set_pid(pid);
    sr.set_pk("testkey");
    sr.set_st(cur_time + 1);
    sr.set_et(cur_time - 1);
    sr.set_limit(10);
    ::rtidb::api::ScanResponse srp;
    tablet1->Scan(NULL, &sr, &srp, &closure);
    ASSERT_EQ(1, srp.count());
    ASSERT_EQ(0, srp.code());
	
    ret = client.Put(tid, pid, "newkey", cur_time, "value2");
    ASSERT_TRUE(ret);
	sleep(2);
    sr.set_pk("newkey");
    tablet1->Scan(NULL, &sr, &srp, &closure);
    ASSERT_EQ(1, srp.count());
    ASSERT_EQ(0, srp.code());
    {
        ::rtidb::api::GetTableFollowerRequest gr;
        ::rtidb::api::GetTableFollowerResponse grs;
        gr.set_tid(tid);
        gr.set_pid(pid);
        tablet->GetTableFollower(NULL, &gr, &grs, &closure);
        ASSERT_EQ(0, grs.code());
        ASSERT_EQ(12, grs.offset());
        ASSERT_EQ(1, grs.follower_info_size());
        ASSERT_STREQ(follower_point.c_str(), grs.follower_info(0).endpoint().c_str());
        ASSERT_EQ(12, grs.follower_info(0).offset());

    }
    ::rtidb::api::DropTableRequest dr;
    dr.set_tid(tid);
    dr.set_pid(pid);
    ::rtidb::api::DropTableResponse drs;
    tablet->DropTable(NULL, &dr, &drs, &closure);
    sleep(2);
}

}
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    srand (time(NULL));
    ::baidu::common::SetLogLevel(::baidu::common::INFO);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    FLAGS_db_root_path = "/tmp/" + ::GenRand();
    return RUN_ALL_TESTS();
}

