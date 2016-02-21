set terminal pngcairo size 700,524 enhanced font 'Verdana,10'
set output 'res.png'
plot 'res.dat' using 0:1 title "x(t)" with lines, '' using 0:2 title "v(t)" with lines, '' using 0:3 title "u(t)" with lines

