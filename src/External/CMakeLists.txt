add_subdirectory(Batsched)
add_subdirectory(OpenXLSX)

add_library(External INTERFACE)

target_link_libraries(External INTERFACE
    Batsched
    OpenXLSX::OpenXLSX
)