#include "echo/stego.h"

#include <ctype.h>
#include <limits.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char ECHO_B64_URLSAFE_ALPHABET[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const char *const ECHO_PT_WORDS[64] = {
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
    if (strcmp(token, ECHO_PT_WORDS[i]) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static echo_error_t echo_text_pt_encode(const uint8_t *in, size_t in_len,
                                        uint8_t **out, size_t *out_len) {
  unsigned char hash[16];
  char hash_hex[33];
  size_t b64_cap;
  char *b64 = NULL;
  size_t body_cap;
  char *body = NULL;
  size_t body_len = 0;
  size_t i;

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

  b64_cap = sodium_base64_ENCODED_LEN(in_len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
  b64 = (char *)malloc(b64_cap);
  if (!b64) {
    return ECHO_ERR_NOMEM;
  }
  sodium_bin2base64(b64, b64_cap, in, in_len,
                    sodium_base64_VARIANT_URLSAFE_NO_PADDING);

  body_cap = 256 + (strlen(b64) * 16);
  body = (char *)malloc(body_cap);
  if (!body) {
    sodium_memzero(b64, b64_cap);
    free(b64);
    return ECHO_ERR_NOMEM;
  }

  body_len += (size_t)snprintf(body + body_len, body_cap - body_len,
                               "ECHO-STEGOTXT1\nlen:%zu\nhash:%s\n\n",
                               in_len, hash_hex);

  {
    size_t wcount = 0;
    for (i = 0; b64[i] != '\0'; i++) {
      int idx = echo_b64_index(b64[i]);
      const char *word;
      size_t need;

      if (idx < 0) {
        free(body);
        sodium_memzero(b64, b64_cap);
        free(b64);
        return ECHO_ERR_INTERNAL;
      }

      word = ECHO_PT_WORDS[idx];
      need = strlen(word) + 2;

      if (body_len + need + 64 >= body_cap) {
        size_t new_cap = body_cap * 2;
        char *new_body = NULL;
        if (new_cap < body_cap) {
          free(body);
          sodium_memzero(b64, b64_cap);
          free(b64);
          return ECHO_ERR_NOMEM;
        }
        new_body = (char *)realloc(body, new_cap);
        if (!new_body) {
          free(body);
          sodium_memzero(b64, b64_cap);
          free(b64);
          return ECHO_ERR_NOMEM;
        }
        body = new_body;
        body_cap = new_cap;
      }

      if (wcount % 12 == 0) {
        body_len += (size_t)snprintf(body + body_len, body_cap - body_len,
                                     "hoje ");
      }

      memcpy(body + body_len, word, strlen(word));
      body_len += strlen(word);

      wcount++;
      if (wcount % 12 == 0) {
        body[body_len++] = '.';
        body[body_len++] = '\n';
      } else {
        body[body_len++] = ' ';
      }
    }

    if (wcount % 12 != 0) {
      body[body_len++] = '.';
      body[body_len++] = '\n';
    }
  }

  body[body_len] = '\0';

  *out = (uint8_t *)body;
  *out_len = body_len;

  sodium_memzero(b64, b64_cap);
  free(b64);
  return ECHO_OK;
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

    while (*cursor && !isspace((unsigned char)*cursor) && tlen + 1 < sizeof(token)) {
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
    .extension = ".txt", .encode = echo_text_pt_encode, .decode = echo_text_pt_decode};

const echo_stego_codec_t *echo_stego_codec_for_object_name(
    const char *object_name
) {
  size_t name_len;
  size_t ext_len;

  if (!object_name) {
    return NULL;
  }

  name_len = strlen(object_name);
  ext_len = strlen(ECHO_STEGO_TEXT_PT.extension);

  if (name_len >= ext_len &&
      memcmp(object_name + (name_len - ext_len), ECHO_STEGO_TEXT_PT.extension,
             ext_len) == 0) {
    return &ECHO_STEGO_TEXT_PT;
  }

  return NULL;
}
