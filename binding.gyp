{
  "variables": {
    "cflags": "-fexceptions",
    "cflags_cc": "-fexceptions"
  },
  "targets": [
    {
      "target_name": "cubesql_addon",
      "sources": [
        "cubesql_addon.cpp",
        "CubeSQL-SDK/C_SDK/cubesql.c",
        "CubeSQL-SDK/C_SDK/crypt/aescrypt.c",
        "CubeSQL-SDK/C_SDK/crypt/aeskey.c",
        "CubeSQL-SDK/C_SDK/crypt/aestab.c",
        "CubeSQL-SDK/C_SDK/crypt/base64.c",
        "CubeSQL-SDK/C_SDK/crypt/pseudorandom.c",
        "CubeSQL-SDK/C_SDK/crypt/sha1.c"
      ],
      
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "GCC_ENABLE_CPP_RTTI": "YES"
        },
    "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "CubeSQL-SDK/C_SDK",
        "CubeSQL-SDK/C_SDK/crypt"
    ],
      "libraries": [
        "-L/opt/homebrew/opt/libressl/lib",
        "-lssl",
        "-lcrypto",
        "-ltls"
      ],
      "cflags": [
        "-I/opt/homebrew/opt/libressl/include",
        "-fexceptions"
      ],
      "cflags_cc": [
        "-fexceptions"
      ],
      "defines": [
        "NODE_ADDON_API_CPP_EXCEPTIONS"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ]
    }
  ]
}