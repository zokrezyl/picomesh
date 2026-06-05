#!/usr/bin/env python3
# Max-throughput driver for an isolated relational_storage node. Spawns N worker
# PROCESSES (real parallelism — not GIL-bound threads), each with its own
# keep-alive connection + db handle, looping a realistic write+read mix against
# the product `namespaces`/`repos` tables via the control /invoke. Reports
# aggregate ops/s and a write/read split so a relational bottleneck is obvious.
import http.client, sys, time, json
from multiprocessing import Process, Queue

HOST = "127.0.0.1"
PORT = int(sys.argv[1])
NPROC = int(sys.argv[2]) if len(sys.argv) > 2 else 8
DUR = float(sys.argv[3]) if len(sys.argv) > 3 else 15.0

def worker(wid, q):
    c = http.client.HTTPConnection(HOST, PORT)
    # Stateless path-based /_rpc (bridge mode): the node creates+invokes+releases
    # the object per call — no per-worker handle, so it's safe across the node's
    # SO_REUSEPORT workers. This is the same call shape the mesh uses internally.
    def rpc(path, args):
        body = json.dumps({"path": path, "args": args})
        c.request("POST", "/_rpc", body, {"Content-Type": "application/json"})
        r = c.getresponse(); data = r.read()
        return r.status == 200 and b'"error"' not in data

    base = wid * 100_000_000
    writes = reads = werr = rerr = 0
    t_write = t_read = 0.0
    n = 0
    end = time.time() + DUR
    INS = "INSERT INTO repos(id,namespace_id,name,owner_uid,visibility,created_at) VALUES(?,?,?,?,?,?)"
    SEL = "SELECT name,owner_uid FROM repos WHERE id=?"
    while time.time() < end:
        rid = base + n; n += 1
        shard = rid                      # engine does shard_id % N
        # write: a repo row (the hot product write)
        t0 = time.time()
        ok = rpc("relational_storage.db.exec",
                 [shard, INS, json.dumps([rid, wid, f"r{wid}_{n}", wid, "private", 0])])
        t_write += time.time() - t0; writes += 1; werr += 0 if ok else 1
        # read: fetch it straight back (PK lookup, like an owner/visibility check)
        t0 = time.time()
        ok = rpc("relational_storage.db.query", [shard, SEL, json.dumps([rid])])
        t_read += time.time() - t0; reads += 1; rerr += 0 if ok else 1
    q.put((writes, reads, werr, rerr, t_write, t_read))

def main():
    q = Queue()
    procs = [Process(target=worker, args=(i, q)) for i in range(NPROC)]
    t0 = time.time()
    for p in procs: p.start()
    for p in procs: p.join()
    dt = time.time() - t0
    W = R = WE = RE = TW = TR = 0
    for _ in procs:
        w, r, we, re_, tw, tr = q.get()
        W += w; R += r; WE += we; RE += re_; TW += tw; TR += tr
    total = W + R
    print(f"relational_storage  conns={NPROC}  wall={dt:.1f}s")
    print(f"  THROUGHPUT : {total/dt:,.0f} ops/s   ({W/dt:,.0f} writes/s + {R/dt:,.0f} reads/s)")
    print(f"  errors     : write {WE} ({100*WE/max(W,1):.1f}%)   read {RE} ({100*RE/max(R,1):.1f}%)")
    print(f"  mean lat   : write {1000*TW/max(W,1):.2f} ms   read {1000*TR/max(R,1):.2f} ms")

if __name__ == "__main__":
    main()
