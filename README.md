# Concurrency Programming

- **Lock-Free Single Producer Single Consumer Circular Queue**: Implemented a lock-free circular queue optimized for the Single Producer Single Consumer (SPSC) model. It constructs a circular linked list using pointer connections to enhance data processing efficiency. The queue features dynamic expansion, automatically allocating new blocks and linking them to the list when capacity is insufficient. A cache line alignment strategy is implemented to reduce false sharing issues.
- **Stack**: Includes a lock stack and a lock-free stack based on double reference counting.
- **Singleton Pattern**: Covers lock-free, static instance, and lock-based implementations.

## Build and Run Tests

To build the project, use the following commands:

```shell
cmake -B build && cmake --build build
```

To run the tests, execute:
```shell
./build/test_singleton
./build/test_stack
./build/test_sq
```