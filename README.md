# Photontool

Extract information from your `.photon` files. Including all layer image data.

This is a proof of concept for an API.

It is written in plain C and multi-threaded, so it runs quit fast.

## Installation
```
sudo make install
```

## Usage

Simply run 
```
photontool -x -i INPUTFILE -o LAYERBASENAME
```

It will create 3 text files: the header, the two preview headers.

Then it will create two png files for the 2 preview images.

Finally it will output all layers as png files. Anti-Aliasing is fully supported. Filename are prepended by `LAYERBASENAME`.

*It does not work with photon v1 files.*

_The naming of the files is still completely messed up. Don't complain. This is work in progress._

It is also handy for checking whether a conversion tool supports anti aliasing.
