# Object Cache with LRU Eviction Policy

This project implements a simple object cache with an LRU (Least Recently Used) eviction policy in C++. 

## Features

- **LRU Eviction**: Uses an LRU strategy to evict the least recently used objects when the cache reaches its maximum size.
- **Memory Management**: Automatically cleans up expired objects using weak pointers.

## How to Use

1. Clone the repository:
   ```bash
   git clone https://github.com/gnersisyann/LRU-Cache.git
2. Compile and run the example:
   ```bash
   cd LRU-Cache
   make
   ./lru-cache
