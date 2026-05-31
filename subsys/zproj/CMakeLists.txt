# SPDX-License-Identifier: MIT

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZPROJ_SHELL zproj_shell.c)
zephyr_library_sources_ifdef(CONFIG_ZPROJ_RUNTIME_THREAD zproj_runtime_thread.c)
zephyr_library_sources_ifdef(CONFIG_ZPROJ_WATCHDOG zproj_watchdog.c)
