add_library(app_backend
        model.cpp model.h
        file_collector.cpp file_collector.h
        scan_workflow.h
        file_name_scan_workflow.cpp file_name_scan_workflow.h
        file_content_scan_workflow.cpp file_content_scan_workflow.h
        file_comparator.cpp file_comparator.h
        file_hasher.h
        scanner.cpp scanner.h
        scan_request.h
        scan_progress.h

        scan_summary/scan_summary.h
        scan_summary/file_name_scan_summary.h
        scan_summary/file_content_scan_summary.h
        scan_summary/scan_summary_logger.h

        types/duplicate_group.h
        types/file_record.h
        types/scan_outcome.h
        types/scan_phase.h
        types/scan_result.h
        types/scan_type.h
)

target_include_directories(app_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(app_backend
        PUBLIC
        Qt6::Core
        PRIVATE
        Qt6::Concurrent
)
