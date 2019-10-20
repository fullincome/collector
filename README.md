# Collector
Collects data blocks from threads and calls custom callback for each block.

## Features
- FIFO queue
- fast (based on ring buffer)
- thread safe

### Lazy logger example
```
void collector_callback(uint8_t* buf, size_t size)
{
    LOG_MESSAGE(buf, sizeof(uint8_t), size, stdout);
}

int main()
{
    ...
    Collector collector(COLLECTOR_SIZE, collector_callback);
    collector.start();

    ...
    std::thread thread1(
        [&]()
        {
                collector.push("Data from first thread");
        });

    ...
    std::thread thread2(
        [&]()
        {
                collector.push("Data from second thread");
        });
    ...
}

```
