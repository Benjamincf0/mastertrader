# Build-Your-Own Stock Exchange: Plan

> Play money only. Real exchanges are heavily regulated, so keep this a simulation.

## 1. Goal

Build, in C++ on AWS:

- Admins list tickers (e.g. `ACME`). Users have accounts holding play cash and shares.
- Users send buy and sell orders. The exchange matches them and publishes trades and prices.
- If the main server crashes, a standby takes over with **no lost or duplicated orders**.
- You can show **numbers** proving it is correct, fast, and resilient.

**Not in the essential plan** (see Section 10): web UI, sharding across servers, auctions, FIX protocol, SQL ledger.

## 2. How to read this plan

- Sections 3–6: background you need before coding.
- Section 7: **essential phases 0–9**. Do them in order. Each ends with "Done when". Don't move on until that is true.
- Section 10: **optional extras**. Ignore until the essentials work.
- Any number written here ("about 1 second", "tens of microseconds") is a target or guess to *test*, not a fact.

Rough effort, part-time, for a beginner:

| Phase | What | Effort |
|---|---|---|
| 0 | Setup | 2–3 days |
| 1 | Matching engine | 1–2 weeks |
| 2 | Prove it's correct | 1 week |
| 3 | Network pipeline (local) | 2–3 weeks |
| 4 | Measure (load generator) | 1 week |
| 5 | Deploy to AWS | 1 week |
| 6 | Basic tuning | 3–5 days |
| 7 | Journal and recovery | 1 week |
| 8 | Standby and failover | 2–3 weeks |
| 9 | Final evaluation | 1 week |

## 3. Glossary

| Term | Meaning |
|---|---|
| **Ticker / symbol** | Short name of a stock, e.g. `ACME`. |
| **Limit order** | "Buy up to 10 shares at $5.00 or lower." Waits in the book if it can't trade right away. |
| **Market order** | "Buy 10 shares now at whatever price is available." |
| **Order book** | All unfilled orders for one ticker. **Bids** (buys) on one side, **asks** (sells) on the other. |
| **Best bid / best ask** | Highest buy price / lowest sell price in the book. The gap between them is the **spread**. |
| **Price level** | All orders waiting at the same price. |
| **Match / trade** | A buy and a sell are compatible when buy price ≥ sell price. They trade at the price of the order that was **already waiting**. |
| **Price-time priority** | Better price goes first. At the same price, the earlier order goes first. |
| **Tick** | Smallest price step. We store prices as whole numbers of ticks (1 tick = $0.01, so $12.34 is `1234`). Decimals (floats) have rounding errors, so never use them for money. |
| **Sequencer** | Stamps every incoming message with the next number (1, 2, 3, …). This defines the one official order of events. |
| **Journal** | Append-only file of every sequenced message. |
| **Deterministic** | Same input gives exactly the same output, every time. |
| **Replay** | Rebuild the exchange's state by feeding the journal back in from the start. |
| **Primary / standby** | Two copies of the core server. The standby receives the same messages and stays in sync. If the primary dies, it takes over (**failover**). |
| **Split-brain** | Both servers think they're primary. Very dangerous. Prevented with an **epoch** (a "term number" that only goes up) and **fencing** (ignoring anything from an older epoch). |
| **Idempotent** | Sending the same message twice has the same effect as sending it once. Needed so retries don't double-submit orders. |
| **Latency** | Time for one order to make a round trip. Measured in microseconds (µs). 1 ms = 1000 µs. |
| **Throughput** | Orders per second. |
| **p50 / p99 / p99.9** | p99 = "99% of orders were at least this fast". Averages hide slow outliers, so we look at these. |
| **Ring buffer (SPSC queue)** | Fixed-size circular queue for passing messages from one thread to another without locks. SPSC = single producer, single consumer. |
| **Busy-spin / pinning** | A thread loops constantly checking for work instead of sleeping, and is locked to one CPU core. Lower and steadier latency, at the cost of 100% CPU. |
| **RTO / RPO** | Recovery Time Objective: how long the outage lasts. Recovery Point Objective: how much data you lose (goal: 0 acknowledged orders). |

