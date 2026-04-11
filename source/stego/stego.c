#include "echo/stego.h"

#include "echo/image_carrier.h"
#include "echo/text_carrier.h"

#include <string.h>

static const echo_stego_codec_t *echo_codec_for_extension(const char *object_name,
                                                          const char *extension,
                                                          const echo_stego_codec_t *codec) {
  size_t name_len;
  size_t ext_len;

  if (!object_name || !extension || !codec) {
    return NULL;
  }

  name_len = strlen(object_name);
  ext_len = strlen(extension);

  if (name_len < ext_len) {
    return NULL;
  }

  if (memcmp(object_name + (name_len - ext_len), extension, ext_len) != 0) {
    return NULL;
  }

  return codec;
}

const echo_stego_codec_t *echo_stego_codec_for_object_name(
    const char *object_name
) {
  const echo_stego_codec_t *codec;

  codec = echo_text_carrier_codec();
  codec = echo_codec_for_extension(object_name, codec ? codec->extension : NULL, codec);
  if (codec) {
    return codec;
  }

  codec = echo_image_carrier_codec();
  codec = echo_codec_for_extension(object_name, codec ? codec->extension : NULL, codec);
  if (codec) {
    return codec;
  }

  return NULL;
}
