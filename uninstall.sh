#!/bin/bash
MODULE=hid-ghlive

echo "* Unloading current driver module"
modprobe -r ${MODULE} || true

echo "* Looking for registered instances"
installed=($(dkms status | grep -E "^${MODULE}," | tr -d ',' | awk '{print $2}'))
if [ ${#installed[@]} -gt 0 ];
then
		for i in "${installed[@]}"
		do
				echo "* Uninstalling and removing ${MODULE} version ${i} from DKMS"
				dkms remove "${MODULE}/${i}" --all

				[ -e /usr/src/${MODULE}-${i} ] && \
						echo "  Removing folder from /usr/src" && \
						rm --recursive "/usr/src/${MODULE}-${i}/"
		done
fi
