set(tmachine_name "vgpu")
set(tmachine_libname "vgpu")
set(tmachine_triple "x86_64-vgpu")
set(elf_machine_id "62")

if(LIBOMPTARGET_DEP_LIBELF_FOUND)
    if(LIBOMPTARGET_DEP_LIBFFI_FOUND)

        libomptarget_say("Building ${tmachine_name} offloading plugin.")

        include_directories(${LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIR})
        include_directories(${LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIR})
        include_directories(${LIBOMPTARGET_INCLUDE_DIR})

        # Define macro to be used as prefix of the runtime messages for this target.
        add_definitions("-DTARGET_NAME=${tmachine_name}")

        # Define macro with the ELF ID for this target.
        add_definitions("-DTARGET_ELF_ID=${elf_machine_id}")

        add_library("omptarget.rtl.${tmachine_libname}" SHARED
                ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadEnvironment.cpp)

        # Install plugin under the lib destination folder.
        install(TARGETS "omptarget.rtl.${tmachine_libname}"
                LIBRARY DESTINATION "${OPENMP_INSTALL_LIBDIR}")

        set_target_properties("omptarget.rtl.${tmachine_libname}" PROPERTIES CXX_STANDARD 20)
        target_compile_options("omptarget.rtl.${tmachine_libname}" PRIVATE "-stdlib=libc++")

        target_link_libraries(
                "omptarget.rtl.${tmachine_libname}"
                elf_common
                ${LIBOMPTARGET_DEP_LIBFFI_LIBRARIES}
                ${LIBOMPTARGET_DEP_LIBELF_LIBRARIES}
                dl
                ${OPENMP_PTHREAD_LIB}
                "-rdynamic"
                c++
                #"-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../exports"
                )

        list(APPEND LIBOMPTARGET_TESTED_PLUGINS
                "omptarget.rtl.${tmachine_libname}")

        # Report to the parent scope that we are building a plugin.
        set(LIBOMPTARGET_SYSTEM_TARGETS
                "${LIBOMPTARGET_SYSTEM_TARGETS} ${tmachine_triple}" PARENT_SCOPE)
        set(LIBOMPTARGET_TESTED_PLUGINS
                "${LIBOMPTARGET_TESTED_PLUGINS}" PARENT_SCOPE)

    else(LIBOMPTARGET_DEP_LIBFFI_FOUND)
        libomptarget_say("Not building ${tmachine_name} offloading plugin: libffi dependency not found.")
    endif(LIBOMPTARGET_DEP_LIBFFI_FOUND)
else(LIBOMPTARGET_DEP_LIBELF_FOUND)
    libomptarget_say("Not building ${tmachine_name} offloading plugin: libelf dependency not found.")
endif(LIBOMPTARGET_DEP_LIBELF_FOUND)