## 4. Architecture

```
  Clients / bots / load generator
            │  TCP
            ▼
      ┌───────────┐
      │  Gateway  │  checks login + message format, relays replies and market data
      └─────┬─────┘
            ▼
 ┌─────────────────────────┐          ┌─────────────────────────┐
 │ PRIMARY core            │  same    │ STANDBY core            │
 │ sequencer → matching    │ ───────▶ │ same program, applies   │
 │ engine, journal file    │ messages │ the same messages       │
 └─────────────────────────┘          └─────────────────────────┘
            │
   DynamoDB: who is primary? (epoch number)
```

**Life of one order**

1. Client sends `NewOrder` to the gateway.
2. Gateway checks it and forwards it to the primary.
3. Primary numbers it (sequencer), writes it to the journal, and sends it to the standby.
4. The matching engine checks risk, matches, and produces events (`Accepted`, `Trade`, …).
5. Once the standby confirms it has the message, the primary releases the events.
6. Gateway delivers them to the right clients.

**Simplifications vs real exchanges** (write these down as known differences):
single AZ, one standby, TCP instead of multicast UDP, gateway also relays market data, play money.

## 5. Design rules

1. **One thread owns the order books.** No locks inside the engine. Speed comes from simplicity.
2. **The engine is deterministic.** It never reads the clock, never uses random numbers, never starts threads. Timestamps arrive *inside* messages (stamped by the sequencer). Don't let iteration order of `unordered_map` affect output.
3. **Everything that changes state goes through the sequencer and journal.** That includes listing a ticker, creating an account, and deposits, not just orders.
4. **No database in the order path.**
5. **Allocate memory at startup**, not per order. `new`/`malloc` can be slow and unpredictable.
6. **Integers only** for price, quantity, and cash.
7. **Measure before optimizing.** Record every change's before/after.

## 6. Tools and repo layout

**Tools**

| Tool | Used for |
|---|---|
| C++20, CMake, GCC or Clang | Build |
| GoogleTest | Unit tests |
| Google Benchmark | Speed of small pieces (e.g. the engine alone) |
| HdrHistogram (`HdrHistogram_c`) | Recording latency percentiles accurately |
| AddressSanitizer / UBSan / ThreadSanitizer | Compiler flags that catch memory bugs, undefined behavior, and thread races |
| Terraform | Create/destroy AWS infrastructure from code |
| `perf` | Find where CPU time goes (some hardware counters may be unavailable on EC2; sampling still works) |
| Python + matplotlib + boto3 | Graphs, scripts, and the small DynamoDB helper |

**Dev machine:** Linux, WSL2, or a Linux VM. Test logic anywhere; take performance numbers only from EC2.

**Reading:** Beej's Guide to Network Programming (sockets), HashiCorp's "Get Started – AWS" Terraform tutorial.

**Layout**

```
exchange/
  PLAN.md
  CMakeLists.txt
  docs/adr/        decision log (one short file per decision)
  src/
    common/        types, messages, ring buffer, hash
    engine/        order book, matching, accounts   (NO I/O, NO clock, NO threads)
    core/          sequencer, journal, replication, primary/standby main
    gateway/
    tools/         CLI client, bots, replay tool
    loadgen/
  tests/           unit, random-vs-reference, invariants
  bench/           Google Benchmark files
  infra/terraform/
  scripts/         deploy.sh, run_bench.sh, chaos tests
  results/         CSVs and graphs, one folder per experiment
```

## 7. Essential phases

### Phase 0: Setup

- Create the repo and layout. Get CMake building with GoogleTest, plus a build mode that turns on `-fsanitize=address,undefined`.
- AWS account hygiene: MFA on the root account, a non-root user for daily work, an **AWS Budgets alarm** (e.g. $20), and one region picked (check that your instance type is offered there and compare prices).
- Install AWS CLI and Terraform. Do the Terraform tutorial: create one tiny instance, SSH in, destroy it.

**Done when:** `ctest` runs a trivial test; Terraform creates and destroys an instance; the budget alarm exists.

---

### Phase 1: Matching engine (a plain C++ library, no networking)

