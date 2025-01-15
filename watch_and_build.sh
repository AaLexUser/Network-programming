#!/bin/bash
while true; do
    jupyter-book build .
    fswatch -1 ./parts ./_toc.yml ./_config.yml ./intro.md
done
