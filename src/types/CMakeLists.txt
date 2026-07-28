add_library(types
        scan_type.h
        file_record.h
        scan_result.h)

target_link_libraries(types
        PUBLIC
        Qt6::Core)
