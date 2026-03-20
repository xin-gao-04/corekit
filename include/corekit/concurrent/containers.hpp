#pragma once

// 并发容器具体实现 —— 直接 include 此文件即可使用所有内置并发容器类型。
//
// 可用类型（均位于 corekit::concurrent 命名空间）：
//
//   BasicMutexQueue<T>      - 互斥锁保护的 FIFO 队列，支持容量限制
//   MoodycamelQueue<T>      - 基于 moodycamel 的无锁并发队列，高吞吐场景首选
//   BasicConcurrentMap<K,V> - 互斥锁保护的哈希映射
//   BasicConcurrentSet<K>   - 互斥锁保护的哈希集合
//   BasicRingBuffer<T>      - 互斥锁保护的定长环形缓冲区
//
// 对象池（位于 corekit::memory 命名空间）：
//
//   BasicObjectPool<T>      - 简单对象复用池，支持预热/借出/归还/裁剪

#include "src/concurrent/basic_map_impl.hpp"
#include "src/concurrent/basic_queue_impl.hpp"
#include "src/concurrent/basic_ring_buffer_impl.hpp"
#include "src/concurrent/basic_set_impl.hpp"
#include "src/concurrent/moodycamel_queue_impl.hpp"
#include "src/memory/basic_object_pool_impl.hpp"
