#!/bin/bash
while true; do
    jupyter-book build exam/
    fswatch -1 exam/parts exam/_toc.yml exam/_config.yml exam/intro.md
done
