# IPC 设计（V3）

## 范围
- 本机进程间通信（IPC）。
- 共享内存可变帧字节环（Windows 后端）。
- 非阻塞 API（`TrySend`、`TryRecv`）。
- 借鉴 Redis 的分阶段发送路径（本地 outbox + 按预算 flush）。

## 共享内存布局
- 头部（`SharedHeader`）：
  - magic/version
  - 逻辑 `capacity` 和 `message_max_bytes`
  - `ring_bytes`（向上取整到 2 的幂）和 `ring_mask`
  - `read_index` / `write_index`（按字节偏移，单调递增）
  - 共享计数器（`send_ok` / `recv_ok` / `dropped_when_full`）
- 环形负载区：
  - 可变帧：`FrameHeader{size,reserved} + payload + padding（8 字节对齐）`

## 为什么是 V3
- V1 固定槽位环在消息大小不一致时会浪费内存。
- V3 使用可变帧，同时保留 FIFO 与非阻塞 API。
- 通过 `ring_mask` 优化取模（`offset = index & mask`）。

## 收发流程
- `TrySend`：
  1) 先入本地 outbox
  2) `ProcessIoOnce(write_budget)` 将待发消息刷入共享环
- `TryRecv`：
  1) `ProcessIoOnce(1)` 顺带推进待写消息
  2) 从共享环解析一帧

这与 Redis “先入缓冲，再在事件循环中 flush” 的思路一致。

## 回绕策略
- 写端不会把单帧跨越环尾写入。
- 若尾部连续空间不足，写端将 `write_index` 推进到环起点后再写。
- 读端在尾部空间不足以容纳帧头时，对 `read_index` 采用同样规则。

## 背压
- 共享环满：flush 以 `kWouldBlock` 结束，消息留在本地 outbox。
- 本地 outbox 满：
  - 返回 `kWouldBlock`
  - 若 `drop_when_full=true`，递增 `dropped_when_full`。

## 并发约定
- 保证 SPSC（单生产者 + 单消费者）。
- MPMC 需要外部加锁或专门的算法改造。

## 校验与安全
- 客户端打开时校验 `magic/version` 与 2 的幂 `ring_bytes`。
- 接收时校验帧大小不超过 `message_max_bytes`。
- 不完整帧按临时不可用处理（`kWouldBlock`）。

## 设计参考
- Redis 风格设计笔记：`docs/design/ipc_redis_inspired_design.md`
- Redis 源码参考：
  - https://github.com/redis/redis/blob/unstable/src/networking.c
  - https://github.com/redis/redis/blob/unstable/src/ae.c

## 后续增强
- 带超时的阻塞模式。
- 显式 `Flush/Poll` API 与 outbox 可观测性指标。
- 面向 MPMC 的安全环策略（基于 sequence 的槽位）。
- Linux/macOS 的跨平台等价实现。
