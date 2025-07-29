# Argonaut Blazing Render (BRender)

This is a fork of the original BRender v1.3.2 open source release at https://github.com/foone/BRender-v1.3.2

Additions:
- CMake support, based on https://github.com/crocguy0688/CrocDE-BRender
- x86 paletted software renderer ported to cross-platform C code
- OpenGL renderer, based on the OpenGL renderer from https://github.com/crocguy0688/CrocDE-BRender
- Virtual framebuffer allowing any windowing backend to be used


## Build
```sh
mkdir build
cd build
cmake -DBRENDER_BUILD_EXAMPLES=on ..
make
```

## Example
Build BRender with examples:

```sh
cmake -DBRENDER_BUILD_EXAMPLES=on ..
```

```sh
./examples/softcube/softcube16
```

## Regenerate generated files
_This is not generally required unless you are adding new token types or adding/change BRender internal function args_

```sh
gcc -D__CLASSGEN__ -E "core/fw/dev_objs.hpp" > /tmp/dev_objs.tmp
perl make/classgen.pl < /tmp/dev_objs.tmp > core/fw/ddi/dev_objs.cgh
cd core/inc
perl make/tokgen.pl pretok.tok
```


## License

This is released, as is the original open source release, under the MIT license.
