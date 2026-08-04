# Result presentation fixture

Scan this directory directly. Both scan workflows should produce exactly 10 duplicate groups
containing 59 duplicate files in total.

| File name | Files in group |
| --- | ---: |
| `a.txt` | 2 |
| `notes.md` | 3 |
| `quarterly_report_2026_final.txt` | 4 |
| `configuration_backup.ini` | 5 |
| `second_five_member_group.log` | 5 |
| `photo_catalog_metadata.json` | 6 |
| `shared_application_cache.bin` | 7 |
| `archive_inventory_very_long_filename_for_column_clipping_test.csv` | 8 |
| `duplicate_document_with_a_name_that_should_be_elided_in_the_compact_column.txt` | 9 |
| `common_payload.dat` | 10 |

Every copy within a filename group also has identical content. Content is unique between groups,
so the same group sizes are expected for both filename and content scans. Folder paths deliberately
include short, long, deeply nested, and space-containing variants.
