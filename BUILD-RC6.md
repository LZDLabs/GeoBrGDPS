# Build RC6 — toolchain

Compilação de 2026-10-31: Android arm64-v8a, Release, Geometry Dash 2.2081, Geode 5.9.0.

| Componente | Origem | Versão |
|---|---|---|
| Android NDK | camada da imagem `cimg/android:2025.08.1-ndk` (Docker Hub), SHA-256 da camada `706d7f98…a8e2ea` | 28.2.13676358 (r28c), Clang 19.0.1 |
| Geode SDK | `github.com/geode-sdk/geode` tag `v5.9.0` | `f326379a1a9ca90201903e009137e3da1aa4c375` |
| Bindings | `github.com/geode-sdk/bindings` | `7f6c2a75742856de88dad354e576dcff8a28e881` (mesmo do build 1.8); Codegen compilado localmente (`-DSKIP_BUILDING_CODEGEN=OFF`) |
| Loader para link | compilado do SDK v5.9.0 (`Geode.android64.so`) | usado só para resolver símbolos; não é distribuído |
| GMD API (link) | `github.com/HJfod/GMD-API` main (mod.json 1.5.0) | `.so` compilado localmente só para link |
| More Icons (headers) | `github.com/hiimjustin000/MoreIcons` tag `v2.1.3` | API por eventos, sem link |
| Empacotamento | `tools/geode_pack.py` (CLI offline) + shim `geode` | layout do Geode CLI 3.9.0 |

Ajustes locais no toolchain (não fazem parte do mod):
- `NdkStdCompat.hpp` só atua em unidades C++20+ (evita conflito com `libunwindstack`, C++17). A cópia em `src/compat/` recebeu o mesmo guarda.
- Loader: `StringBuffer title;` → `StringBuffer<> title;` em dois arquivos de UI do loader (CTAD de alias com Clang 19); `-DGEODE_DISABLE_CLI_CALLS=ON` só para o loader.

Comandos:
```
tools/build_android64.sh <fonte> <build>     # configura e compila; gera <build>/geobr.geobrmod.geode
python3 tests/package_v1.py <build>/geobr.geobrmod.geode
python3 tests/static_validation.py
python3 tests/android_symbols.py $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm $GEODE_SDK/loader/include/link/android64/libcocos2dcpp.so
cmake -S tests -B build-host -DGEOBR_JSON_SOURCE=<geode-sdk/json v3.3.1> && ninja -C build-host
```
