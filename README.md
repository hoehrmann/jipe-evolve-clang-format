# jipe-evolve-clang-format

## Dependencies

* `apt install libclang-dev`

* `apt install libgit2-dev`

* https://github.com/nlohmann/json (included in `ext/`)

## Build

Something like this should work after installing dependencies.

```zsh
git clone ...

take jipe-evolve-clang-format/build

CC=/usr/bin/clang-7 CXX=/usr/bin/clang++-7 cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
```

## License

MIT.
