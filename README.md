https://github.com/user-attachments/assets/2159fc6d-c31c-4e94-9b25-2f946ee1abc2

# Echo

**Echo** é uma ferramenta modular de _chunking_, criptografia e reconstrução de arquivos, escrita em **C**.

O projeto é focado em:

- processamento seguro baseado em chunks
- criptografia autenticada (AEAD) via **libsodium**
- _storage providers_ plugáveis (ex.: filesystem local)
- reconstrução determinística via _manifest_
- testabilidade (unit + integration)
- arquitetura limpa e modular (core/app/providers/stego)

O Echo divide o arquivo em chunks, cifra cada chunk, armazena usando um provider e depois reconstrói o arquivo original a partir de um manifest.

## Features

- Chunking configurável
- Hash SHA-256 e criptografia autenticada com **libsodium**
- Manifest binário (save/load) para reconstrução
- Provider local filesystem (`provider_localfs`)
- Camada de _carrier/embedding_ (steganografia) por extensão do objeto:
  - `.txt` (carrier textual)
  - `.ppm` (carrier de imagem explícito, PPM)
  - `.pnm` (carrier de imagem LSB “imperceptível”, PPM/P6)
  - `.png` (carrier PNG mantendo aparência: adiciona chunk ancilar)

## Uso (CLI)

O binário atual se chama `aaaa` (para evitar conflito com o builtin `echo` do shell).

- Upload (binário “normal”, sem stego):
  - `./aaaa upload <input> <manifest> <password> <storage_dir> <chunk_size>`
- Upload com carrier textual:
  - `./aaaa upload-text <input> <manifest> <password> <storage_dir> <chunk_size>`
- Upload com carrier de imagem:
  - `./aaaa upload-image <input> <manifest> <password> <storage_dir> <chunk_size>` (PPM `.ppm`)
  - `./aaaa upload-image-lsb <input> <manifest> <password> <storage_dir> <chunk_size>` (LSB `.pnm`)
  - `./aaaa upload-image-png <input> <manifest> <password> <storage_dir> <chunk_size>` (PNG `.png`, mantém aparência)
- Download:
  - `./aaaa download <manifest> <output> <password> <storage_dir>`
- Verify:
  - `./aaaa verify <manifest> <storage_dir>`

## Build, Test, Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Instalação (sem sudo, recomendado):

```bash
make install
```

Instalação em `/usr/local` (com sudo):

```bash
make install-system
```

Make all important parts testable

<img width="1150" height="647" alt="576914368-36ab1400-3f1f-4c0e-bbb1-085c608819f3" src="https://github.com/user-attachments/assets/899435d7-d52c-463e-97e9-d88e1de5e3e3" />


## Assets e variáveis de ambiente

- Corpus do carrier textual:
  - default: `assets/corpus/shrek_pt.txt`
  - override: `ECHO_STEGO_CORPUS=/path/to/corpus.txt`
- Cover do carrier PNG:
  - default: `assets/image.png`
  - override: `ECHO_STEGO_PNG_COVER=/path/to/cover.png`
