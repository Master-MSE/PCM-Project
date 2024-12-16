make clean
make tspcc

FILENAME=dj38.tsp

hyperfine "./tspcc -f $FILENAME -t 2" \
            "./tspcc -f $FILENAME -t 4" \
            "./tspcc -f $FILENAME -t 6" \
            "./tspcc -f $FILENAME -t 8" \
            --export-json ./export.json

python3 ./graphs.py