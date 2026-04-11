# Echo

**Echo** is a modular file chunking, encryption, and reconstruction tool written in **C**.

It is designed as a professional systems programming project focused on:

- secure chunk-based file processing
- authenticated encryption
- pluggable storage providers
- deterministic reconstruction
- testability
- clean architecture

Echo splits files into chunks, encrypts them, stores them through configurable providers, and later reconstructs the original file using a manifest.

## Features

- File chunking with configurable chunk size
- Authenticated encryption using **libsodium**
- Manifest-based reconstruction
- Pluggable storage providers
- Local filesystem provider implemented
- Extensible carrier/embedding layer

### Make all important parts testable

<img width="1150" height="647" alt="image" src="https://github.com/user-attachments/assets/36ab1400-3f1f-4c0e-bbb1-085c608819f3" />
