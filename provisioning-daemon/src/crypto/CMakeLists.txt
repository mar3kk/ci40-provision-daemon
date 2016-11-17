set(crypto_source_files
  bigint.c
  rijndael.c
  encoder.c
  diffie_hellman_keys_exchanger.c
)
add_library(crypto ${crypto_source_files})
