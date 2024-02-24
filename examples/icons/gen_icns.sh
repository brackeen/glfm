#/bin/bash
# This script generates an icns file (for iOS and macOS) from a png file.
# Requires macOS (sips and iconutil)

if [ -z "$1" || -z $2 ]; then
    echo "Usage: $0 <icon_name> <output_dir>"
    exit 1
fi

if ! [ -f "$1.png" ]; then
    echo "Error: $1.png does not exist"
    exit 2
fi

mkdir -p $1.iconset
mkdir -p $2

sips -z 16 16     $1.png --out $1.iconset/icon_16x16.png
sips -z 32 32     $1.png --out $1.iconset/icon_16x16@2x.png
sips -z 32 32     $1.png --out $1.iconset/icon_32x32.png
sips -z 64 64     $1.png --out $1.iconset/icon_32x32@2x.png
sips -z 128 128   $1.png --out $1.iconset/icon_128x128.png
sips -z 256 256   $1.png --out $1.iconset/icon_128x128@2x.png
sips -z 256 256   $1.png --out $1.iconset/icon_256x256.png
sips -z 512 512   $1.png --out $1.iconset/icon_256x256@2x.png
sips -z 512 512   $1.png --out $1.iconset/icon_512x512.png
sips -z 1024 1024 $1.png --out $1.iconset/icon_512x512@2x.png

iconutil -c icns --output $2/$1.icns $1.iconset
rm -R $1.iconset
