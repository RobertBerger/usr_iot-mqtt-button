HERE=$(pwd)

source ../../env.sh

set -x
make clean
make
set +x

cd ${HERE}
