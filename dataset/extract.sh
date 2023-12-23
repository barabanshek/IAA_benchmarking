#!/bin/bash

# Usage: ./extract.sh

#
# Extract silesia corpus.
#
source_directory='silesia'
target_directory=${source_directory}_tmp

mkdir ${source_directory}
mkdir ${target_directory}

# Download silesia dataset.
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

# Extract.
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

#
# Extract wiki dataset.
#
source_directory='wiki'
target_directory=${source_directory}_tmp

mkdir ${source_directory}
mkdir ${target_directory}

# Download wiki dataset.
cd ${source_directory}
wget https://mattmahoney.net/dc/enwik9.zip
cd ..

# Extract.
unzip ${source_directory}/enwik9.zip -d ${target_directory}/

#
# Extract snapshot dataset.
#
source_directory='snapshots'
target_directory=${source_directory}_tmp
mkdir ${target_directory}

# Extract.
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
