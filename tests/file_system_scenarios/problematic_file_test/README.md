# Problematic file manual test

This Windows-only scenario verifies how the application behaves when one file is already unreadable when a scan starts.
The ACL is applied at runtime because Git does not preserve NTFS permissions.

## Prepare the scenario

From the directory tests\file_system_scenarios\problematic_file_test, run:

```powershell
powershell.exe -ExecutionPolicy Bypass -File configure-unreadable-file.ps1
```

The script applies a read-deny rule for the current Windows user and verifies that `root\unreadable.txt` cannot be opened.

## Run the scan

Start Duplicate File Finder and scan this directory:

```text
tests\file_system_scenarios\problematic_file_test\root
```

Run both scan modes if both workflows need to be inspected manually.

Expected behavior:

- The scan completes instead of failing.
- `dir1\duplicate.txt` and `dir2\duplicate.txt` form one duplicate group.
- `unreadable.txt` is excluded from the results and scan totals.
- The scan summary reports one problematic file and two scanned files.
- The application log identifies the absolute path of `unreadable.txt` as a file that could not be opened.
- The application displays the "Scan completed with warnings" dialog and directs the user to the log.

## Restore access

After the manual test, run:

```powershell
powershell.exe -ExecutionPolicy Bypass -File configure-unreadable-file.ps1 -Restore
```

The script removes the deny rule and verifies that the file is readable again. Restore access before editing, deleting, or replacing the fixture.
