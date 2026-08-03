# Scan fixtures

## By file name

Expected duplicate groups:

- `duplicate_three.txt` in `dir1`, `dir2`, and `dir3`
- `duplicate_two.txt` in `dir1` and `dir2`

Files with any other name are intentionally unique and should not appear in
filename-duplicate results.

## By file content

Expected duplicate group:

- `unique_1.txt` in `dir1` and `unique_2.log` in `dir2`

Those two files deliberately have different names but identical contents.
The equal-sized `duplicate_three.txt` files in `dir1` and `dir3` deliberately
have the same size but different contents, so hashing must reject them.
