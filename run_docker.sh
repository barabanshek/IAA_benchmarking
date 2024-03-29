#!/usr/bin/env bash

rm -fr out/
mkdir out/

docker run -it --device=/dev/iax \
           --mount type=bind,source="$(pwd)"/dataset,target=/IAA_benchmarking/dataset \
           --mount type=bind,source="$(pwd)"/out,target=/IAA_benchmarking/out \
           barabanshik/sabre_iaa_benchmark:latest \
           $1 $2
