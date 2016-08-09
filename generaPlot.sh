#!/bin/bash

### Script que genera una gráfica de resultados ###

rm -f resultados.plt
{
echo \
"set title \"Tiempo de ejecución frente a Número de procesos hijo utilizados\"
set xlabel \"Nº procs. hijo\"
set ylabel \"Tiempo (s)\"
plot \"-\"  title \"-\" with linespoints"

cat resultados.txt

echo \
"e
pause -1"

}>> resultados.plt