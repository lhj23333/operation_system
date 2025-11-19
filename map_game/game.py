#!/usr/bin/env python3
import argparse
import sys
import os
from subprocess import Popen

C_EXEC = "./labyrinth"
DIRECTIONS = {"up", "down", "left", "right"}

def is_valid_direction(d: str) -> bool:
    return bool(d) and d.lower() in DIRECTIONS

def main():
    parser = argparse.ArgumentParser(description = "Welcome to the labyrinth game!")
    parser.add_argument('-m', '--map', type=str, help='Map file path')
    parser.add_argument('-p', '--player', type=int, help='Player ID(0 to 9)')
    parser.add_argument('-d', '--direction', type=str, help='Move direction(up, down, left, right)')
    parser.add_argument('-s', '--step', type=int, help='Number of steps to move')
    parser.add_argument('--version', action='store_true', help='Show version and exit')
    
    args = parser.parse_args()

    if args.version:
        if args.map or args.player is not None or args.direction or args.step is not None:
            sys.exit(1)
        print("Labyrinth Game v1.0.0")
        sys.exit(0)
    else:
        if not args.map or args.player is None:
            print("Error: Map file and Player ID are required.", file=sys.stderr)
            sys.exit(1)
        if not (0 <= args.player <= 9):
            print("Error: Player ID must be between 0 and 9.", file=sys.stderr)
            sys.exit(1)
        if args.direction and not is_valid_direction(args.direction):
            print("Error: Invalid direction. Must be one of 'up', 'down', 'left', 'right'.", file=sys.stderr)
            sys.exit(1)

    c_argv = [C_EXEC]
    c_argv.extend(['-m', args.map])
    c_argv.extend(['-p', str(args.player)])
    if args.direction:
        c_argv.extend(['-d', args.direction])
        c_argv.extend(['-s', str(args.step) if args.step is not None else '1'])

    try:
        os.execvp(C_EXEC, c_argv)
    except FileNotFoundError:
        print(f"Error: {C_EXEC} not found.", file=sys.stderr)
        sys.exit(1)
    except PermissionError:
        print(f"Error: Permission denied for {C_EXEC}.", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()        