The engine is a function: **command in → events out.**

```cpp
engine.process(const Message& cmd, EventSink& out);
```

**1a. Basic types:** `Price` and `Qty` (both `int64_t`), `OrderId`, `AccountId`, `SymbolId`, `Side {Buy, Sell}`.

**1b. Order book (one per ticker).** Start with the simple, standard version:

- Two sides. Bids sorted high → low, asks sorted low → high. Use `std::map<Price, PriceLevel>` for each.
- Each `PriceLevel` holds a **FIFO queue** (first in, first out) of its orders, e.g. a `std::list`. Front of the queue = earliest order.
- One `std::unordered_map<OrderId, ...>` pointing at where each order lives, so a cancel is a direct lookup rather than a search.

**1c. Matching logic**

```
on NewOrder(o):
    reject if: unknown ticker, price outside allowed band, not enough cash/shares,
               duplicate (session, session_seq)
    reserve cash (buy) or shares (sell)
    while o has quantity left AND opposite side is not empty
          AND best opposite price crosses o's price:        # buy: best_ask <= o.price
                                                            # sell: best_bid >= o.price
        resting = oldest order at the best opposite price level
        qty     = min(o.remaining, resting.remaining)
        emit Trade(price = resting.price, qty)
        move cash and shares between the two accounts; release leftover reservation
        remove resting if fully filled
    if o has quantity left:
        if o is a market order: emit Cancelled(remainder), release reservation
        else:                   add o to the back of its price level
```

**Worked example (use it as your first test).** Asks: A1 = 10 @ $5.02, A2 = 5 @ $5.02, A3 = 20 @ $5.05. A buy limit for 18 @ $5.05 arrives. Result: 10 @ $5.02 (A1), 5 @ $5.02 (A2), 3 @ $5.05 (A3). A3 keeps 17 waiting. Total filled = 18. ✔

**1d. Order types (essential):**
- **Limit order:** rests until filled or cancelled.
- **Cancel.**
- **Market order:** implemented as a limit order whose price is a protective bound (e.g. ±10% from the last trade price, or the listing reference price) that is *immediate-or-cancel* (IOC): trade what you can now, cancel the rest. This keeps cash reservation simple.

**1e. Accounts and risk (inside the engine):**
- Each account has cash and shares per ticker. **No short selling:** a sell needs shares, a buy needs cash.
- A buy reserves `price × qty` cash. A sell reserves shares. Reservations are released on cancel or fill. If a buy trades at a better price than its limit, release the difference.
- Reject reasons: unknown ticker, price outside band, insufficient cash/shares, duplicate order.

**1f. Admin commands** (also go through the sequencer): `ListSymbol(ticker, reference_price)`, `CreateAccount`, `DepositCash`, `GrantShares` (play-money issuance).

**1g. IDs:** the exchange assigns `OrderId` = the sequence number of the message that created it. Deterministic and unique.

**Done when:** the worked example and a few hand-written scenarios pass as unit tests.

---

### Phase 2: Prove the engine is correct

A matching engine that is *almost* right is wrong. Spend real time here.

**2a. Hand-written scenarios** (unit tests): simple cross; partial fill; sweep across several price levels; two orders at the same price (earlier fills first); cancel; cancel of unknown order; market order on an empty book; insufficient funds; duplicate order.

**2b. Reference engine.** Write a deliberately dumb second engine: keep all orders in one vector, and on each command, scan and sort everything. Slow, but easy to get right.

**2c. Random testing against the reference.**
- A seeded random generator produces e.g. 1 million commands (orders, cancels, market orders, several accounts and tickers).
- Feed identical commands to both engines. Compare the output events *exactly*.
- On a mismatch, print the **seed** and the first differing command so you can reproduce it.
- Run thousands of seeds.

**2d. Invariant checks** (run after every command in test builds):
- Best bid < best ask (book never "crossed").
- Orders within a price level are in arrival order.
- Total cash in the system and total shares per ticker never change (except admin deposits/grants).
- No negative balances.
- The order lookup map and the book contain exactly the same orders.

**2e. Sanitizers:** run all tests with AddressSanitizer + UBSan.

