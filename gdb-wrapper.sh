#!/bin/bash
# GDB wrapper script to avoid conda library conflicts
unset LD_LIBRARY_PATH
unset PYTHONHOME
unset PYTHONPATH
exec /usr/bin/gdb-multiarch "$@"
