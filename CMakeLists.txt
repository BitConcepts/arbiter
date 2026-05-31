# SPDX-License-Identifier: MIT

if(CONFIG_ZPROJ)
  zephyr_library()
  zephyr_library_sources(
    lib/ARBITER_engine.c
    lib/ARBITER_fact_store.c
    lib/ARBITER_eval.c
    lib/ARBITER_trace.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_BLOB_LOADER
    lib/ARBITER_blob.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_ACTION_DISPATCHER
    lib/ARBITER_action.c
  )

  zephyr_include_directories(include)

  add_subdirectory_ifdef(CONFIG_ZPROJ subsys/arbiter)
endif()
