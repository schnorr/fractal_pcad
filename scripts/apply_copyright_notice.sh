#!/usr/bin/bash
LICENSE_HEADER=LICENSE_header.c
FILES=$(find ../src ../include -type f -name "*.c" -o -name "*.h" | sort)
for file in $FILES; do
    # remove current license header
    # we assume the header is the first thing in that file
    # so we remove everything up to the first preprocessor directive */
    echo $file
    UP_TO_LINE=$(grep -n '^#' $file | head -n1 | cut -d: -f1)
    UP_TO_LINE=$((${UP_TO_LINE} - 1))
    echo $UP_TO_LINE
    if [ ${UP_TO_LINE} -gt 0 ]; then
	echo "sed -i "\'1,${UP_TO_LINE}d\'" $file"
	echo ${UP_TO_LINE}
	# apply the new licence header
	tmpfile=$(mktemp)
	cat ${LICENSE_HEADER} $file > $tmpfile
	mv -v "$tmpfile" "$file"
    fi
done
