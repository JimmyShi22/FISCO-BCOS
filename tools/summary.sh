#!/bin/bash
execT=$(cat $1 |grep -iaE "asyncExecuteBlock success,sysBlock|BlockSync: applyBlock success" |awk -F 'timeCost=' '{print $2}' |awk -F ',' '{print $1}' |awk '{sum+=$1} END {print sum/NR}')
waitT=$(cat $1 |grep -ia "waitT" |awk -F 'waitT=' '{print $2}' |awk '{sum+=$1} END {print sum/NR}')
commitT=$(cat $1 |grep -iaE "CommitBlock success" |awk -F 'timeCost=' '{print $2}' |awk '{sum+=$1} END {print sum/NR}')
beforeSeal=$(cat $1 |grep -iaE "generate prop" |awk -F '=|,' '{print ($3-$5)}'  |awk '{sum+=$1} END {print sum/NR}')
echo "--------work flow-------- "
echo -e "execT\t\t" ${execT}
echo -e "waitT\t\t" ${waitT}
echo -e "commitT\t\t" ${commitT}
echo -e "beforeSeal\t" ${beforeSeal}
echo "------tx pool (tps)------- "
cat $1 |grep -iaE "tx_pool_in" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "in      \t", sum/NR}'
cat $1 |grep -iaE "tx_pool_seal" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "seal    \t", sum/NR}'
cat $1 |grep -iaE "tx_pool_rm" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "rm      \t", sum/NR}'
echo "------storage (tps)------- "
cat $1 |grep -iaE "create_table_rate_collector" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "create   \t", sum/NR}'
cat $1 |grep -iaE "open_table_rate_collector" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "open    \t", sum/NR}'
cat $1 |grep -iaE "get_row_table_rate_collector" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "get      \t", sum/NR}'
cat $1 |grep -iaE "set_row_table_rate_collector" |grep -v "lastQPS(request/s)=0" |awk -F 'lastQPS\\(request\/s\\)=' '{print $2}' |awk '{sum+=$1} END {print "set      \t", sum/NR}'
echo "-------------------------- "
blkTxs=$(cat $1 |grep "Report,sea" |awk -F 'txs=' '{print $2}' |awk -F ',' '{print $1}'|awk '{sum+=$1} END {print sum/NR}')
echo -e "blkTxs\t\t" ${blkTxs}
cat $1 |grep "Report,sea" |awk -F 'txs=' '{print $2}' |awk -F ',' '{print $1}'|awk '{sum+=$1} END {print "totalTxs \t", sum}'
maxGap=$(echo "$execT + $waitT"|bc)
tps=$(echo "1000 * $blkTxs / $maxGap"|bc)
echo -e "tps\t\t" ${tps}


