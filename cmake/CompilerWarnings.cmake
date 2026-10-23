# Target-scoped compiler warning configuration for GCC, Clang and MSVC.
function(itpixel_set_warnings target)
    set(gcc_warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wformat=2
        -Wdouble-promotion)
    set(msvc_warnings
        /W4
        /permissive-
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /w14296
        /w14311
        /w14545
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928)

    if(MSVC)
        target_compile_options(${target} PRIVATE ${msvc_warnings})
    else()
        target_compile_options(${target} PRIVATE ${gcc_warnings})
    endif()

    if(ITPIXEL_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
