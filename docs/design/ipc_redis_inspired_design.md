# IPC Redis-Inspired Design Notes

## 目标
把 Redis 的非阻塞 I/O 思路映射到 corekit IPC，在不改 `IChannel` 外部接口的前提下提升吞吐与拥塞可控性。

## Redis 中可借鉴的核心模式
- 读阶段：`readQueryFromClient`
- 解析/执行：`processInputBuffer`
- 写阶段：`addReply*` 先写到输出缓冲，再在可写事件中发送（`sendReplyToClient`）
- 调度：单线程事件循环 (`ae.c`) 以预算推进，避免单连接独占

参考：
- https://github.com/redis/redis/blob/unstable/src/networking.c
- https://github.com/redis/redis/blob/unstable/src/ae.c

## corekit IPC 映射
### 1) 两阶段发送
- `TrySend` 先入本地 `outbox`
- `ProcessIoOnce` 再把 `outbox` 冲刷到共享内存环

### 2) 预算化推进
- `ProcessIoOnce(write_budget)` 单次最多推进固定条数
- 每次 `TrySend/TryRecv` 触发一次小预算推进

### 3) 数据结构升级（高风险项）
从固定槽位环升级为变长帧字节环：
- 帧格式：`FrameHeader + payload + 8字节对齐padding`
- 环容量：按目标字节数上调到 2 的幂（`ring_mask` 快速寻址）
- 末尾不足一帧时通过索引跳转到环首（不跨界写帧）

## 当前收益
- 避免固定 `message_max_bytes` 带来的大量内存浪费
- 使用 `index & mask` 替代取模
- 分离热索引缓存行，降低伪共享

## 边界
- 仍是 Windows 共享内存后端
- 仍是 SPSC 约束；MPMC 需要算法升级
- `TrySend` 成功表示进入发送流水线，不保证已落共享环
