#! /bin/bash

#for i in 0 2 4 7 8 11 13 14 16 17 18 19 21 22 30
for i in 24 25 27 28 29 37 38 40 44 
#for i in 0 2 4 7 8 10 11 12 13 14 15 16 17 18 19 21 22 30 31 34 44 47
#for i in 0 
do
  # /dev/sdb
  echo "issdm-$i"
  #echo "sudo parted -s /dev/sdb mklabel gpt"
  ssh issdm-$i "sudo parted -s /dev/sdb mklabel gpt"
  #echo "sudo parted -s /dev/sdb mkpart primary 0% 100%"
  ssh issdm-$i "sudo parted -s /dev/sdb mkpart primary 0% 100%"
  #echo "sudo mkfs.ext4 /dev/sdb1"
  ssh issdm-$i "sudo mkfs.ext4 /dev/sdb1"
  #echo "ls -la /dev/disk/by-uuid | grep sdb1 | awk '{print \$9}' | xargs -I % echo -e \"UUID=%\t/mnt/vol1\text4\trw,user\t0\t2\" | sudo tee --append /etc/fstab"
  ssh issdm-$i "ls -la /dev/disk/by-uuid | grep sdb1 | awk '{print \$9}' | xargs -I % echo -e \"UUID=%\t/mnt/vol1\text4\trw,user,exec\t0\t2\" | sudo tee --append /etc/fstab"

  # /dev/sdc
  ssh issdm-$i "sudo parted -s /dev/sdc mklabel gpt"
  ssh issdm-$i "sudo parted -s /dev/sdc mkpart primary 0% 100%"
  ssh issdm-$i "sudo mkfs.ext4 /dev/sdc1"
  ssh issdm-$i "ls -la /dev/disk/by-uuid | grep sdc1 | awk '{print \$9}' | xargs -I % echo -e \"UUID=%\t/mnt/vol2\text4\trw,user,exec\t0\t2\" | sudo tee --append /etc/fstab"

  # /dev/sdd
  ssh issdm-$i "sudo parted -s /dev/sdd mklabel gpt"
  ssh issdm-$i "sudo parted -s /dev/sdd mkpart primary 0% 100%"
  ssh issdm-$i "sudo mkfs.ext4 /dev/sdd1"
  ssh issdm-$i "ls -la /dev/disk/by-uuid | grep sdd1 | awk '{print \$9}' | xargs -I % echo -e \"UUID=%\t/mnt/vol3\text4\trw,user,exec\t0\t2\" | sudo tee --append /etc/fstab"

  ssh issdm-$i "sudo mount -a"
done

