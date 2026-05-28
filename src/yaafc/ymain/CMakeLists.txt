# yaafc/ymain — driver entry point.
#
# The `yaafc` executable is a thin shell: yargv parsing, engine
# construction, and a small subcommand router. Everything substantive
# lives in the plugins / runtime. The main() lived under bin/ for a
# while; it's now a regular module under src/yaafc/<name>/ so it can
# participate in the build the same way every other yaafc module
# does, and is includable as an object library if a downstream binary
# (tests, embedding host) wants to swap the entry point.

add_library(yaafc_ymain OBJECT ymain.c)
target_link_libraries(yaafc_ymain PUBLIC yaafc_runtime)
# ymain.c uses a couple of plugin public stubs (the legacy `client`
# smoke). All public headers live under
# <repo>/include/yaafc/plugin/<name>/, which is part of yaafc_runtime's
# PUBLIC include dirs — nothing to add here.
