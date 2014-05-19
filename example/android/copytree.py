#!/usr/bin/python
import os, sys, shutil

ignored_names = (".DS_Store", "Thumbs.db", "Desktop.ini")

# Copy tree, copying files only if they have changed.
# TODO: Add error checking. See copytree example https://docs.python.org/2/library/shutil.html
def copy_tree(src_dir, dst_dir):
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
    for name in os.listdir(src_dir):
        if name in ignored_names:
            continue
        src = os.path.join(src_dir, name)
        dst = os.path.join(dst_dir, name)
        if os.path.isfile(src):
           if not os.path.isfile(dst) or os.stat(src).st_mtime != os.stat(dst).st_mtime:
                print(name)
                shutil.copy2(src, dst)
        elif os.path.isdir(src):
            copy_tree(src, dst)

if len(sys.argv) != 3:
    print("Usage: copytree.py src dst")
    sys.exit(1)

copy_tree(sys.argv[1], sys.argv[2]);