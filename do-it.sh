make clean
make tspcc

FILENAME=dj38.tsp

hyperfine "./tspcc -f $FILENAME -t 2" \
            "./tspcc -f $FILENAME -t 4" \
            "./tspcc -f $FILENAME -t 8" \
            "./tspcc -f $FILENAME -t 16" \
            "./tspcc -f $FILENAME -t 32" \
            "./tspcc -f $FILENAME -t 64" \
            "./tspcc -f $FILENAME -t 128" \
            "./tspcc -f $FILENAME -t 256" \
            --export-json ./export.json

python3 ./graphs.py
