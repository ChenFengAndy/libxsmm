#!/bin/bash

if [ $# -ne 3 ]
then
  echo "Usage: `basename $0` mb iters numa; using default values: 256 100 1"
  MB=256
  ITERS=100
  NUMA=1
else
  MB=$1
  ITERS=$2
  NUMA=$3
fi

# Usage: ./conv_knl_jit iters inpWidth inpHeight minibatch nIfm nOfm kw kh pad stride splits
numactl --membind=${NUMA} ./fwd_layer_example ${ITERS} 231 231  ${MB}    3   96 11 11 0 4 1
numactl --membind=${NUMA} ./fwd_layer_example ${ITERS}  28  28  ${MB}   96  256  5  5 0 1 1
numactl --membind=${NUMA} ./fwd_layer_example ${ITERS}  14  14  ${MB}  256  512  3  3 1 1 1
numactl --membind=${NUMA} ./fwd_layer_example ${ITERS}  14  14  ${MB}  512 1024  3  3 1 1 1
numactl --membind=${NUMA} ./fwd_layer_example ${ITERS}  14  14  ${MB} 1024 1024  3  3 0 1 1