**2f. Determinism test:** run the same 1M-command stream twice, and in both Debug and Release builds. Compute a hash of all output events (FNV-1a is about 5 lines of code). The hashes must match.

**2g. Engine speed ceiling:** Google Benchmark of the engine alone: nanoseconds per command and commands per second. Record it. No later part of the system can beat this.

**Done when:** thousands of random seeds match the reference with zero invariant violations; sanitizers are clean; determinism hashes match; engine benchmark recorded in `results/`.

---

### Phase 3: Messages, queues, and the local pipeline

**3a. Binary messages.** Each message is a fixed-size `struct` with a small header (length, type, sequence). Fixed-width integers only (`uint32_t`, `int64_t`), tickers as a numeric `SymbolId`, and `static_assert(sizeof(...) == N)`. Why not JSON: it's slow to parse and variable-sized.
Message types: `NewOrder`, `Cancel`, `Accepted`, `Rejected`, `Trade`, `Cancelled`, admin commands, `Heartbeat`.

**3b. Safe decoding.** The receiver must check length and type *before* reading fields. Unit-test it with truncated and garbage input. It must never crash.

**3c. SPSC ring buffer.** Write it yourself (it's ~50 lines; one thread writes, one reads, using atomic head/tail counters). Test it with ThreadSanitizer plus a stress test that pushes 100M numbers across threads and checks a checksum.

**3d. Core process: threads**

```
 [network-in thread] ─ringA─▶ [engine thread] ─ringB─▶ [network-out thread]
                                    │
                                    └──ringC─▶ [journal thread]
```

- *Network-in:* reads TCP, decodes, pushes to ring A.
- *Engine thread:* the **sequencer and engine in one thread**. Takes a message from ring A, stamps it with the next sequence number and a timestamp, pushes a copy to ring C (journal), runs the engine, pushes resulting events to ring B.
- *Network-out:* sends events to the gateway.
- *Journal:* writes ring C to a file (Phase 7).

**3e. Gateway:**
- Accepts client TCP connections. Simple login: an API key per account.
- Each client numbers its own messages (1, 2, 3, …) per session. The gateway rejects gaps and duplicates. This is what makes retries safe.
- Forwards valid messages to the core. Sends each account its own replies (accepts, fills), plus a **public feed**: trades and best bid/ask changes. Feed messages carry sequence numbers so clients can detect gaps.
- Set `TCP_NODELAY` on every socket (otherwise the OS may delay small messages to batch them).

**3f. Client tools:** a command-line client (send orders, watch the feed) and a simple random-trading bot.

**Done when:** run everything on your laptop; two bots trade; the feed shows trades; a checker program reading the event stream confirms cash and shares are conserved.

---

### Phase 4: Measure: load generator and baseline

You can't claim "fast" without a trustworthy measurement.

**4a. Open-loop load generator.** It sends orders on a fixed schedule (e.g. one every 100 µs) *regardless* of whether replies have arrived, and measures each reply from when the order was **supposed** to be sent. Why: a "send, wait for reply, send next" tool slows down when your system stalls, hiding exactly the delays you care about (called *coordinated omission*).

**4b. Record latency** with HdrHistogram. Print p50, p90, p99, p99.9, max. Save raw results to CSV.

**4c. Workload (starting guess; tune later):** most orders are limit orders near the best price; a large share are cancels; a few are market orders; many accounts; several tickers with some far more popular than others. An empty book tells you nothing, so seed it first.

**4d. Throughput vs latency curve.** Run at 10k, 25k, 50k, 100k, … orders/sec. Graph p99 against rate. The **knee** (where latency shoots up) is your capacity.

**4e. Where time goes.** Add timestamps inside the core (received → sequenced → matched → output queued) and keep a histogram per stage. The difference between client round trip and engine time is network + gateway.

**4f. Method rules:** discard a ~30 s warm-up; run each test ≥ 5 minutes; repeat ≥ 3 times; report the spread, not just the best run. Write your **hypothesis before each run** (e.g. "full pipeline p50 under 100 µs, knee above 200k/s") then compare.

**Done when:** you have the table + graph on your laptop. This is only a local sanity baseline. Official numbers come from EC2 in Phase 5.

---

### Phase 5: Deploy to AWS

**5a. Terraform** creates:
- 1 VPC with **one public subnet in one AZ** (no NAT gateway, no load balancer; they cost money and aren't needed).
- Security group: SSH from *your IP only*; all traffic allowed between members of the group.
- A **cluster placement group** (puts instances physically close for low network latency).
- Instances, Ubuntu **ARM64**: `core-primary`, `core-standby` (c7g.xlarge, 4 cores), `gateway` and `loadgen` (c7g.large). Start with only one core + loadgen until Phase 8.
- A DynamoDB table (on-demand billing) for the epoch, used in Phase 8.

Do **not** use `t`-series instances for anything you measure. They throttle CPU when credits run out and will ruin your numbers.

**5b. Scripts:** `deploy.sh` (copy code, build Release on the instance), `run_bench.sh` (start processes, run load generator, copy CSVs back).

**5c. Noise-floor test.** On an idle instance, run a tiny program that loops reading the clock and records the biggest gaps between reads. It shows how much random pausing the virtual machine itself adds, so you know which tail numbers are meaningful.

**5d. Network floor.** Run a bare TCP echo server/client between `loadgen` and `core` (using **private IPs**). Record p50/p99 round-trip. Your system can never beat this.

**5e. Re-run the Phase 4 benchmark on EC2.** This is your **official baseline**.

**Ports and ARM note:** Graviton is ARM. Standard C++ compiles fine, but avoid x86-only code (`rdtsc`, `_mm_pause`). Use `clock_gettime(CLOCK_MONOTONIC)` / `std::chrono::steady_clock` for time.

**Done when:** from nothing, `terraform apply` → `deploy.sh` → `run_bench.sh` produces results; then `terraform destroy` leaves nothing running. Baseline, noise floor, and network floor are in `results/`.

---

### Phase 6: Basic tuning (one change at a time)

Method: change **one** thing → rerun the benchmark → record before/after in an ADR → keep it only if it helped.

1. **Release flags:** `-O2` vs `-O3`.
2. **Pin threads and busy-spin.** Give each pipeline thread its own core (`pthread_setaffinity_np`), spin instead of sleeping, leave core 0 for the OS.
3. **Memory pool.** Replace per-order `new` with a pre-allocated array of orders plus a free list. Verify with `perf` that nothing allocates per order.
4. **Batch journal writes** (many messages per `write()` call).
5. Run `perf record` and fix the single biggest hot spot once.

**Done when:** each change has a recorded before/after and final numbers are saved.

---

### Phase 7: Journal and recovery

**7a. File format:** each record = sequence number + message bytes + checksum (CRC32). A crash can leave a half-written record at the end; the checksum lets you detect and discard it.

**7b. Journal thread:** appends batches with `write()`; calls `fdatasync()` (force data to disk) every few milliseconds rather than per order. Syncing per order on EBS costs on the order of a millisecond each, which would dwarf everything else. The unsynced window is data you could lose if the machine dies; Phase 8's standby covers it.

**7c. Replay tool:** `replay journal.bin` rebuilds the engine and prints its state hash (and, if you like, the output-event hash).

**7d. Startup recovery:** on start, replay the journal, then accept traffic.

**7e. Measure:**
- Recovery time vs journal length (1M, 10M, 100M messages) and replay speed (messages/sec).
- Latency with `fdatasync` every message vs every few ms vs never.

**Done when:** `kill -9` the core under load, restart it, and its state hash matches what it was at the last journaled message; recovery measurements are recorded.

---

### Phase 8: Standby and failover

Goal: primary dies → standby takes over in roughly a second or two → **no acknowledged order lost, none duplicated.**

**8a. Replication.**
- Primary sends each sequenced message to the standby over TCP. The standby journals it, runs it through its own engine (same code), and replies "I have everything up to sequence N".
- The primary **holds back output events** (acks, trades) until the standby has confirmed the message that produced them. Track this with a `confirmed_seq` number shared between threads.
- Measure the latency this adds versus the Phase 6 result. This is the cost of high availability; report it.

**8b. Catch-up.** If the standby starts late or reconnects, it says "send me from sequence N" and the primary resends from its journal.

**8c. Failure detection.** The primary sends a heartbeat every ~100 ms. If the standby hears nothing for a configurable time (try ~500 ms–1 s), it decides the primary is dead. Shorter = faster failover but more false alarms. Test and pick.

**8d. Who is primary? Epoch in DynamoDB.** Two servers can't tell "the other one died" from "the network between us broke", so use DynamoDB as a tie-breaker:
- One item: `{ name: "leader", epoch: N, owner: <node> }`.
- To become primary, a node does a **conditional write**: "set epoch = N+1, owner = me, *only if* epoch is still N". Only one node can win.
- Every message between nodes and to the gateway carries the epoch. Anyone receiving a **lower** epoch than they've seen ignores it (**fencing**).
- Do this in a small **Python sidecar** next to each core (it only runs during failover, so speed doesn't matter). The C++ core just asks it "am I primary?".
- If a primary's confirmations stop arriving, it checks DynamoDB. If the epoch has moved, it demotes itself and becomes a standby.

**Why this is safe (convince yourself, then test it):**
- *No lost acks:* the primary releases an ack only after the standby has the message. So anything a client was told about is already on the standby.
- *No split-brain:* after the standby promotes itself (epoch N+1) it refuses messages from epoch N. The old primary can no longer get confirmations, so it can never release more output.

**8e. Gateway and client behavior.**
- Gateway keeps connections to both nodes and sends to whichever has the highest epoch.
- On losing the primary, the gateway holds new orders briefly, finds the new primary, and **resends** everything not yet confirmed.
- The core remembers the last session sequence number per session and **ignores duplicates**. This is the idempotency rule from Section 3.
- Output events carry an output sequence number. After failover the gateway says "the last one I saw was M"; the new primary resends from M+1 (it has identical events, since the engine is deterministic).

**8f. Divergence check.** Every N messages (e.g. 100k), primary and standby compare a state hash. A mismatch means a determinism bug: log loudly and halt.

**8g. Policy decision:** if the *standby* dies, does the primary keep trading unprotected or halt? Pick one, document it, and test it.

**8h. Failure tests**, each run under load (use your Phase 4 generator) with a checker:

| # | Fault | How |
|---|---|---|
| 1 | Primary process crash | `kill -9` |
| 2 | Standby crash | `kill -9` |
| 3 | Primary freezes, then wakes up (split-brain test) | `kill -STOP`, wait for failover, `kill -CONT` |
| 4 | Whole primary machine gone | `aws ec2 stop-instances` |
| 5 | Slow or lossy network between nodes | `tc netem` (Linux tool that adds artificial delay/loss) |

**Checker** (compares client logs with the final journal), after every run:
- Every order the client got an ack for appears in the journal **exactly once**.
- No duplicate trade IDs; output sequence numbers have no gaps.
- New primary and old primary (once back as standby) have the same state hash.
- Cash and shares are conserved.

Record: **RTO** (time from fault to the first new ack) and **RPO** (acknowledged orders lost; must be 0).

**Done when:** all 5 faults pass the checker, ≥ 20 repeats each, and you have an RTO table.

---

### Phase 9: Final evaluation and write-up

- **Full benchmark** on the final system, with replication on and off, on EC2. Throughput-vs-latency graph, p50/p99/p99.9/max.
- **Soak test:** run at about half your knee rate for 4–8 hours. Memory must not grow, p99 must stay flat, no errors. Afterwards replay the journal and confirm the hash matches the live state.
- **Cost line:** orders/sec per $/hr for your configuration.
- **README:** architecture diagram, results tables, failure-test results, the list of simplifications from Section 4, and your ADR log.

**ADR template** (one short file per decision in `docs/adr/`):

```
Decision:    e.g. "Replicate by holding output until standby confirms"
Hypothesis:  what you expect and why
Experiment:  what you ran (script + commit)
Result:      graph / table
Conclusion:  keep, change, or drop. Why.
```

## 8. Evidence checklist (definition of done)

| Claim | Evidence |
|---|---|
| Matching is correct | Random tests vs reference engine across thousands of seeds; invariants hold; sanitizers clean |
| Engine is deterministic | Identical output hashes across runs and build types; primary/standby hash checks |
| Decoder is robust | Garbage/truncated input tests pass |
| Engine is fast | Engine-only benchmark (ns per order) |
| System is fast | Open-loop latency percentiles + throughput-vs-latency curve + stage breakdown, from EC2 |
| Measurements are trustworthy | Noise-floor and network-floor results; repeated runs with spread |
| Tuning choices are justified | Before/after per change |
| Recovery works | `kill -9` + replay hash match; recovery time vs journal length |
| HA works | 5 fault tests × 20 repeats, checker passes, RTO table, RPO = 0 |
| HA cost is known | Latency with vs without replication |
| It's stable | 4–8 h soak with flat tails and no memory growth |

## 9. Cost control

- **Develop and test correctness on your laptop** (free). Use EC2 only for performance numbers and failover tests.
- **`terraform destroy` at the end of every session.** Stopped instances still pay for their disks. Check the billing console the next morning.
- Keep the AWS Budgets alarm on.
- Use **private IPs** between instances (same-AZ traffic is free that way). Public IPs carry a small hourly charge, so give them only to instances you need to SSH into, and keep sessions short.
- **Avoid:** NAT gateways, load balancers, Multi-AZ databases, MSK (managed Kafka), high-resolution CloudWatch metrics, `t`-series for measured servers.
- Small `gp3` disks (e.g. 20 GB). DynamoDB on-demand costs pennies.
- Check current prices before you start. A handful of small instances for a few hours should cost a few dollars per session, but verify.
- Optional saving: run the load generator on a Spot instance.

## 10. Optional extras (only after Phase 9)

**More realism in the exchange**
- More order types: FOK (fill-or-kill), post-only, cancel/replace, GTC vs DAY expiry
- Self-trade prevention, position limits, short selling
- Trading halts, circuit breakers (auto-pause on big price moves)
- Opening/closing auctions
- Corporate actions (splits, dividends)
- Full-depth market data feed and candles (OHLC charts)

**Deeper testing**
- **Deterministic simulation testing:** run the failover protocol in one process with a fake network and fake clock, injecting random drops, delays, crashes and pauses over thousands of seeds, checking the Phase 8 checker's rules each time (the technique used by FoundationDB and TigerBeetle)
- Fuzz the message decoder (libFuzzer)
- Property-testing library (RapidCheck) instead of hand-rolled random tests
- Compare x86 (c7i) vs ARM (c7g): speed per dollar, and a memory-ordering stress test

**Deeper performance work**
- Array-based price ladder vs `std::map` (benchmark with cache-miss counts)
- Intrusive linked lists, huge pages, isolating CPU cores from the OS (`isolcpus`)
- `io_uring`, then kernel-bypass networking (DPDK / ENA Express)
- UDP unicast for market data; slow-subscriber handling

**Scale**
- **Sharding:** several primary/standby pairs, each owning a group of tickers; gateway routes by ticker; scaling curve vs number of shards
- Separate market-data publisher process; fan-out latency vs number of subscribers

**Persistence and disaster recovery**
- Snapshots (state saved periodically so recovery doesn't replay everything); compare recovery time with/without
- Ship journal segments to S3; practice a full restore in a fresh region
- Postgres ledger fed from the event stream, plus nightly reconciliation (positions = sum of trades)
- Time-series store (TimescaleDB/ClickHouse) for candles

**Interfaces and operations**
- Web UI (WebSocket bridge), REST API, FIX gateway
- Prometheus + Grafana dashboards on one small instance
- TLS at the gateway, per-session rate limits
- Surveillance (e.g. detect wash trading)

**Alternative designs to compare, to justify your choice with data**
- Raft-replicated log (NuRaft) instead of primary/standby
- Aeron as the transport
- Kafka/MSK as the journal

**Workload realism**
- Calibrate the load generator with public Nasdaq ITCH sample data (check licence and terms)
