# Increment-and-Freeze

Reuse-distance / miss-ratio-curve simulator, used by the WiredTiger cache analysis build
(`HAVE_ANALYZE_CACHE`). WiredTiger calls it through the C shim in `dist/includes/iaf_api.h`
(`Iaf_create`, `Iaf_write`, `Iaf_grab_id`, `Iaf_stringify`, ...).

See `scripts/import.sh` for the upstream revision this was vendored from.

## Licensing

Upstream is **GPL-2.0** (`dist/LICENSE.txt`), which is not compatible with the terms the
server is distributed under. This vendored copy is for local cache-analysis builds only and
must not be part of anything that gets shipped. It is also not registered in the SBOM, and
was not vendored through the `mongodb-forks` process that `src/third_party/README.md`
requires for a real third-party component.

## Vendored subset

Only the files needed by `:iaf_api` are present -- the four translation units that upstream's
`CMakeLists.txt` lists in `IAF_SRCS`, plus their header closure. The simulator drivers
(`simulation.cc`, `dump_traces.cc`), the order-statistic-tree cache simulators, and the test
binaries are not vendored.

## Notes

- `dist/src/increment_and_freeze.cc` carries OpenMP pragmas. They are inert unless the file is
  compiled with `-fopenmp`, which this build does not do, matching upstream's CMake build.
- Upstream's own `BUILD.bazel` is not reused: it compiles the library with `-fopenmp`, links
  `-lgomp`, and its test targets depend on `@googletest`.
