make clean
make tspcc
hyperfine "./tspcc -v dj8.tsp" "./tspcc -v dj8.tsp" --export-json ./export.json