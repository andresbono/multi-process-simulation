#!/bin/bash

# Sup. directorio de ns3 añadido a PATH

MAXPROC=10

FICHERO="resultados.txt"

rm -f $FICHERO


for maxProc in `seq 1 $MAXPROC`; do
  /usr/bin/time -f "$maxProc %e" -ao $FICHERO waf --run "practica05_proc --maxProc=$maxProc"
done


. generaPlot.sh
