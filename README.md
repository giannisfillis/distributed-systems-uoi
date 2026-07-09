# Distributed Systems - Academic Year 2025-2026

[![Course](https://img.shields.io/badge/Course-Distributed_Systems_(MYE017)-blue.svg)]()
[![Institution](https://img.shields.io/badge/Institution-University_of_Ioannina-red.svg)]()
[![Language](https://img.shields.io/badge/Language-C-orange.svg)]()

This repository contains the programming assignments and final reports for the Distributed Systems (MYE017) course at the University of Ioannina, Department of Computer Engineering and Informatics.

**Team Members:**
* Konstantinos Zois, AM: 5226
* Giannis Fillis, AM: 5380

---

## 💬 Project 1: Client-Server Chat Application

### Overview
The first assignment involves implementing a simplified chat application based on the client-server model to practice and understand fundamental distributed system concepts such as processes, threads, communication, request-reply, and synchronization.

### Implementation Details
The project is divided into three incremental versions:
* **Version 1 (Single-threaded Request-Reply):** The client and server utilize one thread each; the client sends a message and must wait for the server to send a reply before being able to send another one.
* **Version 2 (Multi-threaded Request-Reply):** Uses two threads per process, allowing the client and server to manage keyboard input and incoming messages independently without blocking each other.
* **Version 3 (Multithreaded Server):** The server is transformed to handle multiple clients concurrently by creating a new thread for each accepted connection. The server acts as a central host, broadcasting received messages to all other connected clients. Users can also set a custom display name by sending a `\name` command.
* **Bonus (Chat History):** Implemented a chat history feature where the server retains past messages in a linked list structure, and any newly connected client receives the entire previous conversation.

---

## ⚙️ Project 2: Static and Dynamic Scheduling

### Overview
The second assignment focuses on parallelizing a simple loop program using MPI to study message passing and workload distribution across a distributed system of nodes.

### Implementation Details
* **Static Scheduling:** Loop iterations are statically divided and assigned to processes at the start of the execution. If the total iterations are not perfectly divisible by the number of processes, some processes are assigned at most one extra iteration.
* **Dynamic Scheduling:** Implemented using a master-worker paradigm where a single master process dynamically assigns individual iterations to worker processes as soon as they complete their previous task and return a result.

### Key Findings
* Evaluated execution times for varying loop iterations ($n=20$ to $120$) across different numbers of processes ($P=4$ to $13$).
* Increasing the number of processes generally reduced the execution time, as the workload was distributed across more nodes.
* **Conclusion:** For smaller iteration numbers, dynamic scheduling often resulted in slightly better execution times. However, for larger workloads, static scheduling proved to be significantly faster. This was attributed to dynamic scheduling introducing a massive communication overhead (requiring approximately $2*n$ message exchanges), compared to only $2*(P-1)$ exchanges in the static approach.

---

## 🚀 Compilation and Execution

### Project 1
A `Makefile` is provided to automatically compile the client and server executable files.
```bash
# Compile the executables
make

# Run the server (must be executed first)
./server

# Run the client in a separate terminal
./client
```

### Project 2
The code is compiled using the `mpicc` wrapper for the C compiler and executed using the `mpirun` command.
```bash 

# Compile static and dynamic implementations
mpicc -o static-sched static-sched.c
mpicc -o dynamic-sched dynamic-sched.c

# Run the program with a specified number of processes (e.g., 4)
mpirun -np 4 ./static-sched