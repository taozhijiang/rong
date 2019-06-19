### Rong

This is a simple Paxos consensus algorithm implementation and used for stable KV store purpose.   

The core consensus algorithm implementation is mainly inspired by [keyspace](https://github.com/scalien/keyspace).   
As we know that Paxos is notoriously difficult to understand and to implement, much more work need to be done... 


#### How to startup
```bash
# prepare the log and storage directory
~ mkdir log storage

# start the cluster to work
~ bin/rong_service_10901 -c ../rong_example_10901.conf -d
~ bin/rong_service_10902 -c ../rong_example_10902.conf -d
~ bin/rong_service_10903 -c ../rong_example_10903.conf -d

# client set and get
~ bin/clientOps set key val
~ bin/clientOps get key

# client simple performance 
~ bin/clientPerf set 1
```

#### NOTE
1. In order to simplify the implementation, we allow only one-instance executing, for multi-instance executing by window method is hard to synchronize, and the same as crash recovery. New client request must check whether max-InstanceID and log matching, so when an instance is not finished yet but its Proposer is crashed, there will be a pending instance and a GAP between max-InstanceID and log, then the cluster will not be able to handle client request any more. These will be fixed latter.   
2. Currently PaxosLease is not implement for master selecting, which is very important for Multi-Paxos optimization.   