#include "echo/text_carrier.h"

#include <ctype.h>
#include <limits.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

echo_error_t echo_stego_load_default_corpus(char **out_text, size_t *out_len);

static const char ECHO_B64_URLSAFE_ALPHABET[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const char *const ECHO_PT_FALLBACK_WORDS[64] = {
    "amor",     "brisa",   "caminho", "destino",  "espera",  "futuro",
    "girassol", "historia","ideia",   "janela",   "leitura", "marinho",
    "noite",    "origem",  "poesia",  "quietude", "riqueza", "saudade",
    "tempo",    "uniao",   "verdade", "xale",     "zelo",    "alegria",
    "barco",    "carta",   "doce",    "encanto",  "folha",   "grao",
    "horta",    "ilustre", "jardim",  "luz",      "mundo",   "navio",
    "ouro",     "praia",   "quadro",  "rua",      "sol",     "terra",
    "vento",    "xisto",   "zumbido", "abrigo",   "bela",    "calma",
    "danca",    "eco",     "festa",   "gentil",   "honra",   "imagem",
    "juntos",   "livre",   "manso",   "nobre",    "olhar",   "paz",
    "querer",   "riso",    "sutil",   "trilha"};

static int echo_sodium_ready(void) { return sodium_init() >= 0; }

static int echo_b64_index(char c) {
  const char *p = strchr(ECHO_B64_URLSAFE_ALPHABET, c);
  if (!p) {
    return -1;
  }
  return (int)(p - ECHO_B64_URLSAFE_ALPHABET);
}

static void echo_trim_token(char *s) {
  size_t i = 0;
  size_t j = 0;

  while (s[i]) {
    unsigned char ch = (unsigned char)s[i];
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
      s[j++] = (char)tolower(ch);
    }
    i++;
  }
  s[j] = '\0';
}

