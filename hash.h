#ifndef HASH_H_
#define HASH_H_

#include <stdint.h>

extern "C" {

uint64_t hash3(uint8_t *data, uint64_t length, uint64_t level);

uint64_t hash_string(const char *data, size_t length) {
  return hash3(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(data)),
      length, 0xfeeffeef);
}

}  // extern "C"


#endif  // HASH_H_
