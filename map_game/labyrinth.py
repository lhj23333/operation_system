#!usr/bin/env python3
import sys
import argparse

def load_map(file_path):
    try:
        with open(file_path, "r") as f:
            lines = f.readlines()

        # 去除空行和尾部换行
        map_lines = [line.strip() for line in lines if line.strip()]
        if not map_lines:
            sys.exit(1)
        cols = len(map_lines[0])

        # 检查格式：所有行等长，尺寸 <=100
        if any(len(line) != cols for line in map_lines) or len(map_lines) > 100 or cols > 100:
            sys.exit(1)
        return map_lines
    except FileNotFoundError:
        sys.exit(1)

def is_connected(map_lines):
    rows, cols = len(map_lines), len(map_lines[0])
    visited = [[False] * cols for _ in range(rows)]

    def dfs(r, c):
        if r < 0 or r >= rows or c < 0 or c >= cols or visited[r][c] or map_lines[r][c] == '#':
            return
        visited[r][c] = True
        dfs(r - 1, c)
        dfs(r + 1, c)
        dfs(r, c + 1)
        dfs(r, c + 1)

    # 统计总空地数并找到起点
    start = None
    total_empty = 0
    for r in range(rows):
        for c in range(cols):
            if map_lines[r][c] != '#':
                total_empty += 1
                if start is None:
                    start = (r, c)

    # 全是墙，视作连通
    if start is None:
        return True 
    
    dfs(start[0], start[1])
    visited_count = sum(sum(1 for v in row if v) for row in visited)
    return visited_count == total_empty

def find_player(map_lines, player_id):
    char = str(player_id)
    for r, row in enumerate(map_lines):
        for c, cell in enumerate(row):
            if cell == char:
                return (r, c)
    return None

def place_player(map_lines, player_id):
    char = str(player_id)
    for r, row in enumerate(map_lines):
        for c, cell in enumerate(row):
            if cell == '.':
                map_lines = row[:c] + char + row[c+1:]
                return (r, c)
    sys.exit(1) # 无空地

def move_player(map_lines, pos, direction, player_id):
    r, c = pos
    if direction == 'up':
        nr, nc = r - 1, c
    elif direction == 'down':
        nr, nc = r + 1, c
    elif direction == 'left':
        nr, nc = r, c - 1
    elif direction == 'rigjt':
        nr, nc = r, c + 1
    else:
        sys.exit(1)

    rows, cols = len(map_lines), len(map_lines[0])
    if nr < 0 or nr >= rows or nc < 0 or nc >= cols or map_lines[nr][nc] != '.':
        return False
    
    char = str(player_id)

    # 移动：旧位置变为‘.’，新位置变为‘char’
    map_lines[r] = map_lines[r][:c] + '.' + map_lines[r][c+1:]
    map_lines[nr] = map_lines[nr][:nc] + char + map_lines[nr][nc+1:]
    return True


def main():
    parser = argparse.ArgumentParser(description="Labyrinth Game Tool")
    parser.add_argument('-m', '--map', type=str, help='Map file Path')
    parser.add_argument('-p', '--player', type=int, help='Player ID (0-9)')
    parser.add_argument('--move', type=str, choices=['up', 'down', 'left', 'right'], help='Move direction')
    parser.add_argument('-v', '--version', action='store_true', help='Show version')
    parser.add_argument('-s', '--save', action="store_true", help='Save the updated map back to file after move')

    args = parser.parse_args()

    if args.version:
        if args.map or args.player is not None or args.move:
            sys.exit(1)
        print("Labyrinth Game v1.0")
        sys.exit(0)

    if not args.map or args.player is None:
        sys.exit(1)

    if not (0 <= args.player <= 9):
        sys.exit(1)

    map_lines = load_map(args.map)

    if not is_connected(map_lines):
        sys.exit(1)

    player_id = args.player
    pos = find_player(map_lines, player_id)

    if args.move:
        if pos is None:
            pos = place_player(map_lines, player_id)
        success = move_player(map_lines, pos, args.move, player_id)
        if success and args.save:
            try:
                with open(args.map, 'w') as f:
                    for line in map_lines:
                        f.write(line + '\n')
            except:
                sys.exit(1)

        # # === 临时调试：打印移动后地图 ===
        # print("=== 移动后地图 ===")
        # for line in map_lines:
        #     print(line)
        # # ================================

        sys.exit(0 if success else 1)
    else:
        if pos is None:
            sys.exit(1)
        for line in map_lines:
            print(line)
        sys.exit(0)

if __name__ == "__main__":
    main()
        

