# TCP Prague Implementation in ns-3

This project is an implementation of **TCP Prague** in [ns-3](https://www.nsnam.org/), the discrete-event network simulator.  
It is based on the existing implementation of **TCP DCTCP** and aims to comply with the **L4S (Low Latency, Low Loss, Scalable throughput)** architecture.

## 🚧 Status

**In development** — This is a work in progress to implement and validate all the mandatory requirements for TCP Prague as specified by the IETF L4S architecture.

## 🧩 Based On

The implementation is based on:
- The `TcpDctcp` class in ns-3
- Relevant RFCs and drafts on L4S and TCP Prague
- Research literature and presentations from the L4S working group

---

## ✅ Mandatory Requirements Implemented / To Implement

The goal is to fully support the **mandatory requirements** for TCP Prague:

- [✅] **L4S-ECN Packet Identification**: Set ECN field to **ECT(1)**
- [✅] **Accurate ECN Feedback**: Use accurate congestion feedback from receivers (via redefined TCP feedback)
- [✅] **Fall-back to Reno-friendly on Loss**: On detecting loss, fall back to a Reno-friendly behavior
- [✅] **Fall-back to Reno-friendly on Classic ECN Bottleneck**: Detect classic ECN bottlenecks and adjust
- [❌] **Reduce RTT Dependence**: Minimize RTT bias in congestion control
- [❌] **Scale Down to Fractional Window**: Support congestion window sizes smaller than 1 MSS
- [🛠️] **Detecting Loss in Units of Time**: Use time-based rather than packet-count-based loss detection

## ⚙️ Optional Performance Optimizations

Optional optimizations to improve performance (to be considered after core requirements):

- [❌] **ECN-capable TCP Control Packets** (SYN, FIN, RST)
- [❌] **Faster Flow Start** (e.g., Hybrid Slow Start variants)
- [❌] **Faster than Additive Increase** (during congestion avoidance)

