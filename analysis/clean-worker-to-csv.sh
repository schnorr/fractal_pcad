#!/usr/bin/bash
OUTPUT=workers.csv
echo 'worker,duration,pixels,depth,case,nodes,granularity,repetition' > $OUTPUT
for f in $(find experiment -name *worker*.txt)
do
    KEY=$(echo $f | sed -e 's#/#-#g')
    cat $f | \
	grep -v TOTAL | \
	sed \
	    -e 's/.*WORKER_//' \
	    -e 's/_PAYLOAD]: /,/' \
	    -e 's/ //g' \
	    -e "s/$/,$KEY/" \
	    -e 's/experiment-results_raw-//' \
	    -e 's/-n/,/' \
	    -e 's/-g/,/' \
	    -e 's/-r/,/' \
	    -e 's/-worker_.*$//'
done >> $OUTPUT
