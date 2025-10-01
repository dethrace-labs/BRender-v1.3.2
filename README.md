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

## Regenerate generated files if needed
_These are not required unless you are adding new token types or adding/change BRender internal function args_

```sh
gcc -D__CLASSGEN__ -E "core/fw/dev_objs.hpp" > /tmp/dev_objs.tmp
perl make/classgen.pl < /tmp/dev_objs.tmp > core/fw/ddi/dev_objs.cgh
cd core/inc
perl ../../make/tokgen.pl pretok.tok
```

If you change a software renderer primitives config file:
```sh
perl drivers/pentprim/infogen.pl < drivers/pentprim/prim_p8.ifg > drivers/pentprim/prim_p8.c
perl drivers/pentprim/infogen.pl < drivers/pentprim/prim_l8.ifg > drivers/pentprim/prim_l8.c
perl drivers/pentprim/infogen.pl image_suffix=f float_components < drivers/pentprim/prim_t8.ifg > drivers/pentprim/prim_t8f.c
perl drivers/pentprim/infogen.pl image_suffix=x < drivers/pentprim/prim_t8.ifg > drivers/pentprim/prim_t8x.c
```


## License

This is released, as is the original open source release, under the MIT license.
