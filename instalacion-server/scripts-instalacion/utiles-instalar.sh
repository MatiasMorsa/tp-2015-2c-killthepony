#!/bin/bash

cp -a /home/utnso/Escritorio/git/tp-2015-2c-killthepony/instalacion-server/utiles/.  /home/utnso/Escritorio/git/tp-2015-2c-killthepony/utiles/

cd /home/utnso/Escritorio/git/tp-2015-2c-killthepony/utiles/Debug

make clean
make

export LD_LIBRARY_PATH=/home/utnso/Escritorio/git/tp-2015-2c-killthepony/utiles/Debug
echo $LD_LIBRARY_PATH

echo "fin instalacion utilesutilesutilesutilesutilesutiles"

cd /home/utnso/Escritorio/git/tp-2015-2c-killthepony/instalacion-server/scripts-instalacion

