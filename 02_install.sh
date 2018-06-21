OLD_PATH=${PATH}
PWD=$(pwd)

source ../../env.sh

set -x
sudo mkdir -p $NFS_EXPORTED_ROOTFS_ROOT/$(pwd)
sudo chown -R $USER:$USER $NFS_EXPORTED_ROOTFS_ROOT/$(pwd)
cp -r * $NFS_EXPORTED_ROOTFS_ROOT/$(pwd)
set +x

PATH=${OLD_PATH}
cd ${PWD}
