#!/bin/bash

# ./extract.sh silesia

source_directory=$1
target_directory=${source_directory}_tmp

cd ${source_directory}
wget https://sun.aei.polsl.pl//~sdeor/corpus/dickens.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/mozilla.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/mr.bz2¬
wget https://sun.aei.polsl.pl//~sdeor/corpus/nci.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/ooffice.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/osdb.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/reymont.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/samba.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/sao.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/webster.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/xml.bz2
wget https://sun.aei.polsl.pl//~sdeor/corpus/x-ray.bz2
cd ..

mkdir ${target_directory}

if [ ! -d "$source_directory" ]; then
    echo "Error: Directory '$source_directory' does not exist."
    exit 1
fi

for file in $source_directory/*; do
    # Check if it is a regular file
    if [ -f "$file" ]; then
        echo "Decompressing $file into $target_directory"
        # Extract the filename from the path
        filename=$(basename "$file")
        # Decompress the file into the target directory
        bzip2 -dkc "$file" > "$target_directory/${filename%.bz2}"
    fi
done
