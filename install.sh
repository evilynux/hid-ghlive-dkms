#!/bin/bash -e
VERSION=0.1
MODULE=hid-ghlive
DESTDIR="/usr/src/${MODULE}-${VERSION}"

echo "* Copying module to '${DESTDIR}'"
cp --recursive "${MODULE}" "${DESTDIR}"

cd "${DESTDIR}"
sed -i 's/"@DO_NOT_CHANGE@"/"'"${VERSION}"'"/g' dkms.conf src/hid-ghlive.c

echo "* Looking for registered instances"
installed=($(dkms status | grep -E "^${MODULE}," | tr -d ',' | awk '{print $2}'))
if [ ${#installed[@]} -gt 0 ];
then
		for i in "${installed[@]}"
		do
				echo "* Uninstalling and removing ${MODULE} version ${i} from DKMS"
				dkms remove "${MODULE}/${i}" --all
		done
fi

echo "* Adding module to DKMS"
dkms add "${MODULE}/${VERSION}"

echo "* Installing module (using DKMS)"
dkms install "${MODULE}/${VERSION}"

