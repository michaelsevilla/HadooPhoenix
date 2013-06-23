#! /bin/bash

usage() {
        echo "USAGE: ./cluster_status.sh --<option> [<arg1> --<option> <arg2> ...]"
        echo "Options:"
        echo -e "\t-p, --ping\t ping the nodes"
        echo -e "\t-l, --lsmnt\t list the mount directory (/mnt)"
        echo -e "\t-t, --tar\t deploy a tar"
        echo -e "\t-m, --mkdir\t make the /mnt/vol1/msevilla directories"
        echo -e "\t-u, --umount\t unmount volumes"
        echo -e "\t-d, --diskusage\t print the disk usage"
}

PING=0
LS_MNT=0
MKDIR=0
MKDIRDIR=
TAR=0
UMOUNT=0
DISKUSAGE=0

while true; do
        case $1 in
                -h|--help)
                        usage
                        exit 0
                        ;;
                -p|--ping)
			PING=1
                        shift
			;;
                -l|--lsmnt)
			LS_MNT=1
                        shift
                        ;;
                -m|--mkdir)
			MKDIR=1
			MKDIRDIR=${2}
                        shift
			;;
                -t|--tar)
			TAR=1
                        shift
			;;
                -u|--umount)
			UMOUNT=1
                        shift
			;;
                -d|--diskusage)
			DISKUSAGE=1
                        shift
			;;
               --)
                        shift
                        break
                        ;;
                *)      
                        shift
                        break
                        ;;
        esac
        shift
done

n=0
for i in 0 2 4 7 8 10 11 12 13 14 15 16 17 18 19 21 22 30 31 34 44 24 25 27 28 29 35 37 38 40 41
#for i in 44
#for i in 24 25 27 28 29 37 38 40 44
#for i in 0
do 
	echo "issdm-${i} status..."
	if [ ${PING} == 1 ] ; then ping -c 1 issdm-${i}; fi
	if [ ${LS_MNT} == 1 ] ; then 
		echo ===================================
		ssh issdm-${i} -t "\
		ls -alh /mnt/vol1/;\
		mount | grep vol;"; 
		echo ===================================
		echo 
	fi
	if [ ${MKDIR} == 1 ] ; then
		ssh issdm-${i} -t "\
		sudo rm -fr /mnt/vol1/msevilla;\
		sudo rm -fr /mnt/vol2/msevilla;\
		sudo rm -fr /mnt/vol3/msevilla;\
		sudo mkdir -p /mnt/vol1/msevilla/hadoop/tmp;\
		sudo mkdir /mnt/vol2/msevilla;\
		sudo mkdir /mnt/vol3/msevilla;\
		sudo chown -R msevilla:msevilla /mnt/vol1/msevilla;\
		sudo chown -R msevilla:msevilla /mnt/vol2/msevilla;\
		sudo chown -R msevilla:msevilla /mnt/vol3/msevilla;"
	fi
	if [ ${TAR} == 1 ] ; then
		sftp -b sftp_commands issdm-${i}
		ssh issdm-${i} -t "\
		cd /mnt/vol1/msevilla;
		rm -r hadoop_src;
		tar xzvf msevilla2_hadoop.tar.gz;"
	fi
	if [ ${UMOUNT} == 1 ] ; then
		ssh issdm-${i} -t "\
		sudo umount /mnt/vol1;
		sudo umount /mnt/vol2;
		sudo umount /mnt/vol3;"
	fi
	if [ ${DISKUSAGE} == 1 ] ; then 
		echo ===================================
		ssh issdm-${i} -t "\
		df -H"; 
		echo ===================================
		echo 
	fi
	n=$((${n}+1))
done

echo ""
echo "Total nodes: ${n}"