static int echo_word_index(const char *token) {
  size_t i;
  for (i = 0; i < 64; i++) {
    if (strcmp(token, ECHO_PT_FALLBACK_WORDS[i]) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static size_t echo_next_ascii_word(const char *text, size_t len, size_t *io_pos,
                                  char out[32]) {
  size_t i = *io_pos;
  size_t j = 0;

  while (i < len) {
    unsigned char ch = (unsigned char)text[i];
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
      break;
    }
    i++;
  }

  while (i < len && j + 1 < 32) {
    unsigned char ch = (unsigned char)text[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) {
      break;
    }
    out[j++] = (char)tolower(ch);
    i++;
  }

  out[j] = '\0';
  *io_pos = i;
  return j;
}

static int echo_dict_contains(char dict[64][32], size_t count,
                              const char *word) {
  size_t i;
  for (i = 0; i < count; i++) {
    if (strcmp(dict[i], word) == 0) {
      return 1;
    }
  }
  return 0;
}

static void echo_build_dict_from_corpus(const char *corpus, size_t corpus_len,
                                       char out_dict[64][32]) {
  size_t pos = 0;
  size_t count = 0;

  while (count < 64 && pos < corpus_len) {
    char w[32];
    size_t wl = echo_next_ascii_word(corpus, corpus_len, &pos, w);
    if (wl == 0) {
      pos++;
      continue;
    }
    if (!echo_dict_contains(out_dict, count, w)) {
      memcpy(out_dict[count], w, wl + 1);
      count++;
    }
  }

  while (count < 64) {
    size_t fi;
    int added = 0;

    for (fi = 0; fi < 64; fi++) {
      const char *fallback = ECHO_PT_FALLBACK_WORDS[fi];
      if (!echo_dict_contains(out_dict, count, fallback)) {
        snprintf(out_dict[count], 32, "%s", fallback);
        count++;
        added = 1;
        break;
      }
    }

    if (!added) {
      size_t n = count - 64;
      char gen[32];
      snprintf(gen, sizeof(gen), "eco%c%c", (char)('a' + (n / 26) % 26),
               (char)('a' + (n % 26)));
      if (!echo_dict_contains(out_dict, count, gen)) {
        snprintf(out_dict[count], 32, "%s", gen);
        count++;
      }
    }
  }
}

static echo_error_t echo_append_bytes(char **io_buf, size_t *io_len,
                                      size_t *io_cap, const char *s,
                                      size_t s_len) {
  if (*io_len + s_len + 1 > *io_cap) {
    size_t new_cap = (*io_cap == 0) ? 512 : (*io_cap * 2);
    char *new_buf;
    while (*io_len + s_len + 1 > new_cap) {
      size_t next = new_cap * 2;
      if (next < new_cap) {
        return ECHO_ERR_NOMEM;
      }
      new_cap = next;
    }

    new_buf = (char *)realloc(*io_buf, new_cap);
    if (!new_buf) {
      return ECHO_ERR_NOMEM;
    }
    *io_buf = new_buf;
    *io_cap = new_cap;
  }

  memcpy(*io_buf + *io_len, s, s_len);
  *io_len += s_len;
  (*io_buf)[*io_len] = '\0';
  return ECHO_OK;
}

static echo_error_t echo_text_pt_encode_v2(const uint8_t *in, size_t in_len,
                                          uint8_t **out, size_t *out_len) {
  unsigned char hash[16];
  char hash_hex[33];
  size_t b64_cap;
  char *b64 = NULL;
  char *body = NULL;
  size_t body_len = 0;
  size_t body_cap = 0;
  char dict[64][32] = {{0}};
  char *corpus = NULL;
  size_t corpus_len = 0;
  size_t cover_len = 0;
  size_t i;
  echo_error_t err;

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (!echo_sodium_ready()) {
    return ECHO_ERR_CRYPTO;
  }

  if (crypto_generichash(hash, sizeof(hash), in, (unsigned long long)in_len,
                         NULL, 0) != 0) {
    return ECHO_ERR_CRYPTO;
  }
  sodium_bin2hex(hash_hex, sizeof(hash_hex), hash, sizeof(hash));

  b64_cap = sodium_base64_ENCODED_LEN(
      in_len, sodium_base64_VARIANT_URLSAFE_NO_PADDING
  );
  b64 = (char *)malloc(b64_cap);
  if (!b64) {
    return ECHO_ERR_NOMEM;
  }
  sodium_bin2base64(b64, b64_cap, in, in_len,
                    sodium_base64_VARIANT_URLSAFE_NO_PADDING);

  if (echo_stego_load_default_corpus(&corpus, &corpus_len) == ECHO_OK) {
    echo_build_dict_from_corpus(corpus, corpus_len, dict);
    cover_len = corpus_len;
    if (cover_len > 4096) {
      cover_len = 4096;
    }
  } else {
    for (i = 0; i < 64; i++) {
      snprintf(dict[i], 32, "%s", ECHO_PT_FALLBACK_WORDS[i]);
    }
  }

  err = echo_append_bytes(&body, &body_len, &body_cap, "ECHO-STEGOTXT2\n",
                          strlen("ECHO-STEGOTXT2\n"));
  if (err != ECHO_OK)
    goto cleanup;

  {
    char tmp[96];
    int n = snprintf(tmp, sizeof(tmp), "len:%zu\nhash:%s\ndict:", in_len,
                     hash_hex);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
      err = ECHO_ERR_IO;
      goto cleanup;
    }
    err = echo_append_bytes(&body, &body_len, &body_cap, tmp, (size_t)n);
    if (err != ECHO_OK)
      goto cleanup;
  }

  for (i = 0; i < 64; i++) {
    err =
        echo_append_bytes(&body, &body_len, &body_cap, dict[i], strlen(dict[i]));
    if (err != ECHO_OK)
      goto cleanup;
    if (i + 1 < 64) {
      err = echo_append_bytes(&body, &body_len, &body_cap, ",", 1);
      if (err != ECHO_OK)
        goto cleanup;
    }
  }

  err = echo_append_bytes(&body, &body_len, &body_cap, "\n\nBEGIN COVER\n",
                          strlen("\n\nBEGIN COVER\n"));
  if (err != ECHO_OK)
    goto cleanup;

  if (corpus && cover_len > 0) {
    char *cover = (char *)malloc(cover_len);
    size_t ci;

    if (!cover) {
      err = ECHO_ERR_NOMEM;
      goto cleanup;
    }

    for (ci = 0; ci < cover_len; ci++) {
      cover[ci] = corpus[ci] ? corpus[ci] : '\n';
    }

    err = echo_append_bytes(&body, &body_len, &body_cap, cover, cover_len);
    free(cover);
    if (err != ECHO_OK)
      goto cleanup;
    if (cover_len == 0 || corpus[cover_len - 1] != '\n') {
      err = echo_append_bytes(&body, &body_len, &body_cap, "\n", 1);
      if (err != ECHO_OK)
        goto cleanup;
    }
  }

  err = echo_append_bytes(&body, &body_len, &body_cap,
                          "END COVER\n\nBEGIN DATA\n",
                          strlen("END COVER\n\nBEGIN DATA\n"));
  if (err != ECHO_OK)
    goto cleanup;

  {
    size_t wcount = 0;
    for (i = 0; b64[i] != '\0'; i++) {
      int idx = echo_b64_index(b64[i]);
      const char *word;

      if (idx < 0) {
        err = ECHO_ERR_INTERNAL;
        goto cleanup;
      }

      word = dict[idx];

      if (wcount > 0) {
        err = echo_append_bytes(&body, &body_len, &body_cap, " ", 1);
        if (err != ECHO_OK)
          goto cleanup;
      }

      err = echo_append_bytes(&body, &body_len, &body_cap, word, strlen(word));
      if (err != ECHO_OK)
        goto cleanup;

      wcount++;
      if (wcount % 12 == 0) {
        err = echo_append_bytes(&body, &body_len, &body_cap, ".\n", 2);
        if (err != ECHO_OK)
          goto cleanup;
        wcount = 0;
      }
    }

    if (wcount != 0) {
      err = echo_append_bytes(&body, &body_len, &body_cap, ".\n", 2);
      if (err != ECHO_OK)
        goto cleanup;
    }
  }

  err = echo_append_bytes(&body, &body_len, &body_cap, "END DATA\n",
                          strlen("END DATA\n"));
  if (err != ECHO_OK)
    goto cleanup;

  *out = (uint8_t *)body;
  *out_len = body_len;
  body = NULL;
  err = ECHO_OK;

cleanup:
  free(corpus);
  if (b64) {
    sodium_memzero(b64, b64_cap);
    free(b64);
  }
  free(body);
  return err;
}

static echo_error_t echo_text_pt_encode(const uint8_t *in, size_t in_len,
                                        uint8_t **out, size_t *out_len) {
  return echo_text_pt_encode_v2(in, in_len, out, out_len);
}

static echo_error_t echo_text_pt_decode(const uint8_t *in, size_t in_len,
                                        uint8_t **out, size_t *out_len) {
  char *text = NULL;
  char *cursor;
  char *header_end;
  char *line;
  size_t expected_len = 0;
  char expected_hash_hex[33] = {0};
  size_t b64_cap = 0;
  char *b64 = NULL;
  size_t b64_len = 0;
  uint8_t *bin = NULL;
  size_t bin_len = 0;
  unsigned char hash[16];
  char hash_hex[33];

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (!echo_sodium_ready()) {
    return ECHO_ERR_CRYPTO;
  }

  text = (char *)malloc(in_len + 1);
  if (!text) {
    return ECHO_ERR_NOMEM;
  }
  memcpy(text, in, in_len);
  text[in_len] = '\0';
  {
    size_t i;
    for (i = 0; i < in_len; i++) {
      if (text[i] == '\0') {
        text[i] = '\n';
      }
    }
  }

  if (strncmp(text, "ECHO-STEGOTXT2\n", 14) == 0) {
    char *p = text + 14;
    char *header_end2 = strstr(p, "\n\n");
    char dict_line[4096] = {0};
    char dict_words[64][32] = {{0}};
    size_t dict_count = 0;
    char *data_start;
    char *data_end;
    char *data_cursor;

    if (!header_end2) {
      free(text);
      return ECHO_ERR_CORRUPTED;
    }

    *header_end2 = '\0';
    cursor = p;

    while ((line = strsep(&cursor, "\n")) != NULL) {
      if (strncmp(line, "len:", 4) == 0) {
        unsigned long long v = strtoull(line + 4, NULL, 10);
        if (v > SIZE_MAX) {
          free(text);
          return ECHO_ERR_CORRUPTED;
        }
        expected_len = (size_t)v;
        continue;
      }
      if (strncmp(line, "hash:", 5) == 0) {
        size_t hl = strlen(line + 5);
        if (hl != 32) {
          free(text);
          return ECHO_ERR_CORRUPTED;
        }
        memcpy(expected_hash_hex, line + 5, 32);
        expected_hash_hex[32] = '\0';
        continue;
      }
      if (strncmp(line, "dict:", 5) == 0) {
        snprintf(dict_line, sizeof(dict_line), "%s", line + 5);
        continue;
      }
    }

    if (expected_len == 0 || expected_hash_hex[0] == '\0' ||
        dict_line[0] == '\0') {
      free(text);
      return ECHO_ERR_CORRUPTED;
    }

    {
      char *dict_cursor = dict_line;
      char *token;

      while ((token = strsep(&dict_cursor, ",")) != NULL) {
        char w[32];
        size_t wl;
        size_t k = 0;

        while (token[k] && k + 1 < sizeof(w)) {
          unsigned char ch = (unsigned char)token[k];
          if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
            w[k] = (char)tolower(ch);
          } else {
            w[k] = '\0';
            break;
          }
          k++;
        }
        w[k] = '\0';

        wl = strlen(w);
        if (wl == 0) {
          free(text);
          return ECHO_ERR_CORRUPTED;
        }

        if (dict_count >= 64) {
          free(text);
          return ECHO_ERR_CORRUPTED;
        }

        snprintf(dict_words[dict_count], 32, "%s", w);
        dict_count++;
      }
    }

    if (dict_count != 64) {
      free(text);
      return ECHO_ERR_CORRUPTED;
    }

    data_start = strstr(header_end2 + 2, "BEGIN DATA\n");
    if (!data_start) {
      free(text);
      return ECHO_ERR_CORRUPTED;
    }
    data_start += strlen("BEGIN DATA\n");

    data_end = strstr(data_start, "END DATA\n");
    if (!data_end) {
      free(text);
      return ECHO_ERR_CORRUPTED;
    }

    b64_cap = (in_len / 3) + 64;
    b64 = (char *)malloc(b64_cap);
    if (!b64) {
      free(text);
      return ECHO_ERR_NOMEM;
    }

    data_cursor = data_start;
    while (data_cursor < data_end && *data_cursor) {
      char token[64];
      size_t tlen = 0;
      int idx = -1;
      size_t di;

      while (data_cursor < data_end &&
             isspace((unsigned char)*data_cursor)) {
        data_cursor++;
      }
      if (data_cursor >= data_end || !*data_cursor) {
        break;
      }

      while (data_cursor < data_end &&
             !isspace((unsigned char)*data_cursor) &&
             tlen + 1 < sizeof(token)) {
        token[tlen++] = *data_cursor++;
      }
      token[tlen] = '\0';

      echo_trim_token(token);
      if (!token[0]) {
        continue;
      }

      for (di = 0; di < 64; di++) {
        if (strcmp(token, dict_words[di]) == 0) {
          idx = (int)di;
          break;
        }
      }

      if (idx < 0) {
        continue;
      }

      if (b64_len + 2 >= b64_cap) {
        size_t new_cap = b64_cap * 2;
        char *new_b64;
        if (new_cap < b64_cap) {
          free(b64);
          free(text);
          return ECHO_ERR_NOMEM;
        }
        new_b64 = (char *)realloc(b64, new_cap);
        if (!new_b64) {
          free(b64);
          free(text);
          return ECHO_ERR_NOMEM;
        }
        b64 = new_b64;
        b64_cap = new_cap;
      }

      b64[b64_len++] = ECHO_B64_URLSAFE_ALPHABET[idx];
    }
    b64[b64_len] = '\0';

    bin = (uint8_t *)malloc(expected_len);
    if (!bin) {
      sodium_memzero(b64, b64_cap);
      free(b64);
      free(text);
      return ECHO_ERR_NOMEM;
    }

    {
      const char *b64_end = NULL;
      if (sodium_base642bin(
              bin, expected_len, b64, b64_len, NULL, &bin_len, &b64_end,
              sodium_base64_VARIANT_URLSAFE_NO_PADDING
          ) != 0) {
        free(bin);
        sodium_memzero(b64, b64_cap);
        free(b64);
        free(text);
        return ECHO_ERR_CORRUPTED;
      }
    }

    if (bin_len != expected_len) {
      free(bin);
      sodium_memzero(b64, b64_cap);
      free(b64);
      free(text);
      return ECHO_ERR_CORRUPTED;
    }

    if (crypto_generichash(hash, sizeof(hash), bin, (unsigned long long)bin_len,
                           NULL, 0) != 0) {
      free(bin);
      sodium_memzero(b64, b64_cap);
      free(b64);
      free(text);
      return ECHO_ERR_CRYPTO;
    }

    sodium_bin2hex(hash_hex, sizeof(hash_hex), hash, sizeof(hash));
    if (sodium_memcmp(hash_hex, expected_hash_hex, 32) != 0) {
      free(bin);
      sodium_memzero(b64, b64_cap);
      free(b64);
      free(text);
      return ECHO_ERR_CORRUPTED;
    }

    *out = bin;
    *out_len = bin_len;

    sodium_memzero(b64, b64_cap);
    free(b64);
    free(text);
    return ECHO_OK;
  }

  if (strncmp(text, "ECHO-STEGOTXT1\n", 14) != 0) {
    free(text);
    return ECHO_ERR_CORRUPTED;
  }

  header_end = strstr(text, "\n\n");
  if (!header_end) {
    free(text);
    return ECHO_ERR_CORRUPTED;
  }

  *header_end = '\0';
  cursor = text;

  while ((line = strsep(&cursor, "\n")) != NULL) {
    if (strncmp(line, "len:", 4) == 0) {
      unsigned long long v = strtoull(line + 4, NULL, 10);
      if (v > SIZE_MAX) {
        free(text);
        return ECHO_ERR_CORRUPTED;
      }
      expected_len = (size_t)v;
      continue;
    }
    if (strncmp(line, "hash:", 5) == 0) {
      size_t hl = strlen(line + 5);
      if (hl != 32) {
        free(text);
        return ECHO_ERR_CORRUPTED;
      }
      memcpy(expected_hash_hex, line + 5, 32);
      expected_hash_hex[32] = '\0';
      continue;
    }
  }

  if (expected_len == 0 || expected_hash_hex[0] == '\0') {
    free(text);
    return ECHO_ERR_CORRUPTED;
  }

  cursor = header_end + 2;

  b64_cap = (in_len / 3) + 64;
  b64 = (char *)malloc(b64_cap);
  if (!b64) {
    free(text);
    return ECHO_ERR_NOMEM;
  }

  while (*cursor) {
    char token[64];
    size_t tlen = 0;
    int idx;

    while (*cursor && isspace((unsigned char)*cursor)) {
      cursor++;
    }
    if (!*cursor) {
      break;
    }

    while (*cursor && !isspace((unsigned char)*cursor) &&
           tlen + 1 < sizeof(token)) {
      token[tlen++] = *cursor++;
    }
    token[tlen] = '\0';

    echo_trim_token(token);
    if (!token[0]) {
      continue;
    }

    idx = echo_word_index(token);
    if (idx < 0) {
      continue;
    }

    if (b64_len + 2 >= b64_cap) {
      size_t new_cap = b64_cap * 2;
      char *new_b64;
      if (new_cap < b64_cap) {
        free(b64);
        free(text);
        return ECHO_ERR_NOMEM;
      }
      new_b64 = (char *)realloc(b64, new_cap);
      if (!new_b64) {
        free(b64);
        free(text);
        return ECHO_ERR_NOMEM;
      }
      b64 = new_b64;
      b64_cap = new_cap;
    }

    b64[b64_len++] = ECHO_B64_URLSAFE_ALPHABET[idx];
  }
  b64[b64_len] = '\0';

  bin = (uint8_t *)malloc(expected_len);
  if (!bin) {
    sodium_memzero(b64, b64_cap);
    free(b64);
    free(text);
    return ECHO_ERR_NOMEM;
  }

  {
    const char *b64_end = NULL;
    if (sodium_base642bin(bin, expected_len, b64, b64_len, NULL, &bin_len,
                          &b64_end, sodium_base64_VARIANT_URLSAFE_NO_PADDING) !=
        0) {
      free(bin);
      sodium_memzero(b64, b64_cap);
      free(b64);
      free(text);
      return ECHO_ERR_CORRUPTED;
    }
  }

  if (bin_len != expected_len) {
    free(bin);
    sodium_memzero(b64, b64_cap);
    free(b64);
    free(text);
    return ECHO_ERR_CORRUPTED;
  }

  if (crypto_generichash(hash, sizeof(hash), bin, (unsigned long long)bin_len,
                         NULL, 0) != 0) {
    free(bin);
    sodium_memzero(b64, b64_cap);
    free(b64);
    free(text);
    return ECHO_ERR_CRYPTO;
  }

  sodium_bin2hex(hash_hex, sizeof(hash_hex), hash, sizeof(hash));
  if (sodium_memcmp(hash_hex, expected_hash_hex, 32) != 0) {
    free(bin);
    sodium_memzero(b64, b64_cap);
    free(b64);
    free(text);
    return ECHO_ERR_CORRUPTED;
  }

  *out = bin;
  *out_len = bin_len;

  sodium_memzero(b64, b64_cap);
  free(b64);
  free(text);
  return ECHO_OK;
}

static const echo_stego_codec_t ECHO_STEGO_TEXT_PT = {
    .extension = ".txt",
    .encode = echo_text_pt_encode,
    .decode = echo_text_pt_decode};

const echo_stego_codec_t *echo_text_carrier_codec(void) {
  return &ECHO_STEGO_TEXT_PT;
}
